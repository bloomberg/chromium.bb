// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaKeysPolicy_h
#define MediaKeysPolicy_h

#include "modules/encryptedmedia/MediaKeysPolicyInit.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class MediaKeysPolicy final : public GarbageCollectedFinalized<MediaKeysPolicy>,
                              public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static MediaKeysPolicy* Create(const MediaKeysPolicyInit& initializer) {
    return new MediaKeysPolicy(initializer);
  }

  String minHdcpVersion() const { return min_hdcp_version_; }

  virtual void Trace(blink::Visitor*);

 private:
  MediaKeysPolicy() = delete;
  explicit MediaKeysPolicy(const MediaKeysPolicyInit& initializer);

  String min_hdcp_version_;
};

}  // namespace blink

#endif  // MediaKeysPolicy_h
