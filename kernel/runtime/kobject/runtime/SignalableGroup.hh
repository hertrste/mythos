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

#include "runtime/PortalBase.hh"
#include "mythos/protocol/SignalableGroup.hh"
#include "runtime/KernelMemory.hh"
#include "mythos/init.hh"

namespace mythos {

class SignalableGroup : public KObject
{
public:
    SignalableGroup(CapPtr cap) : KObject(cap) {}

    PortalFuture<void> create(PortalLock pr, KernelMemory kmem,
            size_t groupSize, CapPtr factory = init::SIGNALABLE_GROUP_FACTORY) {
        return pr.invoke<protocol::SignalableGroup::Create>(kmem.cap(), _cap, factory, groupSize);
    }

    PortalFuture<void> addMember(PortalLock pr, CapPtr signalable) {
      return pr.invoke<protocol::SignalableGroup::AddMember>(_cap, signalable);
    }

    PortalFuture<void> removeMember(PortalLock pr, CapPtr signalable) {
      return pr.invoke<protocol::SignalableGroup::RemoveMember>(_cap, signalable);
    }

    PortalFuture<void> signalAll(PortalLock pr) {
        return pr.invoke<protocol::SignalableGroup::SignalAll>(_cap);
    }
};

} // namespace mythos