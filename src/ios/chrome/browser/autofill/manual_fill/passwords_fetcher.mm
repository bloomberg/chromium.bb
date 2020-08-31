// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/manual_fill/passwords_fetcher.h"

#include "base/stl_util.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/password_list_sorter.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/passwords/save_passwords_consumer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Protocol to observe changes on the Password Store.
@protocol PasswordStoreObserver<NSObject>

// The logins in the Password Store changed.
- (void)loginsDidChange;

@end

namespace {

// Objective-C bridge to observe changes in the Password Store.
class PasswordStoreObserverBridge
    : public password_manager::PasswordStore::Observer {
 public:
  explicit PasswordStoreObserverBridge(id<PasswordStoreObserver> observer)
      : observer_(observer) {}

  PasswordStoreObserverBridge() {}

 private:
  void OnLoginsChanged(
      const password_manager::PasswordStoreChangeList& changes) override {
    [observer_ loginsDidChange];
  }
  __weak id<PasswordStoreObserver> observer_ = nil;
};

}  // namespace

@interface PasswordFetcher ()<SavePasswordsConsumerDelegate,
                              PasswordStoreObserver> {
  // The interface for getting and manipulating a user's saved passwords.
  scoped_refptr<password_manager::PasswordStore> _passwordStore;
  // A helper object for passing data about saved passwords from a finished
  // password store request to the SavePasswordsCollectionViewController.
  std::unique_ptr<ios::SavePasswordsConsumer> _savedPasswordsConsumer;
  // The object to observe changes in the Password Store.
  std::unique_ptr<PasswordStoreObserverBridge> _passwordStoreObserver;
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
                             delegate:(id<PasswordFetcherDelegate>)delegate
                                  URL:(const GURL&)URL {
  DCHECK(passwordStore);
  DCHECK(delegate);
  self = [super init];
  if (self) {
    _delegate = delegate;
    _passwordStore = passwordStore;
    _savedPasswordsConsumer.reset(new ios::SavePasswordsConsumer(self));
    _passwordStoreObserver.reset(new PasswordStoreObserverBridge(self));
    _passwordStore->AddObserver(_passwordStoreObserver.get());

    if (URL.is_empty()) {
      _passwordStore->GetAutofillableLogins(_savedPasswordsConsumer.get());
    } else {
      password_manager::PasswordStore::FormDigest digest = {
          autofill::PasswordForm::Scheme::kHtml, std::string(), URL};
      digest.signon_realm = URL.spec();
      _passwordStore->GetLogins(digest, _savedPasswordsConsumer.get());
    }
  }
  return self;
}

- (void)dealloc {
  _passwordStore->RemoveObserver(_passwordStoreObserver.get());
}

#pragma mark - SavePasswordsConsumerDelegate

- (void)onGetPasswordStoreResults:
    (std::vector<std::unique_ptr<autofill::PasswordForm>>)results {
  // Filter out Android facet IDs and any blocked passwords.
  base::EraseIf(results, [](const auto& form) {
    return form->blacklisted_by_user ||
           password_manager::IsValidAndroidFacetURI(form->signon_realm);
  });

  password_manager::DuplicatesMap savedPasswordDuplicates;
  password_manager::SortEntriesAndHideDuplicates(&results,
                                                 &savedPasswordDuplicates);
  [self.delegate passwordFetcher:self didFetchPasswords:std::move(results)];
}

#pragma mark - PasswordStoreObserver

- (void)loginsDidChange {
  _passwordStore->GetAutofillableLogins(_savedPasswordsConsumer.get());
}

@end
