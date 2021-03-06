/* -*- mode:C++; -*- */
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
 * Copyright 2014 Randolf Rotta, Maik Krüger, and contributors, BTU Cottbus-Senftenberg
 */
#pragma once

#include "util/streambuf.hh"
#include "util/mstring.hh" // for memcpy, strlen

namespace mythos {

  template<size_t S>
  class FixedStreamBuf
    : public streambuf
  {
  public:
    FixedStreamBuf()
      : used(0)
    {
      buf[S] = 0;
    }
    virtual ~FixedStreamBuf() {}

    size_t size() const { return used; }
    char const* c_str() const { return buf; }
    char* c_str() { return buf; }

    int sputc(char c) {
      if (used<S) buf[used++] = c;
      return c;
    }

    mythos::streamsize sputn(char const* s, mythos::streamsize count) {
      size_t len = (count<= S-used) ? count : S-used;
      memcpy(buf+used, s, len);
      used += len;
      return len;
    }

    void putcstr(char const* s) { sputn(s, strlen(s)); }

    template<size_t N>
    void putcstr(const char (&s)[N]) { sputn(s, N); }

  protected:
    void sync() { buf[used] = 0; }

  protected:
    virtual int _sputc(char c) { return sputc(c); }
    virtual mythos::streamsize _sputn(char const* s, mythos::streamsize count) { return sputn(s,count); }
    virtual void _putcstr(char const* s) { putcstr(s); }
    virtual void _sync() { sync(); }

  protected:
    size_t used;
    char buf[S+1];
  };

} // namespace mythos
