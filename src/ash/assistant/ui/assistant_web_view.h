// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_WEB_VIEW_H_
#define ASH_ASSISTANT_UI_ASSISTANT_WEB_VIEW_H_

#include <memory>

#include "ash/assistant/assistant_controller_observer.h"
#include "ash/assistant/ui/caption_bar.h"
#include "base/macros.h"
#include "base/optional.h"
#include "ui/views/view.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace ash {

class AssistantController;

// AssistantWebView is a child of AssistantBubbleView which allows Assistant UI
// to render remotely hosted content within its bubble. It provides a CaptionBar
// for window level controls and a WebView/ServerRemoteViewHost for embedding
// web contents.
class AssistantWebView : public views::View,
                         public AssistantControllerObserver,
                         public CaptionBarDelegate {
 public:
  explicit AssistantWebView(AssistantController* assistant_controller);
  ~AssistantWebView() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void ChildPreferredSizeChanged(views::View* child) override;

  // CaptionBarDelegate:
  bool OnCaptionButtonPressed(CaptionButtonId id) override;

  // AssistantControllerObserver:
  void OnDeepLinkReceived(
      assistant::util::DeepLinkType type,
      const std::map<std::string, std::string>& params) override;

 private:
  void InitLayout();
  void OnWebContentsReady(
      const base::Optional<base::UnguessableToken>& embed_token);
  void ReleaseWebContents();

  AssistantController* const assistant_controller_;  // Owned by Shell.

  CaptionBar* caption_bar_;  // Owned by view hierarchy.

  // In Mash, |content_view_| is owned by the view hierarchy. Otherwise, the
  // view is owned by the WebContentsManager.
  views::View* content_view_;

  // Uniquely identifies web contents owned by WebContentsManager.
  base::Optional<base::UnguessableToken> web_contents_id_token_;

  // Weak pointer factory used for web contents rendering requests.
  base::WeakPtrFactory<AssistantWebView> web_contents_request_factory_;

  DISALLOW_COPY_AND_ASSIGN(AssistantWebView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_WEB_VIEW_H_
