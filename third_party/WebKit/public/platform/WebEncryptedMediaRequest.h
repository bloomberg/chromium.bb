// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebEncryptedMediaRequest_h
#define WebEncryptedMediaRequest_h

#include "public/platform/WebCommon.h"
#include "public/platform/WebPrivatePtr.h"
#include "public/platform/WebString.h"

namespace blink {

class EncryptedMediaRequest;
class WebContentDecryptionModuleAccess;
struct WebMediaKeySystemConfiguration;
class WebSecurityOrigin;
template <typename T> class WebVector;

class WebEncryptedMediaRequest {
public:
    BLINK_EXPORT WebEncryptedMediaRequest(const WebEncryptedMediaRequest&);
    BLINK_EXPORT ~WebEncryptedMediaRequest();

    BLINK_EXPORT WebString keySystem() const;
    BLINK_EXPORT const WebVector<WebMediaKeySystemConfiguration>& supportedConfigurations() const;

    BLINK_EXPORT WebSecurityOrigin securityOrigin() const;

    BLINK_EXPORT void requestSucceeded(WebContentDecryptionModuleAccess*);
    BLINK_EXPORT void requestNotSupported(const WebString& errorMessage);

#if BLINK_IMPLEMENTATION
    explicit WebEncryptedMediaRequest(EncryptedMediaRequest*);
#endif

private:
    void assign(const WebEncryptedMediaRequest&);
    void reset();

    WebPrivatePtr<EncryptedMediaRequest> m_private;
};

} // namespace blink

#endif // WebEncryptedMediaRequest_h
