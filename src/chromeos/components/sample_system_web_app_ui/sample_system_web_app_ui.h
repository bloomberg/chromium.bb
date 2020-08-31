// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/mojo_web_ui_controller.h"

#ifndef CHROMEOS_COMPONENTS_SAMPLE_SYSTEM_WEB_APP_UI_SAMPLE_SYSTEM_WEB_APP_UI_H_
#define CHROMEOS_COMPONENTS_SAMPLE_SYSTEM_WEB_APP_UI_SAMPLE_SYSTEM_WEB_APP_UI_H_

#if defined(OFFICIAL_BUILD)
#error Sample System Web App should only be included in unofficial builds.
#endif

namespace chromeos {

// The WebUI for chrome://sample-system-web-app/.
class SampleSystemWebAppUI : public ui::MojoWebUIController {
 public:
  explicit SampleSystemWebAppUI(content::WebUI* web_ui);
  SampleSystemWebAppUI(const SampleSystemWebAppUI&) = delete;
  SampleSystemWebAppUI& operator=(const SampleSystemWebAppUI&) = delete;
  ~SampleSystemWebAppUI() override;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SAMPLE_SYSTEM_WEB_APP_UI_SAMPLE_SYSTEM_WEB_APP_UI_H_
