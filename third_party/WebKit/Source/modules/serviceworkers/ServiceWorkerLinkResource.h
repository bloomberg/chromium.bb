// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ServiceWorkerLinkResource_h
#define ServiceWorkerLinkResource_h

#include "base/memory/scoped_refptr.h"
#include "core/html/LinkResource.h"
#include "modules/ModulesExport.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class HTMLLinkElement;

class MODULES_EXPORT ServiceWorkerLinkResource final : public LinkResource {
 public:
  static ServiceWorkerLinkResource* Create(HTMLLinkElement* owner);

  ~ServiceWorkerLinkResource() override;

  // LinkResource implementation:
  void Process() override;
  LinkResourceType GetType() const override { return kOther; }
  bool HasLoaded() const override;
  void OwnerRemoved() override;

 private:
  explicit ServiceWorkerLinkResource(HTMLLinkElement* owner);
};

}  // namespace blink

#endif  // ServiceWorkerLinkResource_h
