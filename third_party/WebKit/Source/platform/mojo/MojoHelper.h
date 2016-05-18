// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MojoHelper_h
#define MojoHelper_h

#include "base/bind.h"
#include "mojo/public/cpp/bindings/callback.h"
#include "mojo/public/cpp/bindings/wtf_array.h"
#include "platform/heap/HeapAllocator.h"
#include <utility>

namespace blink {

namespace internal {

template <typename R, typename... Args>
R CallWTFFunction(Function<R(Args...)>* functor, Args... args)
{
    return (*functor)(std::forward<Args>(args)...);
}

}

template <typename R, typename... Args>
base::Callback<R(Args...)> createBaseCallback(PassOwnPtr<Function<R(Args...)>> functor)
{
    return base::Bind(&internal::CallWTFFunction<R, Args...>, base::Owned(functor.leakPtr()));
}

template <typename R, typename... Args>
base::Callback<R(Args...)> createBaseCallback(std::unique_ptr<Function<R(Args...)>> functor)
{
    return base::Bind(&internal::CallWTFFunction<R, Args...>, base::Owned(functor.release()));
}

} // namespace blink

namespace mojo {

// A |TypeConverter| that will create a |WTFArray<T>| containing a copy of the
// contents of a |blink::HeapVector<E>|, using |TypeConverter<T, E>| to copy
// each element. The returned array will always be non-null.
template <typename T, typename E>
struct TypeConverter<WTFArray<T>, blink::HeapVector<E>> {
    static WTFArray<T> Convert(const blink::HeapVector<E>& input)
    {
        WTFArray<T> result(input.size());
        for (size_t i = 0; i < input.size(); ++i)
            result[i] = TypeConverter<T, E>::Convert(input[i]);
        return std::move(result);
    }
};

} // namespace mojo

#endif // MojoHelper_h
