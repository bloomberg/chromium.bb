// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/mock_webclipboard_impl.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "net/base/escape.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCommon.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebDragData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebImage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "webkit/glue/webclipboard_impl.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/support/webkit_support_gfx.h"

#if WEBKIT_USING_CG
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebVector;

MockWebClipboardImpl::MockWebClipboardImpl() {
}

MockWebClipboardImpl::~MockWebClipboardImpl() {
}

bool MockWebClipboardImpl::isFormatAvailable(Format format, Buffer buffer) {
  switch (format) {
    case FormatPlainText:
      return !m_plainText.isEmpty();

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
#if defined(OS_POSIX) && !defined(OS_MACOSX)
      break;
#endif
    default:
      NOTREACHED();
      return false;
  }

  return true;
}

WebVector<WebString> MockWebClipboardImpl::readAvailableTypes(
    Buffer buffer, bool* containsFilenames) {
  *containsFilenames = false;
  std::vector<WebString> results;
  if (!m_plainText.isEmpty()) {
    results.push_back(WebString("text/plain"));
  }
  if (!m_htmlText.isEmpty()) {
    results.push_back(WebString("text/html"));
  }
  if (!m_image.isNull()) {
    results.push_back(WebString("image/png"));
  }
  return results;
}

WebKit::WebString MockWebClipboardImpl::readPlainText(
    WebKit::WebClipboard::Buffer buffer) {
  return m_plainText;
}

// TODO(wtc): set output argument *url.
WebKit::WebString MockWebClipboardImpl::readHTML(
    WebKit::WebClipboard::Buffer buffer, WebKit::WebURL* url,
    unsigned* fragmentStart, unsigned* fragmentEnd) {
  *fragmentStart = 0;
  *fragmentEnd = static_cast<unsigned>(m_htmlText.length());
  return m_htmlText;
}

WebKit::WebData MockWebClipboardImpl::readImage(
    WebKit::WebClipboard::Buffer buffer) {
  std::string data;
  std::vector<unsigned char> encoded_image;
  // TODO(dcheng): Verify that we can assume the image is ARGB8888. Note that
  // for endianess reasons, it will be BGRA8888 on Windows.
#if WEBKIT_USING_SKIA
  const SkBitmap& bitmap = m_image.getSkBitmap();
  SkAutoLockPixels lock(bitmap);
  webkit_support::EncodeBGRAPNG(static_cast<unsigned char*>(bitmap.getPixels()),
                                bitmap.width(),
                                bitmap.height(),
                                bitmap.rowBytes(),
                                false,
                                &encoded_image);
#elif WEBKIT_USING_CG
  CGImageRef image = m_image.getCGImageRef();
  CFDataRef image_data_ref =
      CGDataProviderCopyData(CGImageGetDataProvider(image));
  webkit_support::EncodeBGRAPNG(CFDataGetBytePtr(image_data_ref),
                                CGImageGetWidth(image),
                                CGImageGetHeight(image),
                                CGImageGetBytesPerRow(image),
                                false,
                                &encoded_image);
  CFRelease(image_data_ref);
#endif
  data.assign(reinterpret_cast<char*>(vector_as_array(&encoded_image)),
              encoded_image.size());
  return data;
}

void MockWebClipboardImpl::writeHTML(
    const WebKit::WebString& htmlText, const WebKit::WebURL& url,
    const WebKit::WebString& plainText, bool writeSmartPaste) {
  m_htmlText = htmlText;
  m_plainText = plainText;
  m_image.reset();
  m_writeSmartPaste = writeSmartPaste;
}

void MockWebClipboardImpl::writePlainText(const WebKit::WebString& plain_text) {
  m_htmlText = WebKit::WebString();
  m_plainText = plain_text;
  m_image.reset();
  m_writeSmartPaste = false;
}

void MockWebClipboardImpl::writeURL(
    const WebKit::WebURL& url, const WebKit::WebString& title) {
  m_htmlText = WebString::fromUTF8(
      webkit_glue::WebClipboardImpl::URLToMarkup(url, title));
  m_plainText = url.spec().utf16();
  m_image.reset();
  m_writeSmartPaste = false;
}

void MockWebClipboardImpl::writeImage(const WebKit::WebImage& image,
    const WebKit::WebURL& url, const WebKit::WebString& title) {
  if (!image.isNull()) {
    m_htmlText = WebString::fromUTF8(
        webkit_glue::WebClipboardImpl::URLToImageMarkup(url, title));
    m_plainText = m_htmlText;
    m_image = image;
    m_writeSmartPaste = false;
  }
}

void MockWebClipboardImpl::writeDataObject(const WebKit::WebDragData& data) {
  m_htmlText = data.htmlText();
  m_plainText = data.plainText();
  m_image.reset();
  m_writeSmartPaste = false;
}
