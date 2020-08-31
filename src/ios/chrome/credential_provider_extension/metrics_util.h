// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_METRICS_UTIL_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_METRICS_UTIL_H_

#import <Foundation/Foundation.h>

// Increases by 1 the app group shared metrics count for given key.
void UpdateUMACountForKey(NSString* key);

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_METRICS_UTIL_H_
