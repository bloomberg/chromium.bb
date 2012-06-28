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

WebIntentServiceData::WebIntentServiceData(const string16& svc_action,
                                           const string16& svc_type,
                                           const string16& svc_scheme,
                                           const GURL& svc_service_url,
                                           const string16& svc_title)
    : action(svc_action),
      type(svc_type),
      scheme(svc_scheme),
      service_url(svc_service_url),
      title(svc_title),
      disposition(WebIntentServiceData::DISPOSITION_WINDOW) {
}

WebIntentServiceData::WebIntentServiceData(
    const WebKit::WebIntentServiceInfo& info)
    : action(info.action()),
      type(info.type()),
      scheme(string16()),
      service_url(info.url()),
      title(info.title()),
      disposition(WebIntentServiceData::DISPOSITION_WINDOW) {
  setDisposition(info.disposition());
}

WebIntentServiceData::~WebIntentServiceData() {}

bool WebIntentServiceData::operator==(const WebIntentServiceData& other) const {
  return action == other.action &&
         type == other.type &&
         scheme == other.scheme &&
         service_url == other.service_url &&
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
         "{action=" << UTF16ToUTF8(intent.action) <<
         "type=, " << UTF16ToUTF8(intent.type) <<
         "scheme=, " << UTF16ToUTF8(intent.scheme) <<
         "service_url=, " << intent.service_url <<
         "title=, " << UTF16ToUTF8(intent.title) <<
         "disposition=, " << intent.disposition <<
         "}";
}

}  // namespace webkit_glue
