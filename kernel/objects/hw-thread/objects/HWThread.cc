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

#include "cpu/hwthreadid.hh"
#include "objects/HWThread.hh"
#include "objects/mlog.hh"
#include "util/Time.hh"
#include "cpu/LAPIC.hh"
#include "cpu/idle.hh"
#include "cpu/SleepEmulator.hh"

namespace mythos {

optional<void> HWThread::deleteCap(Cap self, IDeleter& del) {
    if (self.isOriginal()) {
        del.deleteObject(del_handle);
    }
    RETURN(Error::SUCCESS);
}

optional<void const*> HWThread::vcast(TypeId id) const {
    if (id == typeId<IKernelObject>()) return static_cast<IKernelObject const*>(this);
    if (id == typeId<HWThread>()) return this;
    THROW(Error::TYPE_MISMATCH);
}

void HWThread::invoke(Tasklet* t, Cap self, IInvocation* msg)
{
    monitor.request(t, [ = ](Tasklet * t) {
        Error err = Error::NOT_IMPLEMENTED;
        switch (msg->getProtocol()) {
            case protocol::KernelObject::proto:
                err = protocol::KernelObject::dispatchRequest(this, msg->getMethod(), self, msg);
                break;
            case protocol::HWThread::proto:
                err = protocol::HWThread::dispatchRequest(this, msg->getMethod(), t, self, msg);
                break;
        }
        if (err != Error::INHIBIT) {
            msg->replyResponse(err);
            monitor.requestDone();
        }
    } );
}

Error HWThread::getDebugInfo(Cap self, IInvocation* msg)
{
    return writeDebugInfo("HWThread", self, msg);
}

// usefull protocol implementations

Error HWThread::printMessage(Tasklet*, Cap self, IInvocation* msg)
{
    MLOG_DETAIL(mlog::boot, "invoke printMessage", DVAR(this), DVAR(self), DVAR(msg));
    auto data = msg->getMessage()->cast<protocol::HWThread::PrintMessage>();
    MLOG_ERROR(mlog::boot, mlog::DebugString(data->message, data->bytes));
    return Error::SUCCESS;
}

Error HWThread::setPolicy(Tasklet*, Cap self, IInvocation* msg)
{
    MLOG_INFO(mlog::boot, "invoke set policy", DVAR(this), DVAR(self), DVAR(msg));
    auto data = msg->getMessage()->cast<protocol::HWThread::SpinPolicy>();
    policy = (Policy)data->policy;
    return Error::SUCCESS;
}

extern SleepEmulator emu;
Error HWThread::readSleepState(Tasklet*, Cap, IInvocation *msg) {
    MLOG_DETAIL(mlog::boot, "Read Sleep State");
    auto respData = msg->getMessage()->write<protocol::HWThread::WriteSleepState>();
    auto sleepState = emu.getSleepState(getApicID());
    respData->sleep_state = sleepState;
    return Error::SUCCESS;
}

// actual functionality

void HWThread::runConfigurableDelays() {
  if (idle->shouldDeepSleep()) {
        MLOG_DETAIL(mlog::boot, "Timer interrupt triggered and no new work so far");
        releaseKernel();
        idle->sleep();
    }
    localPlace->processTasks(); // executes all available kernel tasks
    tryRunUser();
    if (idle->getDelayPolling() > 100) { // skip two small poling delays due to performance
      uint64_t start = getTime();
      // one pass of the poll loop takes about 480-500 cycles
      while (idle->alwaysPoll() || start + idle->getDelayPolling() > getTime()) { // poll configured delay
        localPlace->enterKernel();
        localPlace->processTasks();
        tryRunUser();
        mythos::hwthread_pause(500);
        preemption_point(); // allows interrupts even if polling only policy
      }
    }
    releaseKernel();
    MLOG_DETAIL(mlog::boot, DVAR(idle->getDelayLiteSleep()));
    idle->sleep();
    __builtin_unreachable();
}

void HWThread::runSleep() {
    localPlace->enterKernel();
    localPlace->processTasks(); // executes all available kernel tasks
    releaseKernel();
    tryRunUser();
    releaseKernel();
    mythos::idle::sleep(1);
}

void HWThread::runSpin() {
    while (true) {
        localPlace->enterKernel();
        localPlace->processTasks();
        tryRunUser();
        releaseKernel();
        preemption_point(); // allows interrupts even if polling only policy
        mythos::hwthread_pause(500);
    }
}

void HWThread::tryRunUser() {
  auto *ec = localSchedulingContext->tryRunUser();
  if (ec) {
    if (ec->prepareResume()) {
      releaseKernel();
      ec->doResume(); //does not return (hopefully)
      MLOG_ERROR(mlog::boot, "Returned even prepareResume was successful");
      mythos::idle::sleep(1);
    }
  }
}

void HWThread::releaseKernel() {
  while (not localPlace->releaseKernel()) { // release kernel
      localPlace->processTasks();
  }
}

} // namespace mythos
