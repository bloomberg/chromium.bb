// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_BUBBLE_TYPE_H_
#define CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_BUBBLE_TYPE_H_

#include "base/strings/string16.h"
#include "url/gurl.h"

namespace extensions {
class ExtensionRegistry;
}

// Describes the contents of the fullscreen exit bubble.
// For example, if the user already agreed to fullscreen mode and the
// web page then requests mouse lock, "do you want to allow mouse lock"
// will be shown.
enum ExclusiveAccessBubbleType {
  EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE = 0,

  // For tab fullscreen mode.
  // More comments about tab and browser fullscreen mode can be found in
  // chrome/browser/ui/exclusive_access/fullscreen_controller.h.
  EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION,
  EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_MOUSELOCK_EXIT_INSTRUCTION,
  EXCLUSIVE_ACCESS_BUBBLE_TYPE_MOUSELOCK_EXIT_INSTRUCTION,
  EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION,

  // For browser fullscreen mode.
  EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION,
  EXCLUSIVE_ACCESS_BUBBLE_TYPE_EXTENSION_FULLSCREEN_EXIT_INSTRUCTION
};

namespace exclusive_access_bubble {

// Gets the text informing the user what state they have entered.
// DEPRECATED: This is used only by the "classic" exclusive access bubble. The
// new bubble only shows the instruction text.
base::string16 GetLabelTextForType(ExclusiveAccessBubbleType type,
                                   const GURL& url,
                                   extensions::ExtensionRegistry* registry);
// Gets the text for the deny and allow buttons.
// DEPRECATED: This is used only by the "classic" exclusive access bubble. The
// new bubble only shows the instruction text.
base::string16 GetDenyButtonTextForType(ExclusiveAccessBubbleType type);
base::string16 GetAllowButtonTextForType(ExclusiveAccessBubbleType type,
                                         const GURL& url);
// Gets the text instructing the user how to exit an exclusive access mode.
// |accelerator| is the name of the key to exit fullscreen mode.
base::string16 GetInstructionTextForType(ExclusiveAccessBubbleType type,
                                         const base::string16& accelerator);

}  // namespace exclusive_access_bubble

#endif  // CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_BUBBLE_TYPE_H_
