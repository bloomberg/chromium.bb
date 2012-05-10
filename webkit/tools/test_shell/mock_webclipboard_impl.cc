// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/mock_webclipboard_impl.h"

#include <algorithm>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCommon.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebDragData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebImage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "ui/base/clipboard/clipboard.h"
#include "webkit/glue/webclipboard_impl.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/support/webkit_support_gfx.h"

using WebKit::WebDragData;
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
      return !m_plainText.isNull();

    case FormatHTML:
      return !m_htmlText.isNull();

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
  for (std::map<string16, string16>::const_iterator it = m_customData.begin();
       it != m_customData.end(); ++it) {
    CHECK(std::find(results.begin(), results.end(), it->first) ==
          results.end());
    results.push_back(it->first);
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
  const SkBitmap& bitmap = m_image.getSkBitmap();
  SkAutoLockPixels lock(bitmap);
  webkit_support::EncodeBGRAPNG(static_cast<unsigned char*>(bitmap.getPixels()),
                                bitmap.width(),
                                bitmap.height(),
                                bitmap.rowBytes(),
                                false,
                                &encoded_image);
  data.assign(reinterpret_cast<char*>(vector_as_array(&encoded_image)),
              encoded_image.size());
  return data;
}

WebKit::WebString MockWebClipboardImpl::readCustomData(
    WebKit::WebClipboard::Buffer buffer,
    const WebKit::WebString& type) {
  std::map<string16, string16>::const_iterator it = m_customData.find(type);
  if (it != m_customData.end())
    return it->second;
  return WebKit::WebString();
}

void MockWebClipboardImpl::writeHTML(
    const WebKit::WebString& htmlText, const WebKit::WebURL& url,
    const WebKit::WebString& plainText, bool writeSmartPaste) {
  clear();

  m_htmlText = htmlText;
  m_plainText = plainText;
  m_writeSmartPaste = writeSmartPaste;
}

void MockWebClipboardImpl::writePlainText(const WebKit::WebString& plain_text) {
  clear();

  m_plainText = plain_text;
}

void MockWebClipboardImpl::writeURL(
    const WebKit::WebURL& url, const WebKit::WebString& title) {
  clear();

  m_htmlText = WebString::fromUTF8(
      webkit_glue::WebClipboardImpl::URLToMarkup(url, title));
  m_plainText = url.spec().utf16();
}

void MockWebClipboardImpl::writeImage(const WebKit::WebImage& image,
    const WebKit::WebURL& url, const WebKit::WebString& title) {
  if (!image.isNull()) {
    clear();

    m_plainText = m_htmlText;
    m_htmlText = WebString::fromUTF8(
        webkit_glue::WebClipboardImpl::URLToImageMarkup(url, title));
    m_image = image;
  }
}

void MockWebClipboardImpl::writeDataObject(const WebDragData& data) {
  clear();

  const WebVector<WebDragData::Item>& itemList = data.items();
  for (size_t i = 0; i < itemList.size(); ++i) {
    const WebDragData::Item& item = itemList[i];
    switch (item.storageType) {
      case WebDragData::Item::StorageTypeString: {
        if (EqualsASCII(item.stringType, ui::Clipboard::kMimeTypeText)) {
          m_plainText = item.stringData;
          continue;
        }
        if (EqualsASCII(item.stringType, ui::Clipboard::kMimeTypeHTML)) {
          m_htmlText = item.stringData;
          continue;
        }
        m_customData.insert(std::make_pair(item.stringType, item.stringData));
        continue;
      }
      case WebDragData::Item::StorageTypeFilename:
      case WebDragData::Item::StorageTypeBinaryData:
        NOTREACHED();  // Currently unused by the clipboard implementation.
    }
  }
}

void MockWebClipboardImpl::clear() {
  m_plainText = WebString();
  m_htmlText = WebString();
  m_image.reset();
  m_customData.clear();
  m_writeSmartPaste = false;
}
