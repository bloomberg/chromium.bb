// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_CHROME_EXTENSION_MESSAGES_H_
#define CHROME_COMMON_EXTENSIONS_CHROME_EXTENSION_MESSAGES_H_

// Chrome-specific IPC messages for extensions.
// Extension-related messages that aren't specific to Chrome live in
// extensions/common/extension_messages.h.

#include <stdint.h>

#include <string>

#include "base/strings/string16.h"
#include "base/values.h"
#include "chrome/common/extensions/api/automation_internal.h"
#include "extensions/common/stack_frame.h"
#include "ipc/ipc_message_macros.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_relative_bounds.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/gfx/transform.h"
#include "url/gurl.h"

#define IPC_MESSAGE_START ChromeExtensionMsgStart

// Messages sent from the browser to the renderer.

#endif  // CHROME_COMMON_EXTENSIONS_CHROME_EXTENSION_MESSAGES_H_
