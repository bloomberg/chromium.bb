// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_RENDER_FRAME_H_
#define CONTENT_TEST_TEST_RENDER_FRAME_H_

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "content/common/frame.mojom.h"
#include "content/common/input/input_handler.mojom.h"
#include "content/renderer/render_frame_impl.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"

namespace blink {
class WebHistoryItem;
}

namespace content {

struct CommonNavigationParams;
class MockFrameHost;
struct CommitNavigationParams;

// A test class to use in RenderViewTests.
class TestRenderFrame : public RenderFrameImpl {
 public:
  static RenderFrameImpl* CreateTestRenderFrame(
      RenderFrameImpl::CreateParams params);
  ~TestRenderFrame() override;

  const blink::WebHistoryItem& current_history_item() {
    return current_history_item_;
  }

  // Overrides the content in the next navigation originating from the frame.
  // This will also short-circuit browser-side navigation,
  // FrameLoader will always carry out the load renderer-side.
  void SetHTMLOverrideForNextNavigation(const std::string& html);

  void Navigate(const network::ResourceResponseHead& head,
                const CommonNavigationParams& common_params,
                const CommitNavigationParams& commit_params);
  void Navigate(const CommonNavigationParams& common_params,
                const CommitNavigationParams& commit_params);
  void NavigateWithError(const CommonNavigationParams& common_params,
                         const CommitNavigationParams& request_params,
                         int error_code,
                         const base::Optional<std::string>& error_page_content);
  void SwapOut(int proxy_routing_id,
               bool is_loading,
               const FrameReplicationState& replicated_frame_state);
  void SetEditableSelectionOffsets(int start, int end);
  void ExtendSelectionAndDelete(int before, int after);
  void DeleteSurroundingText(int before, int after);
  void DeleteSurroundingTextInCodePoints(int before, int after);
  void CollapseSelection();
  void SetAccessibilityMode(ui::AXMode new_mode);
  void SetCompositionFromExistingText(
      int start,
      int end,
      const std::vector<ui::ImeTextSpan>& ime_text_spans);

  void BeginNavigation(std::unique_ptr<blink::WebNavigationInfo> info) override;

  std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
  TakeLastCommitParams();

  // Sets a callback to be run the next time DidAddMessageToConsole
  // is called (e.g. window.console.log() is called).
  void SetDidAddMessageToConsoleCallback(
      base::OnceCallback<void(const base::string16& msg)> callback);

  service_manager::mojom::InterfaceProviderRequest
  TakeLastInterfaceProviderRequest();

  blink::mojom::DocumentInterfaceBrokerRequest
  TakeLastDocumentInterfaceBrokerRequest();

 private:
  explicit TestRenderFrame(RenderFrameImpl::CreateParams params);

  mojom::FrameHost* GetFrameHost() override;

  mojom::FrameInputHandler* GetFrameInputHandler();

  std::unique_ptr<MockFrameHost> mock_frame_host_;
  base::Optional<std::string> next_navigation_html_override_;
  mojom::FrameInputHandlerPtr frame_input_handler_;

  mojom::NavigationClientAssociatedPtr mock_navigation_client_;

  DISALLOW_COPY_AND_ASSIGN(TestRenderFrame);
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_RENDER_FRAME_H_
