// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/loader/chrome_navigation_data.h"

#include "base/memory/ptr_util.h"
#include "base/supports_user_data.h"
#include "net/url_request/url_request.h"

const void* const kChromeNavigationDataUserDataKey =
    &kChromeNavigationDataUserDataKey;

namespace {

// UserData object that owns ChromeNavigationData. This is used rather than
// having ChromeNavigationData directly extend base::SupportsUserData::Data to
// avoid naming conflicts between Data::Clone() and
// content::NavigationData::Clone().
class NavigationDataOwner : public base::SupportsUserData::Data {
 public:
  NavigationDataOwner() = default;
  ~NavigationDataOwner() override = default;

  ChromeNavigationData* data() { return &data_; }

 private:
  ChromeNavigationData data_;

  DISALLOW_COPY_AND_ASSIGN(NavigationDataOwner);
};

}  // namespace

ChromeNavigationData::ChromeNavigationData() {}

ChromeNavigationData::~ChromeNavigationData() {}

// static
ChromeNavigationData* ChromeNavigationData::GetDataAndCreateIfNecessary(
    net::URLRequest* request) {
  if (!request)
    return nullptr;
  NavigationDataOwner* data_owner_ptr = static_cast<NavigationDataOwner*>(
      request->GetUserData(kChromeNavigationDataUserDataKey));
  if (data_owner_ptr)
    return data_owner_ptr->data();
  std::unique_ptr<NavigationDataOwner> data_owner =
      std::make_unique<NavigationDataOwner>();
  data_owner_ptr = data_owner.get();
  request->SetUserData(kChromeNavigationDataUserDataKey, std::move(data_owner));
  return data_owner_ptr->data();
}

std::unique_ptr<content::NavigationData> ChromeNavigationData::Clone() {
  std::unique_ptr<ChromeNavigationData> copy(new ChromeNavigationData());
  if (data_reduction_proxy_data_)
    copy->SetDataReductionProxyData(data_reduction_proxy_data_->DeepCopy());
  return std::move(copy);
}
