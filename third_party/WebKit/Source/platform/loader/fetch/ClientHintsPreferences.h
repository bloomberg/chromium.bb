// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClientHintsPreferences_h
#define ClientHintsPreferences_h

#include "platform/PlatformExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebClientHintsType.h"

namespace blink {

class PLATFORM_EXPORT ClientHintsPreferences {
  DISALLOW_NEW();

 public:
  class Context {
   public:
    virtual void CountClientHints(WebClientHintsType) = 0;

   protected:
    virtual ~Context() {}
  };

  ClientHintsPreferences();

  void UpdateFrom(const ClientHintsPreferences&);
  void UpdateFromAcceptClientHintsHeader(const String& header_value, Context*);

  bool ShouldSend(WebClientHintsType type) const {
    return enabled_types_[type];
  }
  void SetShouldSendForTesting(WebClientHintsType type) {
    enabled_types_[type] = true;
  }

 private:
  bool enabled_types_[kWebClientHintsTypeLast + 1] = {};
};

}  // namespace blink

#endif
