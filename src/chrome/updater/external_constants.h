// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_EXTERNAL_CONSTANTS_H_
#define CHROME_UPDATER_EXTERNAL_CONSTANTS_H_

#include <memory>
#include <vector>

class GURL;

namespace updater {

// Several constants controlling the program's behavior can come from stateful
// external providers, such as dev-mode overrides or enterprise policies.
class ExternalConstants {
 public:
  explicit ExternalConstants(std::unique_ptr<ExternalConstants> next_provider);
  ExternalConstants(const ExternalConstants&) = delete;
  ExternalConstants& operator=(const ExternalConstants&) = delete;
  virtual ~ExternalConstants();

  // The URL to send update checks to.
  virtual std::vector<GURL> UpdateURL() const = 0;

  // True if client update protocol signing of update checks is enabled.
  virtual bool UseCUP() const = 0;

 protected:
  std::unique_ptr<ExternalConstants> next_provider_;
};

// Sets up an external constants chain of responsibility. May block.
std::unique_ptr<ExternalConstants> CreateExternalConstants();

}  // namespace updater

#endif  // CHROME_UPDATER_EXTERNAL_CONSTANTS_H_
