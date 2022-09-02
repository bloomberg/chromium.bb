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

#include <blpwtk2_measurememoryutil.h>

namespace blpwtk2 {

                  // =======================================
                  // class MeasureMemoryUtil_DelegateAdapter
                  // =======================================

class MeasureMemoryUtil_DelegateAdapter : public v8::MeasureMemoryDelegate {
    // This class provides an implementation of the C++ interface
    // 'v8::MeasureMemoryDelegate' in terms of an interface that does not use
    // any STL types.  This permits components linked against different C++
    // standard libraries to interoperate with the
    // 'v8::Isolate::MeasureMemory(...)' member function.

    // TYPES
    using ContextSizePair = std::pair<v8::Local<v8::Context>, size_t>;
    using ContextAndSize  = MeasureMemoryUtil::ContextAndSize;

    using Delegate = MeasureMemoryUtil::Delegate;
    using Deleter  = MeasureMemoryUtil::Deleter;

    // DATA
    std::unique_ptr<Delegate, Deleter> d_delegate;
        // This holds the delegate and the deleter function. Note that the
        // deleter may use a different C Runtime than the one this object is
        // linked against, so it should be used to delete the object.

  public:
    // CREATORS
    MeasureMemoryUtil_DelegateAdapter(Delegate *delegate, Deleter deleter);
        // Create this object, ensuring that the specified 'delegate' is
        // destroyed before this object is destroyed using the specified
        // 'deleter'.

    ~MeasureMemoryUtil_DelegateAdapter() override;
        // Destroy this object and the 'delegate' this object was constructed
        // with.

    // MODIFIERS
    bool ShouldMeasure(v8::Local<v8::Context> context) override;
        // Delegate the decision on measuring the memory used by the specified
        // 'context' to the delegate this object was created with.

    void MeasurementComplete(
                     const std::vector<ContextSizePair>& contextSizePairs,
                     size_t                              sharedUsage) override;
        // Notify the delegate this object was constructed with of the memory
        // usage given by the specified 'contextsAndSizes' and the specified
        // 'sharedUsage'.
};

                          // -----------------------
                          // class MeasureMemoryUtil
                          // -----------------------

// CLASS METHODS
void MeasureMemoryUtil::measure(v8::Isolate *isolate,
                                Delegate    *delegate,
                                Deleter      deleter)
{
    using Adapter = MeasureMemoryUtil_DelegateAdapter;
    isolate->MeasureMemory(std::make_unique<Adapter>(delegate, deleter));
}

                  // ---------------------------------------
                  // class MeasureMemoryUtil::ContextAndSize
                  // ---------------------------------------

// CREATORS
MeasureMemoryUtil::ContextAndSize::ContextAndSize(
                                         v8::Local<v8::Context> context,
                                         size_t                 exclusiveUsage)
: d_context(context)
, d_exclusiveUsage(exclusiveUsage)
{
}

                     // ---------------------------------
                     // class MeasureMemoryUtil::Delegate
                     // ---------------------------------

// CREATORS
MeasureMemoryUtil::Delegate::~Delegate()
{
}

                  // ---------------------------------------
                  // class MeasureMemoryUtil_DelegateAdapter
                  // ---------------------------------------

// CREATORS
MeasureMemoryUtil_DelegateAdapter::MeasureMemoryUtil_DelegateAdapter(
                                                            Delegate *delegate,
                                                            Deleter   deleter)
: d_delegate(delegate, deleter)
{
}

MeasureMemoryUtil_DelegateAdapter::~MeasureMemoryUtil_DelegateAdapter()
{
}

// MODIFIERS
bool MeasureMemoryUtil_DelegateAdapter::ShouldMeasure(
                                                v8::Local<v8::Context> context)
{
    return d_delegate->shouldMeasure(context);
}

void MeasureMemoryUtil_DelegateAdapter::MeasurementComplete(
                          const std::vector<ContextSizePair>& contextSizePairs,
                          size_t                              sharedUsage)
{
    std::vector<MeasureMemoryUtil::ContextAndSize> contextsAndSizes;
    contextsAndSizes.reserve(contextSizePairs.size());

    for (const ContextSizePair& contextSizePair: contextSizePairs) {
        contextsAndSizes.emplace_back(contextSizePair.first,
                                      contextSizePair.second);
    }

    d_delegate->notifyMemoryUsage(contextsAndSizes.size(),
                                  contextsAndSizes.data(),
                                  sharedUsage);
}

}  // close namespace blpwtk2

// vim: ts=4 et
