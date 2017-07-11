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

#include "public/platform/WebSecurityOrigin.h"

#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"

namespace blink {

WebSecurityOrigin WebSecurityOrigin::CreateFromString(const WebString& origin) {
  return WebSecurityOrigin(SecurityOrigin::CreateFromString(origin));
}

WebSecurityOrigin WebSecurityOrigin::Create(const WebURL& url) {
  return WebSecurityOrigin(SecurityOrigin::Create(url));
}

WebSecurityOrigin WebSecurityOrigin::CreateFromTupleWithSuborigin(
    const WebString& protocol,
    const WebString& host,
    int port,
    const WebString& suborigin) {
  return WebSecurityOrigin(
      SecurityOrigin::Create(protocol, host, port, suborigin));
}

WebSecurityOrigin WebSecurityOrigin::CreateUnique() {
  return WebSecurityOrigin(SecurityOrigin::CreateUnique());
}

void WebSecurityOrigin::Reset() {
  private_ = nullptr;
}

void WebSecurityOrigin::Assign(const WebSecurityOrigin& other) {
  private_ = other.private_;
}

WebString WebSecurityOrigin::Protocol() const {
  DCHECK(private_);
  return private_->Protocol();
}

WebString WebSecurityOrigin::Host() const {
  DCHECK(private_);
  return private_->Host();
}

unsigned short WebSecurityOrigin::Port() const {
  DCHECK(private_);
  return private_->Port();
}

unsigned short WebSecurityOrigin::EffectivePort() const {
  DCHECK(private_);
  return private_->EffectivePort();
}

WebString WebSecurityOrigin::Suborigin() const {
  DCHECK(private_);
  return private_->HasSuborigin()
             ? WebString(private_->GetSuborigin()->GetName())
             : WebString();
}

bool WebSecurityOrigin::IsUnique() const {
  DCHECK(private_);
  return private_->IsUnique();
}

bool WebSecurityOrigin::CanAccess(const WebSecurityOrigin& other) const {
  DCHECK(private_);
  DCHECK(other.private_);
  return private_->CanAccess(other.private_.Get());
}

bool WebSecurityOrigin::CanRequest(const WebURL& url) const {
  DCHECK(private_);
  return private_->CanRequest(url);
}

bool WebSecurityOrigin::IsPotentiallyTrustworthy() const {
  DCHECK(private_);
  return private_->IsPotentiallyTrustworthy();
}

WebString WebSecurityOrigin::ToString() const {
  DCHECK(private_);
  return private_->ToString();
}

bool WebSecurityOrigin::CanAccessPasswordManager() const {
  DCHECK(private_);
  return private_->CanAccessPasswordManager();
}

WebSecurityOrigin::WebSecurityOrigin(WTF::RefPtr<SecurityOrigin> origin)
    : private_(std::move(origin)) {}

WebSecurityOrigin& WebSecurityOrigin::operator=(
    WTF::RefPtr<SecurityOrigin> origin) {
  private_ = std::move(origin);
  return *this;
}

WebSecurityOrigin::operator WTF::RefPtr<SecurityOrigin>() const {
  return private_.Get();
}

SecurityOrigin* WebSecurityOrigin::Get() const {
  return private_.Get();
}

void WebSecurityOrigin::GrantLoadLocalResources() const {
  Get()->GrantLoadLocalResources();
}

}  // namespace blink
