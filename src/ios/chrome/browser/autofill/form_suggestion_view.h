// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_FORM_SUGGESTION_VIEW_H_
#define IOS_CHROME_BROWSER_AUTOFILL_FORM_SUGGESTION_VIEW_H_

#import <UIKit/UIKit.h>

@class FormSuggestion;
@protocol FormSuggestionViewClient;

// A scrollable view for displaying user-selectable autofill form suggestions.
@interface FormSuggestionView : UIScrollView<UIInputViewAudioFeedback>

// The current suggestions this view is showing.
@property(nonatomic, readonly) NSArray<FormSuggestion*>* suggestions;

// A view added at the end of the current suggestions.
@property(nonatomic, strong) UIView* trailingView;

// Initializes with |frame| and |client| to show |suggestions|.
- (instancetype)initWithFrame:(CGRect)frame
                       client:(id<FormSuggestionViewClient>)client
                  suggestions:(NSArray<FormSuggestion*>*)suggestions;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_FORM_SUGGESTION_VIEW_H_
