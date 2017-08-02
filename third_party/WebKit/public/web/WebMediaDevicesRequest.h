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

#ifndef WebMediaDevicesRequest_h
#define WebMediaDevicesRequest_h

#include "public/platform/WebCommon.h"
#include "public/platform/WebPrivatePtr.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebString.h"

namespace blink {

class MediaDevicesRequest;
class WebDocument;
class WebMediaDeviceInfo;
template <typename T>
class WebVector;

class WebMediaDevicesRequest {
 public:
  WebMediaDevicesRequest() {}
  WebMediaDevicesRequest(const WebMediaDevicesRequest& request) {
    Assign(request);
  }
  ~WebMediaDevicesRequest() { Reset(); }

  WebMediaDevicesRequest& operator=(const WebMediaDevicesRequest& other) {
    Assign(other);
    return *this;
  }

  BLINK_EXPORT void Reset();
  bool IsNull() const { return private_.IsNull(); }
  BLINK_EXPORT bool Equals(const WebMediaDevicesRequest&) const;
  BLINK_EXPORT void Assign(const WebMediaDevicesRequest&);

  BLINK_EXPORT WebSecurityOrigin GetSecurityOrigin() const;
  BLINK_EXPORT WebDocument OwnerDocument() const;

  BLINK_EXPORT void RequestSucceeded(WebVector<WebMediaDeviceInfo>);

#if INSIDE_BLINK
  WebMediaDevicesRequest(MediaDevicesRequest*);
  operator MediaDevicesRequest*() const;
#endif

 private:
  WebPrivatePtr<MediaDevicesRequest> private_;
};

inline bool operator==(const WebMediaDevicesRequest& a,
                       const WebMediaDevicesRequest& b) {
  return a.Equals(b);
}

}  // namespace blink

#endif  // WebMediaDevicesRequest_h
