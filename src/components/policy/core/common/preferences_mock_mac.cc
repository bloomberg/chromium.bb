// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/preferences_mock_mac.h"

MockPreferences::MockPreferences() {
  values_.reset(CFDictionaryCreateMutable(kCFAllocatorDefault,
                                          0,
                                          &kCFTypeDictionaryKeyCallBacks,
                                          &kCFTypeDictionaryValueCallBacks));
  forced_.reset(CFSetCreateMutable(kCFAllocatorDefault,
                                   0,
                                   &kCFTypeSetCallBacks));
}

MockPreferences::~MockPreferences() {
}

Boolean MockPreferences::AppSynchronize(CFStringRef applicationID) {
  return true;
}

CFPropertyListRef MockPreferences::CopyAppValue(CFStringRef key,
                                                CFStringRef applicationID) {
  CFPropertyListRef value;
  Boolean found = CFDictionaryGetValueIfPresent(values_,
                                                key,
                                                &value);
  if (!found || !value)
    return NULL;
  CFRetain(value);
  return value;
}

Boolean MockPreferences::AppValueIsForced(CFStringRef key,
                                          CFStringRef applicationID) {
  return CFSetContainsValue(forced_, key);
}

void MockPreferences::AddTestItem(CFStringRef key,
                                  CFPropertyListRef value,
                                  bool is_forced) {
  CFDictionarySetValue(values_, key, value);
  if (is_forced)
    CFSetAddValue(forced_, key);
}
