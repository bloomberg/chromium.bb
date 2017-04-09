// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClientHintsPreferences_h
#define ClientHintsPreferences_h

#include "platform/PlatformExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class PLATFORM_EXPORT ClientHintsPreferences {
  DISALLOW_NEW();

 public:
  class Context {
   public:
    virtual void CountClientHintsDPR() = 0;
    virtual void CountClientHintsResourceWidth() = 0;
    virtual void CountClientHintsViewportWidth() = 0;

   protected:
    virtual ~Context() {}
  };

  ClientHintsPreferences();

  void UpdateFrom(const ClientHintsPreferences&);
  void UpdateFromAcceptClientHintsHeader(const String& header_value, Context*);

  bool ShouldSendDPR() const { return should_send_dpr_; }
  void SetShouldSendDPR(bool should) { should_send_dpr_ = should; }

  bool ShouldSendResourceWidth() const { return should_send_resource_width_; }
  void SetShouldSendResourceWidth(bool should) {
    should_send_resource_width_ = should;
  }

  bool ShouldSendViewportWidth() const { return should_send_viewport_width_; }
  void SetShouldSendViewportWidth(bool should) {
    should_send_viewport_width_ = should;
  }

 private:
  bool should_send_dpr_;
  bool should_send_resource_width_;
  bool should_send_viewport_width_;
};

}  // namespace blink

#endif
