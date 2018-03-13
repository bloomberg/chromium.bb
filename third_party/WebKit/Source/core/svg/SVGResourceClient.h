// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGResourceClient_h
#define SVGResourceClient_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class CORE_EXPORT SVGResourceClient : public GarbageCollectedMixin {
 public:
  virtual ~SVGResourceClient() = default;

  virtual void ResourceContentChanged() = 0;
  virtual void ResourceElementChanged() = 0;

 protected:
  SVGResourceClient() = default;
};

}  // namespace blink

#endif  // SVGResourceClient_h
