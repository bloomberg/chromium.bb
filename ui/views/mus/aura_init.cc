// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/aura_init.h"

#include <utility>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "components/resource_provider/public/cpp/resource_loader.h"
#include "mojo/shell/public/cpp/shell.h"
#include "ui/aura/env.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/views/views_delegate.h"

#if defined(OS_LINUX) && !defined(OS_ANDROID)
#include "components/font_service/public/cpp/font_loader.h"
#endif

namespace views {

namespace {

std::set<std::string> GetResourcePaths(const std::string& resource_file) {
  std::set<std::string> paths;
  paths.insert(resource_file);
  return paths;
}

class MusViewsDelegate : public ViewsDelegate {
 public:
  MusViewsDelegate() {}
  ~MusViewsDelegate() override {}

 private:
#if defined(OS_WIN)
  HICON GetSmallWindowIcon() const override { return nullptr; }
#endif
  void OnBeforeWidgetInit(
      Widget::InitParams* params,
      internal::NativeWidgetDelegate* delegate) override {}

  DISALLOW_COPY_AND_ASSIGN(MusViewsDelegate);
};

}  // namespace

AuraInit::AuraInit(mojo::Shell* shell, const std::string& resource_file)
    : resource_file_(resource_file),
      views_delegate_(new MusViewsDelegate) {
  aura::Env::CreateInstance(false);

  InitializeResources(shell);

  ui::InitializeInputMethodForTesting();
}

AuraInit::~AuraInit() {
#if defined(OS_LINUX) && !defined(OS_ANDROID)
  if (font_loader_.get()) {
    SkFontConfigInterface::SetGlobal(nullptr);
    // FontLoader is ref counted. We need to explicitly shutdown the background
    // thread, otherwise the background thread may be shutdown after the app is
    // torn down, when we're in a bad state.
    font_loader_->Shutdown();
  }
#endif
}

void AuraInit::InitializeResources(mojo::Shell* shell) {
  if (ui::ResourceBundle::HasSharedInstance())
    return;
  resource_provider::ResourceLoader resource_loader(
      shell, GetResourcePaths(resource_file_));
  if (!resource_loader.BlockUntilLoaded())
    return;
  CHECK(resource_loader.loaded());
  ui::RegisterPathProvider();
  base::File pak_file = resource_loader.ReleaseFile(resource_file_);
  base::File pak_file_2 = pak_file.Duplicate();
  ui::ResourceBundle::InitSharedInstanceWithPakFileRegion(
      std::move(pak_file), base::MemoryMappedFile::Region::kWholeFile);
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromFile(
      std::move(pak_file_2), ui::SCALE_FACTOR_100P);

// Initialize the skia font code to go ask fontconfig underneath.
#if defined(OS_LINUX) && !defined(OS_ANDROID)
  font_loader_ = skia::AdoptRef(new font_service::FontLoader(shell));
  SkFontConfigInterface::SetGlobal(font_loader_.get());
#endif

  // There is a bunch of static state in gfx::Font, by running this now,
  // before any other apps load, we ensure all the state is set up.
  gfx::Font();
}

}  // namespace views
