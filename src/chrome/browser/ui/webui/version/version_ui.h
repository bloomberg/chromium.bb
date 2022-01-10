// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_VERSION_VERSION_UI_H_
#define CHROME_BROWSER_UI_WEBUI_VERSION_VERSION_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

// The WebUI handler for chrome://version.
class VersionUI : public content::WebUIController {
 public:
  explicit VersionUI(content::WebUI* web_ui);

  VersionUI(const VersionUI&) = delete;
  VersionUI& operator=(const VersionUI&) = delete;

  ~VersionUI() override;

  // Returns the IDS_* string id for the variation of the processor.
  static int VersionProcessorVariation();

  // Loads a data source with many named details comprising version info.
  // The keys are from version_ui_constants.
  static void AddVersionDetailStrings(content::WebUIDataSource* html_source);
};

#endif  // CHROME_BROWSER_UI_WEBUI_VERSION_VERSION_UI_H_
