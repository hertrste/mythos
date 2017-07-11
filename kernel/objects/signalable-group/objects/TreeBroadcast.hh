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
#include "objects/CapRef.hh"
#include "objects/mlog.hh"


namespace mythos {

class SignalableGroup;

struct TreeBroadcast
{
    Error operator()(CapRef<SignalableGroup, ISignalable> *group, size_t groupSize) {
        MLOG_ERROR(mlog::boot, "signalAll()", DVAR(group), DVAR(groupSize));
        ASSERT(group != nullptr);
        for (uint64_t i = 0; i < groupSize; i++) {
            if (group[i].isUsable()) {
                TypedCap<ISignalable> signalable(group[i].cap());
                if (signalable) {
                    signalable.obj()->signal(0);
                }
            }
        }

        return Error::SUCCESS;
    }
};
} // namespace mythos