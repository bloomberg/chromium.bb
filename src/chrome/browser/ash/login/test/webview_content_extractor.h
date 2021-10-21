// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_WEBVIEW_CONTENT_EXTRACTOR_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_WEBVIEW_CONTENT_EXTRACTOR_H_

#include <string>

namespace ash {
namespace test {

// Returns the contents of the <webview> identified by `element_ids`.
std::string GetWebViewContents(
    std::initializer_list<base::StringPiece> element_ids);

// Returns the contents of the <webview> identified by `element_id`.
std::string GetWebViewContentsById(const std::string& element_id);

}  // namespace test
}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
namespace test {
using ::ash::test::GetWebViewContents;
using ::ash::test::GetWebViewContentsById;
}  // namespace test
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
namespace test {
using ::chromeos::test::GetWebViewContents;
}
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_WEBVIEW_CONTENT_EXTRACTOR_H_
