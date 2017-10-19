// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EncryptedMediaRequest_h
#define EncryptedMediaRequest_h

#include "platform/heap/Handle.h"

namespace blink {

class SecurityOrigin;
class WebContentDecryptionModuleAccess;
struct WebMediaKeySystemConfiguration;
class WebString;
template <typename T>
class WebVector;

class EncryptedMediaRequest
    : public GarbageCollectedFinalized<EncryptedMediaRequest> {
 public:
  virtual ~EncryptedMediaRequest() {}

  virtual WebString KeySystem() const = 0;
  virtual const WebVector<WebMediaKeySystemConfiguration>&
  SupportedConfigurations() const = 0;

  virtual SecurityOrigin* GetSecurityOrigin() const = 0;

  virtual void RequestSucceeded(WebContentDecryptionModuleAccess*) = 0;
  virtual void RequestNotSupported(const WebString& error_message) = 0;

  virtual void Trace(blink::Visitor* visitor) {}
};

}  // namespace blink

#endif  // EncryptedMediaRequest_h
