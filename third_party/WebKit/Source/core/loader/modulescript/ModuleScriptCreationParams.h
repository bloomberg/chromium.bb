// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ModuleScriptCreationParams_h
#define ModuleScriptCreationParams_h

#include "platform/CrossThreadCopier.h"
#include "platform/loader/fetch/AccessControlStatus.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Optional.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

// ModuleScriptCreationParams contains parameters for creating ModuleScript.
class ModuleScriptCreationParams {
 public:
  ModuleScriptCreationParams(
      const KURL& response_url,
      const String& source_text,
      WebURLRequest::FetchCredentialsMode fetch_credentials_mode,
      AccessControlStatus access_control_status)
      : response_url_(response_url),
        source_text_(source_text),
        fetch_credentials_mode_(fetch_credentials_mode),
        access_control_status_(access_control_status) {}
  ~ModuleScriptCreationParams() = default;

  const KURL& GetResponseUrl() const { return response_url_; };
  const String& GetSourceText() const { return source_text_; }
  WebURLRequest::FetchCredentialsMode GetFetchCredentialsMode() const {
    return fetch_credentials_mode_;
  }
  AccessControlStatus GetAccessControlStatus() const {
    return access_control_status_;
  }

 private:
  const KURL response_url_;
  const String source_text_;
  const WebURLRequest::FetchCredentialsMode fetch_credentials_mode_;
  const AccessControlStatus access_control_status_;
};

// Creates a deep copy because |response_url_| and |source_text_| are not
// cross-thread-transfer-safe.
template <>
struct CrossThreadCopier<ModuleScriptCreationParams> {
  static ModuleScriptCreationParams Copy(
      const ModuleScriptCreationParams& params) {
    return ModuleScriptCreationParams(
        params.GetResponseUrl().Copy(), params.GetSourceText().IsolatedCopy(),
        params.GetFetchCredentialsMode(), params.GetAccessControlStatus());
  }
};

}  // namespace blink

#endif  // ModuleScriptCreationParams_h
