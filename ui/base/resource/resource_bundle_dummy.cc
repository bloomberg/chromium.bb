// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle.h"

#include <windows.h>

#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "gfx/font.h"
#include "gfx/platform_font_win.h"

// NOTE(gregoryd): This is a hack to avoid creating more nacl_win64-specific
// files. The font members of ResourceBundle are never initialized in our code
// so this destructor is never called.
namespace gfx {
Font::~Font() {
  NOTREACHED();
}
PlatformFontWin::HFontRef::~HFontRef() {
  NOTREACHED();
}
}

namespace ui {

ResourceBundle* ResourceBundle::g_shared_instance_ = NULL;

/* static */
std::string ResourceBundle::InitSharedInstance(
    const std::string& pref_locale) {
  DCHECK(g_shared_instance_ == NULL) << "ResourceBundle initialized twice";
  g_shared_instance_ = new ResourceBundle();
  return std::string();
}

/* static */
void ResourceBundle::CleanupSharedInstance() {
  if (g_shared_instance_) {
    delete g_shared_instance_;
    g_shared_instance_ = NULL;
  }
}

/* static */
ResourceBundle& ResourceBundle::GetSharedInstance() {
  // Must call InitSharedInstance before this function.
  CHECK(g_shared_instance_ != NULL);
  return *g_shared_instance_;
}

ResourceBundle::ResourceBundle()
    : lock_(new base::Lock),
      resources_data_(NULL),
      locale_resources_data_(NULL) {
}

ResourceBundle::~ResourceBundle() {
}


string16 ResourceBundle::GetLocalizedString(int message_id) {
  return string16();
}

}  // namespace ui
