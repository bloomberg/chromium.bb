// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AppBannerPromptResult_h
#define AppBannerPromptResult_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class AppBannerPromptResult final
    : public GarbageCollectedFinalized<AppBannerPromptResult>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  WTF_MAKE_NONCOPYABLE(AppBannerPromptResult);

 public:
  enum class Outcome { kAccepted, kDismissed };

  static AppBannerPromptResult* Create(const String& platform,
                                       Outcome outcome) {
    return new AppBannerPromptResult(platform, outcome);
  }

  virtual ~AppBannerPromptResult();

  String platform() const { return platform_; }
  String outcome() const;

  DEFINE_INLINE_VIRTUAL_TRACE() {}

 private:
  AppBannerPromptResult(const String& platform, Outcome);

  String platform_;
  Outcome outcome_;
};

}  // namespace blink

#endif  // AppBannerPromptResult_h
