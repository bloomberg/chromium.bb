// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Suborigin_h
#define Suborigin_h

#include "platform/PlatformExport.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class PLATFORM_EXPORT Suborigin {
 public:
  enum class SuboriginPolicyOptions : unsigned {
    kNone = 0,
    kUnsafePostMessageSend = 1 << 0,
    kUnsafePostMessageReceive = 1 << 1,
    kUnsafeCookies = 1 << 2,
    kUnsafeCredentials = 1 << 3
  };

  Suborigin();
  explicit Suborigin(const Suborigin*);

  void SetTo(const Suborigin&);
  String GetName() const { return name_; }
  void SetName(const String& name) {
    DCHECK(!name.IsEmpty());
    name_ = name;
  }
  void AddPolicyOption(SuboriginPolicyOptions);
  bool PolicyContains(SuboriginPolicyOptions) const;
  void Clear();
  // For testing
  unsigned OptionsMask() const { return options_mask_; }

 private:
  String name_;
  unsigned options_mask_;
};

}  // namespace blink

#endif  // Suborigin_h
