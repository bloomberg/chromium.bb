// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_INFO_PAGE_INFO_UI_DELEGATE_H_
#define COMPONENTS_PAGE_INFO_PAGE_INFO_UI_DELEGATE_H_

#include "build/build_config.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_result.h"
#include "url/gurl.h"

class PageInfoUiDelegate {
 public:
  virtual ~PageInfoUiDelegate() = default;
#if !defined(OS_ANDROID)
  virtual bool IsBlockAutoPlayEnabled() = 0;
#endif
  virtual permissions::PermissionResult GetPermissionStatus(
      ContentSettingsType type,
      const GURL& url) = 0;
};

#endif  // COMPONENTS_PAGE_INFO_PAGE_INFO_UI_DELEGATE_H_
