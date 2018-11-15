// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webrunner/browser/frame_impl.h"

#include <zircon/syscalls.h>

#include <string>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/message_port_provider.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/wm/core/base_focus_rules.h"
#include "url/gurl.h"
#include "webrunner/browser/context_impl.h"
#include "webrunner/browser/message_port_impl.h"
#include "webrunner/browser/vmo_util.h"

namespace webrunner {

namespace {

// Layout manager that allows only one child window and stretches it to fill the
// parent.
class LayoutManagerImpl : public aura::LayoutManager {
 public:
  LayoutManagerImpl() = default;
  ~LayoutManagerImpl() override = default;

  // aura::LayoutManager.
  void OnWindowResized() override {
    // Resize the child to match the size of the parent
    if (child_) {
      SetChildBoundsDirect(child_,
                           gfx::Rect(child_->parent()->bounds().size()));
    }
  }
  void OnWindowAddedToLayout(aura::Window* child) override {
    DCHECK(!child_);
    child_ = child;
    SetChildBoundsDirect(child_, gfx::Rect(child_->parent()->bounds().size()));
  }

  void OnWillRemoveWindowFromLayout(aura::Window* child) override {
    DCHECK_EQ(child, child_);
    child_ = nullptr;
  }

  void OnWindowRemovedFromLayout(aura::Window* child) override {}
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override {}
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override {}

 private:
  aura::Window* child_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LayoutManagerImpl);
};

chromium::web::NavigationEntry ConvertContentNavigationEntry(
    content::NavigationEntry* entry) {
  DCHECK(entry);
  chromium::web::NavigationEntry converted;
  converted.title = base::UTF16ToUTF8(entry->GetTitleForDisplay());
  converted.url = entry->GetURL().spec();
  converted.is_error =
      entry->GetPageType() == content::PageType::PAGE_TYPE_ERROR;
  return converted;
}

// Computes the observable differences between |entry_1| and |entry_2|.
// Returns true if they are different, |false| if their observable fields are
// identical.
bool ComputeNavigationEvent(const chromium::web::NavigationEntry& old_entry,
                            const chromium::web::NavigationEntry& new_entry,
                            chromium::web::NavigationEvent* computed_event) {
  DCHECK(computed_event);

  bool is_changed = false;

  if (old_entry.title != new_entry.title) {
    is_changed = true;
    computed_event->title = new_entry.title;
  }

  if (old_entry.url != new_entry.url) {
    is_changed = true;
    computed_event->url = new_entry.url;
  }

  computed_event->is_error = new_entry.is_error;
  if (old_entry.is_error != new_entry.is_error)
    is_changed = true;

  return is_changed;
}

class FrameFocusRules : public wm::BaseFocusRules {
 public:
  FrameFocusRules() = default;
  ~FrameFocusRules() override = default;

  // wm::BaseFocusRules implementation.
  bool SupportsChildActivation(aura::Window*) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FrameFocusRules);
};

bool FrameFocusRules::SupportsChildActivation(aura::Window*) const {
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

}  // namespace

FrameImpl::FrameImpl(std::unique_ptr<content::WebContents> web_contents,
                     ContextImpl* context,
                     fidl::InterfaceRequest<chromium::web::Frame> frame_request)
    : web_contents_(std::move(web_contents)),
      focus_controller_(
          std::make_unique<wm::FocusController>(new FrameFocusRules)),
      context_(context),
      binding_(this, std::move(frame_request)) {
  web_contents_->SetDelegate(this);
  Observe(web_contents_.get());
  binding_.set_error_handler(
      [this](zx_status_t status) { context_->DestroyFrame(this); });
}

FrameImpl::~FrameImpl() {
  if (window_tree_host_) {
    aura::client::SetFocusClient(root_window(), nullptr);
    wm::SetActivationClient(root_window(), nullptr);
    web_contents_->ClosePage();
    window_tree_host_->Hide();
    window_tree_host_->compositor()->SetVisible(false);

    // Allows posted focus events to process before the FocusController
    // is torn down.
    content::BrowserThread::DeleteSoon(content::BrowserThread::UI, FROM_HERE,
                                       focus_controller_.release());
  }
}

zx::unowned_channel FrameImpl::GetBindingChannelForTest() const {
  return zx::unowned_channel(binding_.channel());
}

bool FrameImpl::ShouldCreateWebContents(
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
    content::SessionStorageNamespace* session_storage_namespace) {
  DCHECK_EQ(web_contents, web_contents_.get());

  // Prevent any child WebContents (popup windows, tabs, etc.) from spawning.
  // TODO(crbug.com/888131): Implement support for popup windows.
  NOTIMPLEMENTED() << "Ignored popup window request for URL: "
                   << target_url.spec();

  return false;
}

void FrameImpl::CreateView(
    fidl::InterfaceRequest<fuchsia::ui::viewsv1token::ViewOwner> view_owner,
    fidl::InterfaceRequest<fuchsia::sys::ServiceProvider> services) {
  // Cast |view_owner| request to a eventpair.
  CreateView2(zx::eventpair(view_owner.TakeChannel().release()),
              std::move(services), nullptr);
}

void FrameImpl::CreateView2(
    zx::eventpair view_token,
    fidl::InterfaceRequest<fuchsia::sys::ServiceProvider> incoming_services,
    fidl::InterfaceHandle<fuchsia::sys::ServiceProvider> outgoing_services) {
  ui::PlatformWindowInitProperties properties;
  properties.view_token = std::move(view_token);

  window_tree_host_ =
      std::make_unique<aura::WindowTreeHostPlatform>(std::move(properties));
  window_tree_host_->InitHost();

  aura::client::SetFocusClient(root_window(), focus_controller_.get());
  wm::SetActivationClient(root_window(), focus_controller_.get());

  // Add hooks which automatically set the focus state when input events are
  // received.
  root_window()->AddPreTargetHandler(focus_controller_.get());

  // Track child windows for enforcement of window management policies and
  // propagate window manager events to them (e.g. window resizing).
  root_window()->SetLayoutManager(new LayoutManagerImpl());

  root_window()->AddChild(web_contents_->GetNativeView());
  web_contents_->GetNativeView()->Show();
  window_tree_host_->Show();
}

void FrameImpl::GetNavigationController(
    fidl::InterfaceRequest<chromium::web::NavigationController> controller) {
  controller_bindings_.AddBinding(this, std::move(controller));
}

void FrameImpl::SetJavaScriptLogLevel(chromium::web::LogLevel level) {
  log_level_ = level;
}

void FrameImpl::LoadUrl(fidl::StringPtr url,
                        std::unique_ptr<chromium::web::LoadUrlParams> params) {
  GURL validated_url(*url);
  if (!validated_url.is_valid()) {
    DLOG(WARNING) << "Invalid URL: " << *url;
    return;
  }

  content::NavigationController::LoadURLParams params_converted(validated_url);

  if (validated_url.scheme() == url::kDataScheme)
    params_converted.load_type = content::NavigationController::LOAD_TYPE_DATA;

  params_converted.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  web_contents_->GetController().LoadURLWithParams(params_converted);
}

void FrameImpl::GoBack() {
  if (web_contents_->GetController().CanGoBack())
    web_contents_->GetController().GoBack();
}

void FrameImpl::GoForward() {
  if (web_contents_->GetController().CanGoForward())
    web_contents_->GetController().GoForward();
}

void FrameImpl::Stop() {
  web_contents_->Stop();
}

void FrameImpl::Reload(chromium::web::ReloadType type) {
  content::ReloadType internal_reload_type;
  switch (type) {
    case chromium::web::ReloadType::PARTIAL_CACHE:
      internal_reload_type = content::ReloadType::NORMAL;
      break;
    case chromium::web::ReloadType::NO_CACHE:
      internal_reload_type = content::ReloadType::BYPASSING_CACHE;
      break;
  }
  web_contents_->GetController().Reload(internal_reload_type, false);
}

void FrameImpl::GetVisibleEntry(GetVisibleEntryCallback callback) {
  content::NavigationEntry* entry =
      web_contents_->GetController().GetVisibleEntry();
  if (!entry) {
    callback(nullptr);
    return;
  }

  chromium::web::NavigationEntry output = ConvertContentNavigationEntry(entry);
  callback(std::make_unique<chromium::web::NavigationEntry>(std::move(output)));
}

void FrameImpl::SetNavigationEventObserver(
    fidl::InterfaceHandle<chromium::web::NavigationEventObserver> observer) {
  // Reset the event buffer state.
  waiting_for_navigation_event_ack_ = false;
  cached_navigation_state_ = {};
  pending_navigation_event_ = {};
  pending_navigation_event_is_dirty_ = false;

  if (observer) {
    navigation_observer_.Bind(std::move(observer));
    navigation_observer_.set_error_handler(
        [this](zx_status_t status) { SetNavigationEventObserver(nullptr); });
  } else {
    navigation_observer_.Unbind();
  }
}

void FrameImpl::ExecuteJavaScript(fidl::VectorPtr<::fidl::StringPtr> origins,
                                  fuchsia::mem::Buffer script,
                                  chromium::web::ExecuteMode mode,
                                  ExecuteJavaScriptCallback callback) {
  if (!context_->IsJavaScriptInjectionAllowed()) {
    callback(false);
    return;
  }

  if (origins->empty()) {
    callback(false);
    return;
  }

  base::string16 script_utf16;
  if (!ReadUTF8FromVMOAsUTF16(script, &script_utf16)) {
    DLOG(WARNING) << "Ignored non-UTF8 script passed to ExecuteJavaScript().";
    callback(false);
    return;
  }

  std::vector<std::string> origins_strings;
  for (const auto& origin : *origins)
    origins_strings.push_back(origin);

  if (mode == chromium::web::ExecuteMode::IMMEDIATE_ONCE) {
    if (IsOriginWhitelisted(web_contents_->GetLastCommittedURL(),
                            origins_strings))
      web_contents_->GetMainFrame()->ExecuteJavaScript(script_utf16);
  } else {
    before_load_scripts_.emplace_back(origins_strings, script_utf16);
  }
  callback(true);
}

void FrameImpl::DidFinishLoad(content::RenderFrameHost* render_frame_host,
                              const GURL& validated_url) {
  if (web_contents_->GetMainFrame() != render_frame_host) {
    return;
  }

  chromium::web::NavigationEntry current_navigation_state =
      ConvertContentNavigationEntry(
          web_contents_->GetController().GetVisibleEntry());
  pending_navigation_event_is_dirty_ |=
      ComputeNavigationEvent(cached_navigation_state_, current_navigation_state,
                             &pending_navigation_event_);
  cached_navigation_state_ = std::move(current_navigation_state);

  MaybeSendNavigationEvent();
}

void FrameImpl::MaybeSendNavigationEvent() {
  if (!navigation_observer_)
    return;

  if (!pending_navigation_event_is_dirty_ ||
      waiting_for_navigation_event_ack_) {
    return;
  }

  pending_navigation_event_is_dirty_ = false;
  waiting_for_navigation_event_ack_ = true;

  // Send the event to the observer and, upon acknowledgement, revisit this
  // function to send another update.
  navigation_observer_->OnNavigationStateChanged(
      std::move(pending_navigation_event_), [this]() {
        waiting_for_navigation_event_ack_ = false;
        MaybeSendNavigationEvent();
      });
}

void FrameImpl::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  if (before_load_scripts_.empty())
    return;

  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument() || navigation_handle->IsErrorPage())
    return;

  mojom::OnLoadScriptInjectorAssociatedPtr before_load_script_injector;
  navigation_handle->GetRenderFrameHost()
      ->GetRemoteAssociatedInterfaces()
      ->GetInterface(&before_load_script_injector);

  // Provision the renderer's ScriptInjector with the scripts scoped to this
  // page's origin.
  before_load_script_injector->ClearOnLoadScripts();
  for (const OriginScopedScript& script : before_load_scripts_) {
    if (IsOriginWhitelisted(navigation_handle->GetURL(), script.origins)) {
      before_load_script_injector->AddOnLoadScript(script.script);
    }
  }
}

bool FrameImpl::DidAddMessageToConsole(content::WebContents* source,
                                       int32_t level,
                                       const base::string16& message,
                                       int32_t line_no,
                                       const base::string16& source_id) {
  if (static_cast<std::underlying_type<chromium::web::LogLevel>::type>(
          log_level_) > static_cast<uint32_t>(level)) {
    return false;
  }

  std::string message_formatted =
      base::StringPrintf("%s:%d : %s", base::UTF16ToUTF8(source_id).data(),
                         line_no, base::UTF16ToUTF8(message).data());
  switch (level) {
    case static_cast<std::underlying_type<chromium::web::LogLevel>::type>(
        chromium::web::LogLevel::DEBUG):
      LOG(INFO) << "debug:" << message;
      break;
    case static_cast<std::underlying_type<chromium::web::LogLevel>::type>(
        chromium::web::LogLevel::INFO):
      LOG(INFO) << "info:" << message;
      break;
    case static_cast<std::underlying_type<chromium::web::LogLevel>::type>(
        chromium::web::LogLevel::WARN):
      LOG(WARNING) << "warn:" << message;
      break;
    case static_cast<std::underlying_type<chromium::web::LogLevel>::type>(
        chromium::web::LogLevel::ERROR):
      LOG(ERROR) << "error:" << message;
      break;
    default:
      DLOG(WARNING) << "Unknown log level: " << level;
      return false;
  }

  return true;
}

FrameImpl::OriginScopedScript::OriginScopedScript(
    std::vector<std::string> origins,
    base::string16 script)
    : origins(std::move(origins)), script(script) {}

FrameImpl::OriginScopedScript::~OriginScopedScript() = default;

void FrameImpl::PostMessage(chromium::web::WebMessage message,
                            fidl::StringPtr target_origin,
                            PostMessageCallback callback) {
  constexpr char kWildcardOrigin[] = "*";

  if (target_origin->empty()) {
    callback(false);
    return;
  }

  base::Optional<base::string16> target_origin_utf16;
  if (*target_origin != kWildcardOrigin)
    target_origin_utf16 = base::UTF8ToUTF16(*target_origin);

  base::string16 data_utf16;
  if (!ReadUTF8FromVMOAsUTF16(message.data, &data_utf16)) {
    DLOG(WARNING) << "PostMessage() rejected non-UTF8 |message.data|.";
    callback(false);
    return;
  }

  // Include outgoing MessagePorts in the message.
  std::vector<mojo::ScopedMessagePipeHandle> message_ports;
  if (message.outgoing_transfer) {
    if (!message.outgoing_transfer->is_message_port()) {
      DLOG(WARNING) << "|outgoing_transfer| is not a MessagePort.";
      callback(false);
      return;
    }

    mojo::ScopedMessagePipeHandle port = MessagePortImpl::FromFidl(
        std::move(message.outgoing_transfer->message_port()));
    if (!port) {
      callback(false);
      return;
    }
    message_ports.push_back(std::move(port));
  }

  content::MessagePortProvider::PostMessageToFrame(
      web_contents_.get(), base::string16(), target_origin_utf16,
      std::move(data_utf16), std::move(message_ports));
  callback(true);
}

}  // namespace webrunner
