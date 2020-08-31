// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/public/cpp/rulebased/def/vi_viqr.h"

#include "base/stl_util.h"

namespace vi_viqr {

const char* kId = "vi_viqr";
bool kIs102 = false;
const char* kTransforms[] = {
    u8"dd",      u8"\u0111", u8"D[dD]",   u8"\u0110", u8"a\\(",    u8"\u0103",
    u8"a\\^",    u8"\u00e2", u8"e\\^",    u8"\u00ea", u8"o\\^",    u8"\u00f4",
    u8"o\\+",    u8"\u01a1", u8"u\\+",    u8"\u01b0", u8"A\\(",    u8"\u0102",
    u8"A\\^",    u8"\u00c2", u8"E\\^",    u8"\u00ca", u8"O\\^",    u8"\u00d4",
    u8"O\\+",    u8"\u01a0", u8"U\\+",    u8"\u01af", u8"\\\\\\(", u8"(",
    u8"\\\\\\^", u8"^",      u8"\\\\\\+", u8"+",      u8"\\\\\\`", u8"`",
    u8"\\\\\\'", u8"'",      u8"\\\\\\?", u8"?",      u8"\\\\\\~", u8"~",
    u8"\\\\\\.", u8".",      u8"\\`",     u8"\u0300", u8"\\'",     u8"\u0301",
    u8"\\?",     u8"\u0309", u8"\\~",     u8"\u0303", u8"\\.",     u8"\u0323"};
const unsigned int kTransformsLen = base::size(kTransforms);
const char* kHistoryPrune = nullptr;

}  // namespace vi_viqr
