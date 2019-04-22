// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/test/mojo_test_with_file_service.h"

#include "services/file/public/mojom/constants.mojom.h"
#include "services/file/user_id_map.h"

namespace content {
namespace test {

MojoTestWithFileService::MojoTestWithFileService()
    : file_service_(
          test_connector_factory_.RegisterInstance(file::mojom::kServiceName)) {
  CHECK(temp_path_.CreateUniqueTempDir());
  file::AssociateServiceInstanceGroupWithUserDir(
      test_connector_factory_.test_instance_group(), temp_path_.GetPath());
}

MojoTestWithFileService::~MojoTestWithFileService() = default;

}  // namespace test
}  // namespace content
