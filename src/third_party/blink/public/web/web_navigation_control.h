// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_NAVIGATION_CONTROL_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_NAVIGATION_CONTROL_H_

#include <memory>

#include "base/callback.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace blink {

class WebSecurityOrigin;
class WebURL;
struct WebNavigationInfo;
struct WebNavigationParams;

// This interface gives control to navigation-related functionality of
// WebLocalFrame. It is separated from WebLocalFrame to give precise control
// over callers of navigation methods.
// WebLocalFrameClient gets a reference to this interface in BindToFrame.
class WebNavigationControl : public WebLocalFrame {
 public:
  ~WebNavigationControl() override {}

  // Runs beforeunload handlers for this frame and its local descendants.
  // Returns |true| if all the frames agreed to proceed with unloading
  // from their respective event handlers.
  // Note: this may lead to the destruction of the frame.
  virtual bool DispatchBeforeUnloadEvent(bool is_reload) = 0;

  // Commits a cross-document navigation in the frame. See WebNavigationParams
  // for details. Calls WebLocalFrameClient::DidCommitNavigation synchronously
  // after new document commit, but before loading any content, unless commit
  // fails.
  // TODO(dgozman): return mojom::CommitResult.
  virtual void CommitNavigation(
      std::unique_ptr<WebNavigationParams> navigation_params,
      std::unique_ptr<WebDocumentLoader::ExtraData> extra_data) = 0;

  // Commits a same-document navigation in the frame. For history navigations,
  // a valid WebHistoryItem should be provided. |initiator_origin| is null
  // for browser-initiated navigations. Returns CommitResult::Ok if the
  // navigation has actually committed.
  virtual mojom::CommitResult CommitSameDocumentNavigation(
      const WebURL&,
      WebFrameLoadType,
      const WebHistoryItem&,
      bool is_client_redirect,
      bool has_transient_user_activation,
      const WebSecurityOrigin& initiator_origin,
      bool is_browser_initiated) = 0;

  // Override the normal rules that determine whether the frame is on the
  // initial empty document or not. Used to propagate state when this frame has
  // navigated cross process.
  virtual void SetIsNotOnInitialEmptyDocument() = 0;
  virtual bool IsOnInitialEmptyDocument() = 0;

  // Marks the frame as loading, before WebLocalFrameClient issues a navigation
  // request through the browser process on behalf of the frame.
  // This runs some JavaScript event listeners, which may cancel the navigation
  // or detach the frame. In this case the method returns false and client
  // should not proceed with the navigation.
  virtual bool WillStartNavigation(const WebNavigationInfo&) = 0;

  // Informs the frame that the navigation it asked the client to do was
  // dropped.
  virtual void DidDropNavigation() = 0;

 protected:
  explicit WebNavigationControl(mojom::TreeScopeType scope,
                                const LocalFrameToken& frame_token)
      : WebLocalFrame(scope, frame_token) {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_NAVIGATION_CONTROL_H_
