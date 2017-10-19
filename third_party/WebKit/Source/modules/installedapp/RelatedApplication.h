// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RelatedApplication_h
#define RelatedApplication_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class RelatedApplication final
    : public GarbageCollectedFinalized<RelatedApplication>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static RelatedApplication* Create(const String& platform,
                                    const String& url,
                                    const String& id) {
    return new RelatedApplication(platform, url, id);
  }

  virtual ~RelatedApplication() {}

  String platform() const { return platform_; }
  String url() const { return url_; }
  String id() const { return id_; }

  void Trace(blink::Visitor* visitor) {}

 private:
  RelatedApplication(const String& platform,
                     const String& url,
                     const String& id)
      : platform_(platform), url_(url), id_(id) {}

  const String platform_;
  const String url_;
  const String id_;
};

}  // namespace blink

#endif  // RelatedApplication_h
