// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextCheckerClientImpl_h
#define TextCheckerClientImpl_h

#include "platform/text/TextCheckerClient.h"

namespace blink {

class WebViewImpl;

// TODO(xiaochengh): Move ownership of this class to WebLocalFrameImpl.
class TextCheckerClientImpl final : public TextCheckerClient {
 public:
  TextCheckerClientImpl(WebViewImpl*);
  ~TextCheckerClientImpl() final;

  void checkSpellingOfString(const String&,
                             int* misspellingLocation,
                             int* misspellingLength) final;
  void requestCheckingOfString(TextCheckingRequest*) final;
  void cancelAllPendingRequests() final;

 private:
  WebViewImpl* m_webView;
};

}  // namespace blink

#endif
