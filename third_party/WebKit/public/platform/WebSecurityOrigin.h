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

#ifndef WebSecurityOrigin_h
#define WebSecurityOrigin_h

#include "public/platform/WebCommon.h"
#include "public/platform/WebPrivatePtr.h"
#include "public/platform/WebString.h"

#if INSIDE_BLINK
#include "platform/wtf/RefPtr.h"
#else
#include "url/origin.h"
#endif

namespace blink {

class SecurityOrigin;
class WebURL;

class WebSecurityOrigin {
 public:
  ~WebSecurityOrigin() { Reset(); }

  WebSecurityOrigin() {}
  WebSecurityOrigin(const WebSecurityOrigin& s) { Assign(s); }
  WebSecurityOrigin& operator=(const WebSecurityOrigin& s) {
    Assign(s);
    return *this;
  }

  BLINK_PLATFORM_EXPORT static WebSecurityOrigin CreateFromString(
      const WebString&);
  BLINK_PLATFORM_EXPORT static WebSecurityOrigin Create(const WebURL&);
  BLINK_PLATFORM_EXPORT static WebSecurityOrigin CreateUnique();

  BLINK_PLATFORM_EXPORT void Reset();
  BLINK_PLATFORM_EXPORT void Assign(const WebSecurityOrigin&);

  bool IsNull() const { return private_.IsNull(); }

  BLINK_PLATFORM_EXPORT WebString Protocol() const;
  BLINK_PLATFORM_EXPORT WebString Host() const;
  BLINK_PLATFORM_EXPORT unsigned short Port() const;

  // |port()| will return 0 if the port is the default for an origin. This
  // method instead returns the effective port, even if it is the default port
  // (e.g. "http" => 80).
  BLINK_PLATFORM_EXPORT unsigned short EffectivePort() const;

  BLINK_PLATFORM_EXPORT WebString Suborigin() const;

  // A unique WebSecurityOrigin is the least privileged WebSecurityOrigin.
  BLINK_PLATFORM_EXPORT bool IsUnique() const;

  // Returns true if this WebSecurityOrigin can script objects in the given
  // SecurityOrigin. For example, call this function before allowing
  // script from one security origin to read or write objects from
  // another SecurityOrigin.
  BLINK_PLATFORM_EXPORT bool CanAccess(const WebSecurityOrigin&) const;

  // Returns true if this WebSecurityOrigin can read content retrieved from
  // the given URL. For example, call this function before allowing script
  // from a given security origin to receive contents from a given URL.
  BLINK_PLATFORM_EXPORT bool CanRequest(const WebURL&) const;

  // Returns true if the origin loads resources either from the local
  // machine or over the network from a
  // cryptographically-authenticated origin, as described in
  // https://w3c.github.io/webappsec/specs/powerfulfeatures/#is-origin-trustworthy.
  BLINK_PLATFORM_EXPORT bool IsPotentiallyTrustworthy() const;

  // Returns a string representation of the WebSecurityOrigin.  The empty
  // WebSecurityOrigin is represented by "null".  The representation of a
  // non-empty WebSecurityOrigin resembles a standard URL.
  BLINK_PLATFORM_EXPORT WebString ToString() const;

  // Returns true if this WebSecurityOrigin can access usernames and
  // passwords stored in password manager.
  BLINK_PLATFORM_EXPORT bool CanAccessPasswordManager() const;

  // Allows this WebSecurityOrigin access to local resources.
  BLINK_PLATFORM_EXPORT void GrantLoadLocalResources() const;

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT WebSecurityOrigin(WTF::RefPtr<SecurityOrigin>);
  BLINK_PLATFORM_EXPORT WebSecurityOrigin& operator=(
      WTF::RefPtr<SecurityOrigin>);
  BLINK_PLATFORM_EXPORT operator WTF::RefPtr<SecurityOrigin>() const;
  BLINK_PLATFORM_EXPORT SecurityOrigin* Get() const;
#else
  // TODO(mkwst): A number of properties don't survive a round-trip
  // ('document.domain', for instance).  We'll need to fix that for OOPI-enabled
  // embedders, https://crbug.com/490074.
  operator url::Origin() const {
    return IsUnique() ? url::Origin()
                      : url::Origin::CreateFromNormalizedTupleWithSuborigin(
                            Protocol().Ascii(), Host().Ascii(), EffectivePort(),
                            Suborigin().Ascii());
  }

  WebSecurityOrigin(const url::Origin& origin) {
    if (origin.unique()) {
      Assign(WebSecurityOrigin::CreateUnique());
      return;
    }

    // TODO(mkwst): This might open up issues by double-canonicalizing the host.
    Assign(WebSecurityOrigin::CreateFromTupleWithSuborigin(
        WebString::FromUTF8(origin.scheme()),
        WebString::FromUTF8(origin.host()), origin.port(),
        WebString::FromUTF8(origin.suborigin())));
  }
#endif

 private:
  // Present only to facilitate conversion from 'url::Origin'; this constructor
  // shouldn't be used anywhere else.
  BLINK_PLATFORM_EXPORT static WebSecurityOrigin CreateFromTupleWithSuborigin(
      const WebString& protocol,
      const WebString& host,
      int port,
      const WebString& suborigin);

  WebPrivatePtr<SecurityOrigin> private_;
};

}  // namespace blink

#endif
