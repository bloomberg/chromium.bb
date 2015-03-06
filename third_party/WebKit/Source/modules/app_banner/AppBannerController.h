// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AppBannerController_h
#define AppBannerController_h

namespace blink {

enum class WebAppBannerPromptReply;
class LocalFrame;
class WebString;

// FIXME: unless userChoice ends up implemented, this class should not exist and
// a regular static method could be used instead.
class AppBannerController final {
public:
    static void willShowInstallBannerPrompt(LocalFrame*, const WebString& platform, WebAppBannerPromptReply*);

private:
    AppBannerController() = delete;
    ~AppBannerController() = delete;
};

} // namespace blink

#endif // AppBannerController_h
