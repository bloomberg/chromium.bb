// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MojoHelper_h
#define MojoHelper_h

#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/bindings/wtf_array.h"
#include "platform/heap/HeapAllocator.h"
#include <utility>

namespace mojo {

// A |TypeConverter| that will create a |WTFArray<T>| containing a copy of the
// contents of a |blink::HeapVector<E>|, using |TypeConverter<T, E>| to copy
// each element. The returned array will always be non-null.
template <typename T, typename E>
struct TypeConverter<WTFArray<T>, blink::HeapVector<E>> {
  static WTFArray<T> Convert(const blink::HeapVector<E>& input) {
    WTFArray<T> result(input.size());
    for (size_t i = 0; i < input.size(); ++i)
      result[i] = TypeConverter<T, E>::Convert(input[i]);
    return std::move(result);
  }
};

}  // namespace mojo

namespace blink {

// Used to get whether message loop is ready for current thread, to help
// blink::initialize() determining whether can initialize mojo stuff or not.
// TODO(leonhsl): http://crbug.com/660274 Remove this API by ensuring
// a message loop before calling blink::initialize().
inline bool canInitializeMojo() {
  return base::MessageLoop::current();
}

}  // namespace blink

#endif  // MojoHelper_h
