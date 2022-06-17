// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_ATTACHMENT_CREATION_PASS_KEY_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_ATTACHMENT_CREATION_PASS_KEY_PROVIDER_H_

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/modules/mediasource/media_source.h"
#include "third_party/blink/renderer/modules/mediasource/url_media_source.h"

namespace blink {

class ExceptionState;
class MediaSource;
class MediaSourceHandleImpl;
class ScriptState;

class AttachmentCreationPassKeyProvider {
  STATIC_ONLY(AttachmentCreationPassKeyProvider);

 public:
  using PassKey = base::PassKey<AttachmentCreationPassKeyProvider>;

 private:
  // These specific friend methods are allowed to use GetPassKey so they may
  // create a MediaSourceAttachmentSupplement.
  static PassKey GetPassKey() { return PassKey(); }
  friend String URLMediaSource::createObjectURL(ScriptState*, MediaSource*);
  friend MediaSourceHandleImpl* MediaSource::getHandle(ExceptionState&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_ATTACHMENT_CREATION_PASS_KEY_PROVIDER_H_
