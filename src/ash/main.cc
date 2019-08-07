// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_service.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"

// This path is only hit in testing, not production. Production launches ash by
// way of the utility process, which does not use this.
void ServiceMain(service_manager::mojom::ServiceRequest request) {
  logging::SetLogPrefix("ash");
  // Load ash resources and strings.
  // TODO: investigate nuking ash_service_resources and use the same resources
  // that are used when AshService is launched via the utility process.
  base::FilePath path;
  base::PathService::Get(base::DIR_MODULE, &path);
  base::FilePath ash_resources =
      path.Append(FILE_PATH_LITERAL("ash_service_resources.pak"));
  ui::ResourceBundle::InitSharedInstanceWithPakPath(ash_resources);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (ui::ResourceBundle::IsScaleFactorSupported(ui::SCALE_FACTOR_200P)) {
    base::FilePath ash_resources_200 =
        path.Append(FILE_PATH_LITERAL("ash_service_resources_200.pak"));
    rb.AddDataPackFromPath(ash_resources_200, ui::SCALE_FACTOR_200P);
  }

  // AshService has a distinct code path when running out of process. The out of
  // process code path is triggered by way of |kMash|. As this code is only run
  // when ash is out of process it has to force |kMash| to be enabled
  // (otherwise AshService thinks it's running in process, which does not make
  // sense, nor work).
  DCHECK(base::FeatureList::GetInstance());
  std::string enabled_features;
  std::string disabled_features;
  base::FeatureList::GetInstance()->GetCommandLineFeatureOverrides(
      &enabled_features, &disabled_features);
  if (!enabled_features.empty())
    enabled_features += ",";
  enabled_features += features::kMash.name;
  // Disable SingleProcessMash, even if it's on by default in the test suite.
  if (!disabled_features.empty())
    disabled_features += ",";
  disabled_features += features::kSingleProcessMash.name;
  // This code path is really only for testing (production code uses the utility
  // process to launch AshService), so it's ok to use a for-testing function.
  base::FeatureList::ClearInstanceForTesting();
  CHECK(base::FeatureList::InitializeInstance(enabled_features,
                                              disabled_features));
  CHECK(base::FeatureList::IsEnabled(features::kMash));
  CHECK(!base::FeatureList::IsEnabled(features::kSingleProcessMash));

  ui::MaterialDesignController::Initialize();

  base::MessageLoop message_loop(base::MessageLoop::TYPE_UI);
  ash::AshService(std::move(request)).RunUntilTermination();
}
