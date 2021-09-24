// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_EXTERNAL_CONSTANTS_H_
#define CHROME_UPDATER_EXTERNAL_CONSTANTS_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"

class GURL;

namespace updater {

// Several constants controlling the program's behavior can come from stateful
// external providers, such as dev-mode overrides or enterprise policies.
class ExternalConstants : public base::RefCountedThreadSafe<ExternalConstants> {
 public:
  explicit ExternalConstants(scoped_refptr<ExternalConstants> next_provider);
  ExternalConstants(const ExternalConstants&) = delete;
  ExternalConstants& operator=(const ExternalConstants&) = delete;

  // The URL to send update checks to.
  virtual std::vector<GURL> UpdateURL() const = 0;

  // True if client update protocol signing of update checks is enabled.
  virtual bool UseCUP() const = 0;

  // Number of seconds to delay the start of the automated background tasks
  // such as update checks.
  virtual double InitialDelay() const = 0;

  // Minimum number of of seconds the server needs to stay alive.
  virtual int ServerKeepAliveSeconds() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<ExternalConstants>;
  scoped_refptr<ExternalConstants> next_provider_;
  virtual ~ExternalConstants();
};

// Sets up an external constants chain of responsibility. May block.
scoped_refptr<ExternalConstants> CreateExternalConstants();

// Sets up an external constants provider yielding only default values.
// Intended only for testing of other constants providers.
scoped_refptr<ExternalConstants> CreateDefaultExternalConstantsForTesting();

}  // namespace updater

#endif  // CHROME_UPDATER_EXTERNAL_CONSTANTS_H_
