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

#ifndef INCLUDED_BLPWTK2_EMBEDDERHEAPTRACERSHIM
#define INCLUDED_BLPWTK2_EMBEDDERHEAPTRACERSHIM

#include <blpwtk2_embedderheaptracer.h>
#include <v8.h>

#include <cstddef>
#include <utility>
#include <vector>

namespace blpwtk2 {

                        // ============================
                        // class EmbedderHeapTracerShim
                        // ============================

class EmbedderHeapTracerShim : public v8::EmbedderHeapTracer {
    // This class provides a shim around a 'blpwtk2::EmbedderHeapTracer',
    // converting the fields in the 'RegisterV8References' call from a
    // 'std::vector<std::pair<void *, void *>>' to a 'void *(*)[2]' and a
    // length.

    // DATA
    void                        *d_fields_p;    // Cached memory to avoid
                                                // repeated allocations.

    std::size_t                  d_fieldsSize;  // Size of 'd_fields_p'.

    blpwtk2::EmbedderHeapTracer *d_tracer_p;    // The wrapped tracer.

  public:
    // CREATORS
    EmbedderHeapTracerShim(blpwtk2::EmbedderHeapTracer *tracer);
        // Create a new 'EmbedderHeapTracerShim'.

    EmbedderHeapTracerShim(const EmbedderHeapTracerShim&) = delete;
    EmbedderHeapTracerShim(EmbedderHeapTracerShim&&)      = delete;
        // Not implemented.

    ~EmbedderHeapTracerShim() final;
        // Destroy this object.

    // MANIPULATORS
    EmbedderHeapTracerShim& operator=(const EmbedderHeapTracerShim&) = delete;
    EmbedderHeapTracerShim& operator=(EmbedderHeapTracerShim&&)      = delete;
        // Not implemented.

    bool AdvanceTracing(double deadline_in_ms) override;
        // Notify the wrapped tracer that it should advance tracing, by the
        // specified 'deadline_in_ms'.  If 'deadline_in_ms' is 'Infinity',
        // tracing should continue to completion.  Return 'true' if tracing is
        // done, and 'false' otherwise.

    void EnterFinalPause(EmbedderStackState stack_state) override;
        // Notify the wrapped tracer that we are entering the final pause.

    bool IsTracingDone() override;
        // Return 'true' if there is no more work to be done, and 'false'
        // otherwise.

    void RegisterV8References(
       const std::vector<std::pair<void *, void *>>& embedder_fields) override;
        // Notify the wrapped tracer of the embedder fields V8 has discovered
        // that may need to be traced.

    void TraceEpilogue() override;
        // Notify the wrapped tracer of the trace epilogue.

    void TracePrologue() override;
        // Notify the wrapped tracer of the trace prologue.
};

// ============================================================================
//                            INLINE DEFINITIONS
// ============================================================================

                        // ----------------------------
                        // class EmbedderHeapTracerShim
                        // ----------------------------

inline
bool EmbedderHeapTracerShim::AdvanceTracing(double deadline_in_ms)
{
    return d_tracer_p->AdvanceTracing(deadline_in_ms);
}

inline
void EmbedderHeapTracerShim::EnterFinalPause(EmbedderStackState stack_state)
{
    d_tracer_p->EnterFinalPause(stack_state);
}

inline
bool EmbedderHeapTracerShim::IsTracingDone()
{
    return d_tracer_p->IsTracingDone();
}

inline
void EmbedderHeapTracerShim::TraceEpilogue()
{
    d_tracer_p->TraceEpilogue();
}

inline
void EmbedderHeapTracerShim::TracePrologue()
{
    d_tracer_p->TracePrologue();
}

}  // close blpwtk2 namespace

#endif
