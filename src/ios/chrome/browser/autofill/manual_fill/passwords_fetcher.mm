// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/manual_fill/passwords_fetcher.h"

#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_list_sorter.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/passwords/save_passwords_consumer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordFetcher ()<SavePasswordsConsumerDelegate> {
  // The interface for getting and manipulating a user's saved passwords.
  scoped_refptr<password_manager::PasswordStore> _passwordStore;
  // A helper object for passing data about saved passwords from a finished
  // password store request to the SavePasswordsCollectionViewController.
  std::unique_ptr<ios::SavePasswordsConsumer> _savedPasswordsConsumer;
  // The list of the user's saved passwords.
  std::vector<std::unique_ptr<autofill::PasswordForm>> _savedForms;
}

// Delegate to send the fetchted passwords.
@property(nonatomic, weak) id<PasswordFetcherDelegate> delegate;

@end

@implementation PasswordFetcher

@synthesize delegate = _delegate;

#pragma mark - Initialization

- (instancetype)initWithPasswordStore:
                    (scoped_refptr<password_manager::PasswordStore>)
                        passwordStore
                             delegate:(id<PasswordFetcherDelegate>)delegate {
  DCHECK(passwordStore);
  DCHECK(delegate);
  self = [super init];
  if (self) {
    _delegate = delegate;
    _passwordStore = passwordStore;
    _savedPasswordsConsumer.reset(new ios::SavePasswordsConsumer(self));
    _passwordStore->GetAutofillableLogins(_savedPasswordsConsumer.get());
  }
  return self;
}

#pragma mark - SavePasswordsConsumerDelegate

- (void)onGetPasswordStoreResults:
    (std::vector<std::unique_ptr<autofill::PasswordForm>>&)result {
  for (auto it = result.begin(); it != result.end(); ++it) {
    if (!(*it)->blacklisted_by_user)
      _savedForms.push_back(std::move(*it));
  }

  password_manager::DuplicatesMap savedPasswordDuplicates;
  password_manager::SortEntriesAndHideDuplicates(&_savedForms,
                                                 &savedPasswordDuplicates);
  [self.delegate passwordFetcher:self didFetchPasswords:_savedForms];
}

@end
