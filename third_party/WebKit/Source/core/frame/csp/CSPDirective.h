// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSPDirective_h
#define CSPDirective_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class ContentSecurityPolicy;

class CORE_EXPORT CSPDirective
    : public GarbageCollectedFinalized<CSPDirective> {
  WTF_MAKE_NONCOPYABLE(CSPDirective);

 public:
  CSPDirective(const String& name,
               const String& value,
               ContentSecurityPolicy* policy)
      : name_(name), text_(name + ' ' + value), policy_(policy) {}
  virtual ~CSPDirective() {}
  virtual void Trace(blink::Visitor* visitor) { visitor->Trace(policy_); }

  const String& GetName() const { return name_; }
  const String& GetText() const { return text_; }

 protected:
  ContentSecurityPolicy* Policy() const { return policy_; }

 private:
  String name_;
  String text_;
  Member<ContentSecurityPolicy> policy_;
};

}  // namespace blink

#endif
