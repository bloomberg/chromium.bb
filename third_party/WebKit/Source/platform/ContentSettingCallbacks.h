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
  static std::unique_ptr<ContentSettingCallbacks> Create(
      base::OnceClosure allowed,
      base::OnceClosure denied);
  virtual ~ContentSettingCallbacks() = default;

  void OnAllowed() { std::move(allowed_).Run(); }
  void OnDenied() { std::move(denied_).Run(); }

 private:
  ContentSettingCallbacks(base::OnceClosure allowed, base::OnceClosure denied);

  base::OnceClosure allowed_;
  base::OnceClosure denied_;
};

}  // namespace blink

#endif  // ContentSettingCallbacks_h
