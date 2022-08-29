// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_UTILS_H_
#define CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_UTILS_H_

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/autofill_assistant/password_change/proto/extensions.pb.h"

// Returns the icon for Google Assistant on Google-branded builds and a
// Chromium icon as a placeholder on non-branded builds.
const gfx::VectorIcon& GetAssistantIconOrFallback();

// Convert the protobuf enum that specifies a top icon for an Automate Password
// Change flow to its correct `gfx::VectorIcon` counterpart.
const gfx::VectorIcon& GetApcTopIconFromEnum(
    autofill_assistant::password_change::TopIcon icon);

#endif  // CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_UTILS_H_
