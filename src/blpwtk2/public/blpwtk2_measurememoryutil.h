/*
 * Copyright (C) 2022 Bloomberg Finance L.P.
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

#pragma once

#include <blpwtk2_config.h>
#include <v8.h>

namespace blpwtk2 {

                          // =======================
                          // class MeasureMemoryUtil
                          // =======================

struct BLPWTK2_EXPORT MeasureMemoryUtil {
    // This class provides a 'struct namespace' of a utility function that can
    // measure memory. In particular it provides an STL-free API surface
    // permitting components linked against different STLs to interoperate.

    // TYPES

                  // =======================================
                  // class MeasureMemoryUtil::ContextAndSize
                  // =======================================

    struct ContextAndSize {
        // This utility struct pacakges a context along with the amount of
        // exclusive memory it has used in bytes.

        // DATA
        v8::Local<v8::Context> d_context;
            // The context in question.

        size_t                 d_exclusiveUsage;
            // The memory this context exclusively uses in bytes.

        // CREATORS
        ContextAndSize(const v8::Local<v8::Context>& context,
                       size_t                        exclusiveUsage);
            // Create this object with the specified 'context' and
            // 'exclusiveUsage'.
    };

                     // =================================
                     // class MeasureMemoryUtil::Delegate
                     // =================================

    class Delegate {
        // This abstract interface should be implemented by callers interested
        // in measuring memory used by a subset of the contexts within a
        // 'v8::Isolate'. This delegate is used by 'MeasureMemory' to determine
        // the subset of contexts and this delegate is notified of the memory
        // usage when the measurement is complete.

      public:
        // CREATORS
        virtual ~Delegate();
            // Destroy this object.

        // MODIFIERS
        virtual bool shouldMeasure(v8::Local<v8::Context> context) = 0;
            // Return 'true' if the specified 'context' should be included in
            // the memory usage measurement.

        virtual void notifyMemoryUsage(size_t                numItems,
                                       const ContextAndSize *contextsAndSizes,
                                       size_t                sharedUsage) = 0;
            // Notify this object of the memory usage of the contexts this
            // object denoted interest in by returning 'true' in previous
            // invocations of 'ShouldMeasure'.
            //
            // The specified 'numItems' indicates the number of such
            // 'contexts'.  The specified 'contextsAndSizes' is an array of
            // length 'numItems'. Each entry in the array specifies the amount
            // of memory exclusively used in bytes by the corresponding
            // context.
            //
            // Additionally, all such contexts share usage of the amounting to
            // the specified number of 'sharedUsage' bytes.
    };

    using Deleter = void (*)(Delegate *);

    // CLASS METHODS
    static void measure(v8::Isolate *isolate,
                        Delegate    *delegate,
                        Deleter      deleter);
        // Measure the memory used by contexts in the specified 'isolate',
        // using the specified 'delegate' to decide which contexts to consider
        // and to notify when measurement is complete (see the documentation of
        // 'MeasureMemoryDelegate' for more information). Use the specified
        // 'deleter' to delete 'delegate' once the measurement is complete.
        //
        // This interface specifically provides an STL-free API surface
        // permitting components linked against different STLs to interoperate.
};

}  // close namespace blpwtk2
