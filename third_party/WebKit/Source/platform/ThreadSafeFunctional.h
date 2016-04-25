// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ThreadSafeFunctional_h
#define ThreadSafeFunctional_h

#include "platform/CrossThreadCopier.h"
#include "wtf/Functional.h"
#include <type_traits>

namespace blink {

// threadSafeBind() is bind() for cross-thread task posting.
// threadSafeBind() applies CrossThreadCopier to the arguments.
//
// Example:
//     void func1(int, const String&);
//     f = threadSafeBind(func1, 42, str);
// func1(42, str2) will be called when |f()| is executed,
// where |str2| is a deep copy of |str| (created by str.isolatedCopy()).
//
// threadSafeBind(str) is similar to bind(str.isolatedCopy()), but the latter
// is NOT thread-safe due to temporary objects (https://crbug.com/390851).
//
// Don't (if you pass the task across threads):
//     bind(func1, 42, str);
//     bind(func1, 42, str.isolatedCopy());

template<typename... FreeVariableTypes, typename FunctionType, typename... Ps>
PassOwnPtr<Function<typename WTF::FunctionWrapper<FunctionType>::ResultType(FreeVariableTypes...), WTF::CrossThreadAffinity>> threadSafeBind(
    FunctionType function,
    Ps&&... parameters)
{
    return WTF::bindInternal<WTF::CrossThreadAffinity, FreeVariableTypes...>(
        function,
        CrossThreadCopier<typename std::decay<Ps>::type>::copy(std::forward<Ps>(parameters))...);
}

} // namespace blink

#endif // ThreadSafeFunctional_h
