// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UPDATE_CLIENT_IOS_CHROME_UPDATE_QUERY_PARAMS_DELEGATE_H_
#define IOS_CHROME_BROWSER_UPDATE_CLIENT_IOS_CHROME_UPDATE_QUERY_PARAMS_DELEGATE_H_

#include <string>

#include "base/macros.h"
#include "components/update_client/update_query_params_delegate.h"

class IOSChromeUpdateQueryParamsDelegate
    : public update_client::UpdateQueryParamsDelegate {
 public:
  IOSChromeUpdateQueryParamsDelegate();
  ~IOSChromeUpdateQueryParamsDelegate() override;

  // Gets the instance for IOSChromeUpdateQueryParamsDelegate.
  static IOSChromeUpdateQueryParamsDelegate* GetInstance();

  // update_client::UpdateQueryParamsDelegate:
  std::string GetExtraParams() override;

  // Returns the language for the present locale. Possible return values are
  // standard tags for languages, such as "en", "en-US", "de", "fr", "af", etc.
  static const std::string& GetLang();

 private:
  DISALLOW_COPY_AND_ASSIGN(IOSChromeUpdateQueryParamsDelegate);
};

#endif  // IOS_CHROME_BROWSER_UPDATE_CLIENT_IOS_CHROME_UPDATE_QUERY_PARAMS_DELEGATE_H_
