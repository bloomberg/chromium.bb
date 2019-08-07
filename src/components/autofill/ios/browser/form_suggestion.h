// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_FORM_SUGGESTION_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_FORM_SUGGESTION_H_

#import <Foundation/Foundation.h>

// Represents a user-selectable suggestion for a single field within a form
// on a web page.
@interface FormSuggestion : NSObject

// The string in the form to show to the user to represent the suggestion.
@property(copy, readonly, nonatomic) NSString* value;

// An optional user-visible description for this suggestion.
@property(copy, readonly, nonatomic) NSString* displayDescription;

// The string in the form to identify credit card icon.
@property(copy, readonly, nonatomic) NSString* icon;

// The integer identifier associated with the suggestion. Identifiers greater
// than zero are profile or credit card identifiers.
@property(assign, readonly, nonatomic) NSInteger identifier;

// Returns FormSuggestion (immutable) with given values.
+ (FormSuggestion*)suggestionWithValue:(NSString*)value
                    displayDescription:(NSString*)displayDescription
                                  icon:(NSString*)icon
                            identifier:(NSInteger)identifier;

@end

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_FORM_SUGGESTION_H_
