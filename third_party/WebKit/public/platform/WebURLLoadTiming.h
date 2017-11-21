/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebURLLoadTiming_h
#define WebURLLoadTiming_h

#include "WebCommon.h"
#include "WebPrivatePtr.h"

#if INSIDE_BLINK
#include "base/memory/scoped_refptr.h"
#endif

namespace blink {

class ResourceLoadTiming;

class WebURLLoadTiming {
 public:
  ~WebURLLoadTiming() { Reset(); }

  WebURLLoadTiming() {}
  WebURLLoadTiming(const WebURLLoadTiming& d) { Assign(d); }
  WebURLLoadTiming& operator=(const WebURLLoadTiming& d) {
    Assign(d);
    return *this;
  }

  BLINK_PLATFORM_EXPORT void Initialize();
  BLINK_PLATFORM_EXPORT void Reset();
  BLINK_PLATFORM_EXPORT void Assign(const WebURLLoadTiming&);

  bool IsNull() const { return private_.IsNull(); }

  BLINK_PLATFORM_EXPORT double RequestTime() const;
  BLINK_PLATFORM_EXPORT void SetRequestTime(double);

  BLINK_PLATFORM_EXPORT double ProxyStart() const;
  BLINK_PLATFORM_EXPORT void SetProxyStart(double);

  BLINK_PLATFORM_EXPORT double ProxyEnd() const;
  BLINK_PLATFORM_EXPORT void SetProxyEnd(double);

  BLINK_PLATFORM_EXPORT double DnsStart() const;
  BLINK_PLATFORM_EXPORT void SetDNSStart(double);

  BLINK_PLATFORM_EXPORT double DnsEnd() const;
  BLINK_PLATFORM_EXPORT void SetDNSEnd(double);

  BLINK_PLATFORM_EXPORT double ConnectStart() const;
  BLINK_PLATFORM_EXPORT void SetConnectStart(double);

  BLINK_PLATFORM_EXPORT double ConnectEnd() const;
  BLINK_PLATFORM_EXPORT void SetConnectEnd(double);

  BLINK_PLATFORM_EXPORT double WorkerStart() const;
  BLINK_PLATFORM_EXPORT void SetWorkerStart(double);

  BLINK_PLATFORM_EXPORT double WorkerReady() const;
  BLINK_PLATFORM_EXPORT void SetWorkerReady(double);

  BLINK_PLATFORM_EXPORT double SendStart() const;
  BLINK_PLATFORM_EXPORT void SetSendStart(double);

  BLINK_PLATFORM_EXPORT double SendEnd() const;
  BLINK_PLATFORM_EXPORT void SetSendEnd(double);

  BLINK_PLATFORM_EXPORT double ReceiveHeadersEnd() const;
  BLINK_PLATFORM_EXPORT void SetReceiveHeadersEnd(double);

  BLINK_PLATFORM_EXPORT double SslStart() const;
  BLINK_PLATFORM_EXPORT void SetSSLStart(double);

  BLINK_PLATFORM_EXPORT double SslEnd() const;
  BLINK_PLATFORM_EXPORT void SetSSLEnd(double);

  BLINK_PLATFORM_EXPORT double PushStart() const;
  BLINK_PLATFORM_EXPORT void SetPushStart(double);

  BLINK_PLATFORM_EXPORT double PushEnd() const;
  BLINK_PLATFORM_EXPORT void SetPushEnd(double);

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT WebURLLoadTiming(scoped_refptr<ResourceLoadTiming>);
  BLINK_PLATFORM_EXPORT WebURLLoadTiming& operator=(
      scoped_refptr<ResourceLoadTiming>);
  BLINK_PLATFORM_EXPORT operator scoped_refptr<ResourceLoadTiming>() const;
#endif

 private:
  WebPrivatePtr<ResourceLoadTiming> private_;
};

}  // namespace blink

#endif
