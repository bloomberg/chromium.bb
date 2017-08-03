// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/content/keyboard_ui_content.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "base/values.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/bindings_policy.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/keyboard/content/keyboard_constants.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/wm/core/shadow.h"
#include "ui/wm/core/shadow_types.h"

namespace {

// The WebContentsDelegate for the keyboard.
// The delegate deletes itself when the keyboard is destroyed.
class KeyboardContentsDelegate : public content::WebContentsDelegate,
                                 public content::WebContentsObserver {
 public:
  explicit KeyboardContentsDelegate(keyboard::KeyboardUIContent* ui)
      : ui_(ui) {}
  ~KeyboardContentsDelegate() override {}

 private:
  // Overridden from content::WebContentsDelegate:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override {
    source->GetController().LoadURL(
        params.url, params.referrer, params.transition, params.extra_headers);
    Observe(source);
    return source;
  }

  bool CanDragEnter(content::WebContents* source,
                    const content::DropData& data,
                    blink::WebDragOperationsMask operations_allowed) override {
    return false;
  }

  bool ShouldCreateWebContents(
      content::WebContents* web_contents,
      content::RenderFrameHost* opener,
      content::SiteInstance* source_site_instance,
      int32_t route_id,
      int32_t main_frame_route_id,
      int32_t main_frame_widget_route_id,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url,
      const std::string& partition_id,
      content::SessionStorageNamespace* session_storage_namespace) override {
    return false;
  }

  bool IsPopupOrPanel(const content::WebContents* source) const override {
    return true;
  }

  void MoveContents(content::WebContents* source,
                    const gfx::Rect& pos) override {
    aura::Window* keyboard = ui_->GetContentsWindow();
    // keyboard window must have been added to keyboard container window at this
    // point. Otherwise, wrong keyboard bounds is used and may cause problem as
    // described in crbug.com/367788.
    DCHECK(keyboard->parent());
    // keyboard window bounds may not set to |pos| after this call. If keyboard
    // is in FULL_WIDTH mode, only the height of keyboard window will be
    // changed.
    keyboard->SetBounds(pos);
  }

  // Overridden from content::WebContentsDelegate:
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) override {
    ui_->RequestAudioInput(web_contents, request, callback);
  }

  // Overridden from content::WebContentsDelegate:
  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) override {
    switch (event.GetType()) {
      // Scroll events are not suppressed because the menu to select IME should
      // be scrollable.
      case blink::WebInputEvent::kGestureScrollBegin:
      case blink::WebInputEvent::kGestureScrollEnd:
      case blink::WebInputEvent::kGestureScrollUpdate:
      case blink::WebInputEvent::kGestureFlingStart:
      case blink::WebInputEvent::kGestureFlingCancel:
        return false;
      default:
        // Stop gesture events from being passed to renderer to suppress the
        // context menu. crbug.com/685140
        return true;
    }
  }

  // Overridden from content::WebContentsObserver:
  void WebContentsDestroyed() override { delete this; }

  keyboard::KeyboardUIContent* ui_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardContentsDelegate);
};

}  // namespace

namespace keyboard {

class WindowBoundsChangeObserver : public aura::WindowObserver {
 public:
  explicit WindowBoundsChangeObserver(KeyboardUIContent* ui) : ui_(ui) {}
  ~WindowBoundsChangeObserver() override {}

  void AddObservedWindow(aura::Window* window) {
    if (!window->HasObserver(this)) {
      window->AddObserver(this);
      observed_windows_.insert(window);
    }
  }
  void RemoveAllObservedWindows() {
    for (std::set<aura::Window*>::iterator it = observed_windows_.begin();
         it != observed_windows_.end(); ++it)
      (*it)->RemoveObserver(this);
    observed_windows_.clear();
  }

 private:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override {
    ui_->UpdateInsetsForWindow(window);
  }
  void OnWindowDestroyed(aura::Window* window) override {
    if (window->HasObserver(this))
      window->RemoveObserver(this);
    observed_windows_.erase(window);
  }

  KeyboardUIContent* ui_;
  std::set<aura::Window*> observed_windows_;

  DISALLOW_COPY_AND_ASSIGN(WindowBoundsChangeObserver);
};

KeyboardUIContent::KeyboardUIContent(content::BrowserContext* context)
    : browser_context_(context),
      default_url_(kKeyboardURL),
      window_bounds_observer_(new WindowBoundsChangeObserver(this)) {
}

KeyboardUIContent::~KeyboardUIContent() {
  ResetInsets();
}

void KeyboardUIContent::UpdateInsetsForWindow(aura::Window* window) {
  aura::Window* keyboard_container =
      keyboard_controller()->GetContainerWindow();
  if (!ShouldWindowOverscroll(window))
    return;

  std::unique_ptr<content::RenderWidgetHostIterator> widgets(
      content::RenderWidgetHost::GetRenderWidgetHosts());
  while (content::RenderWidgetHost* widget = widgets->GetNextHost()) {
    content::RenderWidgetHostView* view = widget->GetView();
    if (view && window->Contains(view->GetNativeView())) {
      gfx::Rect window_bounds = view->GetNativeView()->GetBoundsInScreen();
      gfx::Rect intersect =
          gfx::IntersectRects(window_bounds, keyboard_container->bounds());
      int overlap = ShouldEnableInsets(window) ? intersect.height() : 0;
      if (overlap > 0 && overlap < window_bounds.height())
        view->SetInsets(gfx::Insets(0, 0, overlap, 0));
      else
        view->SetInsets(gfx::Insets());
      return;
    }
  }
}

aura::Window* KeyboardUIContent::GetContentsWindow() {
  if (!keyboard_contents_) {
    keyboard_contents_.reset(CreateWebContents());
    keyboard_contents_->SetDelegate(new KeyboardContentsDelegate(this));
    SetupWebContents(keyboard_contents_.get());
    LoadContents(GetVirtualKeyboardUrl());
    keyboard_contents_->GetNativeView()->AddObserver(this);
  }

  return keyboard_contents_->GetNativeView();
}

bool KeyboardUIContent::HasContentsWindow() const {
  return !!keyboard_contents_;
}

bool KeyboardUIContent::ShouldWindowOverscroll(aura::Window* window) const {
  return true;
}

void KeyboardUIContent::ReloadKeyboardIfNeeded() {
  DCHECK(keyboard_contents_);
  if (keyboard_contents_->GetURL() != GetVirtualKeyboardUrl()) {
    if (keyboard_contents_->GetURL().GetOrigin() !=
        GetVirtualKeyboardUrl().GetOrigin()) {
      // Sets keyboard window rectangle to 0 and close current page before
      // navigate to a keyboard in a different extension. This keeps the UX the
      // same as Android. Note we need to explicitly close current page as it
      // might try to resize keyboard window in javascript on a resize event.
      TRACE_EVENT0("vk", "ReloadKeyboardIfNeeded");
      GetContentsWindow()->SetBounds(gfx::Rect());
      keyboard_contents_->ClosePage();
    }
    LoadContents(GetVirtualKeyboardUrl());
  }
}

void KeyboardUIContent::InitInsets(const gfx::Rect& new_bounds) {
  // Adjust the height of the viewport for visible windows on the primary
  // display.
  // TODO(kevers): Add EnvObserver to properly initialize insets if a
  // window is created while the keyboard is visible.
  std::unique_ptr<content::RenderWidgetHostIterator> widgets(
      content::RenderWidgetHost::GetRenderWidgetHosts());
  while (content::RenderWidgetHost* widget = widgets->GetNextHost()) {
    content::RenderWidgetHostView* view = widget->GetView();
    // Can be NULL, e.g. if the RenderWidget is being destroyed or
    // the render process crashed.
    if (view) {
      aura::Window* window = view->GetNativeView();
      // Added while we determine if RenderWidgetHostViewChildFrame can be
      // changed to always return a non-null value: https://crbug.com/644726 .
      // If we cannot guarantee a non-null value, then this may need to stay.
      if (!window)
        continue;

      if (ShouldWindowOverscroll(window)) {
        gfx::Rect window_bounds = window->GetBoundsInScreen();
        gfx::Rect intersect = gfx::IntersectRects(window_bounds,
                                                  new_bounds);
        int overlap = intersect.height();
        if (overlap > 0 && overlap < window_bounds.height())
          view->SetInsets(gfx::Insets(0, 0, overlap, 0));
        else
          view->SetInsets(gfx::Insets());
        AddBoundsChangedObserver(window);
      }
    }
  }
}

void KeyboardUIContent::ResetInsets() {
  const gfx::Insets insets;
  std::unique_ptr<content::RenderWidgetHostIterator> widgets(
      content::RenderWidgetHost::GetRenderWidgetHosts());
  while (content::RenderWidgetHost* widget = widgets->GetNextHost()) {
    content::RenderWidgetHostView* view = widget->GetView();
    if (view)
      view->SetInsets(insets);
  }
  window_bounds_observer_->RemoveAllObservedWindows();
}

void KeyboardUIContent::SetupWebContents(content::WebContents* contents) {
}

void KeyboardUIContent::OnWindowBoundsChanged(aura::Window* window,
                                              const gfx::Rect& old_bounds,
                                              const gfx::Rect& new_bounds) {
  SetShadowAroundKeyboard();
}

void KeyboardUIContent::OnWindowDestroyed(aura::Window* window) {
  window->RemoveObserver(this);
}

void KeyboardUIContent::OnWindowParentChanged(aura::Window* window,
                                              aura::Window* parent) {
  SetShadowAroundKeyboard();
}

const aura::Window* KeyboardUIContent::GetKeyboardRootWindow() const {
  if (!keyboard_contents_) {
    return nullptr;
  }
  return keyboard_contents_->GetNativeView()->GetRootWindow();
}

content::WebContents* KeyboardUIContent::CreateWebContents() {
  content::BrowserContext* context = browser_context();
  return content::WebContents::Create(content::WebContents::CreateParams(
      context,
      content::SiteInstance::CreateForURL(context, GetVirtualKeyboardUrl())));
}

void KeyboardUIContent::LoadContents(const GURL& url) {
  if (keyboard_contents_) {
    TRACE_EVENT0("vk", "LoadContents");
    content::OpenURLParams params(url, content::Referrer(),
                                  WindowOpenDisposition::SINGLETON_TAB,
                                  ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false);
    keyboard_contents_->OpenURL(params);
  }
}

const GURL& KeyboardUIContent::GetVirtualKeyboardUrl() {
  if (keyboard::IsInputViewEnabled()) {
    const GURL& override_url = GetOverrideContentUrl();
    return override_url.is_valid() ? override_url : default_url_;
  } else {
    return default_url_;
  }
}

bool KeyboardUIContent::ShouldEnableInsets(aura::Window* window) {
  aura::Window* contents_window = GetContentsWindow();
  return (contents_window->GetRootWindow() == window->GetRootWindow() &&
          keyboard::IsKeyboardOverscrollEnabled() &&
          contents_window->IsVisible() &&
          keyboard_controller()->keyboard_visible());
}

void KeyboardUIContent::AddBoundsChangedObserver(aura::Window* window) {
  aura::Window* target_window = window ? window->GetToplevelWindow() : nullptr;
  if (target_window)
    window_bounds_observer_->AddObservedWindow(target_window);
}

void KeyboardUIContent::SetShadowAroundKeyboard() {
  aura::Window* contents_window = keyboard_contents_->GetNativeView();
  if (!contents_window->parent())
    return;

  if (!shadow_) {
    shadow_.reset(new wm::Shadow());
    shadow_->Init(wm::ShadowElevation::LARGE);
    shadow_->layer()->SetVisible(true);
    contents_window->parent()->layer()->Add(shadow_->layer());
  }

  shadow_->SetContentBounds(contents_window->bounds());
}

}  // namespace keyboard
