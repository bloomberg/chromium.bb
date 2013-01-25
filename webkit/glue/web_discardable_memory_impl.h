// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEB_DISCARDABLE_MEMORY_IMPL_H_
#define WEBKIT_GLUE_WEB_DISCARDABLE_MEMORY_IMPL_H_

#include "base/memory/discardable_memory.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebDiscardableMemory.h"
#include "webkit/glue/webkit_glue_export.h"

namespace WebKit {
class WebDiscardableMemory;
}

namespace webkit_glue {

// Implementation of WebDiscardableMemory that is responsible for allocating
// discardable memory.
class WEBKIT_GLUE_EXPORT WebDiscardableMemoryImpl
    : NON_EXPORTED_BASE(public WebKit::WebDiscardableMemory) {
 public:
  WebDiscardableMemoryImpl();
  virtual ~WebDiscardableMemoryImpl();

  // WebKit::WebDiscardableMemory implementation.
  virtual bool lock();
  virtual void unlock();
  virtual void* data();

  // Initialize the WebDiscardableMemoryImpl object and lock the memory.
  // Returns true on success. No memory is allocated if this call returns
  // false. This call should only be called once.
  bool InitializeAndLock(size_t size);

 private:
  scoped_ptr<base::DiscardableMemory> discardable_;
  DISALLOW_COPY_AND_ASSIGN(WebDiscardableMemoryImpl);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEB_DISCARDABLE_MEMORY_IMPL_H_
