// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_WINDOW_MANAGER_CONNECTION_H_
#define UI_VIEWS_MUS_WINDOW_MANAGER_CONNECTION_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/mus/public/cpp/window_tree_delegate.h"
#include "ui/views/mus/mus_export.h"
#include "ui/views/mus/screen_mus_delegate.h"
#include "ui/views/widget/widget.h"

namespace mojo {
class ApplicationImpl;
}

namespace views {
class NativeWidget;
class ScreenMus;
namespace internal {
class NativeWidgetDelegate;
}

// Provides configuration to mus in views. This consists of the following:
// . Provides a Screen implementation backed by mus.
// . Creates and owns a WindowTreeConnection.
// . Registers itself as the factory for creating NativeWidgets so that a
//   NativeWidgetMus is created.
// WindowManagerConnection is a singleton and should be created early on.
//
// TODO(sky): this name is now totally confusing. Come up with a better one.
class VIEWS_MUS_EXPORT WindowManagerConnection
    : public NON_EXPORTED_BASE(mus::WindowTreeDelegate),
      public ScreenMusDelegate {
 public:
  static void Create(mojo::ApplicationImpl* app);
  static WindowManagerConnection* Get();

  mojo::ApplicationImpl* app() { return app_; }

  mus::Window* NewWindow(const std::map<std::string,
                         std::vector<uint8_t>>& properties);

 private:
  explicit WindowManagerConnection(mojo::ApplicationImpl* app);
  ~WindowManagerConnection() override;

  // mus::WindowTreeDelegate:
  void OnEmbed(mus::Window* root) override;
  void OnConnectionLost(mus::WindowTreeConnection* connection) override;

  // ScreenMusDelegate:
  void OnWindowManagerFrameValuesChanged() override;

  NativeWidget* CreateNativeWidget(const Widget::InitParams& init_params,
                                   internal::NativeWidgetDelegate* delegate);

  mojo::ApplicationImpl* app_;
  scoped_ptr<ScreenMus> screen_;
  scoped_ptr<mus::WindowTreeConnection> window_tree_connection_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerConnection);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_WINDOW_MANAGER_CONNECTION_H_
