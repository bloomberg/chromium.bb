// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/shell/app/shell_main_params.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "net/base/filename_util.h"
#include "url/gurl.h"
#include "weblayer/public/main.h"
#include "weblayer/public/profile.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/shell/common/shell_switches.h"

namespace weblayer {

namespace {

GURL GetStartupURL() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kNoInitialNavigation))
    return GURL();

#if defined(OS_ANDROID)
  // Delay renderer creation on Android until surface is ready.
  return GURL();
#else
  const base::CommandLine::StringVector& args = command_line->GetArgs();

  if (args.empty())
    return GURL("https://www.google.com/");

  GURL url(args[0]);
  if (url.is_valid() && url.has_scheme())
    return url;

  return net::FilePathToFileURL(
      base::MakeAbsoluteFilePath(base::FilePath(args[0])));
#endif
}

class MainDelegateImpl : public MainDelegate {
 public:
  void PreMainMessageLoopRun() override {
    // On Android the Profile is created and owned in Java via an
    // embedder-specific call to WebLayer.createBrowserFragment().
#if !defined(OS_ANDROID)
    InitializeProfile();
#endif

    Shell::Initialize();

#if defined(OS_ANDROID)
    Shell::CreateNewWindow(GetStartupURL(), gfx::Size());
#else
    Shell::CreateNewWindow(profile_.get(), GetStartupURL(), gfx::Size());
#endif
  }

  void PostMainMessageLoopRun() override {
#if !defined(OS_ANDROID)
    DestroyProfile();
#endif
  }

  void SetMainMessageLoopQuitClosure(base::OnceClosure quit_closure) override {
    Shell::SetMainMessageLoopQuitClosure(std::move(quit_closure));
  }

 private:
#if !defined(OS_ANDROID)
  void InitializeProfile() {
    auto* command_line = base::CommandLine::ForCurrentProcess();
    const bool is_incognito =
        command_line->HasSwitch(switches::kStartInIncognito);
    std::string profile_name = is_incognito ? "" : "web_shell";

    profile_ = Profile::Create(profile_name, is_incognito);
  }

  void DestroyProfile() { profile_.reset(); }

  std::unique_ptr<Profile> profile_;
#endif
};

}  // namespace

MainParams CreateMainParams() {
  static const base::NoDestructor<MainDelegateImpl> weblayer_delegate;
  MainParams params;
  params.delegate = const_cast<MainDelegateImpl*>(&(*weblayer_delegate));

  base::PathService::Get(base::DIR_EXE, &params.log_filename);
  params.log_filename = params.log_filename.AppendASCII("weblayer_shell.log");

  params.pak_name = "weblayer.pak";

  return params;
}

}  //  namespace weblayer
