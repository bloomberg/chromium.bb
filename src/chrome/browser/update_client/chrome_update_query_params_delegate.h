// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATE_CLIENT_CHROME_UPDATE_QUERY_PARAMS_DELEGATE_H_
#define CHROME_BROWSER_UPDATE_CLIENT_CHROME_UPDATE_QUERY_PARAMS_DELEGATE_H_

#include <string>

#include "base/macros.h"
#include "components/update_client/update_query_params_delegate.h"

class ChromeUpdateQueryParamsDelegate
    : public update_client::UpdateQueryParamsDelegate {
 public:
  ChromeUpdateQueryParamsDelegate();
  ~ChromeUpdateQueryParamsDelegate() override;

  // Gets the LazyInstance for ChromeUpdateQueryParamsDelegate.
  static ChromeUpdateQueryParamsDelegate* GetInstance();

  // update_client::UpdateQueryParamsDelegate:
  std::string GetExtraParams() override;

  // Returns the language for the present locale. Possible return values are
  // standard tags for languages, such as "en", "en-US", "de", "fr", "af", etc.
  static const char* GetLang();

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeUpdateQueryParamsDelegate);
};

#endif  // CHROME_BROWSER_UPDATE_CLIENT_CHROME_UPDATE_QUERY_PARAMS_DELEGATE_H_
