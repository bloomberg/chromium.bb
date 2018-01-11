// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGResourceClient_h
#define SVGResourceClient_h

#include "core/CoreExport.h"
#include "core/loader/resource/DocumentResource.h"
#include "platform/heap/Handle.h"

namespace blink {

class TreeScope;

class CORE_EXPORT SVGResourceClient : public ResourceClient {
 public:
  virtual ~SVGResourceClient() = default;

  virtual TreeScope* GetTreeScope() = 0;

  virtual void ResourceContentChanged() = 0;
  virtual void ResourceElementChanged() = 0;

 protected:
  SVGResourceClient() = default;

  String DebugName() const override { return "SVGResourceClient"; }
  void NotifyFinished(Resource*) override { ResourceElementChanged(); }
};

}  // namespace blink

#endif  // SVGResourceClient_h
