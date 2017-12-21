// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Credential_h
#define Credential_h

#include "modules/ModulesExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class ExceptionState;

class MODULES_EXPORT Credential : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  virtual ~Credential();
  virtual void Trace(blink::Visitor*);

  virtual bool IsPasswordCredential() const { return false; }
  virtual bool IsFederatedCredential() const { return false; }
  virtual bool IsPublicKeyCredential() const { return false; }

  // Credential.idl
  const String& id() const { return id_; }
  const String& type() const { return type_; }

 protected:
  Credential(const String& id, const String& type);

  // Parses a String into a KURL that is potentially empty or null. Throws an
  // exception via |exceptionState| if an invalid URL is produced.
  static KURL ParseStringAsURLOrThrow(const String&, ExceptionState&);

 private:
  String id_;
  String type_;
};

}  // namespace blink

#endif  // Credential_h
