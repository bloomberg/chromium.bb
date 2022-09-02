/*
 * Copyright (C) 2015 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef INCLUDED_BLPWTK2_IATPATCHFUNCTION_H
#define INCLUDED_BLPWTK2_IATPATCHFUNCTION_H

#include <blpwtk2_config.h>

namespace blpwtk2 {

// Wraps base::win::IATPatchFunction from the Chromium source code.
class BLPWTK2_EXPORT IATPatchFunction {
  public:
    IATPatchFunction();
    ~IATPatchFunction();

    DWORD patch(const wchar_t* module,
                const char* importedFromModule,
                const char* functionName,
                void* newFunction);

    DWORD unpatch();

    bool isPatched() const;
    void* originalFunction() const;

  private:
    struct Impl;
    Impl* d_impl;

    IATPatchFunction(const IATPatchFunction&);
    IATPatchFunction& operator=(const IATPatchFunction&);
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_IATPATCHFUNCTION_H

// vim: ts=4 et

