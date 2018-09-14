// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webrunner/service/common.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"

namespace webrunner {

constexpr char kIncognitoSwitch[] = "incognito";

constexpr char kWebContextDataPath[] = "/web_context_data";

}  // namespace webrunner
