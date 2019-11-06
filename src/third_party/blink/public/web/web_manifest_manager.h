// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_MANIFEST_MANAGER_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_MANIFEST_MANAGER_H_

#include "base/callback.h"
#include "third_party/blink/public/platform/web_common.h"

namespace blink {

struct Manifest;
class WebLocalFrame;
class WebURL;

class BLINK_EXPORT WebManifestManager {
 public:
  using WebCallback = base::OnceCallback<void(const WebURL&, const Manifest&)>;

  static WebManifestManager* FromFrame(WebLocalFrame*);

  virtual ~WebManifestManager() = default;

  virtual void RequestManifest(WebCallback callback) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_MANIFEST_MANAGER_H_
