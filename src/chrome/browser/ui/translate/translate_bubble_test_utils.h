// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_BUBBLE_TEST_UTILS_H_
#define CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_BUBBLE_TEST_UTILS_H_

#include "base/strings/string16.h"

class TranslateBubbleModel;
class Browser;

namespace translate {

// TODO(groby): Move this over to TranslateBubbleDelegate once the translate
// bubble has been migrated over to the new Bubble system.
namespace test_utils {

// Obtain the TranslateModel associated with the current bubble.
const TranslateBubbleModel* GetCurrentModel(Browser* browser);

// Presses 'Translate' on the currently open translate bubble.
void PressTranslate(Browser* browser);

// Presses 'Revert' on the currently opened translate bubble.
void PressRevert(Browser* browser);

// Selects the target language with the given display name on the opened
// translate bubble.
void SelectTargetLanguageByDisplayName(Browser* browser,
                                       const base::string16& display_name);

}  // namespace test_utils

}  // namespace translate

#endif  // CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_BUBBLE_TEST_UTILS_H_
