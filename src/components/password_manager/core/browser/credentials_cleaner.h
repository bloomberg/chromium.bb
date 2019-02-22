// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIALS_CLEANER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIALS_CLEANER_H_

namespace password_manager {

// Interface that allows CredentialsCleanerRunner class to easily manipulate
// credential clean-ups that request credentials from PasswordStore.
// Every clean-up starts when StartCleaning is called and must call
// CleaningCompleted on its observer once done.
class CredentialsCleaner {
 public:
  // Interface to be implemented by callers of
  // CredentialsCleaner::StartCleaning. It allows the callers to get notified
  // about the completion of the clean-up.
  class Observer {
   public:
    virtual ~Observer() = default;

    // Notifies the observer that the clean-up is completed.
    virtual void CleaningCompleted() = 0;
  };

  CredentialsCleaner() = default;

  virtual ~CredentialsCleaner() = default;

  // Calling this initiates the clean-up. The function should only be called
  // once in the lifetime of this class. The clean-up may consist of
  // asynchronous tasks, so exiting from StartCleaning does not mean the
  // clean-up is complete. The caller needs to provide the |observer| to be
  // notified about the completion of the clean-up, so the |observer| should not
  // be null.
  virtual void StartCleaning(Observer* observer) = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIALS_CLEANER_H_