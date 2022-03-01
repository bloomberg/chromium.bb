// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_REPORT_MULTI_PARAMETER_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_REPORT_MULTI_PARAMETER_H_

#import <Foundation/Foundation.h>

#include "base/strings/string_piece.h"
#include "components/crash/core/common/crash_key.h"

// CrashReportMultiParameter keeps state of multiple report values that will be
// grouped in a single breakpad element to save limited number of breakpad
// values.
@interface CrashReportMultiParameter : NSObject
// Init with the breakpad parameter key.
- (instancetype)initWithKey:(crash_reporter::CrashKeyString<256>&)key
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Adds or updates an element in the dictionary of the breakpad report. Value
// are stored in the instance so every time you call this function, the whole
// JSON dictionary is regenerated. The total size of the serialized dictionary
// must be under 256 bytes due to Breakpad limitation. Setting a value to 0 will
// not remove the key. Use [removeValue:key] instead.
- (void)setValue:(base::StringPiece)key withValue:(int)value;

// Removes the key element from the dictionary. Note that this is different from
// setting the parameter to 0 or false.
- (void)removeValue:(base::StringPiece)key;

// Increases the key element by one.
- (void)incrementValue:(base::StringPiece)key;

// Decreases the key element by one. If the element is 0, the element is removed
// from the dictionary.
- (void)decrementValue:(base::StringPiece)key;
@end

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_REPORT_MULTI_PARAMETER_H_
