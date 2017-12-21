// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FederatedCredential_h
#define FederatedCredential_h

#include "modules/ModulesExport.h"
#include "modules/credentialmanager/Credential.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace blink {

class FederatedCredentialInit;

class MODULES_EXPORT FederatedCredential final : public Credential {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static FederatedCredential* Create(const FederatedCredentialInit&,
                                     ExceptionState&);
  static FederatedCredential* Create(
      const String& id,
      scoped_refptr<const SecurityOrigin> provider,
      const String& name,
      const KURL& icon_url);

  scoped_refptr<const SecurityOrigin> GetProviderAsOrigin() const {
    return provider_;
  }

  // Credential:
  bool IsFederatedCredential() const override;

  // FederatedCredential.idl
  String provider() const {
    CHECK(provider_);
    return provider_->ToString();
  }
  const String& name() const { return name_; }
  const KURL& iconURL() const { return icon_url_; }
  const String& protocol() const {
    // TODO(mkwst): This is a stub, as we don't yet have any support on the
    // Chromium-side.
    return g_empty_string;
  }

 private:
  FederatedCredential(const String& id,
                      scoped_refptr<const SecurityOrigin> provider,
                      const String& name,
                      const KURL& icon_url);

  const scoped_refptr<const SecurityOrigin> provider_;
  const String name_;
  const KURL icon_url_;
};

}  // namespace blink

#endif  // FederatedCredential_h
