// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_PATCH_PATCH_IMPL_H_
#define COMPONENTS_UPDATE_CLIENT_PATCH_PATCH_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/update_client/patcher.h"

namespace service_manager {
class Connector;
}

namespace update_client {

class PatchChromiumFactory : public PatcherFactory {
 public:
  explicit PatchChromiumFactory(
      std::unique_ptr<service_manager::Connector> connector);

  scoped_refptr<Patcher> Create() const override;

 protected:
  ~PatchChromiumFactory() override;

 private:
  std::unique_ptr<service_manager::Connector> connector_;

  DISALLOW_COPY_AND_ASSIGN(PatchChromiumFactory);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_PATCH_PATCH_IMPL_H_
