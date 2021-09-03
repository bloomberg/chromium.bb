// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/automation_internal/automation_internal_api.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/media_player_id.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/browser/api/automation_internal/automation_event_router.h"
#include "extensions/browser/api/automation_internal/automation_internal_api_delegate.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/common/api/automation.h"
#include "extensions/common/api/automation_internal.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest_handlers/automation.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_action_handler_base.h"
#include "ui/accessibility/ax_action_handler_registry.h"
#include "ui/accessibility/ax_enum_util.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#endif

namespace extensions {
class AutomationWebContentsObserver;
}  // namespace extensions

namespace extensions {

namespace {

const char kCannotRequestAutomationOnPage[] =
    "Failed request of automation on a page";
const char kRendererDestroyed[] = "The tab was closed.";
const char kNoDocument[] = "No document.";
const char kNodeDestroyed[] =
    "domQuerySelector sent on node which is no longer in the tree.";

// Handles sending and receiving IPCs for a single querySelector request. On
// creation, sends the request IPC, and is destroyed either when the response is
// received or the renderer is destroyed.
class QuerySelectorHandler : public content::WebContentsObserver {
 public:
  QuerySelectorHandler(
      content::WebContents* web_contents,
      int request_id,
      int acc_obj_id,
      const std::u16string& query,
      extensions::AutomationInternalQuerySelectorFunction::Callback callback)
      : content::WebContentsObserver(web_contents),
        request_id_(request_id),
        callback_(std::move(callback)) {
    content::RenderFrameHost* rfh = web_contents->GetMainFrame();

    rfh->Send(new ExtensionMsg_AutomationQuerySelector(
        rfh->GetRoutingID(), request_id, acc_obj_id, query));
  }

  ~QuerySelectorHandler() override {}

  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override {
    if (message.type() != ExtensionHostMsg_AutomationQuerySelector_Result::ID)
      return false;

    // There may be several requests in flight; check this response matches.
    int message_request_id = 0;
    base::PickleIterator iter(message);
    if (!iter.ReadInt(&message_request_id))
      return false;

    if (message_request_id != request_id_)
      return false;

    IPC_BEGIN_MESSAGE_MAP(QuerySelectorHandler, message)
      IPC_MESSAGE_HANDLER(ExtensionHostMsg_AutomationQuerySelector_Result,
                          OnQueryResponse)
    IPC_END_MESSAGE_MAP()
    return true;
  }

  void WebContentsDestroyed() override {
    std::move(callback_).Run(kRendererDestroyed, 0);
    delete this;
  }

 private:
  void OnQueryResponse(int request_id,
                       ExtensionHostMsg_AutomationQuerySelector_Error error,
                       int result_acc_obj_id) {
    std::string error_string;
    switch (error.value) {
      case ExtensionHostMsg_AutomationQuerySelector_Error::kNone:
        break;
      case ExtensionHostMsg_AutomationQuerySelector_Error::kNoDocument:
        error_string = kNoDocument;
        break;
      case ExtensionHostMsg_AutomationQuerySelector_Error::kNodeDestroyed:
        error_string = kNodeDestroyed;
        break;
    }
    std::move(callback_).Run(error_string, result_acc_obj_id);
    delete this;
  }

  int request_id_;
  extensions::AutomationInternalQuerySelectorFunction::Callback callback_;
};

}  // namespace

using OldAXTreeIdMap = std::map<content::NavigationHandle*, ui::AXTreeID>;
base::LazyInstance<OldAXTreeIdMap>::DestructorAtExit g_old_ax_tree =
    LAZY_INSTANCE_INITIALIZER;

// Helper class that receives accessibility data from |WebContents|.
class AutomationWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<AutomationWebContentsObserver>,
      public AutomationEventRouterObserver {
 public:
  ~AutomationWebContentsObserver() override {
    automation_event_router_observer_.Reset();
  }

  // content::WebContentsObserver overrides.
  void AccessibilityEventReceived(const content::AXEventNotificationDetails&
                                      content_event_bundle) override {
    gfx::Point mouse_location;
#if defined(USE_AURA)
    mouse_location = aura::Env::GetInstance()->last_mouse_location();
#endif
    AutomationEventRouter* router = AutomationEventRouter::GetInstance();
    router->DispatchAccessibilityEvents(
        std::move(content_event_bundle.ax_tree_id),
        std::move(content_event_bundle.updates), mouse_location,
        std::move(content_event_bundle.events));
  }

  void DidStartNavigation(content::NavigationHandle* navigation) override {
    content::RenderFrameHost* previous_rfh = content::RenderFrameHost::FromID(
        navigation->GetPreviousRenderFrameHostId());
    if (previous_rfh)
      g_old_ax_tree.Get()[navigation] = previous_rfh->GetAXTreeID();
  }

  void DidFinishNavigation(content::NavigationHandle* navigation) override {
    ui::AXTreeID old_ax_tree = g_old_ax_tree.Get()[navigation];
    g_old_ax_tree.Get().erase(navigation);

    if (old_ax_tree == ui::AXTreeIDUnknown())
      return;

    ui::AXTreeID new_ax_tree = ui::AXTreeIDUnknown();

    // If navigation was canceled, render frame host will not
    // be set and there is no new tree.
    if (navigation->HasCommitted() && navigation->GetRenderFrameHost())
      new_ax_tree = navigation->GetRenderFrameHost()->GetAXTreeID();

    if (old_ax_tree == new_ax_tree)
      return;

    AutomationEventRouter::GetInstance()->DispatchTreeDestroyedEvent(
        old_ax_tree, browser_context_);
  }

  void AccessibilityLocationChangesReceived(
      const std::vector<content::AXLocationChangeNotificationDetails>& details)
      override {
    for (const auto& src : details) {
      ExtensionMsg_AccessibilityLocationChangeParams dst;
      dst.id = src.id;
      dst.tree_id = src.ax_tree_id;
      dst.new_location = src.new_location;
      AutomationEventRouter* router = AutomationEventRouter::GetInstance();
      router->DispatchAccessibilityLocationChange(dst);
    }
  }

  void RenderFrameDeleted(
      content::RenderFrameHost* render_frame_host) override {
    ui::AXTreeID tree_id = render_frame_host->GetAXTreeID();
    if (tree_id == ui::AXTreeIDUnknown())
      return;

    AutomationEventRouter::GetInstance()->DispatchTreeDestroyedEvent(
        tree_id, browser_context_);
  }

  void MediaStartedPlaying(const MediaPlayerInfo& video_type,
                           const content::MediaPlayerId& id) override {
    auto* render_frame_host =
        content::RenderFrameHost::FromID(id.frame_routing_id);
    if (!render_frame_host)
      return;

    content::AXEventNotificationDetails content_event_bundle;
    content_event_bundle.ax_tree_id = render_frame_host->GetAXTreeID();
    content_event_bundle.events.resize(1);
    content_event_bundle.events[0].event_type =
        ax::mojom::Event::kMediaStartedPlaying;
    AccessibilityEventReceived(content_event_bundle);
  }

  void MediaStoppedPlaying(
      const MediaPlayerInfo& video_type,
      const content::MediaPlayerId& id,
      WebContentsObserver::MediaStoppedReason reason) override {
    auto* render_frame_host =
        content::RenderFrameHost::FromID(id.frame_routing_id);
    if (!render_frame_host)
      return;

    content::AXEventNotificationDetails content_event_bundle;
    content_event_bundle.ax_tree_id = render_frame_host->GetAXTreeID();
    content_event_bundle.events.resize(1);
    content_event_bundle.events[0].event_type =
        ax::mojom::Event::kMediaStoppedPlaying;
    AccessibilityEventReceived(content_event_bundle);
  }

  // AutomationEventRouterObserver overrides.
  void AllAutomationExtensionsGone() override {
    if (!web_contents())
      return;

    ui::AXMode new_mode = web_contents()->GetAccessibilityMode();
    uint8_t flags = ui::kAXModeWebContentsOnly.mode();
    new_mode.set_mode(flags, false);
    web_contents()->SetAccessibilityMode(std::move(new_mode));
  }

 private:
  friend class content::WebContentsUserData<AutomationWebContentsObserver>;

  explicit AutomationWebContentsObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents),
        browser_context_(web_contents->GetBrowserContext()) {
    if (web_contents->IsCurrentlyAudible()) {
      content::RenderFrameHost* rfh = web_contents->GetMainFrame();
      if (!rfh)
        return;

      content::AXEventNotificationDetails content_event_bundle;
      content_event_bundle.ax_tree_id = rfh->GetAXTreeID();
      content_event_bundle.events.resize(1);
      content_event_bundle.events[0].event_type =
          ax::mojom::Event::kMediaStartedPlaying;
      AccessibilityEventReceived(content_event_bundle);
    }

    automation_event_router_observer_.Observe(
        AutomationEventRouter::GetInstance());
  }

  content::BrowserContext* browser_context_;

  base::ScopedObservation<extensions::AutomationEventRouter,
                          extensions::AutomationEventRouterObserver>
      automation_event_router_observer_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(AutomationWebContentsObserver);
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(AutomationWebContentsObserver)

ExtensionFunction::ResponseAction AutomationInternalEnableTabFunction::Run() {
  const AutomationInfo* automation_info = AutomationInfo::Get(extension());
  EXTENSION_FUNCTION_VALIDATE(automation_info);

  using api::automation_internal::EnableTab::Params;
  std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  content::WebContents* contents = NULL;
  AutomationInternalApiDelegate* automation_api_delegate =
      ExtensionsAPIClient::Get()->GetAutomationInternalApiDelegate();
  int tab_id = -1;
  if (params->args.tab_id.get()) {
    tab_id = *params->args.tab_id;
    std::string error_string;
    if (!automation_api_delegate->GetTabById(tab_id, browser_context(),
                                             include_incognito_information(),
                                             &contents, &error_string)) {
      return RespondNow(Error(error_string, base::NumberToString(tab_id)));
    }
  } else {
    contents = automation_api_delegate->GetActiveWebContents(this);
    if (!contents)
      return RespondNow(Error("No active tab"));

    tab_id = automation_api_delegate->GetTabId(contents);
  }

  content::RenderFrameHost* rfh = contents->GetMainFrame();
  if (!rfh)
    return RespondNow(Error("Could not enable accessibility for active tab"));

  if (!automation_api_delegate->CanRequestAutomation(
          extension(), automation_info, contents)) {
    return RespondNow(Error(kCannotRequestAutomationOnPage));
  }

  AutomationWebContentsObserver::CreateForWebContents(contents);
  contents->EnableWebContentsOnlyAccessibilityMode();

  ui::AXTreeID ax_tree_id = rfh->GetAXTreeID();

  // The AXTreeID is not yet ready/set.
  if (ax_tree_id == ui::AXTreeIDUnknown())
    return RespondNow(Error("Tab is not ready."));

  // This gets removed when the extension process dies.
  AutomationEventRouter::GetInstance()->RegisterListenerForOneTree(
      extension_id(), source_process_id(), ax_tree_id);

  return RespondNow(
      ArgumentList(api::automation_internal::EnableTab::Results::Create(
          ax_tree_id.ToString(), tab_id)));
}

absl::optional<std::string> AutomationInternalEnableTreeFunction::EnableTree(
    const ui::AXTreeID& ax_tree_id,
    const ExtensionId& extension_id) {
  ui::AXActionHandlerRegistry* registry =
      ui::AXActionHandlerRegistry::GetInstance();
  ui::AXActionHandlerBase* action_handler =
      registry->GetActionHandler(ax_tree_id);
  if (action_handler) {
    // Explicitly invalidate the pre-existing source tree first. This ensures
    // the source tree sends a complete tree when the next event occurs. This
    // is required whenever the client extension is reloaded.
    ui::AXActionData action;
    action.target_tree_id = ax_tree_id;
    action.source_extension_id = extension_id;
    action.action = ax::mojom::Action::kInternalInvalidateTree;
    action_handler->PerformAction(action);
  }

  AutomationInternalApiDelegate* automation_api_delegate =
      ExtensionsAPIClient::Get()->GetAutomationInternalApiDelegate();
  if (automation_api_delegate->EnableTree(ax_tree_id))
    return absl::nullopt;

  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromAXTreeID(ax_tree_id);
  if (!rfh)
    return "unable to load tab";

  content::WebContents* contents =
      content::WebContents::FromRenderFrameHost(rfh);
  AutomationWebContentsObserver::CreateForWebContents(contents);

  // Only call this if this is the root of a frame tree, to avoid resetting
  // the accessibility state multiple times.
  if (!rfh->GetParent())
    contents->EnableWebContentsOnlyAccessibilityMode();

  return absl::nullopt;
}

ExtensionFunction::ResponseAction AutomationInternalEnableTreeFunction::Run() {
  using api::automation_internal::EnableTree::Params;

  std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  ui::AXTreeID ax_tree_id = ui::AXTreeID::FromString(params->tree_id);
  absl::optional<std::string> error = EnableTree(ax_tree_id, extension_id());
  if (error) {
    return RespondNow(Error(error.value()));
  } else {
    return RespondNow(NoArguments());
  }
}

AutomationInternalPerformActionFunction::Result
AutomationInternalPerformActionFunction::ConvertToAXActionData(
    const ui::AXTreeID& tree_id,
    int32_t automation_node_id,
    const std::string& action_type_string,
    int request_id,
    const base::DictionaryValue& additional_properties,
    const std::string& extension_id,
    ui::AXActionData* action) {
  AutomationInternalPerformActionFunction::Result validation_error_result;
  validation_error_result.validation_success = false;
  AutomationInternalPerformActionFunction::Result success_result;
  success_result.validation_success = true;
  action->target_tree_id = tree_id;
  action->source_extension_id = extension_id;
  action->target_node_id = automation_node_id;
  action->request_id = request_id;
  api::automation::ActionType action_type =
      api::automation::ParseActionType(action_type_string);
  switch (action_type) {
    case api::automation::ACTION_TYPE_BLUR:
      action->action = ax::mojom::Action::kBlur;
      break;
    case api::automation::ACTION_TYPE_CLEARACCESSIBILITYFOCUS:
      action->action = ax::mojom::Action::kClearAccessibilityFocus;
      break;
    case api::automation::ACTION_TYPE_DECREMENT:
      action->action = ax::mojom::Action::kDecrement;
      break;
    case api::automation::ACTION_TYPE_DODEFAULT:
      action->action = ax::mojom::Action::kDoDefault;
      break;
    case api::automation::ACTION_TYPE_INCREMENT:
      action->action = ax::mojom::Action::kIncrement;
      break;
    case api::automation::ACTION_TYPE_FOCUS:
      action->action = ax::mojom::Action::kFocus;
      break;
    case api::automation::ACTION_TYPE_GETIMAGEDATA: {
      api::automation_internal::GetImageDataParams get_image_data_params;
      bool result = api::automation_internal::GetImageDataParams::Populate(
          additional_properties, &get_image_data_params);
      if (!result) {
        return validation_error_result;
      }
      action->action = ax::mojom::Action::kGetImageData;
      action->target_rect = gfx::Rect(0, 0, get_image_data_params.max_width,
                                      get_image_data_params.max_height);
      break;
    }
    case api::automation::ACTION_TYPE_HITTEST: {
      api::automation_internal::HitTestParams hit_test_params;
      bool result = api::automation_internal::HitTestParams::Populate(
          additional_properties, &hit_test_params);
      if (!result) {
        return validation_error_result;
      }
      action->action = ax::mojom::Action::kHitTest;
      action->target_point = gfx::Point(hit_test_params.x, hit_test_params.y);
      action->hit_test_event_to_fire = ui::ParseAXEnum<ax::mojom::Event>(
          hit_test_params.event_to_fire.c_str());
      if (action->hit_test_event_to_fire == ax::mojom::Event::kNone) {
        return success_result;
      }
      break;
    }
    case api::automation::ACTION_TYPE_LOADINLINETEXTBOXES:
      action->action = ax::mojom::Action::kLoadInlineTextBoxes;
      break;
    case api::automation::ACTION_TYPE_SETACCESSIBILITYFOCUS:
      action->action = ax::mojom::Action::kSetAccessibilityFocus;
      break;
    case api::automation::ACTION_TYPE_SCROLLTOMAKEVISIBLE:
      action->action = ax::mojom::Action::kScrollToMakeVisible;
      action->horizontal_scroll_alignment =
          ax::mojom::ScrollAlignment::kScrollAlignmentCenter;
      action->vertical_scroll_alignment =
          ax::mojom::ScrollAlignment::kScrollAlignmentCenter;
      action->scroll_behavior =
          ax::mojom::ScrollBehavior::kDoNotScrollIfVisible;
      break;
    case api::automation::ACTION_TYPE_SCROLLBACKWARD:
      action->action = ax::mojom::Action::kScrollBackward;
      break;
    case api::automation::ACTION_TYPE_SCROLLFORWARD:
      action->action = ax::mojom::Action::kScrollForward;
      break;
    case api::automation::ACTION_TYPE_SCROLLUP:
      action->action = ax::mojom::Action::kScrollUp;
      break;
    case api::automation::ACTION_TYPE_SCROLLDOWN:
      action->action = ax::mojom::Action::kScrollDown;
      break;
    case api::automation::ACTION_TYPE_SCROLLLEFT:
      action->action = ax::mojom::Action::kScrollLeft;
      break;
    case api::automation::ACTION_TYPE_SCROLLRIGHT:
      action->action = ax::mojom::Action::kScrollRight;
      break;
    case api::automation::ACTION_TYPE_SETSELECTION: {
      api::automation_internal::SetSelectionParams selection_params;
      bool result = api::automation_internal::SetSelectionParams::Populate(
          additional_properties, &selection_params);
      if (!result) {
        return validation_error_result;
      }
      action->anchor_node_id = automation_node_id;
      action->anchor_offset = selection_params.anchor_offset;
      action->focus_node_id = selection_params.focus_node_id;
      action->focus_offset = selection_params.focus_offset;
      action->action = ax::mojom::Action::kSetSelection;
      break;
    }
    case api::automation::ACTION_TYPE_SHOWCONTEXTMENU: {
      action->action = ax::mojom::Action::kShowContextMenu;
      break;
    }
    case api::automation::
        ACTION_TYPE_SETSEQUENTIALFOCUSNAVIGATIONSTARTINGPOINT: {
      action->action =
          ax::mojom::Action::kSetSequentialFocusNavigationStartingPoint;
      break;
    }
    case api::automation::ACTION_TYPE_CUSTOMACTION: {
      api::automation_internal::PerformCustomActionParams
          perform_custom_action_params;
      bool result =
          api::automation_internal::PerformCustomActionParams::Populate(
              additional_properties, &perform_custom_action_params);
      if (!result) {
        return validation_error_result;
      }
      action->action = ax::mojom::Action::kCustomAction;
      action->custom_action_id = perform_custom_action_params.custom_action_id;
      break;
    }
    case api::automation::ACTION_TYPE_REPLACESELECTEDTEXT: {
      api::automation_internal::ReplaceSelectedTextParams
          replace_selected_text_params;
      bool result =
          api::automation_internal::ReplaceSelectedTextParams::Populate(
              additional_properties, &replace_selected_text_params);
      if (!result) {
        return validation_error_result;
      }
      action->action = ax::mojom::Action::kReplaceSelectedText;
      action->value = replace_selected_text_params.value;
      break;
    }
    case api::automation::ACTION_TYPE_SETVALUE: {
      api::automation_internal::SetValueParams set_value_params;
      bool result = api::automation_internal::SetValueParams::Populate(
          additional_properties, &set_value_params);
      if (!result) {
        return validation_error_result;
      }
      action->action = ax::mojom::Action::kSetValue;
      action->value = set_value_params.value;
      break;
    }
    case api::automation::ACTION_TYPE_SCROLLTOPOINT: {
      api::automation_internal::ScrollToPointParams scroll_to_point_params;
      bool result = api::automation_internal::ScrollToPointParams::Populate(
          additional_properties, &scroll_to_point_params);
      if (!result) {
        return validation_error_result;
      }
      action->action = ax::mojom::Action::kScrollToPoint;
      action->target_point =
          gfx::Point(scroll_to_point_params.x, scroll_to_point_params.y);
      break;
    }
    case api::automation::ACTION_TYPE_SETSCROLLOFFSET: {
      api::automation_internal::SetScrollOffsetParams set_scroll_offset_params;
      bool result = api::automation_internal::SetScrollOffsetParams::Populate(
          additional_properties, &set_scroll_offset_params);
      if (!result) {
        return validation_error_result;
      }
      action->action = ax::mojom::Action::kSetScrollOffset;
      action->target_point =
          gfx::Point(set_scroll_offset_params.x, set_scroll_offset_params.y);
      break;
    }
    case api::automation::ACTION_TYPE_GETTEXTLOCATION: {
      api::automation_internal::GetTextLocationDataParams
          get_text_location_params;
      bool result =
          api::automation_internal::GetTextLocationDataParams::Populate(
              additional_properties, &get_text_location_params);
      if (!result) {
        return validation_error_result;
      }
      action->action = ax::mojom::Action::kGetTextLocation;
      action->start_index = get_text_location_params.start_index;
      action->end_index = get_text_location_params.end_index;
      break;
    }
    case api::automation::ACTION_TYPE_SHOWTOOLTIP:
      action->action = ax::mojom::Action::kShowTooltip;
      break;
    case api::automation::ACTION_TYPE_HIDETOOLTIP:
      action->action = ax::mojom::Action::kHideTooltip;
      break;
    case api::automation::ACTION_TYPE_COLLAPSE:
      action->action = ax::mojom::Action::kCollapse;
      break;
    case api::automation::ACTION_TYPE_EXPAND:
      action->action = ax::mojom::Action::kExpand;
      break;
    case api::automation::ACTION_TYPE_ANNOTATEPAGEIMAGES:
    case api::automation::ACTION_TYPE_SIGNALENDOFTEST:
    case api::automation::ACTION_TYPE_INTERNALINVALIDATETREE:
    case api::automation::ACTION_TYPE_NONE:
      break;
  }
  return success_result;
}

AutomationInternalPerformActionFunction::Result::Result() = default;
AutomationInternalPerformActionFunction::Result::Result(const Result&) =
    default;
AutomationInternalPerformActionFunction::Result::~Result() = default;

AutomationInternalPerformActionFunction::Result
AutomationInternalPerformActionFunction::PerformAction(
    const ui::AXTreeID& tree_id,
    int32_t automation_node_id,
    const std::string& action_type,
    int request_id,
    const base::DictionaryValue& additional_properties,
    const std::string& extension_id,
    const Extension* extension,
    const AutomationInfo* automation_info) {
  Result result;
  result.validation_success = true;

  ui::AXActionHandlerRegistry* registry =
      ui::AXActionHandlerRegistry::GetInstance();

  // The ash implementation of crosapi registers itself as an action observer.
  // This allows it to forward actions in parallel to Lacros.
  registry->PerformAction(tree_id, automation_node_id, action_type, request_id,
                          additional_properties);

  ui::AXActionHandlerBase* action_handler = registry->GetActionHandler(tree_id);
  if (action_handler) {
    // Handle an AXActionHandler with a rfh first. Some actions require a rfh ->
    // web contents and this api requires web contents to perform a permissions
    // check.
    content::RenderFrameHost* rfh =
        content::RenderFrameHost::FromAXTreeID(tree_id);
    if (rfh) {
      content::WebContents* contents =
          content::WebContents::FromRenderFrameHost(rfh);
      if (extension && automation_info) {
        if (!ExtensionsAPIClient::Get()
                 ->GetAutomationInternalApiDelegate()
                 ->CanRequestAutomation(extension, automation_info, contents)) {
          result.automation_error = kCannotRequestAutomationOnPage;
          return result;
        }
      } else {
        // If |extension| is nullptr, then Lacros is receiving a crosapi request
        // from ash to perform an action. We make the assumption this this is
        // allowed.
        // TODO(https://crbug.com/1185764): Confirm whether this assumption is
        // valid.
      }

      // Handle internal actions.
      api::automation_internal::ActionTypePrivate internal_action_type =
          api::automation_internal::ParseActionTypePrivate(action_type);
      content::MediaSession* session = content::MediaSession::Get(contents);
      switch (internal_action_type) {
        case api::automation_internal::ACTION_TYPE_PRIVATE_STARTDUCKINGMEDIA:
          session->StartDucking();
          return result;
        case api::automation_internal::ACTION_TYPE_PRIVATE_STOPDUCKINGMEDIA:
          session->StopDucking();
          return result;
        case api::automation_internal::ACTION_TYPE_PRIVATE_RESUMEMEDIA:
          session->Resume(content::MediaSession::SuspendType::kSystem);
          return result;
        case api::automation_internal::ACTION_TYPE_PRIVATE_SUSPENDMEDIA:
          session->Suspend(content::MediaSession::SuspendType::kSystem);
          return result;
        case api::automation_internal::ACTION_TYPE_PRIVATE_NONE:
          // Not a private action.
          break;
      }
    }

    ui::AXActionData data;
    Result result = ConvertToAXActionData(
        tree_id, automation_node_id, action_type, request_id,
        additional_properties, extension_id, &data);
    action_handler->PerformAction(data);
    return result;
  }
  result.automation_error = "Unable to perform action on unknown tree.";
  return result;
}

ExtensionFunction::ResponseAction
AutomationInternalPerformActionFunction::Run() {
  const AutomationInfo* automation_info = AutomationInfo::Get(extension());
  EXTENSION_FUNCTION_VALIDATE(automation_info && automation_info->interact);

  using api::automation_internal::PerformAction::Params;
  std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int* request_id_ptr = params->args.request_id.get();
  int request_id = request_id_ptr ? *request_id_ptr : -1;
  Result result =
      PerformAction(ui::AXTreeID::FromString(params->args.tree_id),
                    params->args.automation_node_id, params->args.action_type,
                    request_id, params->opt_args.additional_properties,
                    extension_id(), extension(), automation_info);
  if (!result.validation_success) {
    // This macro has a built in |return|.
    EXTENSION_FUNCTION_VALIDATE(false);
  }

  if (result.automation_error) {
    return RespondNow(Error(result.automation_error.value()));
  } else {
    return RespondNow(NoArguments());
  }
}

ExtensionFunction::ResponseAction
AutomationInternalEnableDesktopFunction::Run() {
#if defined(USE_AURA)
  const AutomationInfo* automation_info = AutomationInfo::Get(extension());
  if (!automation_info || !automation_info->desktop)
    return RespondNow(Error("desktop permission must be requested"));

  // This gets removed when the extension process dies.
  AutomationEventRouter::GetInstance()->RegisterListenerWithDesktopPermission(
      extension_id(), source_process_id());

  AutomationInternalApiDelegate* automation_api_delegate =
      ExtensionsAPIClient::Get()->GetAutomationInternalApiDelegate();
  automation_api_delegate->EnableDesktop();
  ui::AXTreeID ax_tree_id = automation_api_delegate->GetAXTreeID();
  return RespondNow(
      ArgumentList(api::automation_internal::EnableDesktop::Results::Create(
          ax_tree_id.ToString())));
#else
  return RespondNow(Error("getDesktop is unsupported by this platform"));
#endif  // defined(USE_AURA)
}

// static
int AutomationInternalQuerySelectorFunction::query_request_id_counter_ = 0;

ExtensionFunction::ResponseAction
AutomationInternalQuerySelectorFunction::Run() {
  const AutomationInfo* automation_info = AutomationInfo::Get(extension());
  EXTENSION_FUNCTION_VALIDATE(automation_info);

  using api::automation_internal::QuerySelector::Params;
  std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  content::RenderFrameHost* rfh = content::RenderFrameHost::FromAXTreeID(
      ui::AXTreeID::FromString(params->args.tree_id));
  if (!rfh) {
    return RespondNow(
        Error("domQuerySelector query sent on non-web or destroyed tree."));
  }

  content::WebContents* contents =
      content::WebContents::FromRenderFrameHost(rfh);

  int request_id = query_request_id_counter_++;
  std::u16string selector = base::UTF8ToUTF16(params->args.selector);

  // QuerySelectorHandler handles IPCs and deletes itself on completion.
  new QuerySelectorHandler(
      contents, request_id, params->args.automation_node_id, selector,
      base::BindOnce(&AutomationInternalQuerySelectorFunction::OnResponse,
                     this));

  return RespondLater();
}

void AutomationInternalQuerySelectorFunction::OnResponse(
    const std::string& error,
    int result_acc_obj_id) {
  if (!error.empty()) {
    Respond(Error(error));
    return;
  }

  Respond(OneArgument(base::Value(result_acc_obj_id)));
}

}  // namespace extensions
