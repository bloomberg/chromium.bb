// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PlatformCredential_h
#define PlatformCredential_h

#include "platform/heap/Handle.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class PLATFORM_EXPORT PlatformCredential
    : public GarbageCollectedFinalized<PlatformCredential> {
  WTF_MAKE_NONCOPYABLE(PlatformCredential);

 public:
  static PlatformCredential* Create(const String& id);
  virtual ~PlatformCredential();

  const String& Id() const { return id_; }
  const String& GetType() const { return type_; }

  virtual bool IsPassword() { return false; }
  virtual bool IsFederated() { return false; }

  virtual void Trace(blink::Visitor* visitor) {}

 protected:
  PlatformCredential(const String& id);

  void SetType(const String& type) { type_ = type; }

 private:
  String id_;
  String type_;
};

}  // namespace blink

#endif  // PlatformCredential_h
