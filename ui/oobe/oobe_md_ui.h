// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OOBE_OOBE_MD_UI_H_
#define UI_OOBE_OOBE_MD_UI_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/oobe/oobe_export.h"

namespace content {
class WebUI;
}

namespace gen {
class OobeWebUIView;
}

class OOBE_EXPORT OobeMdUI : public content::WebUIController {
 public:
  OobeMdUI(content::WebUI* web_ui, const std::string& host);
  ~OobeMdUI() override;

 private:
  scoped_ptr<gen::OobeWebUIView> view_;

  DISALLOW_COPY_AND_ASSIGN(OobeMdUI);
};

#endif  // UI_OOBE_OOBE_MD_UI_H_
