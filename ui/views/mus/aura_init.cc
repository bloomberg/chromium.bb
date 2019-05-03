// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/aura_init.h"

#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/aura/env.h"
#include "ui/base/ime/init/input_method_initializer.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/mus/mus_views_delegate.h"

namespace views {

AuraInit::InitParams::InitParams() : resource_file("views_mus_resources.pak") {}

AuraInit::InitParams::~InitParams() = default;

AuraInit::AuraInit() {
  if (!ViewsDelegate::GetInstance())
    views_delegate_ = std::make_unique<MusViewsDelegate>();
}

AuraInit::~AuraInit() = default;

// static
std::unique_ptr<AuraInit> AuraInit::Create(const InitParams& params) {
  // Using 'new' to access a non-public constructor. go/totw/134
  std::unique_ptr<AuraInit> aura_init = base::WrapUnique(new AuraInit());
  if (!aura_init->Init(params))
    aura_init.reset();
  return aura_init;
}

bool AuraInit::Init(const InitParams& params) {
  env_ = aura::Env::CreateInstance(aura::Env::Mode::MUS);

  MusClient::InitParams mus_params;
  mus_params.connector = params.connector;
  mus_params.identity = params.identity;
  mus_params.io_task_runner = params.io_task_runner;
  mus_params.create_wm_state = true;
  mus_client_ = std::make_unique<MusClient>(mus_params);
  ui::MaterialDesignController::Initialize();

  DCHECK(ui::ResourceBundle::HasSharedInstance());

  ui::InitializeInputMethodForTesting();
  return true;
}

}  // namespace views
