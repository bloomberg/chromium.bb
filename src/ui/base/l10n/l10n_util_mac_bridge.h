// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_L10N_L10N_UTIL_MAC_BRIDGE_H_
#define UI_BASE_L10N_L10N_UTIL_MAC_BRIDGE_H_

// An ObjC wrapper around namespaced C++ l10n methods.
@interface L10NUtils : NSObject

+ (NSString*)stringForMessageId:(int)messageId;

@end

#endif  // UI_BASE_L10N_L10N_UTIL_MAC_BRIDGE_H_
