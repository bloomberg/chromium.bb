// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.
//
// This file mocks out just enough of the WebClipboard API for running the
// webkit tests. This is so we can run webkit tests without them sharing a
// clipboard, which allows for running them in parallel and having the tests
// not interact with actual user actions.

#ifndef WEBKIT_TOOLS_TEST_SHELL_MOCK_WEBCLIPBOARD_IMPL_H_
#define WEBKIT_TOOLS_TEST_SHELL_MOCK_WEBCLIPBOARD_IMPL_H_

#include "third_party/WebKit/Source/WebKit/chromium/public/WebClipboard.h"

class MockWebClipboardImpl : public WebKit::WebClipboard {
 public:
  virtual bool isFormatAvailable(WebKit::WebClipboard::Format,
                                 WebKit::WebClipboard::Buffer);

  virtual WebKit::WebString readPlainText(WebKit::WebClipboard::Buffer);
  virtual WebKit::WebString readHTML(WebKit::WebClipboard::Buffer,
                                     WebKit::WebURL*);

  virtual void writePlainText(const WebKit::WebString& plain_text);
  virtual void writeHTML(
      const WebKit::WebString& htmlText, const WebKit::WebURL&,
      const WebKit::WebString& plainText, bool writeSmartPaste);
  virtual void writeURL(
      const WebKit::WebURL&, const WebKit::WebString& title);
  virtual void writeImage(
      const WebKit::WebImage&, const WebKit::WebURL&,
      const WebKit::WebString& title);

  virtual WebKit::WebVector<WebKit::WebString> readAvailableTypes(
      WebKit::WebClipboard::Buffer, bool* containsFilenames);

 private:
  WebKit::WebString m_plainText;
  WebKit::WebString m_htmlText;
  bool m_writeSmartPaste;
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_MOCK_WEBCLIPBOARD_IMPL_H_
