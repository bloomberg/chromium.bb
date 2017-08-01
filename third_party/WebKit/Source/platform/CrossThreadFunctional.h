// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CrossThreadFunctional_h
#define CrossThreadFunctional_h

#include <type_traits>
#include "base/bind.h"
#include "platform/CrossThreadCopier.h"
#include "platform/wtf/Functional.h"

namespace blink {

// CrossThreadBind() is Bind() for cross-thread task posting.
// CrossThreadBind() applies CrossThreadCopier to the arguments.
//
// Example:
//     void Func1(int, const String&);
//     f = CrossThreadBind(&Func1, 42, str);
// Func1(42, str2) will be called when |f()| is executed,
// where |str2| is a deep copy of |str| (created by str.IsolatedCopy()).
//
// CrossThreadBind(str) is similar to Bind(str.IsolatedCopy()), but the latter
// is NOT thread-safe due to temporary objects (https://crbug.com/390851).
//
// Don't (if you pass the task across threads):
//     Bind(&Func1, 42, str);
//     Bind(&Func1, 42, str.IsolatedCopy());

template <typename FunctionType, typename... Ps>
Function<base::MakeUnboundRunType<FunctionType, Ps...>,
         WTF::kCrossThreadAffinity>
CrossThreadBind(FunctionType function, Ps&&... parameters) {
  return WTF::BindInternal<WTF::kCrossThreadAffinity>(
      function, CrossThreadCopier<typename std::decay<Ps>::type>::Copy(
                    std::forward<Ps>(parameters))...);
}

}  // namespace blink

#endif  // CrossThreadFunctional_h
