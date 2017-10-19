// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaCapabilitiesInfo_h
#define MediaCapabilitiesInfo_h

#include <memory>

#include "platform/bindings/ScriptWrappable.h"
#include "public/platform/modules/media_capabilities/WebMediaCapabilitiesInfo.h"

namespace blink {

class ScriptPromiseResolver;

// Implementation of the MediaCapabilitiesInfo interface.
class MediaCapabilitiesInfo final
    : public GarbageCollectedFinalized<MediaCapabilitiesInfo>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using WebType = std::unique_ptr<WebMediaCapabilitiesInfo>;
  static MediaCapabilitiesInfo* Take(ScriptPromiseResolver*,
                                     std::unique_ptr<WebMediaCapabilitiesInfo>);

  bool supported() const;
  bool smooth() const;
  bool powerEfficient() const;

  virtual void Trace(blink::Visitor*);

 private:
  MediaCapabilitiesInfo() = delete;
  explicit MediaCapabilitiesInfo(std::unique_ptr<WebMediaCapabilitiesInfo>);

  std::unique_ptr<WebMediaCapabilitiesInfo> web_media_capabilities_info_;
};

}  // namespace blink

#endif  // MediaCapabilitiesInfo_h
