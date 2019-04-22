// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ash_keyboard_ui.h"

#include <set>
#include <string>
#include <utility>

#include "ash/keyboard/ash_keyboard_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/ws/window_service_owner.h"
#include "base/bind.h"
#include "base/scoped_observer.h"
#include "services/ws/public/mojom/window_tree.mojom.h"
#include "services/ws/remote_view_host/server_remote_view_host.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/views/background.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/shadow_types.h"

namespace {

const int kShadowElevationVirtualKeyboard = 2;

}  // namespace

namespace ash {

////////////////////////////////////////////////////////////////////////////////
// AshKeyboardView

class AshKeyboardUI::AshKeyboardView : public views::WidgetDelegateView,
                                       public aura::WindowObserver {
 public:
  explicit AshKeyboardView(aura::Window* context) : scoped_observer_(this) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    InitWidget(context);

    // Set the background to be transparent for custom keyboard window shape.
    SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));
    window()->SetTransparent(true);
  }

  ~AshKeyboardView() override { widget_.reset(); }

  void SetShadowElevation(bool show) {
    ::wm::SetShadowElevation(window(),
                             show ? kShadowElevationVirtualKeyboard : 0);
  }

  void Embed(base::UnguessableToken token, const gfx::Size& size) {
    VLOG(1) << "Embed contents. Size: " << size.ToString();
    if (!size.IsEmpty())
      window()->SetBounds(gfx::Rect(size));
    if (!server_remote_view_host_) {
      server_remote_view_host_ = new ws::ServerRemoteViewHost(
          Shell::Get()->window_service_owner()->window_service());
      AddChildView(server_remote_view_host_);
      scoped_observer_.Add(server_remote_view_host_->embedding_root());
    }
    server_remote_view_host_->EmbedUsingToken(
        token, ws::mojom::kEmbedFlagEmbedderControlsVisibility,
        base::BindOnce(
            [](base::UnguessableToken token, bool result) {
              DVLOG(1) << "Embed: " << token << ". Result: " << result;
            },
            token));
    Layout();
  }

  // views::WidgetDelegateView:
  const char* GetClassName() const override { return "AshKeyboardView"; }
  void DeleteDelegate() override {}

  // aura::WindowObserver:
  void OnWindowAdded(aura::Window* new_window) override {
    if (new_window->parent() == server_remote_view_host_->embedding_root())
      scoped_observer_.Add(new_window);
  }
  void OnWillRemoveWindow(aura::Window* window) override {
    if (window->parent() == server_remote_view_host_->embedding_root())
      scoped_observer_.Remove(window);
  }
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override {
    if (window == server_remote_view_host_->embedding_root())
      return;

    VLOG(1) << "OnWindowBoundsChanged: " << new_bounds.ToString();

    // This happens when the client requests to resize the keyboard window,
    // typically through window.resizeTo in JS. Ash keyboard window bounds
    // should be updated to reflect the request.
    gfx::Rect new_bounds_in_screen = new_bounds;
    ::wm::ConvertRectToScreen(window->parent(), &new_bounds_in_screen);
    GetWidget()->SetBounds(new_bounds_in_screen);
  }

  aura::Window* window() { return GetWidget()->GetNativeView(); }

 private:
  void InitWidget(aura::Window* context) {
    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.name = "AshKeyboardUI";
    params.delegate = this;
    params.context = context;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget_->Init(params);
  }

  std::unique_ptr<views::Widget> widget_;
  ws::ServerRemoteViewHost* server_remote_view_host_ = nullptr;
  ScopedObserver<aura::Window, aura::WindowObserver> scoped_observer_;

  DISALLOW_COPY_AND_ASSIGN(AshKeyboardView);
};

////////////////////////////////////////////////////////////////////////////////
// AshKeyboardUI

AshKeyboardUI::AshKeyboardUI(AshKeyboardController* ash_keyboard_controller)
    : ash_keyboard_controller_(ash_keyboard_controller) {}

AshKeyboardUI::~AshKeyboardUI() {
  ash_keyboard_controller_->SendOnKeyboardUIDestroyed();
  if (ash_keyboard_view_) {
    aura::Window* keyboard_window = ash_keyboard_view_->window();
    if (keyboard_window)
      keyboard_window->RemoveObserver(this);
  }
}

// keyboard::KeyboardUI:

aura::Window* AshKeyboardUI::LoadKeyboardWindow(LoadCallback callback) {
  DVLOG(1) << "LoadKeyboardWindow";
  DCHECK(!ash_keyboard_view_);
  load_callback_ = std::move(callback);

  ash_keyboard_view_ = std::make_unique<AshKeyboardView>(
      keyboard::KeyboardController::Get()->parent_container());

  if (!contents_window_token_.is_empty())
    EmbedContents();

  aura::Window* keyboard_window = ash_keyboard_view_->window();
  keyboard_window->AddObserver(this);

  ash_keyboard_controller_->SendOnLoadKeyboardContentsRequested();

  return keyboard_window;
}

aura::Window* AshKeyboardUI::GetKeyboardWindow() const {
  return ash_keyboard_view_ ? ash_keyboard_view_->window() : nullptr;
}

ui::InputMethod* AshKeyboardUI::GetInputMethod() {
  // TODO(stevenjb/shuchen): |bridge| may be null in Multi-Process Mash.
  // KeyboardController needs to use the TBD Mojo based IMF services instead.
  ui::IMEBridge* bridge = ui::IMEBridge::Get();
  if (!bridge || !bridge->GetInputContextHandler()) {
    // Needed by a handful of browser tests that use MockInputMethod.
    return Shell::GetRootWindowForNewWindows()->GetHost()->GetInputMethod();
  }
  return bridge->GetInputContextHandler()->GetInputMethod();
}

void AshKeyboardUI::ReloadKeyboardIfNeeded() {
  DVLOG(1) << "ReloadKeyboardIfNeeded";
  ash_keyboard_controller_->SendOnLoadKeyboardContentsRequested();
}

void AshKeyboardUI::KeyboardContentsLoaded(const base::UnguessableToken& token,
                                           const gfx::Size& size) {
  DVLOG(1) << "KeyboardContentsLoaded. Token: " << token
           << " Size: " << size.ToString();
  if (token == contents_window_token_ && size == contents_window_size_)
    return;
  contents_window_token_ = token;
  contents_window_size_ = size;
  if (ash_keyboard_view_)
    EmbedContents();
}

// aura::WindowObserver:

void AshKeyboardUI::OnWindowBoundsChanged(aura::Window* window,
                                          const gfx::Rect& old_bounds,
                                          const gfx::Rect& new_bounds,
                                          ui::PropertyChangeReason reason) {
  DVLOG(1) << "OnWindowBoundsChanged: " << window->GetName() << ": "
           << new_bounds.ToString();

  // Normally OnKeyboardVisibleBoundsChanged is triggered from
  // keyboard::KeyboardController::NotifyKeyboardBoundsChanging, however if the
  // contents window token has not been provided, an intial bounds change
  // needs to be sent so that the contents can size itself.
  if (contents_window_token_.is_empty())
    ash_keyboard_controller_->SendOnKeyboardVisibleBoundsChanged(new_bounds);

  // The ContainerType changes after ash_keyboard_view_ is created, so update
  // the shadow on bounds change. TODO(stevenjb): Find a better way to do this.
  SetShadowElevation();
}

void AshKeyboardUI::OnWindowDestroyed(aura::Window* window) {
  window->RemoveObserver(this);
}

void AshKeyboardUI::OnWindowParentChanged(aura::Window* window,
                                          aura::Window* parent) {
  SetShadowElevation();
}

// private methods:

void AshKeyboardUI::EmbedContents() {
  ash_keyboard_view_->Embed(contents_window_token_, contents_window_size_);
  if (!load_callback_.is_null())
    std::move(load_callback_).Run();
}

void AshKeyboardUI::SetShadowElevation() {
  ash_keyboard_view_->SetShadowElevation(
      ash_keyboard_controller_->keyboard_controller()
          ->GetActiveContainerType() ==
      keyboard::mojom::ContainerType::kFullWidth);
}

}  // namespace ash
