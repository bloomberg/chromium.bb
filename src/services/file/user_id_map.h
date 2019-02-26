// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_FILE_USER_ID_MAP_H_
#define SERVICES_FILE_USER_ID_MAP_H_

#include "base/files/file_path.h"

namespace base {
class Token;
}

namespace file {

// These methods are called from BrowserContext::Initialize() to associate
// the BrowserContext's Service instance group with its user directory.
//
// TODO(https://crbug.com/895591): Rename this file.
void AssociateServiceInstanceGroupWithUserDir(const base::Token& instance_group,
                                              const base::FilePath& user_dir);
void ForgetServiceInstanceGroupUserDirAssociation(
    const base::Token& instance_group);

base::FilePath GetUserDirForInstanceGroup(const base::Token& instance_group);

}  // namespace file

#endif  // SERVICES_FILE_USER_ID_MAP_H_
