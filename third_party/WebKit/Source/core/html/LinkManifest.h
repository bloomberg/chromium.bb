// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LinkManifest_h
#define LinkManifest_h

#include "base/memory/scoped_refptr.h"
#include "core/html/LinkResource.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class HTMLLinkElement;

class LinkManifest final : public LinkResource {
 public:
  static LinkManifest* Create(HTMLLinkElement* owner);

  ~LinkManifest() override;

  // LinkResource
  void Process() override;
  LinkResourceType GetType() const override { return kManifest; }
  bool HasLoaded() const override;
  void OwnerRemoved() override;

 private:
  explicit LinkManifest(HTMLLinkElement* owner);
};

}  // namespace blink

#endif  // LinkManifest_h
