// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_CHILD_WEB_DISCARDABLE_MEMORY_IMPL_H_
#define WEBKIT_CHILD_WEB_DISCARDABLE_MEMORY_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/discardable_memory.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebDiscardableMemory.h"
#include "webkit/child/webkit_child_export.h"

namespace blink {
class WebDiscardableMemory;
}

namespace webkit_glue {

// Implementation of WebDiscardableMemory that is responsible for allocating
// discardable memory.
class WEBKIT_CHILD_EXPORT WebDiscardableMemoryImpl
    : NON_EXPORTED_BASE(public blink::WebDiscardableMemory) {
 public:
  virtual ~WebDiscardableMemoryImpl();

  static scoped_ptr<WebDiscardableMemoryImpl> CreateLockedMemory(size_t size);

  // blink::WebDiscardableMemory:
  virtual bool lock();
  virtual void unlock();
  virtual void* data();

 private:
  WebDiscardableMemoryImpl(scoped_ptr<base::DiscardableMemory> memory);

  scoped_ptr<base::DiscardableMemory> discardable_;

  DISALLOW_COPY_AND_ASSIGN(WebDiscardableMemoryImpl);
};

}  // namespace webkit_glue

#endif  // WEBKIT_CHILD_WEB_DISCARDABLE_MEMORY_IMPL_H_
