// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>

#include "base/utf_string_conversions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIntentServiceInfo.h"
#include "webkit/glue/web_intent_service_data.h"

namespace webkit_glue {

static const char kIntentsInlineDisposition[] = "inline";

WebIntentServiceData::WebIntentServiceData()
    : disposition(WebIntentServiceData::DISPOSITION_WINDOW) {
}

WebIntentServiceData::WebIntentServiceData(const GURL& svc_url,
                                           const string16& svc_action,
                                           const string16& svc_type,
                                           const string16& svc_title)
    : service_url(svc_url),
      action(svc_action),
      type(svc_type),
      title(svc_title),
      disposition(WebIntentServiceData::DISPOSITION_WINDOW) {
}

WebIntentServiceData::WebIntentServiceData(
    const WebKit::WebIntentServiceInfo& info)
    : service_url(info.url()),
      action(info.action()),
      type(info.type()),
      title(info.title()),
      disposition(WebIntentServiceData::DISPOSITION_WINDOW) {
  setDisposition(info.disposition());
}

WebIntentServiceData::~WebIntentServiceData() {}

bool WebIntentServiceData::operator==(const WebIntentServiceData& other) const {
  return service_url == other.service_url &&
         action == other.action &&
         type == other.type &&
         title == other.title &&
         disposition == other.disposition;
}

void WebIntentServiceData::setDisposition(const string16& disp) {
  if (disp == ASCIIToUTF16(webkit_glue::kIntentsInlineDisposition))
    disposition = DISPOSITION_INLINE;
  else
    disposition = DISPOSITION_WINDOW;
}

std::ostream& operator<<(::std::ostream& os,
                         const WebIntentServiceData& intent) {
  return os <<
         "{" << intent.service_url <<
         ", " << UTF16ToUTF8(intent.action) <<
         ", " << UTF16ToUTF8(intent.type) <<
         ", " << UTF16ToUTF8(intent.title) <<
         ", " << intent.disposition <<
         "}";
}

}  // namespace webkit_glue
