// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APPS_DIRECTORY_ACCESS_CONFIRMATION_DIALOG_H_
#define CHROME_BROWSER_UI_APPS_DIRECTORY_ACCESS_CONFIRMATION_DIALOG_H_

#include "base/callback_forward.h"
#include "base/strings/string16.h"

namespace content {
class WebContents;
}

void CreateDirectoryAccessConfirmationDialog(bool writable,
                                             const base::string16& app_name,
                                             content::WebContents* web_contents,
                                             const base::Closure& on_accept,
                                             const base::Closure& on_cancel);

#endif  // CHROME_BROWSER_UI_APPS_DIRECTORY_ACCESS_CONFIRMATION_DIALOG_H_
