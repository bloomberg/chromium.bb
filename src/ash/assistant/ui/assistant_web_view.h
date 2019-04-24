// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_WEB_VIEW_H_
#define ASH_ASSISTANT_UI_ASSISTANT_WEB_VIEW_H_

#include <map>
#include <memory>
#include <string>

#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/caption_bar.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/optional.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/content/public/cpp/navigable_contents.h"
#include "ui/aura/window_observer.h"
#include "ui/views/view.h"

namespace ash {

enum class AssistantButtonId;
class AssistantViewDelegate;

// AssistantWebView is a child of AssistantBubbleView which allows Assistant UI
// to render remotely hosted content within its bubble. It provides a CaptionBar
// for window level controls and embeds web contents with help from the Content
// Service.
class COMPONENT_EXPORT(ASSISTANT_UI) AssistantWebView
    : public views::View,
      public aura::WindowObserver,
      public AssistantViewDelegateObserver,
      public CaptionBarDelegate,
      public content::NavigableContentsObserver,
      public AssistantUiModelObserver {
 public:
  explicit AssistantWebView(AssistantViewDelegate* delegate);
  ~AssistantWebView() override;

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void OnFocus() override;
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;

  // views::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowDestroying(aura::Window* window) override;

  // CaptionBarDelegate:
  bool OnCaptionButtonPressed(AssistantButtonId id) override;

  // AssistantViewDelegateObserver:
  void OnDeepLinkReceived(
      assistant::util::DeepLinkType type,
      const std::map<std::string, std::string>& params) override;

  // content::NavigableContentsObserver:
  void DidAutoResizeView(const gfx::Size& new_size) override;
  void DidStopLoading() override;
  void DidSuppressNavigation(const GURL& url,
                             WindowOpenDisposition disposition,
                             bool from_user_gesture) override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      base::Optional<AssistantEntryPoint> entry_point,
      base::Optional<AssistantExitPoint> exit_point) override;

 private:
  void InitLayout();
  void RemoveContents();

  AssistantViewDelegate* const delegate_;

  CaptionBar* caption_bar_;  // Owned by view hierarchy.

  content::mojom::NavigableContentsFactoryPtr contents_factory_;
  std::unique_ptr<content::NavigableContents> contents_;

  // Our contents are drawn to a layer that is not masked by our widget's layer.
  // This causes our contents to ignore the corner radius that we have set on
  // the widget. To address this, we apply a separate layer mask to the
  // contents' native view layer enforcing our desired corner radius.
  std::unique_ptr<ui::LayerOwner> contents_mask_;

  base::WeakPtrFactory<AssistantWebView> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AssistantWebView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_WEB_VIEW_H_
