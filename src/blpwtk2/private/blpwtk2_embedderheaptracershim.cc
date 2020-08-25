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

#include <blpwtk2_embedderheaptracershim.h>

#include <cstdlib>

namespace blpwtk2 {

                        // ----------------------------
                        // class EmbedderHeapTracerShim
                        // ----------------------------

// CREATORS
EmbedderHeapTracerShim::EmbedderHeapTracerShim(
                                           blpwtk2::EmbedderHeapTracer *tracer)
: d_fields_p(nullptr)
, d_fieldsSize(0)
, d_tracer_p(tracer)
{
}

EmbedderHeapTracerShim::~EmbedderHeapTracerShim()
{
    std::free(d_fields_p);
}

// MANIPULATORS
void EmbedderHeapTracerShim::RegisterV8References(
                 const std::vector<std::pair<void *, void *>>& embedder_fields)
{
    const std::size_t fieldsSize = embedder_fields.size();

    if (fieldsSize > d_fieldsSize) {
        // If there are more fields than we have memory for, allocate a new
        // block of memory.

        std::free(d_fields_p);

        d_fields_p   = std::malloc(sizeof(void *) * 2 * fieldsSize);
        d_fieldsSize = fieldsSize;
    }

    void *(*fields)[2] = static_cast<void *(*)[2]>(d_fields_p);

    for (std::size_t index = 0; index < fieldsSize; ++index) {
        fields[index][0] = embedder_fields[index].first;
        fields[index][1] = embedder_fields[index].second;
    }

    d_tracer_p->RegisterV8ReferencesV(fields, fieldsSize);
}

}  // close blpwtk2 namespace

