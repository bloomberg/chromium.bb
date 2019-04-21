// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/translate/cwv_translation_language_internal.h"

#import <Foundation/Foundation.h>

#include "base/strings/sys_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

using CWVTranslationLanguageTest = PlatformTest;

// Tests CWVTranslationLanguage initialization.
TEST_F(CWVTranslationLanguageTest, Initialization) {
  NSString* language_code = @"ja";
  NSString* localized_name = @"Japanese";
  NSString* native_name = @"日本語";
  CWVTranslationLanguage* language = [[CWVTranslationLanguage alloc]
      initWithLanguageCode:base::SysNSStringToUTF8(language_code)
             localizedName:base::SysNSStringToUTF16(localized_name)
                nativeName:base::SysNSStringToUTF16(native_name)];

  EXPECT_NSEQ(language_code, language.languageCode);
  EXPECT_NSEQ(localized_name, language.localizedName);
  EXPECT_NSEQ(native_name, language.nativeName);
}

}  // namespace ios_web_view
