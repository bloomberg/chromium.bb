// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaKeySystemAccess_h
#define MediaKeySystemAccess_h

#include <memory>
#include "bindings/core/v8/ScriptPromise.h"
#include "modules/encryptedmedia/MediaKeySystemConfiguration.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/wtf/Forward.h"
#include "public/platform/WebContentDecryptionModuleAccess.h"

namespace blink {

class MediaKeySystemAccess final
    : public GarbageCollectedFinalized<MediaKeySystemAccess>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  MediaKeySystemAccess(const String& key_system,
                       std::unique_ptr<WebContentDecryptionModuleAccess>);
  virtual ~MediaKeySystemAccess();

  const String& keySystem() const { return key_system_; }
  void getConfiguration(MediaKeySystemConfiguration& result);
  ScriptPromise createMediaKeys(ScriptState*);

  void Trace(blink::Visitor*);

 private:
  const String key_system_;
  std::unique_ptr<WebContentDecryptionModuleAccess> access_;
};

}  // namespace blink

#endif  // MediaKeySystemAccess_h
