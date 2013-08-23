// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/os_exchange_data_provider_aurax11.h"

// Clean up X11 header polution
#undef None
#undef Bool

#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

const char kGoogleTitle[] = "Google";
const char kGoogleURL[] = "http://www.google.com/";

TEST(OSExchangeDataProviderAuraX11Test, MozillaURL) {
  base::MessageLoopForUI message_loop;
  ui::OSExchangeDataProviderAuraX11 provider;

  // Check that we can get titled entries.
  provider.SetURL(GURL(kGoogleURL), ASCIIToUTF16(kGoogleTitle));
  {
    GURL out_gurl;
    base::string16 out_str;
    EXPECT_TRUE(provider.GetURLAndTitle(&out_gurl, &out_str));
    EXPECT_EQ(ASCIIToUTF16(kGoogleTitle), out_str);
    EXPECT_EQ(kGoogleURL, out_gurl.spec());
  }

  // Check that we can get non-titled entries.
  provider.SetURL(GURL(kGoogleURL), string16());
  {
    GURL out_gurl;
    base::string16 out_str;
    EXPECT_TRUE(provider.GetURLAndTitle(&out_gurl, &out_str));
    EXPECT_EQ(string16(), out_str);
    EXPECT_EQ(kGoogleURL, out_gurl.spec());
  }
}
