// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/permission_bubble/permission_prompt_impl.h"

// static
std::unique_ptr<PermissionPrompt> PermissionPrompt::Create(
    content::WebContents* web_contents,
    Delegate* delegate) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser) {
    DLOG(WARNING) << "Permission prompt suppressed because the WebContents is "
                     "not attached to any Browser window.";
    return nullptr;
  }
  return base::WrapUnique(new PermissionPromptImpl(browser, delegate));
}
