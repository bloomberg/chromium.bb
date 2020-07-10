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

#include <blpwtk2_iatpatchfunction.h>

#include <base/win/iat_patch_function.h>

namespace blpwtk2 {

struct IATPatchFunction::Impl {
    base::win::IATPatchFunction d_baseImpl;
};

IATPatchFunction::IATPatchFunction()
: d_impl(new Impl())
{
}

IATPatchFunction::~IATPatchFunction()
{
    delete d_impl;
}

DWORD IATPatchFunction::patch(const wchar_t* module,
                              const char* importedFromModule,
                              const char* functionName,
                              void* newFunction)
{
    return d_impl->d_baseImpl.Patch(module,
                                    importedFromModule,
                                    functionName,
                                    newFunction);
}

DWORD IATPatchFunction::unpatch()
{
    return d_impl->d_baseImpl.Unpatch();
}

bool IATPatchFunction::isPatched() const
{
    return d_impl->d_baseImpl.is_patched();
}

void* IATPatchFunction::originalFunction() const
{
    return d_impl->d_baseImpl.original_function();
}

}  // close namespace blpwtk2


// vim: ts=4 et

