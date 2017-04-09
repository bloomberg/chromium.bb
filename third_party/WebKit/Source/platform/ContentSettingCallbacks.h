// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ContentSettingCallbacks_h
#define ContentSettingCallbacks_h

#include "platform/PlatformExport.h"
#include "wtf/Allocator.h"
#include "wtf/Functional.h"
#include "wtf/Noncopyable.h"
#include <memory>

namespace blink {

class PLATFORM_EXPORT ContentSettingCallbacks {
  USING_FAST_MALLOC(ContentSettingCallbacks);
  WTF_MAKE_NONCOPYABLE(ContentSettingCallbacks);

 public:
  static std::unique_ptr<ContentSettingCallbacks> Create(
      std::unique_ptr<WTF::Closure> allowed,
      std::unique_ptr<WTF::Closure> denied);
  virtual ~ContentSettingCallbacks() {}

  void OnAllowed() { (*allowed_)(); }
  void OnDenied() { (*denied_)(); }

 private:
  ContentSettingCallbacks(std::unique_ptr<WTF::Closure> allowed,
                          std::unique_ptr<WTF::Closure> denied);

  std::unique_ptr<WTF::Closure> allowed_;
  std::unique_ptr<WTF::Closure> denied_;
};

}  // namespace blink

#endif  // ContentSettingCallbacks_h
