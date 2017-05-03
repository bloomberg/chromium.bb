/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/mediastream/MediaDeviceInfo.h"

#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8ObjectBuilder.h"
#include "platform/bindings/ScriptState.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

MediaDeviceInfo* MediaDeviceInfo::Create(
    const WebMediaDeviceInfo& web_media_device_info) {
  DCHECK(!web_media_device_info.IsNull());
  return new MediaDeviceInfo(web_media_device_info);
}

MediaDeviceInfo::MediaDeviceInfo(
    const WebMediaDeviceInfo& web_media_device_info)
    : web_media_device_info_(web_media_device_info) {}

String MediaDeviceInfo::deviceId() const {
  return web_media_device_info_.DeviceId();
}

String MediaDeviceInfo::kind() const {
  switch (web_media_device_info_.Kind()) {
    case WebMediaDeviceInfo::kMediaDeviceKindAudioInput:
      return "audioinput";
    case WebMediaDeviceInfo::kMediaDeviceKindAudioOutput:
      return "audiooutput";
    case WebMediaDeviceInfo::kMediaDeviceKindVideoInput:
      return "videoinput";
  }

  NOTREACHED();
  return String();
}

String MediaDeviceInfo::label() const {
  return web_media_device_info_.Label();
}

String MediaDeviceInfo::groupId() const {
  return web_media_device_info_.GroupId();
}

ScriptValue MediaDeviceInfo::toJSONForBinding(ScriptState* script_state) {
  V8ObjectBuilder result(script_state);
  result.AddString("deviceId", deviceId());
  result.AddString("kind", kind());
  result.AddString("label", label());
  result.AddString("groupId", groupId());
  return result.GetScriptValue();
}

}  // namespace blink
