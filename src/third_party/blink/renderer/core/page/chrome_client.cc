/*
 * Copyright (C) 2006, 2007, 2009, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2012, Samsung Electronics. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/page/chrome_client.h"

#include <algorithm>

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_prescient_networking.h"
#include "third_party/blink/public/platform/web_screen_info.h"
#include "third_party/blink/renderer/core/core_initializer.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scoped_page_pauser.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

void ChromeClient::Trace(Visitor* visitor) {
  visitor->Trace(last_mouse_over_node_);
}

void ChromeClient::InstallSupplements(LocalFrame& frame) {
  CoreInitializer::GetInstance().InstallSupplements(frame);
}

void ChromeClient::SetWindowRectWithAdjustment(const IntRect& pending_rect,
                                               LocalFrame& frame) {
  IntRect screen = GetScreenInfo(frame).available_rect;
  IntRect window = pending_rect;

  IntSize minimum_size = MinimumWindowSize();
  IntSize size_for_constraining_move = minimum_size;
  // Let size 0 pass through, since that indicates default size, not minimum
  // size.
  if (window.Width()) {
    int width = std::max(minimum_size.Width(), window.Width());
    // If the Window Placement experiment is enabled, the window could be placed
    // on another screen, and so it should not be limited by the current screen.
    // This relies on the embedder clamping bounds to the target screen for now.
    // TODO(http://crbug.com/897300): Implement multi-screen clamping in Blink.
    if (!RuntimeEnabledFeatures::WindowPlacementEnabled())
      width = std::min(width, screen.Width());
    window.SetWidth(width);
    size_for_constraining_move.SetWidth(window.Width());
  }
  if (window.Height()) {
    int height = std::max(minimum_size.Height(), window.Height());
    // If the Window Placement experiment is enabled, the window could be placed
    // on another screen, and so it should not be limited by the current screen.
    // This relies on the embedder clamping bounds to the target screen for now.
    // TODO(http://crbug.com/897300): Implement multi-screen clamping in Blink.
    if (!RuntimeEnabledFeatures::WindowPlacementEnabled())
      height = std::min(height, screen.Height());
    window.SetHeight(height);
    size_for_constraining_move.SetHeight(window.Height());
  }

  // If the Window Placement experiment is enabled, the window could be placed
  // on another screen, and so it should not be limited by the current screen.
  // This relies on the embedder clamping bounds to the target screen for now.
  // TODO(http://crbug.com/897300): Implement multi-screen clamping in Blink.
  if (!RuntimeEnabledFeatures::WindowPlacementEnabled()) {
    // Constrain the window position within the valid screen area.
    window.SetX(
        std::max(screen.X(),
                 std::min(window.X(),
                          screen.MaxX() - size_for_constraining_move.Width())));
    window.SetY(std::max(
        screen.Y(),
        std::min(window.Y(),
                 screen.MaxY() - size_for_constraining_move.Height())));
  }

  SetWindowRect(window, frame);
}

bool ChromeClient::CanOpenUIElementIfDuringPageDismissal(
    Frame& main_frame,
    UIElementType ui_element_type,
    const String& message) {
  for (Frame* frame = &main_frame; frame;
       frame = frame->Tree().TraverseNext()) {
    auto* local_frame = DynamicTo<LocalFrame>(frame);
    if (!local_frame)
      continue;
    Document::PageDismissalType dismissal =
        local_frame->GetDocument()->PageDismissalEventBeingDispatched();
    if (dismissal != Document::kNoDismissal) {
      return ShouldOpenUIElementDuringPageDismissal(
          *local_frame, ui_element_type, message, dismissal);
    }
  }
  return true;
}

Page* ChromeClient::CreateWindow(
    LocalFrame* frame,
    const FrameLoadRequest& r,
    const AtomicString& frame_name,
    const WebWindowFeatures& features,
    network::mojom::blink::WebSandboxFlags sandbox_flags,
    const FeaturePolicy::FeatureState& opener_feature_state,
    const SessionStorageNamespaceId& session_storage_namespace_id) {
  if (!CanOpenUIElementIfDuringPageDismissal(
          frame->Tree().Top(), UIElementType::kPopup, g_empty_string)) {
    return nullptr;
  }

  return CreateWindowDelegate(frame, r, frame_name, features, sandbox_flags,
                              opener_feature_state,
                              session_storage_namespace_id);
}

template <typename Delegate>
static bool OpenJavaScriptDialog(LocalFrame* frame,
                                 const String& message,
                                 const Delegate& delegate) {
  // Suspend pages in case the client method runs a new event loop that would
  // otherwise cause the load to continue while we're in the middle of
  // executing JavaScript.
  ScopedPagePauser pauser;
  probe::WillRunJavaScriptDialog(frame);
  bool result = delegate();
  probe::DidRunJavaScriptDialog(frame);
  return result;
}

bool ChromeClient::OpenBeforeUnloadConfirmPanel(const String& message,
                                                LocalFrame* frame,
                                                bool is_reload) {
  DCHECK(frame);
  return OpenJavaScriptDialog(frame, message, [this, frame, is_reload]() {
    return OpenBeforeUnloadConfirmPanelDelegate(frame, is_reload);
  });
}

bool ChromeClient::OpenJavaScriptAlert(LocalFrame* frame,
                                       const String& message) {
  DCHECK(frame);
  if (!CanOpenUIElementIfDuringPageDismissal(
          frame->Tree().Top(), UIElementType::kAlertDialog, message)) {
    return false;
  }
  return OpenJavaScriptDialog(frame, message, [this, frame, &message]() {
    return OpenJavaScriptAlertDelegate(frame, message);
  });
}

bool ChromeClient::OpenJavaScriptConfirm(LocalFrame* frame,
                                         const String& message) {
  DCHECK(frame);
  if (!CanOpenUIElementIfDuringPageDismissal(
          frame->Tree().Top(), UIElementType::kConfirmDialog, message)) {
    return false;
  }
  return OpenJavaScriptDialog(frame, message, [this, frame, &message]() {
    return OpenJavaScriptConfirmDelegate(frame, message);
  });
}

bool ChromeClient::OpenJavaScriptPrompt(LocalFrame* frame,
                                        const String& prompt,
                                        const String& default_value,
                                        String& result) {
  DCHECK(frame);
  if (!CanOpenUIElementIfDuringPageDismissal(
          frame->Tree().Top(), UIElementType::kPromptDialog, prompt)) {
    return false;
  }
  return OpenJavaScriptDialog(
      frame, prompt, [this, frame, &prompt, &default_value, &result]() {
        return OpenJavaScriptPromptDelegate(frame, prompt, default_value,
                                            result);
      });
}

void ChromeClient::MouseDidMoveOverElement(LocalFrame& frame,
                                           const HitTestLocation& location,
                                           const HitTestResult& result) {
  if (!result.GetScrollbar() && result.InnerNode() &&
      result.InnerNode()->GetDocument().IsDNSPrefetchEnabled()) {
    WebPrescientNetworking* web_prescient_networking =
        frame.PrescientNetworking();
    if (web_prescient_networking) {
      web_prescient_networking->PrefetchDNS(result.AbsoluteLinkURL().Host());
    }
  }

  ShowMouseOverURL(result);

  if (result.GetScrollbar())
    ClearToolTip(frame);
  else
    SetToolTip(frame, location, result);
}

void ChromeClient::SetToolTip(LocalFrame& frame,
                              const HitTestLocation& location,
                              const HitTestResult& result) {
  // First priority is a tooltip for element with "title" attribute.
  TextDirection tool_tip_direction;
  String tool_tip = result.Title(tool_tip_direction);

  // Lastly, some elements provide default tooltip strings.  e.g. <input
  // type="file" multiple> shows a tooltip for the selected filenames.
  if (tool_tip.IsNull()) {
    if (auto* element = DynamicTo<Element>(result.InnerNode())) {
      tool_tip = element->DefaultToolTip();

      // FIXME: We should obtain text direction of tooltip from
      // ChromeClient or platform. As of October 2011, all client
      // implementations don't use text direction information for
      // ChromeClient::setToolTip. We'll work on tooltip text
      // direction during bidi cleanup in form inputs.
      tool_tip_direction = TextDirection::kLtr;
    }
  }

  if (last_tool_tip_point_ == location.Point() &&
      last_tool_tip_text_ == tool_tip)
    return;

  // If a tooltip was displayed earlier, and mouse cursor moves over
  // a different node with the same tooltip text, make sure the previous
  // tooltip is unset, so that it does not get stuck positioned relative
  // to the previous node).
  // The ::setToolTip overload, which is be called down the road,
  // ensures a new tooltip to be displayed with the new context.
  if (result.InnerNodeOrImageMapImage() != last_mouse_over_node_ &&
      !last_tool_tip_text_.IsEmpty() && tool_tip == last_tool_tip_text_)
    ClearToolTip(frame);

  last_tool_tip_point_ = location.Point();
  last_tool_tip_text_ = tool_tip;
  last_mouse_over_node_ = result.InnerNodeOrImageMapImage();
  SetToolTip(frame, tool_tip, tool_tip_direction);
}

void ChromeClient::ClearToolTip(LocalFrame& frame) {
  // Do not check m_lastToolTip* and do not update them intentionally.
  // We don't want to show tooltips with same content after clearToolTip().
  SetToolTip(frame, String(), TextDirection::kLtr);
}

bool ChromeClient::Print(LocalFrame* frame) {
  if (!CanOpenUIElementIfDuringPageDismissal(*frame->GetPage()->MainFrame(),
                                             UIElementType::kPrintDialog,
                                             g_empty_string)) {
    return false;
  }

  if (frame->GetDocument()->IsSandboxed(
          network::mojom::blink::WebSandboxFlags::kModals)) {
    UseCounter::Count(frame->GetDocument(),
                      WebFeature::kDialogInSandboxedContext);
    frame->Console().AddMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kSecurity,
        mojom::ConsoleMessageLevel::kError,
        "Ignored call to 'print()'. The document is sandboxed, and the "
        "'allow-modals' keyword is not set."));
    return false;
  }

  // Suspend pages in case the client method runs a new event loop that would
  // otherwise cause the load to continue while we're in the middle of
  // executing JavaScript.
  ScopedPagePauser pauser;

  PrintDelegate(frame);
  return true;
}

}  // namespace blink
