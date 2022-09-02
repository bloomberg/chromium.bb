// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATER_BROWSER_UPDATER_HELPER_CLIENT_MAC_H_
#define CHROME_BROWSER_UPDATER_BROWSER_UPDATER_HELPER_CLIENT_MAC_H_

#import <Foundation/Foundation.h>

#include "base/callback_forward.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/updater/mac/privileged_helper/service_protocol.h"

// Client that will create a connection between the browser and the privileged
// helper for the Chromium updater. Helps with setting up the system-level
// updater during promotion.
class BrowserUpdaterHelperClientMac
    : public base::RefCountedThreadSafe<BrowserUpdaterHelperClientMac> {
 public:
  BrowserUpdaterHelperClientMac();

  // Calls on the privileged helper to set up the system-level updater. Upon
  // setup completion, an integer return code will be sent back in a callback.
  void SetupSystemUpdater(base::OnceCallback<void(int)> result);

 protected:
  friend class base::RefCountedThreadSafe<BrowserUpdaterHelperClientMac>;
  virtual ~BrowserUpdaterHelperClientMac();

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  base::scoped_nsobject<NSXPCConnection> xpc_connection_;
};

#endif  // CHROME_BROWSER_UPDATER_BROWSER_UPDATER_HELPER_CLIENT_MAC_H_
