/*
 * Copyright (C) 2018 Bloomberg Finance L.P.
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

#ifndef INCLUDED_BLPWTK2_EMBEDDERHEAPTRACER
#define INCLUDED_BLPWTK2_EMBEDDERHEAPTRACER

#include <cstddef>
#include <v8.h>

namespace blpwtk2 {

                          // ========================
                          // class EmbedderHeapTracer
                          // ========================

class EmbedderHeapTracer : public v8::EmbedderHeapTracer {
    // The v8::EmbedderHeapTracer protocol wants to pass fields as a
    // 'std::vector<std::pair<void *, void *>>'.  This is an issue when the
    // 'EmbedderHeapTracer' implementation lives in a different dll/executable,
    // as it is unsafe to pass this type across the boundary unless all
    // dll/executables were built with exactly the same toolchain.
    // To work around this, 'blpwtk2' provides an extended protocol, which
    // passes the fields as a 'pointer to an array of two void *' and a length.

  public:
    // MANIPULATORS
    virtual void RegisterV8ReferencesV(void        *(*embedder_fields)[2],
                                       std::size_t    length) = 0;
};

}  // close blpwtk2 namespace

#endif
