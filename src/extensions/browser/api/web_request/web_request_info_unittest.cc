// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_info.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/api/web_request/web_request_api_constants.h"
#include "extensions/browser/extension_navigation_ui_data.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "base/json/json_writer.h"

namespace extensions {
namespace {
constexpr base::FilePath::CharType kFilePath[] = FILE_PATH_LITERAL("some_path");
}

TEST(WebRequestInfoTest, CreateRequestBodyDataFromFile) {
  content::TestBrowserThreadBundle test_bundle;

  network::ResourceRequest request;
  request.method = "POST";
  request.request_body = base::MakeRefCounted<network::ResourceRequestBody>();
  request.request_body->AppendFileRange(base::FilePath(kFilePath), 0,
                                        std::numeric_limits<uint64_t>::max(),
                                        base::Time());
  WebRequestInfo info(0, 0, 0, nullptr, 0, nullptr, request, false);
  ASSERT_TRUE(info.request_body_data);
  auto* value = info.request_body_data->FindKey(
      extension_web_request_api_constants::kRequestBodyRawKey);
  ASSERT_TRUE(value);

  base::ListValue expected_value;
  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetString(extension_web_request_api_constants::kRequestBodyRawFileKey,
                  kFilePath);
  expected_value.Append(std::move(dict));
  EXPECT_TRUE(value->Equals(&expected_value));
}

}  // namespace extensions
