// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ContentSettingCallbacks_h
#define ContentSettingCallbacks_h

#include <memory>
#include "platform/PlatformExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class PLATFORM_EXPORT ContentSettingCallbacks {
  USING_FAST_MALLOC(ContentSettingCallbacks);
  WTF_MAKE_NONCOPYABLE(ContentSettingCallbacks);

 public:
  static std::unique_ptr<ContentSettingCallbacks> Create(WTF::Closure allowed,
                                                         WTF::Closure denied);
  virtual ~ContentSettingCallbacks() {}

  void OnAllowed() { allowed_(); }
  void OnDenied() { denied_(); }

 private:
  ContentSettingCallbacks(WTF::Closure allowed, WTF::Closure denied);

  WTF::Closure allowed_;
  WTF::Closure denied_;
};

}  // namespace blink

#endif  // ContentSettingCallbacks_h
