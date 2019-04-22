// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/aura_test_suite_setup.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "ui/aura/env.h"
#include "ui/aura/test/aura_test_context_factory.h"
#include "ui/base/ui_base_features.h"

#if BUILDFLAG(ENABLE_MUS)
#include "ui/aura/test/mus/test_window_tree_client_delegate.h"
#include "ui/aura/test/mus/test_window_tree_client_setup.h"
#endif

namespace aura {
namespace {

#if BUILDFLAG(ENABLE_MUS)
bool g_disable_mus_features = false;
#endif

}  // namespace

AuraTestSuiteSetup::AuraTestSuiteSetup() {
  DCHECK(!Env::HasInstance());
#if BUILDFLAG(ENABLE_MUS)
  if (g_disable_mus_features) {
    scoped_feature_list_.InitWithFeatures(
        {} /* enabled */,
        {features::kMash, features::kSingleProcessMash} /* disabled */);
  }
  const Env::Mode env_mode =
      features::IsUsingWindowService() ? Env::Mode::MUS : Env::Mode::LOCAL;
  env_ = Env::CreateInstance(env_mode);
  if (env_mode == Env::Mode::MUS)
    ConfigureMus();
#else
  env_ = Env::CreateInstance(Env::Mode::LOCAL);
#endif
}

AuraTestSuiteSetup::~AuraTestSuiteSetup() = default;

#if BUILDFLAG(ENABLE_MUS)
// static
void AuraTestSuiteSetup::DisableMusFeatures() {
  g_disable_mus_features = true;
}
#endif

#if BUILDFLAG(ENABLE_MUS)
void AuraTestSuiteSetup::ConfigureMus() {
  // Configure the WindowTreeClient in a mode similar to that of connecting via
  // a WindowTreeFactory. This gives WindowTreeClient a mock WindowTree.
  test_window_tree_client_delegate_ =
      std::make_unique<TestWindowTreeClientDelegate>();
  window_tree_client_setup_ = std::make_unique<TestWindowTreeClientSetup>();
  window_tree_client_setup_->InitWithoutEmbed(
      test_window_tree_client_delegate_.get());
  env_->SetWindowTreeClient(window_tree_client_setup_->window_tree_client());

#if !defined(USE_OZONE)
  context_factory_ = std::make_unique<test::AuraTestContextFactory>();
  env_->set_context_factory(context_factory_.get());
  env_->set_context_factory_private(nullptr);
#endif
}
#endif

}  // namespace aura
