// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SYNC_FILE_SYSTEM_INTERNALS_SYNC_FILE_SYSTEM_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SYNC_FILE_SYSTEM_INTERNALS_SYNC_FILE_SYSTEM_INTERNALS_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

class SyncFileSystemInternalsUI : public content::WebUIController {
 public:
  explicit SyncFileSystemInternalsUI(content::WebUI* web_ui);
  ~SyncFileSystemInternalsUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncFileSystemInternalsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SYNC_FILE_SYSTEM_INTERNALS_SYNC_FILE_SYSTEM_INTERNALS_UI_H_
