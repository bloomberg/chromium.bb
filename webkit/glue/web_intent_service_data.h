// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEB_INTENT_SERVICE_DATA_H_
#define WEBKIT_GLUE_WEB_INTENT_SERVICE_DATA_H_
#pragma once

#include <iosfwd>

#include "base/string16.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/webkit_glue_export.h"

namespace WebKit {
class WebIntentServiceInfo;
}

namespace webkit_glue {

// Describes the relevant elements of a WebIntent service.
struct WEBKIT_GLUE_EXPORT WebIntentServiceData {
  // An intents disposition determines which context the service is opened in.
  enum Disposition {
    DISPOSITION_WINDOW,  // Open service inside a new window. (Default)
    DISPOSITION_INLINE,  // Open service inside the picker UI window.
  };

  WebIntentServiceData();
  WebIntentServiceData(const string16& action,
                       const string16& type,
                       const string16& scheme,
                       const GURL& service_url,
                       const string16& title);
  explicit WebIntentServiceData(const WebKit::WebIntentServiceInfo& info);
  ~WebIntentServiceData();

  bool operator==(const WebIntentServiceData& other) const;

  void setDisposition(const string16& disp);

  // |action|+|type| forms one type of unique service key. The other is
  // |scheme|.
  string16 action;  // Name of action provided by service.
  string16 type;  // MIME type of data accepted by service.

  // |scheme| forms one type of unique service key. The other is
  // |action|+|type|. Note that this scheme is for purposes
  // of matching intent services, not the scheme associated
  // with the service_url.
  string16 scheme;  // Protocol scheme for intent service matching.

  GURL service_url;  // URL for service invocation.
  string16 title;  // The title of the service.

  // Disposition specifies the way in which a service is surfaced to the user.
  // Current supported dispositions are declared in the |Disposition| enum.
  Disposition disposition;
};

// Printing operator - helps gtest produce readable error messages.
WEBKIT_GLUE_EXPORT std::ostream& operator<<(
    std::ostream& os,
    const webkit_glue::WebIntentServiceData& intent);

}  // namespace webkit_glue

#endif  // CHROME_BROWSER_INTENTS_WEB_INTENT_SERVICE_DATA_H_
