// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaDecodingAbility_h
#define MediaDecodingAbility_h

#include <memory>

#include "bindings/core/v8/ScriptWrappable.h"
#include "public/platform/modules/media_capabilities/WebMediaDecodingAbility.h"

namespace blink {

class ScriptPromiseResolver;

// Implementation of the MediaDecodingAbility interface.
class MediaDecodingAbility final
    : public GarbageCollectedFinalized<MediaDecodingAbility>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using WebType = std::unique_ptr<WebMediaDecodingAbility>;
  static MediaDecodingAbility* Take(ScriptPromiseResolver*,
                                    std::unique_ptr<WebMediaDecodingAbility>);

  bool supported() const;
  bool smooth() const;
  bool powerEfficient() const;

  DECLARE_VIRTUAL_TRACE();

 private:
  MediaDecodingAbility() = delete;
  explicit MediaDecodingAbility(std::unique_ptr<WebMediaDecodingAbility>);

  std::unique_ptr<WebMediaDecodingAbility> web_media_decoding_ability_;
};

}  // namespace blink

#endif  // MediaDecodingAbility_h
