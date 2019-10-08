// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_BINARY_SIZE_LIBSUPERSIZE_CASPIAN_FILE_FORMAT_H_
#define TOOLS_BINARY_SIZE_LIBSUPERSIZE_CASPIAN_FILE_FORMAT_H_

#include <istream>

namespace caspian {

struct SizeInfo;  // forward declare

void ParseSizeInfo(std::istream* gzstream, caspian::SizeInfo* info);

}  // namespace caspian

#endif  // TOOLS_BINARY_SIZE_LIBSUPERSIZE_CASPIAN_FILE_FORMAT_H_
