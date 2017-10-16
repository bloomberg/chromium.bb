// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebPluginScriptForbiddenScope.h"

#include "platform/plugins/PluginScriptForbiddenScope.h"

namespace blink {

bool WebPluginScriptForbiddenScope::IsForbidden() {
  return PluginScriptForbiddenScope::IsForbidden();
}

}  // namespace blink
