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

#include <cstdint>
#include <cstddef>

namespace mythos {

  struct StackFrame {
    StackFrame* bp;
    void* ret;
  };

  class stack_iterator {
  public:

    typedef StackFrame* reg_t;
    typedef StackFrame value_type;
    typedef value_type& reference_type;

    stack_iterator(reg_t bp) : _bp(bp) {}

    stack_iterator& operator++()
    {
      if (_bp->ret) {
        // no return address: end of stack
        _bp = _bp->bp;
      } else {
        _bp = nullptr;
      }
      return *this;
    }

    stack_iterator operator++(int)
    {
      stack_iterator tmp(*this);
      ++tmp;
      return tmp;
    }

    reference_type operator*()
    {
      return *(_bp);
    }

    bool operator==(const stack_iterator& other) const
    {
      return other._bp == _bp;
    }
    bool operator!=(const stack_iterator& other) const { return !(*this == other); }

  private:
    reg_t _bp;
  };

  class StackTrace {
  public:

    typedef stack_iterator::reg_t reg_t;

    StackTrace() { asm volatile ("mov %%rbp, %0\n\t" : "=r" (_rbp)); }
    StackTrace(reg_t bp) : _rbp(bp) {}
    StackTrace(void* bp) : _rbp(reinterpret_cast<reg_t>(bp)) {}
    StackTrace(uintptr_t bp) : _rbp(reinterpret_cast<reg_t>(bp)) {}

    StackTrace(const StackTrace&) = delete;

    stack_iterator begin() const
    {
      return stack_iterator(_rbp);
    }

    stack_iterator end() const
    {
      return stack_iterator(nullptr);
    }

  private:

    reg_t _rbp;
  };

} // namespace mythos
