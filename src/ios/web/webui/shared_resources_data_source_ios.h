// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEBUI_SHARED_RESOURCES_DATA_SOURCE_IOS_H_
#define IOS_WEB_WEBUI_SHARED_RESOURCES_DATA_SOURCE_IOS_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ios/web/public/webui/url_data_source_ios.h"

namespace web {

// A DataSource for chrome://resources/ URLs.
class SharedResourcesDataSourceIOS : public URLDataSourceIOS {
 public:
  SharedResourcesDataSourceIOS();

  // web::URLDataSourceIOS implementation.
  std::string GetSource() const override;
  void StartDataRequest(const std::string& path,
                        URLDataSourceIOS::GotDataCallback callback) override;
  std::string GetMimeType(const std::string& path) const override;

 private:
  ~SharedResourcesDataSourceIOS() override;

  DISALLOW_COPY_AND_ASSIGN(SharedResourcesDataSourceIOS);
};

}  // namespace web

#endif  // IOS_WEB_WEBUI_SHARED_RESOURCES_DATA_SOURCE_IOS_H_
