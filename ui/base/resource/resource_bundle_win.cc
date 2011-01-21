// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle.h"

#include <atlbase.h>

#include "app/l10n_util.h"
#include "base/debug/stack_trace.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/resource_util.h"
#include "base/stl_util-inl.h"
#include "base/string_piece.h"
#include "base/synchronization/lock.h"
#include "base/win/windows_version.h"
#include "gfx/font.h"
#include "ui/base/ui_base_paths.h"
#include "ui/base/resource/data_pack.h"

namespace ui {

namespace {

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
  resources_data_ = _AtlBaseModule.GetResourceInstance();
}

std::string ResourceBundle::LoadLocaleResources(
    const std::string& pref_locale) {
  DCHECK(NULL == locale_resources_data_) << "locale dll already loaded";
  const std::string app_locale = l10n_util::GetApplicationLocale(pref_locale);
  const FilePath& locale_path = GetLocaleFilePath(app_locale);
  if (locale_path.value().empty()) {
    // It's possible that there are no locale dlls found, in which case we just
    // return.
    NOTREACHED();
    return std::string();
  }

  // The dll should only have resources, not executable code.
  locale_resources_data_ = LoadLibraryEx(locale_path.value().c_str(), NULL,
                                         GetDataDllLoadFlags());
  DCHECK(locale_resources_data_ != NULL) <<
      "unable to load generated resources";
  return app_locale;
}

void ResourceBundle::UnloadLocaleResources() {
  if (locale_resources_data_) {
    BOOL rv = FreeLibrary(locale_resources_data_);
    DCHECK(rv);
    locale_resources_data_ = NULL;
  }
}

// static
FilePath ResourceBundle::GetLocaleFilePath(const std::string& app_locale) {
  FilePath locale_path;
  PathService::Get(ui::DIR_LOCALES, &locale_path);

  if (app_locale.empty())
    return FilePath();

  return locale_path.AppendASCII(app_locale + ".dll");
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

HICON ResourceBundle::LoadThemeIcon(int icon_id) {
  return ::LoadIcon(resources_data_, MAKEINTRESOURCE(icon_id));
}

base::StringPiece ResourceBundle::GetRawDataResource(int resource_id) const {
  void* data_ptr;
  size_t data_size;
  if (base::GetDataResourceFromModule(_AtlBaseModule.GetModuleInstance(),
                                      resource_id,
                                      &data_ptr,
                                      &data_size)) {
    return base::StringPiece(static_cast<const char*>(data_ptr), data_size);
  } else if (locale_resources_data_ &&
             base::GetDataResourceFromModule(locale_resources_data_,
                                             resource_id,
                                             &data_ptr,
                                             &data_size)) {
    return base::StringPiece(static_cast<const char*>(data_ptr), data_size);
  }

  base::StringPiece data;
  for (size_t i = 0; i < data_packs_.size(); ++i) {
    if (data_packs_[i]->GetStringPiece(resource_id, &data))
      return data;
  }

  return base::StringPiece();
}

// Loads and returns a cursor from the current module.
HCURSOR ResourceBundle::LoadCursor(int cursor_id) {
  return ::LoadCursor(_AtlBaseModule.GetModuleInstance(),
                      MAKEINTRESOURCE(cursor_id));
}

string16 ResourceBundle::GetLocalizedString(int message_id) {
  // If for some reason we were unable to load a resource dll, return an empty
  // string (better than crashing).
  if (!locale_resources_data_) {
    base::debug::StackTrace().PrintBacktrace();  // See http://crbug.com/21925.
    LOG(WARNING) << "locale resources are not loaded";
    return string16();
  }

  DCHECK(IS_INTRESOURCE(message_id));

  // Get a reference directly to the string resource.
  HINSTANCE hinstance = locale_resources_data_;
  const ATLSTRINGRESOURCEIMAGE* image = AtlGetStringResourceImage(hinstance,
                                                                  message_id);
  if (!image) {
    // Fall back on the current module (shouldn't be any strings here except
    // in unittests).
    image = AtlGetStringResourceImage(_AtlBaseModule.GetModuleInstance(),
                                      message_id);
    if (!image) {
      // See http://crbug.com/21925.
      base::debug::StackTrace().PrintBacktrace();
      NOTREACHED() << "unable to find resource: " << message_id;
      return string16();
    }
  }
  // Copy into a string16 and return.
  return string16(image->achString, image->nLength);
}

}  // namespace ui;
