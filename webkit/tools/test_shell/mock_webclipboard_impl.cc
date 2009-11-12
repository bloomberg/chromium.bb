// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "webkit/tools/test_shell/mock_webclipboard_impl.h"

#include "app/clipboard/clipboard.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "net/base/escape.h"
#include "third_party/WebKit/WebKit/chromium/public/WebImage.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"
#include "webkit/glue/webclipboard_impl.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebString;
using WebKit::WebURL;

bool MockWebClipboardImpl::isFormatAvailable(Format format, Buffer buffer) {
  switch (format) {
    case FormatHTML:
      return !m_htmlText.isEmpty();

    case FormatSmartPaste:
      return m_writeSmartPaste;

    default:
      NOTREACHED();
      return false;
  }

  switch (buffer) {
    case BufferStandard:
      break;
    case BufferSelection:
#if defined(OS_LINUX)
      break;
#endif
    default:
      NOTREACHED();
      return false;
  }

  return true;
}

WebKit::WebString MockWebClipboardImpl::readPlainText(
    WebKit::WebClipboard::Buffer buffer) {
  return m_plainText;
}

// TODO(wtc): set output argument *url.
WebKit::WebString MockWebClipboardImpl::readHTML(
    WebKit::WebClipboard::Buffer buffer, WebKit::WebURL* url) {
  return m_htmlText;
}

void MockWebClipboardImpl::writeHTML(
    const WebKit::WebString& htmlText, const WebKit::WebURL& url,
    const WebKit::WebString& plainText, bool writeSmartPaste) {
  m_htmlText = htmlText;
  m_plainText = plainText;
  m_writeSmartPaste = writeSmartPaste;
}

void MockWebClipboardImpl::writePlainText(const WebKit::WebString& plain_text) {
  m_htmlText = WebKit::WebString();
  m_plainText = plain_text;
  m_writeSmartPaste = false;
}

void MockWebClipboardImpl::writeURL(
    const WebKit::WebURL& url, const WebKit::WebString& title) {
  m_htmlText = WebString::fromUTF8(
      webkit_glue::WebClipboardImpl::URLToMarkup(url, title));
  m_plainText = url.spec().utf16();
  m_writeSmartPaste = false;
}

void MockWebClipboardImpl::writeImage(const WebKit::WebImage& image,
    const WebKit::WebURL& url, const WebKit::WebString& title) {
  if (!image.isNull()) {
    m_htmlText = WebString::fromUTF8(
        webkit_glue::WebClipboardImpl::URLToImageMarkup(url, title));
    m_plainText = m_htmlText;
    m_writeSmartPaste = false;
  }
}
