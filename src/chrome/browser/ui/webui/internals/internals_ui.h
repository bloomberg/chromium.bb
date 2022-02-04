// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INTERNALS_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_INTERNALS_INTERNALS_UI_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/mojo_web_ui_controller.h"

#if !BUILDFLAG(IS_ANDROID)
// gn check doesn't understand "#if !BUILDFLAG(IS_ANDROID)" and fails this
// non-Android include on Android.
#include "chrome/browser/ui/webui/internals/user_education/user_education_internals.mojom.h"  // nogncheck
#endif

namespace content {
class WebUI;
}  // namespace content

// Client could put debug WebUI as sub-URL under chrome://internals/.
// e.g. chrome://internals/your-feature.
class InternalsUI : public ui::MojoWebUIController {
 public:
  explicit InternalsUI(content::WebUI* web_ui);
  ~InternalsUI() override;

#if !BUILDFLAG(IS_ANDROID)
  void BindInterface(
      mojo::PendingReceiver<
          mojom::user_education_internals::UserEducationInternalsPageHandler>
          receiver);
#endif  // !BUILDFLAG(IS_ANDROID)

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();

#if BUILDFLAG(IS_ANDROID)
  // Add resources and message handler for chrome://internals/query-tiles.
  void AddQueryTilesInternals(content::WebUI* web_ui);

  // Add resources and message handler for chrome://internals/lens.
  void AddLensInternals(content::WebUI* web_ui);
#endif  // BUILDFLAG(IS_ANDROID)

  raw_ptr<Profile> profile_;
  raw_ptr<content::WebUIDataSource> source_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_INTERNALS_INTERNALS_UI_H_
