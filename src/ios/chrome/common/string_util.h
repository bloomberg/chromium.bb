// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_STRING_UTIL_H_
#define IOS_CHROME_COMMON_STRING_UTIL_H_

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include <string>
#include <vector>

// Stores a string and a NSRange corresponding to the sub-string that was
// found between tag when looking for range with ParseStringWithTag.
struct StringWithTag {
  NSString* string;
  NSRange range;
};

// Stores a string and a list of NSRange corresponding to the sub-string
// that were found in tag when looking for range with ParseStringWithTags.
struct StringWithTags {
  NSString* string;
  std::vector<NSRange> ranges;

  StringWithTags();
  StringWithTags(NSString* string, std::vector<NSRange> ranges);

  StringWithTags(const StringWithTags& other);
  StringWithTags& operator=(const StringWithTags& other);

  StringWithTags(StringWithTags&& other);
  StringWithTags& operator=(StringWithTags&& other);

  ~StringWithTags();
};

// Parses a string with an embedded link inside, delineated by "BEGIN_LINK" and
// "END_LINK". Returns an attributed string with the text set as the parsed
// string with given |text_attributes| and the link range with
// |link_attributes|. The function asserts that there is one link.
NSAttributedString* AttributedStringFromStringWithLink(
    NSString* text,
    NSDictionary* text_attributes,
    NSDictionary* link_attributes);

// Parses a string with embedded links inside, delineated by "BEGIN_LINK" and
// "END_LINK". Returns the string without the delimiters and a list of all
// ranges for text contained inside the tag delimiters.
StringWithTags ParseStringWithLinks(NSString* text);

// Parses a string with an embedded tag inside, delineated by |begin_tag| and
// |end_tag|. Returns the string without the delimiters. The function asserts
// that there is at most one tag.
StringWithTag ParseStringWithTag(NSString* text,
                                 NSString* begin_tag,
                                 NSString* end_tag);

// Parses a string with embedded tags inside, delineated by |begin_tag| and
// |end_tag|. Returns the string without the delimiters and a list of all ranges
// for text contained inside the tag delimiters.
StringWithTags ParseStringWithTags(NSString* text,
                                   NSString* begin_tag,
                                   NSString* end_tag);

// Utility method that returns an NSCharacterSet containing Unicode graphics
// and drawing characters (but not including the Braille Patterns characters).
NSCharacterSet* GraphicCharactersSet();

// Cleans an NSString by collapsing whitespace and removing leading and trailing
// spaces. If |removeGraphicChars| is true, unicode graphic characters will also
// be removed from the string.
NSString* CleanNSStringForDisplay(NSString* dirty, BOOL removeGraphicChars);

// Cleans a std::string identically to CleanNSStringForDisplay()
std::string CleanStringForDisplay(const std::string& dirty,
                                  BOOL removeGraphicChars);

// Find the longest leading substring of |string| that, when rendered with
// |attributes|, will fit on a single line inside |targetWidth|. If |trailing|
// is YES, then find the trailing (instead of leading) substring.
NSString* SubstringOfWidth(NSString* string,
                           NSDictionary* attributes,
                           CGFloat targetWidth,
                           BOOL trailing);

// Returns the bound of an attributed string with NSRange
// |characterRange| in the |textView|.
CGRect TextViewLinkBound(UITextView* textView, NSRange characterRange);

#endif  // IOS_CHROME_COMMON_STRING_UTIL_H_
