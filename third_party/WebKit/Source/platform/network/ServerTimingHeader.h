// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ServerTimingHeader_h
#define ServerTimingHeader_h

#include "platform/PlatformExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class ServerTimingHeader {
 public:
  ServerTimingHeader(const String& name) : name_(name) {}

  const String& Name() const { return name_; }
  const double& Duration() const { return duration_; }
  const String& Description() const { return description_; }

  void SetParameter(StringView, String);

 private:
  String name_;
  double duration_ = 0.0;
  String description_ = "";

  bool duration_set_ = false;
  bool description_set_ = false;
};

}  // namespace blink

#endif
