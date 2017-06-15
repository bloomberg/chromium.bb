// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AppBannerController_h
#define AppBannerController_h

#include "modules/ModulesExport.h"
#include "platform/heap/Persistent.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Vector.h"
#include "public/platform/modules/app_banner/app_banner.mojom-blink.h"

namespace blink {

class LocalFrame;

class MODULES_EXPORT AppBannerController final
    : public mojom::blink::AppBannerController {
 public:
  explicit AppBannerController(LocalFrame&);

  static void BindMojoRequest(LocalFrame*,
                              mojom::blink::AppBannerControllerRequest);

  void BannerPromptRequest(mojom::blink::AppBannerServicePtr,
                           mojom::blink::AppBannerEventRequest,
                           const Vector<String>& platforms,
                           const BannerPromptRequestCallback&) override;

 private:
  WeakPersistent<LocalFrame> frame_;
};

}  // namespace blink

#endif  // AppBannerController_h
