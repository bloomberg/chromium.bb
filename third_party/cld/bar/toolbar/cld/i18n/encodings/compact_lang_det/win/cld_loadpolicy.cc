// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_loadpolicy.h"

#include <atlstr.h>
using ATL::CString;
#include <tchar.h>
#include <windows.h>

#include <vector>  // to compile bar/common/component.h

#include "bar/common/component.h"
#include "bar/common/execute/execute_utils.h"
#include "bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_serviceinterface.h"

// TODO(jcampan): implement when needed.
static bool UseComponent(const TCHAR* component_name, CString* component_path) {
  return false;
}

CldLoadPolicy::CldLoadPolicy() {
}

CldLoadPolicy::~CldLoadPolicy() {
}


DWORD CldLoadPolicy::Ready(CldServiceInterface* cld_service) {
  _ASSERT(NULL != cld_service);
  if (NULL == cld_service)
    return ERROR_INVALID_PARAMETER;

  if (cld_service->IsLoaded())
    return ERROR_SUCCESS;

  // Retrieve fully specified file name for the resource DLL and verifies that
  // this DLL is properly signed by Google.
  CString component_file_name;
  if (!UseComponent(kCLDTablesDllName, &component_file_name))
    return ERROR_FILE_NOT_FOUND;

  return cld_service->Load(component_file_name);
}
