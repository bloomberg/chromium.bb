// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_WEB_ENGINE_CONFIG_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_WEB_ENGINE_CONFIG_H_

#include "fuchsia_web/webengine/web_engine_export.h"

namespace base {
class Value;
class CommandLine;
}  // namespace base

// Updates the `command_line` based on `config`. Returns `false` if the config
// is invalid.
WEB_ENGINE_EXPORT bool UpdateCommandLineFromConfigFile(
    const base::Value& config,
    base::CommandLine* command_line);

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_WEB_ENGINE_CONFIG_H_
