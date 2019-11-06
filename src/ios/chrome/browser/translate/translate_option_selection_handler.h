// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TRANSLATE_TRANSLATE_OPTION_SELECTION_HANDLER_H_
#define IOS_CHROME_BROWSER_TRANSLATE_TRANSLATE_OPTION_SELECTION_HANDLER_H_

#import <Foundation/Foundation.h>

namespace translate {
class TranslateInfoBarDelegate;
}
@protocol TranslateOptionSelectionDelegate;

// Protocol adopted by an object that can provide an interface for a user to
// select a translate option given by the translate::TranslateInfoBarDelegate.
@protocol TranslateOptionSelectionHandler

// Tells the handler to display the translate options menu using the information
// in |infobarDelegate| and telling |delegate| the results of the selection.
- (void)
    showTranslateOptionSelectorWithInfoBarDelegate:
        (const translate::TranslateInfoBarDelegate*)infobarDelegate
                                          delegate:
                                              (id<TranslateOptionSelectionDelegate>)
                                                  delegate;

// Tells the handler to stop displaying the translate options menu and inform
// the delegate no selection was made.
- (void)dismissTranslateOptionSelector;

@end

#endif  // IOS_CHROME_BROWSER_TRANSLATE_TRANSLATE_OPTION_SELECTION_HANDLER_H_
