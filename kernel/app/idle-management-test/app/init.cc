/* -*- mode:C++; indent-tabs-mode:nil; -*- */
/* MyThOS: The Many-Threads Operating System
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright 2016 Randolf Rotta, Robert Kuban, and contributors, BTU Cottbus-Senftenberg
 */

#include "mythos/init.hh"
#include "mythos/invocation.hh"
#include "runtime/Portal.hh"
#include "runtime/ExecutionContext.hh"
#include "runtime/CapMap.hh"
#include "runtime/Example.hh"
#include "runtime/PageMap.hh"
#include "runtime/KernelMemory.hh"
#include "runtime/SimpleCapAlloc.hh"
#include "app/mlog.hh"
#include <cstdint>
#include "util/optional.hh"

#define NUM_RUNS 100
#define PARALLEL_ECS 240
#define AVAILABLE_HWTS 240

mythos::InvocationBuf* msg_ptr asm("msg_ptr");
int main() asm("main");

constexpr uint64_t stacksize = AVAILABLE_HWTS*4096;
char initstack[4096];
char* initstack_top = initstack+4096;

mythos::Portal myPortal(mythos::init::PORTAL, msg_ptr);
mythos::CapMap myCS(mythos::init::CSPACE);
mythos::PageMap myAS(mythos::init::PML4);
mythos::KernelMemory kmem(mythos::init::KM);
mythos::SimpleCapAllocDel capAlloc(myPortal, myCS, mythos::init::APP_CAP_START,
    mythos::init::SIZE-mythos::init::APP_CAP_START);

char threadstack[stacksize];
char* threadstack_top = threadstack+stacksize;

mythos::CapPtr portals[AVAILABLE_HWTS-1];
mythos::InvocationBuf* invocationBuffers[AVAILABLE_HWTS-1];
mythos::CapPtr exampleObject;

std::atomic<bool> passable {false};

uint64_t getTime(){
  uint64_t hi, lo;
  asm volatile("rdtsc":"=a"(lo), "=d"(hi));
  return ((uint64_t)lo)|( ((uint64_t)hi)<<32);
}

void thread_mobileKernelObjectLatency(mythos::PortalLock pl, mythos::CapPtr exampleCap){
//  MLOG_ERROR(mlog::app, "Mobile Thread Latency Start");
  while(passable.load() == false) {}


  mythos::Example example(exampleCap);

  uint64_t sum_mid = 0, sum_end = 0;
  //Invocation latency to mobile kernel object
  uint64_t start, mid, end;
  for (size_t i = 0; i < NUM_RUNS; i++){
    start = getTime();
    auto res1 = example.ping(pl, 1e5);
    mid = getTime();
    res1.wait();
    ASSERT(res1);
    end = getTime();
    sum_mid += mid - start;
    sum_end += end - start;
    if (i % 10 == 0) {
      //MLOG_ERROR(mlog::app, "object location:", ib->cast<mythos::protocol::Example::Ping>()->place);
      //MLOG_ERROR(mlog::app, "ECs: ", PARALLEL_ECS, " t_return: ", mid - start, " t_reply: ", end - start, " obj: ");
     // MLOG_ERROR(mlog::app, "duration until first return: ", mid - start);
     // MLOG_ERROR(mlog::app, "duration until reply: ", end - start);
    }
  }
  MLOG_ERROR(mlog::app, "ECs: ", PARALLEL_ECS, " t_return: ", sum_mid / NUM_RUNS, " t_reply: ", sum_end / NUM_RUNS, " obj: ");
}

void* thread_invocationLatencyMain(void* ctx)
{
  size_t ownid = (size_t)ctx;


  mythos::Portal portal(portals[ownid], invocationBuffers[ownid]);

  mythos::PortalLock pl(portal);

  thread_mobileKernelObjectLatency(pl, exampleObject);

  return 0;
}

void* thread_main(void* ctx){
  return 0;
}

void mobileKernelObjectLatency(){

  mythos::PortalLock pl(myPortal);
  //Create a mobile kernel object
  mythos::Example example(capAlloc());
  auto res1 = example.create(pl, kmem);
  res1.wait();
  ASSERT(res1);

  exampleObject = example.cap();

  res1 = example.ping(pl,0);
  res1.wait();
  ASSERT(res1);

  mythos::CapPtr createdECs[PARALLEL_ECS-1];

  //Create (parallel_ecs - 1) additional ECs
  for (size_t worker = 0; worker < PARALLEL_ECS - 1; worker++){
    //Create a new invocation buffer
    mythos::Frame new_msg_ptr(capAlloc());
    res1 = new_msg_ptr.create(pl, kmem, 1<<21, 1<<21);
    res1.wait();
    ASSERT(res1);

    //Map the invocation buffer frame into user space memory
    mythos::InvocationBuf* ib = (mythos::InvocationBuf*)(((11+worker)<<21));
    auto res2 = myAS.mmap(pl, new_msg_ptr, (uintptr_t)ib, 1<<21, mythos::protocol::PageMap::MapFlags().writable(true));
    res2.wait();
    ASSERT(res2);
    invocationBuffers[worker] = ib;

    //Create a second portal
    mythos::Portal portal2(capAlloc(), ib);
    res1 = portal2.create(pl, kmem);
    res1.wait();
    ASSERT(res1);
    portals[worker] = portal2.cap();

    //Create a second EC on its own HWT
    createdECs[worker] = capAlloc();
    mythos::ExecutionContext ec(createdECs[worker]);
    res1 = ec.create(pl, kmem,
        myAS, myCS, mythos::init::SCHEDULERS_START+1+worker,
        initstack_top+stacksize-4096*worker, &thread_invocationLatencyMain, (void*)worker);
    res1.wait();
    ASSERT(res1);

    //Bind the new portal to the new EC
    res1 = portal2.bind(pl, new_msg_ptr, 0, ec.cap());
    res1.wait();
    ASSERT(res1);
  }

  //Run all created ECs
  passable.store(true);
}

void localKernelObjectLatency(){

  mythos::PortalLock pl(myPortal);
  //Create a "homed" example object
  mythos::ExampleHome example(capAlloc());
  auto res1 = example.create(pl, kmem);
  res1.wait();
  ASSERT(res1);

  uint64_t start, end;

  //Measure the invocation latency to all available HWTs
  for(size_t place = 0; place < AVAILABLE_HWTS; place++){

    res1 = example.moveHome(pl, place);
    res1.wait();
    ASSERT(res1);

    uint64_t sum = 0;
    for (size_t i = 0; i < NUM_RUNS; i++){
      start = getTime();
      ASSERT(example.ping(pl,0).wait());
      end = getTime();
      if (end < start){
        i--;
        continue;
      }
      sum += end - start;
    }
    MLOG_ERROR(mlog::app, "Invocation Latency to HWT ", place, " : ", sum/NUM_RUNS);
    sum = 0;
  }
}


void benchmarks(){
  //invocation latency to mobile kernel object
  //mobileKernelObjectLatency();

  //invocation latency to local kernel object
  localKernelObjectLatency();
}

int main()
{
  char const begin[] = "Starting the benchmarks";
  char const end[] = "Benchmarks are done";
  mythos::syscall_debug(begin, sizeof(begin)-1);

  benchmarks();

  mythos::syscall_debug(end, sizeof(end)-1);

  return 0;
}