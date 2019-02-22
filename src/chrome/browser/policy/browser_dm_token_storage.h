// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_BROWSER_DM_TOKEN_STORAGE_H_
#define CHROME_BROWSER_POLICY_BROWSER_DM_TOKEN_STORAGE_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"

namespace policy {

// Manages storing and retrieving tokens and client ID used to enroll browser
// instances for enterprise management. The tokens are read from disk or
// registry once and cached values are returned in subsequent calls.
//
// All calls to member functions must be sequenced. It is an error to attempt
// concurrent store operations. RetrieveClientId must be the first method
// called.
class BrowserDMTokenStorage {
 public:
  using StoreCallback = base::OnceCallback<void(bool success)>;

  // Returns the global singleton object. Must be called from the UI thread.
  // This implementation is platform dependant.
  static BrowserDMTokenStorage* Get();
  // Returns a client ID unique to the machine. Virtual for tests.
  virtual std::string RetrieveClientId();
  // Returns the enrollment token, or an empty string if there is none. Virtual
  // for tests.
  virtual std::string RetrieveEnrollmentToken();
  // Asynchronously stores |dm_token| and calls |callback| with a boolean to
  // indicate success or failure. It is an error to attempt concurrent store
  // operations. Virtual for tests.
  virtual void StoreDMToken(const std::string& dm_token,
                            StoreCallback callback);
  // Returns an already stored DM token. An empty token is returned if no DM
  // token exists on the system or an error is encountered. Virtual for tests.
  virtual std::string RetrieveDMToken();
  // Must be called after the DM token is saved, to ensure that the callback is
  // invoked.
  void OnDMTokenStored(bool success);

  // Set the mock BrowserDMTokenStorage for testing. The caller owns the
  // instance of the storage.
  static void SetForTesting(BrowserDMTokenStorage* storage) {
    storage_for_testing_ = storage;
  }

  // Schedules a task to delete the empty policy directory that contains DM
  // token.
  void ScheduleUnusedPolicyDirectoryDeletion();

 protected:
  friend class base::NoDestructor<BrowserDMTokenStorage>;

  // Get the global singleton instance by calling BrowserDMTokenStorage::Get().
  BrowserDMTokenStorage();
  virtual ~BrowserDMTokenStorage();

 private:
  static BrowserDMTokenStorage* storage_for_testing_;

  // Initializes the DMTokenStorage object and caches the ids and tokens. This
  // is called the first time the BrowserDMTokenStorage is interacted with.
  void InitIfNeeded();

  // Gets the client ID and stores it in |client_id_|. This implementation is
  // platform dependant.
  virtual std::string InitClientId() = 0;
  // Gets the enrollment token and stores it in |enrollment_token_|. This
  // implementation is platform dependant.
  virtual std::string InitEnrollmentToken() = 0;
  // Gets the DM token and stores it in |dm_token_|. This implementation is
  // platform dependant.
  virtual std::string InitDMToken() = 0;
  // Saves the DM token. This implementation is platform dependant.
  virtual void SaveDMToken(const std::string& token) = 0;

  // Deletes the policy directory if it's empty.
  virtual void DeletePolicyDirectory();

  // Will be called after the DM token is stored.
  StoreCallback store_callback_;

  bool is_initialized_;

  std::string client_id_;
  std::string enrollment_token_;
  std::string dm_token_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(BrowserDMTokenStorage);
};

}  // namespace policy
#endif  // CHROME_BROWSER_POLICY_BROWSER_DM_TOKEN_STORAGE_H_
