// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_PASSWORDS_FETCHER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_PASSWORDS_FETCHER_H_

#import <Foundation/Foundation.h>
#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"

namespace autofill {
struct PasswordForm;
}  // namespace autofill

@class PasswordFetcher;

namespace password_manager {
class PasswordStore;
}  // namespace password_manager

// Protocol to receive the passwords fetched asynchronously.
@protocol PasswordFetcherDelegate

// Saved passwords has been fetched or updated.
- (void)passwordFetcher:(PasswordFetcher*)passwordFetcher
      didFetchPasswords:
          (std::vector<std::unique_ptr<autofill::PasswordForm>>&)passwords;

@end

@interface PasswordFetcher : NSObject

// The designated initializer. |browserState| must not be nil.
- (instancetype)initWithPasswordStore:
                    (scoped_refptr<password_manager::PasswordStore>)
                        passwordStore
                             delegate:(id<PasswordFetcherDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_PASSWORDS_FETCHER_H_
