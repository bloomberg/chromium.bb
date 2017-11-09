// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CacheStorageError_h
#define CacheStorageError_h

#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"
#include "public/platform/modules/cache_storage/cache_storage.mojom-blink.h"

namespace blink {

class DOMException;
class ScriptPromiseResolver;

class CacheStorageError {
  STATIC_ONLY(CacheStorageError);

 public:
  // For CallbackPromiseAdapter. Ownership of a given error is not
  // transferred.
  using WebType = mojom::CacheStorageError;
  static DOMException* Take(ScriptPromiseResolver*,
                            mojom::CacheStorageError web_error) {
    return CreateException(web_error);
  }

  static DOMException* CreateException(mojom::CacheStorageError web_error);
};

}  // namespace blink

#endif  // CacheStorageError_h
