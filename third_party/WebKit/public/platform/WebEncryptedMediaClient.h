// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebEncryptedMediaClient_h
#define WebEncryptedMediaClient_h

namespace blink {

class WebEncryptedMediaRequest;

class WebEncryptedMediaClient {
public:
    virtual ~WebEncryptedMediaClient() { }

    virtual void requestMediaKeySystemAccess(WebEncryptedMediaRequest) = 0;
};

} // namespace blink

#endif // WebEncryptedMediaClient_h
