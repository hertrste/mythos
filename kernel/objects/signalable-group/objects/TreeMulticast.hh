/* -*- mode:C++; indent-tabs-mode:nil; -*- */
/* MIT License -- MyThOS: The Many-Threads Operating System
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
#pragma once


#include "objects/ISignalable.hh"
#include "objects/ExecutionContext.hh"
#include "objects/CapRef.hh"
#include "objects/mlog.hh"
#include "util/Time.hh"


namespace mythos {

struct TreeMulticastFun {
  void operator()(Tasklet *t, SignalableGroup *group, size_t idx, size_t groupSize) {
    t->set([=] (Tasklet *t) {
        //MLOG_ERROR(mlog::boot, "in Monitor", DVAR(idx), DVAR(groupSize));
        TypedCap<ISignalable> own(group->getMember(idx)->cap());
        ASSERT(own);
        ASSERT(group != nullptr);
        ASSERT(groupSize > 0);
        // Signal own EC, will be scheduled after kernel task handling
        own->signal(0);
        size_t N = 2;
        //MLOG_ERROR(mlog::boot, DVAR(idx), DVAR(N));
        for (size_t i = 0; i < N; ++i) { // for all children in tree
            ASSERT(N != 0);
            size_t child_idx = idx * N + i + 1;
            if (child_idx >= groupSize) return;
            //MLOG_ERROR(mlog::boot, "broadcast child", DVAR(child_idx), DVAR(groupSize));
            TypedCap<ISignalable> signalable(group->getMember(child_idx)->cap());
            if (signalable) {
                //MLOG_ERROR(mlog::boot, "forward broadcast", DVAR(groupSize), DVAR(child_idx));
                signalable->broadcast(group->getTasklet(child_idx), group, child_idx, groupSize);
                signalable->signal(0);
            } else {
                PANIC("Signalable not valid anymore");
            }
        }
    });
  }
};


class SignalableGroup;
std::atomic<uint64_t> counter {0};
class TreeMulticast
{
public:
    static Error multicast(SignalableGroup *group, size_t idx, size_t groupSize) {
        ASSERT(group != nullptr);
        TypedCap<ISignalable> signalable(group->getMember(0)->cap());
        if (signalable) {
            signalable->broadcast(group->getTasklet(0), group, 0, groupSize);
        }
        /* OLD VERSION
        //MLOG_ERROR(mlog::boot, "signalAll()", DVAR(group), DVAR(groupSize));

        uint64_t start, end, tmp = 0;
        start = getTime();

        TypedCap<ISignalable> signalable(group[0].cap());
        if (signalable) {
            signalable->bc.set(group, groupSize, 0, N_ARY_TREE);
            signalable->signal(0);
        }
        while (counter.load() < groupSize) {
            hwthread_pause();
            if (tmp != counter.load()) {
                tmp = counter.load();
                //MLOG_ERROR(mlog::boot, DVAR(tmp));
            }

        }

        end = getTime();
        MLOG_ERROR(mlog::boot, DVAR(end - start));
        counter.store(0);
        */
        return Error::SUCCESS;
    }
private:
    static constexpr uint64_t N_ARY_TREE = 2;
};



} // namespace mythos
