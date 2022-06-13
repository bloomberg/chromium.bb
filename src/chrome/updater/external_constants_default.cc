// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/external_constants_default.h"

#include "base/memory/scoped_refptr.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/updater_branding.h"
#include "url/gurl.h"

namespace updater {
namespace {

class DefaultExternalConstants : public ExternalConstants {
 public:
  DefaultExternalConstants() : ExternalConstants(nullptr) {}

  // Overrides of ExternalConstants:
  std::vector<GURL> UpdateURL() const override {
    return std::vector<GURL>{GURL(UPDATE_CHECK_URL)};
  }

  bool UseCUP() const override { return true; }

  double InitialDelay() const override { return kInitialDelay; }

  int ServerKeepAliveSeconds() const override {
    return kServerKeepAliveSeconds;
  }

 private:
  ~DefaultExternalConstants() override = default;
};

}  // namespace

scoped_refptr<ExternalConstants> CreateDefaultExternalConstants() {
  return base::MakeRefCounted<DefaultExternalConstants>();
}

}  // namespace updater
