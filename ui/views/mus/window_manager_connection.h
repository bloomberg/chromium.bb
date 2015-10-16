// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_WINDOW_MANAGER_CONNECTION_H_
#define UI_VIEWS_MUS_WINDOW_MANAGER_CONNECTION_H_

#include "base/memory/scoped_ptr.h"
#include "components/mus/public/cpp/window_tree_delegate.h"
#include "components/mus/public/interfaces/window_manager.mojom.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/views_delegate.h"

namespace mojo {
class ApplicationImpl;
}

namespace views {
class AuraInit;

// Establishes a connection to the window manager for use by views within an
// application, and performs Aura initialization.
class WindowManagerConnection : public ViewsDelegate,
                                public mus::WindowTreeDelegate {
 public:
  explicit WindowManagerConnection(const std::string& window_manager_url,
                                   mojo::ApplicationImpl* app);
  ~WindowManagerConnection() override;

 private:
  mus::Window* CreateWindow();

  // ViewsDelegate:
  NativeWidget* CreateNativeWidget(
      internal::NativeWidgetDelegate* delegate) override;
  void OnBeforeWidgetInit(
      views::Widget::InitParams* params,
      views::internal::NativeWidgetDelegate* delegate) override;

  // mus::WindowTreeDelegate:
  void OnEmbed(mus::Window* root) override;
  void OnConnectionLost(mus::WindowTreeConnection* connection) override;
#if defined(OS_WIN)
  HICON GetSmallWindowIcon() const override;
#endif

  mojo::ApplicationImpl* app_;
  scoped_ptr<AuraInit> aura_init_;
  mus::mojom::WindowManagerPtr window_manager_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerConnection);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_WINDOW_MANAGER_CONNECTION_H_
