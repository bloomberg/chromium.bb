// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_SK_DISCARDABLE_MEMORY_CHROME_H_
#define SKIA_EXT_SK_DISCARDABLE_MEMORY_CHROME_H_

#include "base/memory/discardable_memory.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/skia/src/core/SkDiscardableMemory.h"

// This class implements the SkDiscardableMemory interface using
// base::DiscardableMemory.
class SK_API SkDiscardableMemoryChrome : public SkDiscardableMemory {
public:
  SkDiscardableMemoryChrome();
  virtual ~SkDiscardableMemoryChrome();

  // Initialize the SkDiscardableMemoryChrome object and lock the memory.
  // Returns true on success. No memory is allocated if this call returns
  // false. This call should only be called once.
  bool InitializeAndLock(size_t bytes);

  // Implementation of SkDiscardableMemory interface.
  virtual bool lock() OVERRIDE;
  virtual void* data() OVERRIDE;
  virtual void unlock() OVERRIDE;

private:
  scoped_ptr<base::DiscardableMemory> discardable_;
};

#endif  // SKIA_EXT_SK_DISCARDABLE_MEMORY_CHROME_H_
