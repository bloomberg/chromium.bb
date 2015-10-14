// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_AURA_INIT_H_
#define UI_VIEWS_MUS_AURA_INIT_H_

#include <string>

#include "skia/ext/refptr.h"
#include "ui/mojo/init/ui_init.h"

namespace font_service {
class FontLoader;
}

namespace mojo {
class Shell;
}

namespace mus {
class View;
}

namespace views {

// Sets up necessary state for aura when run with the viewmanager.
// |resource_file| is the path to the apk file containing the resources.
class AuraInit {
 public:
  AuraInit(mus::View* root,
           mojo::Shell* shell,
           const std::string& resource_file);
  ~AuraInit();

 private:
  void InitializeResources(mojo::Shell* shell);

  ui::mojo::UIInit ui_init_;

#if defined(OS_LINUX) && !defined(OS_ANDROID)
  skia::RefPtr<font_service::FontLoader> font_loader_;
#endif

  const std::string resource_file_;

  DISALLOW_COPY_AND_ASSIGN(AuraInit);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_AURA_INIT_H_
