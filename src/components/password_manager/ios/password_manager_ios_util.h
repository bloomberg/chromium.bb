// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_MANAGER_IOS_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_MANAGER_IOS_UTIL_H_

#import <Foundation/Foundation.h>

#include "url/gurl.h"

namespace web {
class WebState;
}

namespace autofill {
struct FormData;
}

namespace password_manager {

// Checks if |web_state|'s content is a secure HTML. This is done in order to
// ignore API calls from insecure context.
bool WebStateContentIsSecureHtml(const web::WebState* web_state);

// Extracts password form data from |json_string| to |form_data| and returns
// whether the xtraction was successful.
bool JsonStringToFormData(NSString* json_string,
                          autofill::FormData* form_data,
                          GURL page_url);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_MANAGER_IOS_UTIL_H_
