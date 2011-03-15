// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBCLIPBOARD_IMPL_H_
#define WEBCLIPBOARD_IMPL_H_

#include "third_party/WebKit/Source/WebKit/chromium/public/WebClipboard.h"
#include "ui/base/clipboard/clipboard.h"

#include <string>

namespace webkit_glue {

class WebClipboardImpl : public WebKit::WebClipboard {
 public:
  static std::string URLToMarkup(const WebKit::WebURL& url,
      const WebKit::WebString& title);
  static std::string URLToImageMarkup(const WebKit::WebURL& url,
      const WebKit::WebString& title);

  virtual ~WebClipboardImpl();

  // WebClipboard methods:
  virtual bool isFormatAvailable(Format, Buffer);
  virtual WebKit::WebString readPlainText(Buffer);
  virtual WebKit::WebString readHTML(Buffer, WebKit::WebURL* source_url);
  virtual WebKit::WebData readImage(Buffer);
  virtual void writeHTML(
      const WebKit::WebString& html_text,
      const WebKit::WebURL& source_url,
      const WebKit::WebString& plain_text,
      bool write_smart_paste);
  virtual void writePlainText(const WebKit::WebString& plain_text);
  virtual void writeURL(
      const WebKit::WebURL&,
      const WebKit::WebString& title);
  virtual void writeImage(
      const WebKit::WebImage&,
      const WebKit::WebURL& source_url,
      const WebKit::WebString& title);
  virtual void writeData(
      const WebKit::WebString& type,
      const WebKit::WebString& data,
      const WebKit::WebString& metadata);

  virtual WebKit::WebVector<WebKit::WebString> readAvailableTypes(
      Buffer, bool* contains_filenames);
  virtual bool readData(Buffer, const WebKit::WebString& type,
      WebKit::WebString* data, WebKit::WebString* metadata);
  virtual WebKit::WebVector<WebKit::WebString> readFilenames(Buffer);

 private:
  bool ConvertBufferType(Buffer, ui::Clipboard::Buffer*);
};

}  // namespace webkit_glue

#endif  // WEBCLIPBOARD_IMPL_H_
