// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/avatar/mock_user_image_manager.h"

namespace chromeos {

MockUserImageManager::MockUserImageManager(const std::string& user_id)
    : UserImageManager(user_id) {}

MockUserImageManager::~MockUserImageManager() {}

}  // namespace chromeos
