// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle.h"

#include <atlbase.h>

#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/resource_util.h"
#include "base/stl_util.h"
#include "base/string_piece.h"
#include "base/win/windows_version.h"
#include "ui/base/resource/data_pack.h"
#include "ui/gfx/font.h"

namespace ui {

namespace {

HINSTANCE resources_data_dll;

// Returns the flags that should be passed to LoadLibraryEx.
DWORD GetDataDllLoadFlags() {
  if (base::win::GetVersion() >= base::win::VERSION_VISTA)
    return LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE | LOAD_LIBRARY_AS_IMAGE_RESOURCE;

  return DONT_RESOLVE_DLL_REFERENCES;
}

}  // end anonymous namespace

ResourceBundle::~ResourceBundle() {
  FreeImages();
  UnloadLocaleResources();
  STLDeleteContainerPointers(data_packs_.begin(),
                             data_packs_.end());
  resources_data_ = NULL;
}

void ResourceBundle::LoadCommonResources() {
  // As a convenience, set resources_data_ to the current resource module.
  DCHECK(NULL == resources_data_) << "common resources already loaded";

  if (resources_data_dll) {
    resources_data_ = resources_data_dll;
  } else {
    resources_data_ = GetModuleHandle(NULL);
  }
}

void ResourceBundle::LoadTestResources(const FilePath& path) {
  // On Windows, the test resources are normally compiled into the binary
  // itself.
}

// static
RefCountedStaticMemory* ResourceBundle::LoadResourceBytes(
    DataHandle module, int resource_id) {
  void* data_ptr;
  size_t data_size;
  if (base::GetDataResourceFromModule(module, resource_id, &data_ptr,
                                      &data_size)) {
    return new RefCountedStaticMemory(
        reinterpret_cast<const unsigned char*>(data_ptr), data_size);
  } else {
    return NULL;
  }
}

// static
void ResourceBundle::SetResourcesDataDLL(HINSTANCE handle) {
  resources_data_dll = handle;
}

HICON ResourceBundle::LoadThemeIcon(int icon_id) {
  return ::LoadIcon(resources_data_, MAKEINTRESOURCE(icon_id));
}

base::StringPiece ResourceBundle::GetRawDataResource(int resource_id) const {
  void* data_ptr;
  size_t data_size;
  base::StringPiece data;
  if (base::GetDataResourceFromModule(resources_data_,
                                      resource_id,
                                      &data_ptr,
                                      &data_size)) {
    return base::StringPiece(static_cast<const char*>(data_ptr), data_size);
  } else if (locale_resources_data_.get() &&
             locale_resources_data_->GetStringPiece(resource_id, &data)) {
    return data;
  }

  // TODO(tony): Remove this ATL code once we remove the strings in
  // chrome.dll.
  const ATLSTRINGRESOURCEIMAGE* image = AtlGetStringResourceImage(
    resources_data_, resource_id);
  if (image) {
    return base::StringPiece(reinterpret_cast<const char*>(image->achString),
                             image->nLength * 2);
  }

  for (size_t i = 0; i < data_packs_.size(); ++i) {
    if (data_packs_[i]->GetStringPiece(resource_id, &data))
      return data;
  }

  return base::StringPiece();
}

// Loads and returns a cursor from the current module.
HCURSOR ResourceBundle::LoadCursor(int cursor_id) {
  return ::LoadCursor(resources_data_, MAKEINTRESOURCE(cursor_id));
}

// Windows only uses SkBitmap for gfx::Image, so this is the same as
// GetImageNamed.
gfx::Image& ResourceBundle::GetNativeImageNamed(int resource_id) {
  return GetImageNamed(resource_id);
}

}  // namespace ui;
