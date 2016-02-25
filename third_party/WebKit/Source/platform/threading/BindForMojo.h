// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BindForMojo_h
#define BindForMojo_h

#include "base/bind.h"
#include "mojo/public/cpp/bindings/callback.h"

// This file provides limited access to base::Bind() function. base::Bind()
// could be dangerous if it's used across threads, so we don't want to allow
// general use of base::Bind().
// See also "platform/ThreadSafeFunctional.h".

namespace blink {

// Binds an instance of a class to its member function. Does not bind anything
// else.
template <typename Class, typename ReturnType, typename... Args>
mojo::Callback<ReturnType(Args...)>
sameThreadBindForMojo(ReturnType (Class::*method)(Args...), Class* instance)
{
    return base::Bind(method, base::Unretained(instance));
}

} // namespace blink

#endif // BindForMojo_h
