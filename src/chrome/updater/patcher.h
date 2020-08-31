// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_PATCHER_H_
#define CHROME_UPDATER_PATCHER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/update_client/patcher.h"

namespace updater {

class PatcherFactory : public update_client::PatcherFactory {
 public:
  PatcherFactory();
  PatcherFactory(const PatcherFactory&) = delete;
  PatcherFactory& operator=(const PatcherFactory&) = delete;

  scoped_refptr<update_client::Patcher> Create() const override;

 protected:
  ~PatcherFactory() override;
};

}  // namespace updater

#endif  // CHROME_UPDATER_PATCHER_H_
