// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/frame_impl.h"

#include <fuchsia/ui/gfx/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>
#include <limits>

#include "base/bind_helpers.h"
#include "base/fuchsia/default_context.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/json/json_writer.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/message_port_provider.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/renderer_preferences_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/was_activated_option.mojom.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/base/message_port.h"
#include "fuchsia/engine/browser/accessibility_bridge.h"
#include "fuchsia/engine/browser/context_impl.h"
#include "fuchsia/engine/browser/event_filter.h"
#include "fuchsia/engine/browser/frame_layout_manager.h"
#include "fuchsia/engine/browser/frame_window_tree_host.h"
#include "fuchsia/engine/browser/media_player_impl.h"
#include "fuchsia/engine/browser/web_engine_devtools_controller.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/logging/logging_utils.h"
#include "third_party/blink/public/common/messaging/web_message_port.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "ui/aura/window.h"
#include "ui/gfx/switches.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/wm/core/base_focus_rules.h"
#include "url/gurl.h"

namespace {

// logging::LogSeverity does not define a value to disable logging so set a
// value much lower than logging::LOG_VERBOSE here.
const logging::LogSeverity kLogSeverityNone =
    std::numeric_limits<logging::LogSeverity>::min();

// Simulated screen bounds to use when headless rendering is enabled.
constexpr gfx::Size kHeadlessWindowSize = {1, 1};

// Simulated screen bounds to use when testing the SemanticsManager.
constexpr gfx::Size kSemanticsTestingWindowSize = {720, 640};

// Used for attaching popup-related metadata to a WebContents.
constexpr char kPopupCreationInfo[] = "popup-creation-info";
class PopupFrameCreationInfoUserData : public base::SupportsUserData::Data {
 public:
  fuchsia::web::PopupFrameCreationInfo info;
};

class FrameFocusRules : public wm::BaseFocusRules {
 public:
  FrameFocusRules() = default;
  ~FrameFocusRules() override = default;

  // wm::BaseFocusRules implementation.
  bool SupportsChildActivation(const aura::Window*) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FrameFocusRules);
};

bool FrameFocusRules::SupportsChildActivation(const aura::Window*) const {
  // TODO(crbug.com/878439): Return a result based on window properties such as
  // visibility.
  return true;
}

bool IsOriginWhitelisted(const GURL& url,
                         const std::vector<std::string>& allowed_origins) {
  constexpr const char kWildcard[] = "*";

  for (const std::string& origin : allowed_origins) {
    if (origin == kWildcard)
      return true;

    GURL origin_url(origin);
    if (!origin_url.is_valid()) {
      DLOG(WARNING) << "Ignored invalid origin spec for whitelisting: "
                    << origin;
      continue;
    }

    if (origin_url != url.GetOrigin())
      continue;

    // TODO(crbug.com/893236): Add handling for nonstandard origins
    // (e.g. data: URIs).

    return true;
  }
  return false;
}

logging::LogSeverity ConsoleLogLevelToLoggingSeverity(
    fuchsia::web::ConsoleLogLevel level) {
  switch (level) {
    case fuchsia::web::ConsoleLogLevel::NONE:
      return kLogSeverityNone;
    case fuchsia::web::ConsoleLogLevel::DEBUG:
      return logging::LOG_VERBOSE;
    case fuchsia::web::ConsoleLogLevel::INFO:
      return logging::LOG_INFO;
    case fuchsia::web::ConsoleLogLevel::WARN:
      return logging::LOG_WARNING;
    case fuchsia::web::ConsoleLogLevel::ERROR:
      return logging::LOG_ERROR;
  }
  NOTREACHED()
      << "Unknown log level: "
      << static_cast<std::underlying_type<fuchsia::web::ConsoleLogLevel>::type>(
             level);
}

bool IsHeadless() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kHeadless);
}

using FrameImplMap =
    base::small_map<std::map<content::WebContents*, FrameImpl*>>;

FrameImplMap& WebContentsToFrameImplMap() {
  static base::NoDestructor<FrameImplMap> frame_impl_map;
  return *frame_impl_map;
}

content::PermissionType FidlPermissionTypeToContentPermissionType(
    fuchsia::web::PermissionType fidl_type) {
  switch (fidl_type) {
    case fuchsia::web::PermissionType::MICROPHONE:
      return content::PermissionType::AUDIO_CAPTURE;
    case fuchsia::web::PermissionType::CAMERA:
      return content::PermissionType::VIDEO_CAPTURE;
    case fuchsia::web::PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      return content::PermissionType::PROTECTED_MEDIA_IDENTIFIER;
    case fuchsia::web::PermissionType::PERSISTENT_STORAGE:
      return content::PermissionType::DURABLE_STORAGE;
  }
}

// Permission request callback for FrameImpl::RequestMediaAccessPermission.
void HandleMediaPermissionsRequestResult(
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const std::vector<blink::mojom::PermissionStatus>& result) {
  blink::MediaStreamDevices devices;

  int result_pos = 0;

  if (request.audio_type ==
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
    if (result[result_pos] == blink::mojom::PermissionStatus::GRANTED) {
      devices.push_back(blink::MediaStreamDevice(
          request.audio_type, request.requested_audio_device_id,
          /*name=*/""));
    }
    result_pos++;
  }

  if (request.video_type ==
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
    if (result[result_pos] == blink::mojom::PermissionStatus::GRANTED) {
      devices.push_back(blink::MediaStreamDevice(
          request.video_type, request.requested_video_device_id,
          /*name=*/""));
    }
  }

  std::move(callback).Run(
      devices,
      devices.empty() ? blink::mojom::MediaStreamRequestResult::NO_HARDWARE
                      : blink::mojom::MediaStreamRequestResult::OK,
      nullptr);
}

base::Optional<url::Origin> ParseAndValidateWebOrigin(
    const std::string& origin_str) {
  GURL origin_url(origin_str);
  if (!origin_url.username().empty() || !origin_url.password().empty() ||
      !origin_url.query().empty() || !origin_url.ref().empty()) {
    return base::nullopt;
  }

  if (!origin_url.path().empty() && origin_url.path() != "/")
    return base::nullopt;

  auto origin = url::Origin::Create(origin_url);
  if (origin.opaque())
    return base::nullopt;

  return origin;
}

}  // namespace

// static
FrameImpl* FrameImpl::FromRenderFrameHost(
    content::RenderFrameHost* render_frame_host) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents)
    return nullptr;

  auto& map = WebContentsToFrameImplMap();
  auto it = map.find(web_contents);
  if (it == map.end())
    return nullptr;
  return it->second;
}

FrameImpl::FrameImpl(std::unique_ptr<content::WebContents> web_contents,
                     ContextImpl* context,
                     fidl::InterfaceRequest<fuchsia::web::Frame> frame_request)
    : web_contents_(std::move(web_contents)),
      context_(context),
      navigation_controller_(web_contents_.get()),
      log_level_(kLogSeverityNone),
      url_request_rewrite_rules_manager_(web_contents_.get()),
      binding_(this, std::move(frame_request)),
      media_blocker_(web_contents_.get()) {
  DCHECK(!WebContentsToFrameImplMap()[web_contents_.get()]);
  WebContentsToFrameImplMap()[web_contents_.get()] = this;

  web_contents_->SetDelegate(this);
  Observe(web_contents_.get());

  binding_.set_error_handler([this](zx_status_t status) {
    ZX_LOG_IF(ERROR, status != ZX_ERR_PEER_CLOSED, status)
        << " Frame disconnected.";
    context_->DestroyFrame(this);
  });

  content::UpdateFontRendererPreferencesFromSystemSettings(
      web_contents_->GetMutableRendererPrefs());
}

FrameImpl::~FrameImpl() {
  DestroyWindowTreeHost();
  context_->devtools_controller()->OnFrameDestroyed(web_contents_.get());

  auto& map = WebContentsToFrameImplMap();
  auto it = WebContentsToFrameImplMap().find(web_contents_.get());
  DCHECK(it != map.end() && it->second == this);
  map.erase(it);
}

zx::unowned_channel FrameImpl::GetBindingChannelForTest() const {
  return zx::unowned_channel(binding_.channel());
}

FrameImpl::OriginScopedScript::OriginScopedScript() = default;

FrameImpl::OriginScopedScript::OriginScopedScript(
    std::vector<std::string> origins,
    base::ReadOnlySharedMemoryRegion script)
    : origins_(std::move(origins)), script_(std::move(script)) {}

FrameImpl::OriginScopedScript& FrameImpl::OriginScopedScript::operator=(
    FrameImpl::OriginScopedScript&& other) {
  origins_ = std::move(other.origins_);
  script_ = std::move(other.script_);
  return *this;
}

FrameImpl::OriginScopedScript::~OriginScopedScript() = default;

aura::Window* FrameImpl::root_window() const {
  return window_tree_host_->window();
}

void FrameImpl::ExecuteJavaScriptInternal(std::vector<std::string> origins,
                                          fuchsia::mem::Buffer script,
                                          ExecuteJavaScriptCallback callback,
                                          bool need_result) {
  fuchsia::web::Frame_ExecuteJavaScript_Result result;
  if (!context_->IsJavaScriptInjectionAllowed()) {
    result.set_err(fuchsia::web::FrameError::INTERNAL_ERROR);
    callback(std::move(result));
    return;
  }

  if (!IsOriginWhitelisted(web_contents_->GetLastCommittedURL(), origins)) {
    result.set_err(fuchsia::web::FrameError::INVALID_ORIGIN);
    callback(std::move(result));
    return;
  }

  base::string16 script_utf16;
  if (!cr_fuchsia::ReadUTF8FromVMOAsUTF16(script, &script_utf16)) {
    result.set_err(fuchsia::web::FrameError::BUFFER_NOT_UTF8);
    callback(std::move(result));
    return;
  }

  content::RenderFrameHost::JavaScriptResultCallback result_callback;
  if (need_result) {
    result_callback = base::BindOnce(
        [](ExecuteJavaScriptCallback callback, base::Value result_value) {
          fuchsia::web::Frame_ExecuteJavaScript_Result result;

          std::string result_json;
          if (!base::JSONWriter::Write(result_value, &result_json)) {
            result.set_err(fuchsia::web::FrameError::INTERNAL_ERROR);
            callback(std::move(result));
            return;
          }

          fuchsia::web::Frame_ExecuteJavaScript_Response response;
          response.result = cr_fuchsia::MemBufferFromString(
              std::move(result_json), "cr-execute-js-response");
          result.set_response(std::move(response));
          callback(std::move(result));
        },
        std::move(callback));
  }

  web_contents_->GetMainFrame()->ExecuteJavaScript(script_utf16,
                                                   std::move(result_callback));

  if (!need_result) {
    // If no result is required then invoke callback() immediately.
    fuchsia::web::Frame_ExecuteJavaScript_Result result;
    result.set_response(fuchsia::web::Frame_ExecuteJavaScript_Response());
    callback(std::move(result));
  }
}

bool FrameImpl::IsWebContentsCreationOverridden(
    content::SiteInstance* source_site_instance,
    content::mojom::WindowContainerType window_container_type,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url) {
  // Specify a generous upper bound for unacknowledged popup windows, so that we
  // can catch bad client behavior while not interfering with normal operation.
  constexpr size_t kMaxPendingWebContentsCount = 10;

  if (!popup_listener_)
    return true;

  if (pending_popups_.size() >= kMaxPendingWebContentsCount) {
    // The content is producing popups faster than the embedder can process
    // them. Drop the popups so as to prevent resource exhaustion.
    LOG(WARNING) << "Too many pending popups, ignoring request.";

    // Don't produce a WebContents for this popup.
    return true;
  }

  return false;
}

void FrameImpl::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_rect,
    bool user_gesture,
    bool* was_blocked) {
  DCHECK_EQ(source, web_contents_.get());

  // TODO(crbug.com/995395): Add window disposition to the FIDL interface.
  switch (disposition) {
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
    case WindowOpenDisposition::NEW_BACKGROUND_TAB:
    case WindowOpenDisposition::NEW_POPUP:
    case WindowOpenDisposition::NEW_WINDOW: {
      if (url_request_rewrite_rules_manager_.GetCachedRules()) {
        // There is no support for URL request rules rewriting with popups.
        *was_blocked = true;
        return;
      }

      PopupFrameCreationInfoUserData* popup_creation_info =
          reinterpret_cast<PopupFrameCreationInfoUserData*>(
              new_contents->GetUserData(kPopupCreationInfo));
      popup_creation_info->info.set_initiated_by_user(user_gesture);
      pending_popups_.emplace_back(std::move(new_contents));
      MaybeSendPopup();
      return;
    }

    // These kinds of windows don't produce Frames.
    case WindowOpenDisposition::CURRENT_TAB:
    case WindowOpenDisposition::SINGLETON_TAB:
    case WindowOpenDisposition::SAVE_TO_DISK:
    case WindowOpenDisposition::OFF_THE_RECORD:
    case WindowOpenDisposition::IGNORE_ACTION:
    case WindowOpenDisposition::SWITCH_TO_TAB:
    case WindowOpenDisposition::UNKNOWN:
      NOTIMPLEMENTED() << "Dropped new web contents (disposition: "
                       << static_cast<int>(disposition) << ")";
      return;
  }
}

void FrameImpl::WebContentsCreated(content::WebContents* source_contents,
                                   int opener_render_process_id,
                                   int opener_render_frame_id,
                                   const std::string& frame_name,
                                   const GURL& target_url,
                                   content::WebContents* new_contents) {
  auto creation_info = std::make_unique<PopupFrameCreationInfoUserData>();
  creation_info->info.set_initial_url(target_url.spec());
  new_contents->SetUserData(kPopupCreationInfo, std::move(creation_info));
}

void FrameImpl::MaybeSendPopup() {
  if (!popup_listener_)
    return;

  if (popup_ack_outstanding_ || pending_popups_.empty())
    return;

  std::unique_ptr<content::WebContents> popup =
      std::move(pending_popups_.front());
  pending_popups_.pop_front();

  fuchsia::web::PopupFrameCreationInfo creation_info =
      std::move(reinterpret_cast<PopupFrameCreationInfoUserData*>(
                    popup->GetUserData(kPopupCreationInfo))
                    ->info);

  // The PopupFrameCreationInfo won't be needed anymore, so clear it out.
  popup->SetUserData(kPopupCreationInfo, nullptr);

  popup_listener_->OnPopupFrameCreated(
      context_->CreateFrameForPopupWebContents(std::move(popup)),
      std::move(creation_info), [this] {
        popup_ack_outstanding_ = false;
        MaybeSendPopup();
      });
  popup_ack_outstanding_ = true;
}

void FrameImpl::DestroyWindowTreeHost() {
  if (!window_tree_host_)
    return;

  aura::client::SetFocusClient(root_window(), nullptr);
  wm::SetActivationClient(root_window(), nullptr);
  root_window()->RemovePreTargetHandler(&event_filter_);
  root_window()->RemovePreTargetHandler(focus_controller_.get());
  web_contents_->GetNativeView()->Hide();
  window_tree_host_->Hide();
  window_tree_host_->compositor()->SetVisible(false);
  window_tree_host_.reset();

  // Allows posted focus events to process before the FocusController is torn
  // down.
  base::DeleteSoon(FROM_HERE, {content::BrowserThread::UI},
                   std::move(focus_controller_));
}

void FrameImpl::CloseAndDestroyFrame(zx_status_t error) {
  DCHECK(binding_.is_bound());
  binding_.Close(error);
  context_->DestroyFrame(this);
}

void FrameImpl::OnPopupListenerDisconnected(zx_status_t status) {
  ZX_LOG_IF(WARNING, status != ZX_ERR_PEER_CLOSED, status)
      << "Popup listener disconnected.";
  pending_popups_.clear();
}

void FrameImpl::OnMediaPlayerDisconnect() {
  media_player_ = nullptr;
}

void FrameImpl::CreateView(fuchsia::ui::views::ViewToken view_token) {
  if (IsHeadless()) {
    LOG(WARNING) << "CreateView() called on a HEADLESS Context.";
    CloseAndDestroyFrame(ZX_ERR_INVALID_ARGS);
    return;
  }

  // If a View to this Frame is already active then disconnect it.
  DestroyWindowTreeHost();

  InitWindowTreeHost(std::move(view_token));

  fuchsia::accessibility::semantics::SemanticsManagerPtr semantics_manager =
      base::fuchsia::ComponentContextForCurrentProcess()
          ->svc()
          ->Connect<fuchsia::accessibility::semantics::SemanticsManager>();
  accessibility_bridge_ = std::make_unique<AccessibilityBridge>(
      std::move(semantics_manager), window_tree_host_->CreateViewRef(),
      web_contents_.get());
}

void FrameImpl::GetMediaPlayer(
    fidl::InterfaceRequest<fuchsia::media::sessions2::Player> player) {
  media_player_ = std::make_unique<MediaPlayerImpl>(
      content::MediaSession::Get(web_contents_.get()), std::move(player),
      base::BindOnce(&FrameImpl::OnMediaPlayerDisconnect,
                     base::Unretained(this)));
}

void FrameImpl::GetNavigationController(
    fidl::InterfaceRequest<fuchsia::web::NavigationController> controller) {
  navigation_controller_.AddBinding(std::move(controller));
}

void FrameImpl::ExecuteJavaScript(std::vector<std::string> origins,
                                  fuchsia::mem::Buffer script,
                                  ExecuteJavaScriptCallback callback) {
  ExecuteJavaScriptInternal(std::move(origins), std::move(script),
                            std::move(callback), true);
}

void FrameImpl::ExecuteJavaScriptNoResult(
    std::vector<std::string> origins,
    fuchsia::mem::Buffer script,
    ExecuteJavaScriptNoResultCallback callback) {
  ExecuteJavaScriptInternal(
      std::move(origins), std::move(script),
      [callback = std::move(callback)](
          fuchsia::web::Frame_ExecuteJavaScript_Result result_with_value) {
        fuchsia::web::Frame_ExecuteJavaScriptNoResult_Result result;
        if (result_with_value.is_err()) {
          result.set_err(result_with_value.err());
        } else {
          result.set_response(
              fuchsia::web::Frame_ExecuteJavaScriptNoResult_Response());
        }
        callback(std::move(result));
      },
      false);
}

void FrameImpl::AddBeforeLoadJavaScript(
    uint64_t id,
    std::vector<std::string> origins,
    fuchsia::mem::Buffer script,
    AddBeforeLoadJavaScriptCallback callback) {
  fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result;
  if (!context_->IsJavaScriptInjectionAllowed()) {
    result.set_err(fuchsia::web::FrameError::INTERNAL_ERROR);
    callback(std::move(result));
    return;
  }

  // Convert the script to UTF8 and store it as a shared memory buffer, so that
  // it can be efficiently shared with multiple renderer processes.
  base::string16 script_utf16;
  if (!cr_fuchsia::ReadUTF8FromVMOAsUTF16(script, &script_utf16)) {
    result.set_err(fuchsia::web::FrameError::BUFFER_NOT_UTF8);
    callback(std::move(result));
    return;
  }

  // Create a read-only VMO from |script|.
  fuchsia::mem::Buffer script_buffer =
      cr_fuchsia::MemBufferFromString16(script_utf16, "cr-before-load-js");

  // Wrap the VMO into a read-only shared-memory container that Mojo can work
  // with.
  base::subtle::PlatformSharedMemoryRegion script_region =
      base::subtle::PlatformSharedMemoryRegion::Take(
          std::move(script_buffer.vmo),
          base::subtle::PlatformSharedMemoryRegion::Mode::kWritable,
          script_buffer.size, base::UnguessableToken::Create());
  script_region.ConvertToReadOnly();
  auto script_region_mojo =
      base::ReadOnlySharedMemoryRegion::Deserialize(std::move(script_region));

  // If there is no script with the identifier |id|, then create a place for it
  // at the end of the injection sequence.
  if (before_load_scripts_.find(id) == before_load_scripts_.end())
    before_load_scripts_order_.push_back(id);

  before_load_scripts_[id] =
      OriginScopedScript(origins, std::move(script_region_mojo));

  result.set_response(fuchsia::web::Frame_AddBeforeLoadJavaScript_Response());
  callback(std::move(result));
}

void FrameImpl::RemoveBeforeLoadJavaScript(uint64_t id) {
  before_load_scripts_.erase(id);

  for (auto script_id_iter = before_load_scripts_order_.begin();
       script_id_iter != before_load_scripts_order_.end(); ++script_id_iter) {
    if (*script_id_iter == id) {
      before_load_scripts_order_.erase(script_id_iter);
      return;
    }
  }
}

void FrameImpl::PostMessage(std::string origin,
                            fuchsia::web::WebMessage message,
                            PostMessageCallback callback) {
  constexpr char kWildcardOrigin[] = "*";

  fuchsia::web::Frame_PostMessage_Result result;
  if (origin.empty()) {
    result.set_err(fuchsia::web::FrameError::INVALID_ORIGIN);
    callback(std::move(result));
    return;
  }

  if (!message.has_data()) {
    result.set_err(fuchsia::web::FrameError::NO_DATA_IN_MESSAGE);
    callback(std::move(result));
    return;
  }

  base::Optional<base::string16> origin_utf16;
  if (origin != kWildcardOrigin)
    origin_utf16 = base::UTF8ToUTF16(origin);

  base::string16 data_utf16;
  if (!cr_fuchsia::ReadUTF8FromVMOAsUTF16(message.data(), &data_utf16)) {
    result.set_err(fuchsia::web::FrameError::BUFFER_NOT_UTF8);
    callback(std::move(result));
    return;
  }

  // Include outgoing MessagePorts in the message.
  std::vector<blink::WebMessagePort> message_ports;
  if (message.has_outgoing_transfer()) {
    // Verify that all the Transferables are valid before we start allocating
    // resources to them.
    for (const fuchsia::web::OutgoingTransferable& outgoing :
         message.outgoing_transfer()) {
      if (!outgoing.is_message_port()) {
        result.set_err(fuchsia::web::FrameError::INTERNAL_ERROR);
        callback(std::move(result));
        return;
      }
    }

    for (fuchsia::web::OutgoingTransferable& outgoing :
         *message.mutable_outgoing_transfer()) {
      blink::WebMessagePort blink_port = cr_fuchsia::BlinkMessagePortFromFidl(
          std::move(outgoing.message_port()));
      if (!blink_port.IsValid()) {
        result.set_err(fuchsia::web::FrameError::INTERNAL_ERROR);
        callback(std::move(result));
        return;
      }
      message_ports.push_back(std::move(blink_port));
    }
  }

  content::MessagePortProvider::PostMessageToFrame(
      web_contents_.get(), base::string16(), origin_utf16,
      std::move(data_utf16), std::move(message_ports));
  result.set_response(fuchsia::web::Frame_PostMessage_Response());
  callback(std::move(result));
}

void FrameImpl::SetNavigationEventListener(
    fidl::InterfaceHandle<fuchsia::web::NavigationEventListener> listener) {
  navigation_controller_.SetEventListener(std::move(listener));
}

void FrameImpl::SetJavaScriptLogLevel(fuchsia::web::ConsoleLogLevel level) {
  log_level_ = ConsoleLogLevelToLoggingSeverity(level);
}

void FrameImpl::ConfigureInputTypes(fuchsia::web::InputTypes types,
                                    fuchsia::web::AllowInputState allow) {
  event_filter_.ConfigureInputTypes(types, allow);
}

void FrameImpl::SetPopupFrameCreationListener(
    fidl::InterfaceHandle<fuchsia::web::PopupFrameCreationListener> listener) {
  popup_listener_ = listener.Bind();
  popup_listener_.set_error_handler(
      fit::bind_member(this, &FrameImpl::OnPopupListenerDisconnected));
}

void FrameImpl::SetUrlRequestRewriteRules(
    std::vector<fuchsia::web::UrlRequestRewriteRule> rules,
    SetUrlRequestRewriteRulesCallback callback) {
  zx_status_t error = url_request_rewrite_rules_manager_.OnRulesUpdated(
      std::move(rules), std::move(callback));
  if (error != ZX_OK) {
    CloseAndDestroyFrame(error);
    return;
  }
}

void FrameImpl::EnableHeadlessRendering() {
  if (!IsHeadless()) {
    LOG(ERROR) << "EnableHeadlessRendering() on non-HEADLESS Context.";
    CloseAndDestroyFrame(ZX_ERR_INVALID_ARGS);
    return;
  }

  InitWindowTreeHost(fuchsia::ui::views::ViewToken());

  gfx::Rect bounds(kHeadlessWindowSize);
  if (semantics_manager_for_test_) {
    accessibility_bridge_ = std::make_unique<AccessibilityBridge>(
        std::move(semantics_manager_for_test_),
        window_tree_host_->CreateViewRef(), web_contents_.get());

    // Set bounds for testing hit testing.
    bounds.set_size(kSemanticsTestingWindowSize);
  }

  window_tree_host_->SetBoundsInPixels(bounds);
}

void FrameImpl::DisableHeadlessRendering() {
  if (!IsHeadless()) {
    LOG(ERROR)
        << "Attempted to disable headless rendering on non-HEADLESS Context.";
    CloseAndDestroyFrame(ZX_ERR_INVALID_ARGS);
    return;
  }

  DestroyWindowTreeHost();
}

void FrameImpl::InitWindowTreeHost(fuchsia::ui::views::ViewToken view_token) {
  DCHECK(!window_tree_host_);

  window_tree_host_ = std::make_unique<FrameWindowTreeHost>(
      std::move(view_token), web_contents_.get());
  window_tree_host_->InitHost();
  root_window()->AddPreTargetHandler(&event_filter_);

  // Add hooks which automatically set the focus state when input events are
  // received.
  focus_controller_ =
      std::make_unique<wm::FocusController>(new FrameFocusRules);
  root_window()->AddPreTargetHandler(focus_controller_.get());
  aura::client::SetFocusClient(root_window(), focus_controller_.get());

  wm::SetActivationClient(root_window(), focus_controller_.get());

  layout_manager_ = new FrameLayoutManager;
  root_window()->SetLayoutManager(layout_manager_);  // Transfers ownership.
  if (!render_size_override_.IsEmpty())
    layout_manager_->ForceContentDimensions(render_size_override_);

  root_window()->AddChild(web_contents_->GetNativeView());
  web_contents_->GetNativeView()->Show();

  window_tree_host_->Show();
}

void FrameImpl::SetMediaSessionId(uint64_t session_id) {
  media_session_id_ = session_id;
}

void FrameImpl::MediaStartedPlaying(const MediaPlayerInfo& video_type,
                                    const content::MediaPlayerId& id) {
  base::RecordComputedAction("MediaPlay");
}

void FrameImpl::MediaStoppedPlaying(
    const MediaPlayerInfo& video_type,
    const content::MediaPlayerId& id,
    WebContentsObserver::MediaStoppedReason reason) {
  base::RecordComputedAction("MediaPause");
}

void FrameImpl::GetPrivateMemorySize(GetPrivateMemorySizeCallback callback) {
  if (!web_contents_->GetMainFrame()->GetProcess()->IsReady()) {
    // Renderer process is not yet started.
    callback(0);
    return;
  }

  zx_info_task_stats_t task_stats;
  zx_status_t status = zx_object_get_info(
      web_contents_->GetMainFrame()->GetProcess()->GetProcess().Handle(),
      ZX_INFO_TASK_STATS, &task_stats, sizeof(task_stats), nullptr, nullptr);

  if (status != ZX_OK) {
    // Fail gracefully by returning zero.
    ZX_LOG(WARNING, status) << "zx_object_get_info(ZX_INFO_TASK_STATS)";
    callback(0);
    return;
  }

  callback(task_stats.mem_private_bytes);
}

void FrameImpl::ForceContentDimensions(
    std::unique_ptr<fuchsia::ui::gfx::vec2> web_dips) {
  if (!web_dips) {
    render_size_override_ = {};
    if (layout_manager_)
      layout_manager_->ForceContentDimensions({});
    return;
  }

  gfx::Size web_dips_converted(web_dips->x, web_dips->y);
  if (web_dips_converted.IsEmpty()) {
    LOG(WARNING) << "Rejecting zero-area size for ForceContentDimensions().";
    CloseAndDestroyFrame(ZX_ERR_INVALID_ARGS);
    return;
  }

  render_size_override_ = web_dips_converted;
  if (layout_manager_)
    layout_manager_->ForceContentDimensions(web_dips_converted);
}

void FrameImpl::SetPermissionState(
    fuchsia::web::PermissionDescriptor fidl_permission,
    std::string web_origin_string,
    fuchsia::web::PermissionState fidl_state) {
  if (!fidl_permission.has_type()) {
    LOG(ERROR) << "PermissionDescriptor.type is not specified in "
                  "SetPermissionState().";
    CloseAndDestroyFrame(ZX_ERR_INVALID_ARGS);
    return;
  }

  auto web_origin = ParseAndValidateWebOrigin(web_origin_string);
  if (!web_origin) {
    LOG(ERROR) << "SetPermissionState() called with invalid web_origin: "
               << web_origin_string;
    CloseAndDestroyFrame(ZX_ERR_INVALID_ARGS);
    return;
  }

  content::PermissionType type =
      FidlPermissionTypeToContentPermissionType(fidl_permission.type());

  blink::mojom::PermissionStatus state =
      (fidl_state == fuchsia::web::PermissionState::GRANTED)
          ? blink::mojom::PermissionStatus::GRANTED
          : blink::mojom::PermissionStatus::DENIED;

  permission_controller_.SetPermissionState(type, web_origin.value(), state);
}

void FrameImpl::CloseContents(content::WebContents* source) {
  DCHECK_EQ(source, web_contents_.get());
  context_->DestroyFrame(this);
}

void FrameImpl::SetBlockMediaLoading(bool blocked) {
  media_blocker_.BlockMediaLoading(blocked);
}

bool FrameImpl::DidAddMessageToConsole(
    content::WebContents* source,
    blink::mojom::ConsoleMessageLevel log_level,
    const base::string16& message,
    int32_t line_no,
    const base::string16& source_id) {
  logging::LogSeverity log_severity =
      blink::ConsoleMessageLevelToLogSeverity(log_level);
  if (log_level_ > log_severity) {
    return false;
  }

  std::string formatted_message =
      base::StringPrintf("%s:%d : %s", base::UTF16ToUTF8(source_id).data(),
                         line_no, base::UTF16ToUTF8(message).data());
  switch (log_level) {
    case blink::mojom::ConsoleMessageLevel::kVerbose:
      LOG(INFO) << "debug:" << formatted_message;
      break;
    case blink::mojom::ConsoleMessageLevel::kInfo:
      LOG(INFO) << "info:" << formatted_message;
      break;
    case blink::mojom::ConsoleMessageLevel::kWarning:
      LOG(WARNING) << "warn:" << formatted_message;
      break;
    case blink::mojom::ConsoleMessageLevel::kError:
      LOG(ERROR) << "error:" << formatted_message;
      break;
    default:
      DLOG(WARNING) << "Unknown log level: " << log_severity;
      return false;
  }

  if (console_log_message_hook_)
    console_log_message_hook_.Run(formatted_message);

  return true;
}

void FrameImpl::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  DCHECK_EQ(web_contents_.get(), web_contents);

  std::vector<content::PermissionType> permissions;

  if (request.audio_type ==
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
    permissions.push_back(content::PermissionType::AUDIO_CAPTURE);
  } else if (request.audio_type != blink::mojom::MediaStreamType::NO_SERVICE) {
    std::move(callback).Run(
        blink::MediaStreamDevices(),
        blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED, nullptr);
    return;
  }

  if (request.video_type ==
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
    permissions.push_back(content::PermissionType::VIDEO_CAPTURE);
  } else if (request.video_type != blink::mojom::MediaStreamType::NO_SERVICE) {
    std::move(callback).Run(
        blink::MediaStreamDevices(),
        blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED, nullptr);
    return;
  }

  auto* render_frame_host = content::RenderFrameHost::FromID(
      request.render_process_id, request.render_frame_id);
  if (!render_frame_host) {
    std::move(callback).Run(
        blink::MediaStreamDevices(),
        blink::mojom::MediaStreamRequestResult::INVALID_STATE, nullptr);
    return;
  }

  auto* permission_controller =
      web_contents_->GetBrowserContext()->GetPermissionControllerDelegate();
  permission_controller->RequestPermissions(
      permissions, render_frame_host, request.security_origin,
      request.user_gesture,
      base::BindOnce(&HandleMediaPermissionsRequestResult, request,
                     std::move(callback)));
}

bool FrameImpl::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type) {
  content::PermissionType permission;
  switch (type) {
    case blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE:
      permission = content::PermissionType::AUDIO_CAPTURE;
      break;
    case blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE:
      permission = content::PermissionType::VIDEO_CAPTURE;
      break;
    default:
      NOTREACHED();
      return false;
  }
  auto* permission_controller =
      web_contents_->GetBrowserContext()->GetPermissionControllerDelegate();
  return permission_controller->GetPermissionStatusForFrame(
             permission, render_frame_host, security_origin) ==
         blink::mojom::PermissionStatus::GRANTED;
}

void FrameImpl::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  if (before_load_scripts_.empty())
    return;

  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument() || navigation_handle->IsErrorPage()) {
    return;
  }

  mojo::AssociatedRemote<mojom::OnLoadScriptInjector>
      before_load_script_injector;
  navigation_handle->GetRenderFrameHost()
      ->GetRemoteAssociatedInterfaces()
      ->GetInterface(&before_load_script_injector);

  // Provision the renderer's ScriptInjector with the scripts scoped to this
  // page's origin.
  before_load_script_injector->ClearOnLoadScripts();
  for (uint64_t script_id : before_load_scripts_order_) {
    const OriginScopedScript& script = before_load_scripts_[script_id];
    if (IsOriginWhitelisted(navigation_handle->GetURL(), script.origins())) {
      before_load_script_injector->AddOnLoadScript(
          mojo::WrapReadOnlySharedMemoryRegion(script.script().Duplicate()));
    }
  }
}

void FrameImpl::DidFinishLoad(content::RenderFrameHost* render_frame_host,
                              const GURL& validated_url) {
  context_->devtools_controller()->OnFrameLoaded(web_contents_.get());
}

void FrameImpl::RenderViewCreated(content::RenderViewHost* render_view_host) {
  render_view_host->GetWidget()->GetView()->SetBackgroundColor(
      SK_AlphaTRANSPARENT);
}

void FrameImpl::RenderViewReady() {
  web_contents_->GetRenderViewHost()
      ->GetWidget()
      ->GetView()
      ->SetBackgroundColor(SK_AlphaTRANSPARENT);

  // Setting the background color doesn't necessarily apply it right away, so
  // request a redraw if there is a view connected to this Frame.
  if (window_tree_host_)
    window_tree_host_->compositor()->ScheduleDraw();
}

void FrameImpl::DidFirstVisuallyNonEmptyPaint() {
  base::RecordComputedAction("AppFirstPaint");
}

void FrameImpl::ResourceLoadComplete(
    content::RenderFrameHost* render_frame_host,
    const content::GlobalRequestID& request_id,
    const blink::mojom::ResourceLoadInfo& resource_load_info) {
  int net_error = resource_load_info.net_error;
  if (net_error != net::OK) {
    base::RecordComputedAction(
        base::StringPrintf("WebEngine.ResourceRequestError:%d", net_error));
  }
}
