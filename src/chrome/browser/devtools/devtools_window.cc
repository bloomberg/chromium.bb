// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_window.h"

#include <algorithm>
#include <set>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"
#include "chrome/browser/devtools/devtools_eye_dropper.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/color_chooser.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/devtools_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/javascript_dialogs/app_modal_dialog_manager.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/zoom/page_zoom.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "net/base/escape.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/public_buildflags.h"
#include "ui/base/page_transition_types.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes.h"

using base::DictionaryValue;
using blink::WebInputEvent;
using content::BrowserThread;
using content::DevToolsAgentHost;
using content::WebContents;

namespace {

typedef std::vector<DevToolsWindow*> DevToolsWindows;
base::LazyInstance<DevToolsWindows>::Leaky g_devtools_window_instances =
    LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<std::vector<base::Callback<void(DevToolsWindow*)>>>::Leaky
    g_creation_callbacks = LAZY_INSTANCE_INITIALIZER;

static const char kKeyUpEventName[] = "keyup";
static const char kKeyDownEventName[] = "keydown";
static const char kDefaultFrontendURL[] =
    "devtools://devtools/bundled/devtools_app.html";
static const char kNodeFrontendURL[] =
    "devtools://devtools/bundled/node_app.html";
static const char kWorkerFrontendURL[] =
    "devtools://devtools/bundled/worker_app.html";
static const char kJSFrontendURL[] = "devtools://devtools/bundled/js_app.html";
static const char kFallbackFrontendURL[] =
    "devtools://devtools/bundled/inspector.html";

bool FindInspectedBrowserAndTabIndex(
    WebContents* inspected_web_contents, Browser** browser, int* tab) {
  if (!inspected_web_contents)
    return false;

  for (auto* b : *BrowserList::GetInstance()) {
    int tab_index =
        b->tab_strip_model()->GetIndexOfWebContents(inspected_web_contents);
    if (tab_index != TabStripModel::kNoTab) {
      *browser = b;
      *tab = tab_index;
      return true;
    }
  }
  return false;
}

void SetPreferencesFromJson(Profile* profile, const std::string& json) {
  base::Optional<base::Value> parsed = base::JSONReader::Read(json);
  if (!parsed || !parsed->is_dict())
    return;
  DictionaryPrefUpdate update(profile->GetPrefs(), prefs::kDevToolsPreferences);
  for (const auto& dict_value : parsed->DictItems()) {
    if (!dict_value.second.is_string())
      continue;
    update.Get()->SetKey(dict_value.first, std::move(dict_value.second));
  }
}

// DevToolsToolboxDelegate ----------------------------------------------------

class DevToolsToolboxDelegate
    : public content::WebContentsObserver,
      public content::WebContentsDelegate {
 public:
  DevToolsToolboxDelegate(
      WebContents* toolbox_contents,
      DevToolsWindow::ObserverWithAccessor* web_contents_observer);
  ~DevToolsToolboxDelegate() override;

  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  void WebContentsDestroyed() override;

 private:
  BrowserWindow* GetInspectedBrowserWindow();
  DevToolsWindow::ObserverWithAccessor* inspected_contents_observer_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsToolboxDelegate);
};

DevToolsToolboxDelegate::DevToolsToolboxDelegate(
    WebContents* toolbox_contents,
    DevToolsWindow::ObserverWithAccessor* web_contents_observer)
    : WebContentsObserver(toolbox_contents),
      inspected_contents_observer_(web_contents_observer) {
}

DevToolsToolboxDelegate::~DevToolsToolboxDelegate() {
}

content::WebContents* DevToolsToolboxDelegate::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  DCHECK(source == web_contents());
  if (!params.url.SchemeIs(content::kChromeDevToolsScheme))
    return NULL;
  source->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(params));
  return source;
}

content::KeyboardEventProcessingResult
DevToolsToolboxDelegate::PreHandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  BrowserWindow* window = GetInspectedBrowserWindow();
  if (window)
    return window->PreHandleKeyboardEvent(event);
  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}

bool DevToolsToolboxDelegate::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  if (event.windows_key_code == 0x08) {
    // Do not navigate back in history on Windows (http://crbug.com/74156).
    return false;
  }
  BrowserWindow* window = GetInspectedBrowserWindow();
  return window && window->HandleKeyboardEvent(event);
}

void DevToolsToolboxDelegate::WebContentsDestroyed() {
  delete this;
}

BrowserWindow* DevToolsToolboxDelegate::GetInspectedBrowserWindow() {
  WebContents* inspected_contents =
      inspected_contents_observer_->web_contents();
  if (!inspected_contents)
    return NULL;
  Browser* browser = NULL;
  int tab = 0;
  if (FindInspectedBrowserAndTabIndex(inspected_contents, &browser, &tab))
    return browser->window();
  return NULL;
}

// static
GURL DecorateFrontendURL(const GURL& base_url) {
  std::string frontend_url = base_url.spec();
  std::string url_string(
      frontend_url +
      ((frontend_url.find("?") == std::string::npos) ? "?" : "&") +
      "dockSide=undocked");  // TODO(dgozman): remove this support in M38.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(switches::kDevToolsFlags)) {
    url_string += "&" + command_line->GetSwitchValueASCII(
        switches::kDevToolsFlags);
  }

#if BUILDFLAG(DEBUG_DEVTOOLS)
  url_string += "&debugFrontend=true";
#endif  // BUILDFLAG(DEBUG_DEVTOOLS)

  return GURL(url_string);
}

}  // namespace

// DevToolsEventForwarder -----------------------------------------------------

class DevToolsEventForwarder {
 public:
  explicit DevToolsEventForwarder(DevToolsWindow* window)
     : devtools_window_(window) {}

  // Registers whitelisted shortcuts with the forwarder.
  // Only registered keys will be forwarded to the DevTools frontend.
  void SetWhitelistedShortcuts(const std::string& message);

  // Forwards a keyboard event to the DevTools frontend if it is whitelisted.
  // Returns |true| if the event has been forwarded, |false| otherwise.
  bool ForwardEvent(const content::NativeWebKeyboardEvent& event);

 private:
  static bool KeyWhitelistingAllowed(int key_code, int modifiers);
  static int CombineKeyCodeAndModifiers(int key_code, int modifiers);

  DevToolsWindow* devtools_window_;
  std::set<int> whitelisted_keys_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsEventForwarder);
};

void DevToolsEventForwarder::SetWhitelistedShortcuts(
    const std::string& message) {
  base::Optional<base::Value> parsed_message = base::JSONReader::Read(message);
  if (!parsed_message || !parsed_message->is_list())
    return;
  for (const auto& list_item : parsed_message->GetList()) {
    if (!list_item.is_dict())
      continue;
    int key_code = list_item.FindIntKey("keyCode").value_or(0);
    if (key_code == 0)
      continue;
    int modifiers = list_item.FindIntKey("modifiers").value_or(0);
    if (!KeyWhitelistingAllowed(key_code, modifiers)) {
      LOG(WARNING) << "Key whitelisting forbidden: "
                   << "(" << key_code << "," << modifiers << ")";
      continue;
    }
    whitelisted_keys_.insert(CombineKeyCodeAndModifiers(key_code, modifiers));
  }
}

bool DevToolsEventForwarder::ForwardEvent(
    const content::NativeWebKeyboardEvent& event) {
  std::string event_type;
  switch (event.GetType()) {
    case WebInputEvent::Type::kKeyDown:
    case WebInputEvent::Type::kRawKeyDown:
      event_type = kKeyDownEventName;
      break;
    case WebInputEvent::Type::kKeyUp:
      event_type = kKeyUpEventName;
      break;
    default:
      return false;
  }

  int key_code = ui::LocatedToNonLocatedKeyboardCode(
      static_cast<ui::KeyboardCode>(event.windows_key_code));
  int modifiers = event.GetModifiers() &
                  (WebInputEvent::kShiftKey | WebInputEvent::kControlKey |
                   WebInputEvent::kAltKey | WebInputEvent::kMetaKey);
  int key = CombineKeyCodeAndModifiers(key_code, modifiers);
  if (whitelisted_keys_.find(key) == whitelisted_keys_.end())
    return false;

  base::Value event_data(base::Value::Type::DICTIONARY);
  event_data.SetStringKey("type", event_type);
  event_data.SetStringKey("key", ui::KeycodeConverter::DomKeyToKeyString(
                                     static_cast<ui::DomKey>(event.dom_key)));
  event_data.SetStringKey("code",
                          ui::KeycodeConverter::DomCodeToCodeString(
                              static_cast<ui::DomCode>(event.dom_code)));
  event_data.SetIntKey("keyCode", key_code);
  event_data.SetIntKey("modifiers", modifiers);
  devtools_window_->bindings_->CallClientMethod(
      "DevToolsAPI", "keyEventUnhandled", event_data);
  return true;
}

int DevToolsEventForwarder::CombineKeyCodeAndModifiers(int key_code,
                                                       int modifiers) {
  return key_code | (modifiers << 16);
}

bool DevToolsEventForwarder::KeyWhitelistingAllowed(int key_code,
                                                    int modifiers) {
  return (ui::VKEY_F1 <= key_code && key_code <= ui::VKEY_F12) ||
      modifiers != 0;
}

void DevToolsWindow::OpenNodeFrontend() {
  DevToolsWindow::OpenNodeFrontendWindow(profile_);
}

// DevToolsWindow::ObserverWithAccessor -------------------------------

DevToolsWindow::ObserverWithAccessor::ObserverWithAccessor(
    WebContents* web_contents)
    : WebContentsObserver(web_contents) {
}

DevToolsWindow::ObserverWithAccessor::~ObserverWithAccessor() {
}

// DevToolsWindow::Throttle ------------------------------------------

class DevToolsWindow::Throttle : public content::NavigationThrottle {
 public:
  Throttle(content::NavigationHandle* navigation_handle,
           DevToolsWindow* devtools_window)
      : content::NavigationThrottle(navigation_handle),
        devtools_window_(devtools_window) {
    devtools_window_->throttle_ = this;
  }

  ~Throttle() override {
    if (devtools_window_)
      devtools_window_->throttle_ = nullptr;
  }

  // content::NavigationThrottle implementation:
  NavigationThrottle::ThrottleCheckResult WillStartRequest() override {
    return DEFER;
  }

  const char* GetNameForLogging() override { return "DevToolsWindowThrottle"; }

  void ResumeThrottle() {
    if (devtools_window_) {
      devtools_window_->throttle_ = nullptr;
      devtools_window_ = nullptr;
    }
    Resume();
  }

 private:
  DevToolsWindow* devtools_window_;

  DISALLOW_COPY_AND_ASSIGN(Throttle);
};

// DevToolsWindow -------------------------------------------------------------

const char DevToolsWindow::kDevToolsApp[] = "DevToolsApp";

// static
void DevToolsWindow::AddCreationCallbackForTest(
    const CreationCallback& callback) {
  g_creation_callbacks.Get().push_back(callback);
}

// static
void DevToolsWindow::RemoveCreationCallbackForTest(
    const CreationCallback& callback) {
  for (size_t i = 0; i < g_creation_callbacks.Get().size(); ++i) {
    if (g_creation_callbacks.Get().at(i) == callback) {
      g_creation_callbacks.Get().erase(g_creation_callbacks.Get().begin() + i);
      return;
    }
  }
}

DevToolsWindow::~DevToolsWindow() {
  if (throttle_)
    throttle_->ResumeThrottle();

  life_stage_ = kClosing;

  UpdateBrowserWindow();
  UpdateBrowserToolbar();

  owned_toolbox_web_contents_.reset();

  DevToolsWindows* instances = g_devtools_window_instances.Pointer();
  auto it(std::find(instances->begin(), instances->end(), this));
  DCHECK(it != instances->end());
  instances->erase(it);

  if (!close_callback_.is_null())
    std::move(close_callback_).Run();
  // Defer deletion of the main web contents, since we could get here
  // via RenderFrameHostImpl method that expects WebContents to live
  // for some time. See http://crbug.com/997299 for details.
  if (owned_main_web_contents_) {
    base::SequencedTaskRunnerHandle::Get()->DeleteSoon(
        FROM_HERE, std::move(owned_main_web_contents_));
  }
}

// static
void DevToolsWindow::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kDevToolsEditedFiles);
  registry->RegisterDictionaryPref(prefs::kDevToolsFileSystemPaths);
  registry->RegisterStringPref(prefs::kDevToolsAdbKey, std::string());

  registry->RegisterBooleanPref(prefs::kDevToolsDiscoverUsbDevicesEnabled,
                                true);
  registry->RegisterBooleanPref(prefs::kDevToolsPortForwardingEnabled, false);
  registry->RegisterBooleanPref(prefs::kDevToolsPortForwardingDefaultSet,
                                false);
  registry->RegisterDictionaryPref(prefs::kDevToolsPortForwardingConfig);
  registry->RegisterBooleanPref(prefs::kDevToolsDiscoverTCPTargetsEnabled,
                                true);
  registry->RegisterListPref(prefs::kDevToolsTCPDiscoveryConfig);
  registry->RegisterDictionaryPref(prefs::kDevToolsPreferences);
}

// static
content::WebContents* DevToolsWindow::GetInTabWebContents(
    WebContents* inspected_web_contents,
    DevToolsContentsResizingStrategy* out_strategy) {
  DevToolsWindow* window = GetInstanceForInspectedWebContents(
      inspected_web_contents);
  if (!window || window->life_stage_ == kClosing)
    return NULL;

  // Not yet loaded window is treated as docked, but we should not present it
  // until we decided on docking.
  bool is_docked_set = window->life_stage_ == kLoadCompleted ||
      window->life_stage_ == kIsDockedSet;
  if (!is_docked_set)
    return NULL;

  // Undocked window should have toolbox web contents.
  if (!window->is_docked_ && !window->toolbox_web_contents_)
    return NULL;

  if (out_strategy)
    out_strategy->CopyFrom(window->contents_resizing_strategy_);

  return window->is_docked_ ? window->main_web_contents_ :
      window->toolbox_web_contents_;
}

// static
DevToolsWindow* DevToolsWindow::GetInstanceForInspectedWebContents(
    WebContents* inspected_web_contents) {
  if (!inspected_web_contents || !g_devtools_window_instances.IsCreated())
    return NULL;
  DevToolsWindows* instances = g_devtools_window_instances.Pointer();
  for (auto it(instances->begin()); it != instances->end(); ++it) {
    if ((*it)->GetInspectedWebContents() == inspected_web_contents)
      return *it;
  }
  return NULL;
}

// static
bool DevToolsWindow::IsDevToolsWindow(content::WebContents* web_contents) {
  if (!web_contents || !g_devtools_window_instances.IsCreated())
    return false;
  DevToolsWindows* instances = g_devtools_window_instances.Pointer();
  for (auto it(instances->begin()); it != instances->end(); ++it) {
    if ((*it)->main_web_contents_ == web_contents ||
        (*it)->toolbox_web_contents_ == web_contents)
      return true;
  }
  return false;
}

// static
void DevToolsWindow::OpenDevToolsWindowForWorker(
    Profile* profile,
    const scoped_refptr<DevToolsAgentHost>& worker_agent) {
  DevToolsWindow* window = FindDevToolsWindow(worker_agent.get());
  if (!window) {
    base::RecordAction(base::UserMetricsAction("DevTools_InspectWorker"));
    window = Create(profile, nullptr, kFrontendWorker, std::string(), false, "",
                    "", worker_agent->IsAttached());
    if (!window)
      return;
    window->bindings_->AttachTo(worker_agent);
  }
  window->ScheduleShow(DevToolsToggleAction::Show());
}

// static
void DevToolsWindow::OpenDevToolsWindow(
    content::WebContents* inspected_web_contents) {
  ToggleDevToolsWindow(
        inspected_web_contents, true, DevToolsToggleAction::Show(), "");
}

// static
void DevToolsWindow::OpenDevToolsWindow(
    scoped_refptr<content::DevToolsAgentHost> agent_host,
    Profile* profile) {
  OpenDevToolsWindow(agent_host, profile, false /* use_bundled_frontend */);
}

// static
void DevToolsWindow::OpenDevToolsWindowWithBundledFrontend(
    scoped_refptr<content::DevToolsAgentHost> agent_host,
    Profile* profile) {
  OpenDevToolsWindow(agent_host, profile, true /* use_bundled_frontend */);
}

// static
void DevToolsWindow::OpenDevToolsWindow(
    scoped_refptr<content::DevToolsAgentHost> agent_host,
    Profile* profile,
    bool use_bundled_frontend) {
  if (!profile)
    profile = Profile::FromBrowserContext(agent_host->GetBrowserContext());

  if (!profile)
    return;

  std::string type = agent_host->GetType();

  bool is_worker = type == DevToolsAgentHost::kTypeServiceWorker ||
                   type == DevToolsAgentHost::kTypeSharedWorker;

  if (!agent_host->GetFrontendURL().empty()) {
    DevToolsWindow::OpenExternalFrontend(profile, agent_host->GetFrontendURL(),
                                         agent_host, use_bundled_frontend);
    return;
  }

  if (is_worker) {
    DevToolsWindow::OpenDevToolsWindowForWorker(profile, agent_host);
    return;
  }

  if (type == content::DevToolsAgentHost::kTypeFrame) {
    DevToolsWindow::OpenDevToolsWindowForFrame(profile, agent_host);
    return;
  }

  content::WebContents* web_contents = agent_host->GetWebContents();
  if (web_contents)
    DevToolsWindow::OpenDevToolsWindow(web_contents);
}

// static
void DevToolsWindow::OpenDevToolsWindow(
    content::WebContents* inspected_web_contents,
    const DevToolsToggleAction& action) {
  ToggleDevToolsWindow(inspected_web_contents, true, action, "");
}

// static
void DevToolsWindow::OpenDevToolsWindowForFrame(
    Profile* profile,
    const scoped_refptr<content::DevToolsAgentHost>& agent_host) {
  DevToolsWindow* window = FindDevToolsWindow(agent_host.get());
  if (!window) {
    window = DevToolsWindow::Create(profile, nullptr, kFrontendDefault,
                                    std::string(), false, std::string(),
                                    std::string(), agent_host->IsAttached());
    if (!window)
      return;
    window->bindings_->AttachTo(agent_host);
  }
  window->ScheduleShow(DevToolsToggleAction::Show());
}

// static
void DevToolsWindow::ToggleDevToolsWindow(
    Browser* browser,
    const DevToolsToggleAction& action) {
  if (action.type() == DevToolsToggleAction::kToggle &&
      browser->is_type_devtools()) {
    browser->tab_strip_model()->CloseAllTabs();
    return;
  }

  ToggleDevToolsWindow(
      browser->tab_strip_model()->GetActiveWebContents(),
      action.type() == DevToolsToggleAction::kInspect,
      action, "");
}

// static
void DevToolsWindow::OpenExternalFrontend(
    Profile* profile,
    const std::string& frontend_url,
    const scoped_refptr<content::DevToolsAgentHost>& agent_host,
    bool use_bundled_frontend) {
  DevToolsWindow* window = FindDevToolsWindow(agent_host.get());
  if (window) {
    window->ScheduleShow(DevToolsToggleAction::Show());
    return;
  }

  std::string type = agent_host->GetType();
  if (type == "node") {
    // Direct node targets will always open using ToT front-end.
    window = Create(profile, nullptr, kFrontendV8, std::string(), false,
                    std::string(), std::string(), agent_host->IsAttached());
  } else {
    bool is_worker = type == DevToolsAgentHost::kTypeServiceWorker ||
                     type == DevToolsAgentHost::kTypeSharedWorker;

    FrontendType frontend_type =
        is_worker ? kFrontendRemoteWorker : kFrontendRemote;
    std::string effective_frontend_url =
        use_bundled_frontend ? kFallbackFrontendURL
                             : DevToolsUI::GetProxyURL(frontend_url).spec();
    window =
        Create(profile, nullptr, frontend_type, effective_frontend_url, false,
               std::string(), std::string(), agent_host->IsAttached());
  }
  if (!window)
    return;
  window->bindings_->AttachTo(agent_host);
  window->close_on_detach_ = false;
  window->ScheduleShow(DevToolsToggleAction::Show());
}

// static
DevToolsWindow* DevToolsWindow::OpenNodeFrontendWindow(Profile* profile) {
  for (DevToolsWindow* window : g_devtools_window_instances.Get()) {
    if (window->frontend_type_ == kFrontendNode) {
      window->ActivateWindow();
      return window;
    }
  }

  DevToolsWindow* window =
      Create(profile, nullptr, kFrontendNode, std::string(), false,
             std::string(), std::string(), false);
  if (!window)
    return nullptr;
  window->bindings_->AttachTo(DevToolsAgentHost::CreateForDiscovery());
  window->ScheduleShow(DevToolsToggleAction::Show());
  return window;
}

// static
void DevToolsWindow::ToggleDevToolsWindow(
    content::WebContents* inspected_web_contents,
    bool force_open,
    const DevToolsToggleAction& action,
    const std::string& settings) {
  scoped_refptr<DevToolsAgentHost> agent(
      DevToolsAgentHost::GetOrCreateFor(inspected_web_contents));
  DevToolsWindow* window = FindDevToolsWindow(agent.get());
  bool do_open = force_open;
  if (!window) {
    Profile* profile = Profile::FromBrowserContext(
        inspected_web_contents->GetBrowserContext());
    base::RecordAction(base::UserMetricsAction("DevTools_InspectRenderer"));
    std::string panel;
    switch (action.type()) {
      case DevToolsToggleAction::kInspect:
      case DevToolsToggleAction::kShowElementsPanel:
        panel = "elements";
        break;
      case DevToolsToggleAction::kShowConsolePanel:
        panel = "console";
        break;
      case DevToolsToggleAction::kPauseInDebugger:
        panel = "sources";
        break;
      case DevToolsToggleAction::kShow:
      case DevToolsToggleAction::kToggle:
      case DevToolsToggleAction::kReveal:
      case DevToolsToggleAction::kNoOp:
        break;
    }
    window = Create(profile, inspected_web_contents, kFrontendDefault,
                    std::string(), true, settings, panel, agent->IsAttached());
    if (!window)
      return;
    window->bindings_->AttachTo(agent.get());
    do_open = true;
  }

  // Update toolbar to reflect DevTools changes.
  window->UpdateBrowserToolbar();

  // If window is docked and visible, we hide it on toggle. If window is
  // undocked, we show (activate) it.
  if (!window->is_docked_ || do_open)
    window->ScheduleShow(action);
  else
    window->CloseWindow();
}

// static
void DevToolsWindow::InspectElement(
    content::RenderFrameHost* inspected_frame_host,
    int x,
    int y) {
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(inspected_frame_host);
  scoped_refptr<DevToolsAgentHost> agent(
      DevToolsAgentHost::GetOrCreateFor(web_contents));
  agent->InspectElement(inspected_frame_host, x, y);
  bool should_measure_time = FindDevToolsWindow(agent.get()) == NULL;
  base::TimeTicks start_time = base::TimeTicks::Now();
  // TODO(loislo): we should initiate DevTools window opening from within
  // renderer. Otherwise, we still can hit a race condition here.
  OpenDevToolsWindow(web_contents, DevToolsToggleAction::ShowElementsPanel());
  DevToolsWindow* window = FindDevToolsWindow(agent.get());
  if (window && should_measure_time)
    window->inspect_element_start_time_ = start_time;
}

// static
std::unique_ptr<content::NavigationThrottle>
DevToolsWindow::MaybeCreateNavigationThrottle(
    content::NavigationHandle* handle) {
  WebContents* web_contents = handle->GetWebContents();
  if (!web_contents || !web_contents->HasOriginalOpener() ||
      web_contents->GetController().GetLastCommittedEntry()) {
    return nullptr;
  }

  WebContents* opener = WebContents::FromRenderFrameHost(
      handle->GetWebContents()->GetOriginalOpener());
  DevToolsWindow* window = GetInstanceForInspectedWebContents(opener);
  if (!window || !window->open_new_window_for_popups_ ||
      GetInstanceForInspectedWebContents(web_contents))
    return nullptr;

  DevToolsWindow::OpenDevToolsWindow(web_contents);
  window = GetInstanceForInspectedWebContents(web_contents);
  if (!window)
    return nullptr;

  return std::make_unique<Throttle>(handle, window);
}

void DevToolsWindow::UpdateInspectedWebContents(
    content::WebContents* new_web_contents) {
  inspected_contents_observer_ =
      std::make_unique<ObserverWithAccessor>(new_web_contents);
  bindings_->AttachTo(
      content::DevToolsAgentHost::GetOrCreateFor(new_web_contents));
  bindings_->CallClientMethod("DevToolsAPI", "reattachMainTarget");
}

void DevToolsWindow::ScheduleShow(const DevToolsToggleAction& action) {
  if (life_stage_ == kLoadCompleted) {
    Show(action);
    return;
  }

  // Action will be done only after load completed.
  action_on_load_ = action;

  if (!can_dock_) {
    // No harm to show always-undocked window right away.
    is_docked_ = false;
    Show(DevToolsToggleAction::Show());
  }
}

void DevToolsWindow::Show(const DevToolsToggleAction& action) {
  if (life_stage_ == kClosing)
    return;

  if (action.type() == DevToolsToggleAction::kNoOp)
    return;
  if (is_docked_) {
    DCHECK(can_dock_);
    Browser* inspected_browser = NULL;
    int inspected_tab_index = -1;
    FindInspectedBrowserAndTabIndex(GetInspectedWebContents(),
                                    &inspected_browser,
                                    &inspected_tab_index);
    DCHECK(inspected_browser);
    DCHECK_NE(-1, inspected_tab_index);

    RegisterModalDialogManager(inspected_browser);

    // Tell inspected browser to update splitter and switch to inspected panel.
    BrowserWindow* inspected_window = inspected_browser->window();
    main_web_contents_->SetDelegate(this);

    TabStripModel* tab_strip_model = inspected_browser->tab_strip_model();
    tab_strip_model->ActivateTabAt(inspected_tab_index,
                                   {TabStripModel::GestureType::kOther});

    inspected_window->UpdateDevTools();
    main_web_contents_->SetInitialFocus();
    inspected_window->Show();
    // On Aura, focusing once is not enough. Do it again.
    // Note that focusing only here but not before isn't enough either. We just
    // need to focus twice.
    main_web_contents_->SetInitialFocus();

    PrefsTabHelper::CreateForWebContents(main_web_contents_);
    main_web_contents_->SyncRendererPrefs();

    DoAction(action);
    return;
  }

  // Avoid consecutive window switching if the devtools window has been opened
  // and the Inspect Element shortcut is pressed in the inspected tab.
  bool should_show_window =
      !browser_ || (action.type() != DevToolsToggleAction::kInspect);

  if (!browser_)
    CreateDevToolsBrowser();

  RegisterModalDialogManager(browser_);

  if (should_show_window) {
    browser_->window()->Show();
    main_web_contents_->SetInitialFocus();
  }
  if (toolbox_web_contents_)
    UpdateBrowserWindow();

  DoAction(action);
}

// static
bool DevToolsWindow::HandleBeforeUnload(WebContents* frontend_contents,
    bool proceed, bool* proceed_to_fire_unload) {
  DevToolsWindow* window = AsDevToolsWindow(frontend_contents);
  if (!window)
    return false;
  if (!window->intercepted_page_beforeunload_)
    return false;
  window->BeforeUnloadFired(frontend_contents, proceed,
      proceed_to_fire_unload);
  return true;
}

// static
bool DevToolsWindow::InterceptPageBeforeUnload(WebContents* contents) {
  DevToolsWindow* window =
      DevToolsWindow::GetInstanceForInspectedWebContents(contents);
  if (!window || window->intercepted_page_beforeunload_)
    return false;

  // Not yet loaded frontend will not handle beforeunload.
  if (window->life_stage_ != kLoadCompleted)
    return false;

  window->intercepted_page_beforeunload_ = true;
  // Handle case of devtools inspecting another devtools instance by passing
  // the call up to the inspecting devtools instance.
  // TODO(chrisha): Make devtools handle |auto_cancel=false| unload handler
  // dispatches; otherwise, discarding queries can cause unload dialogs to
  // pop-up for tabs with an attached devtools.
  if (!DevToolsWindow::InterceptPageBeforeUnload(window->main_web_contents_)) {
    window->main_web_contents_->DispatchBeforeUnload(false /* auto_cancel */);
  }
  return true;
}

// static
bool DevToolsWindow::NeedsToInterceptBeforeUnload(
    WebContents* contents) {
  DevToolsWindow* window =
      DevToolsWindow::GetInstanceForInspectedWebContents(contents);
  return window && !window->intercepted_page_beforeunload_ &&
         window->life_stage_ == kLoadCompleted;
}

// static
bool DevToolsWindow::HasFiredBeforeUnloadEventForDevToolsBrowser(
    Browser* browser) {
  DCHECK(browser->is_type_devtools());
  // When FastUnloadController is used, devtools frontend will be detached
  // from the browser window at this point which means we've already fired
  // beforeunload.
  if (browser->tab_strip_model()->empty())
    return true;
  WebContents* contents =
      browser->tab_strip_model()->GetWebContentsAt(0);
  DevToolsWindow* window = AsDevToolsWindow(contents);
  if (!window)
    return false;
  return window->intercepted_page_beforeunload_;
}

// static
void DevToolsWindow::OnPageCloseCanceled(WebContents* contents) {
  DevToolsWindow* window =
      DevToolsWindow::GetInstanceForInspectedWebContents(contents);
  if (!window)
    return;
  window->intercepted_page_beforeunload_ = false;
  // Propagate to devtools opened on devtools if any.
  DevToolsWindow::OnPageCloseCanceled(window->main_web_contents_);
}

DevToolsWindow::DevToolsWindow(FrontendType frontend_type,
                               Profile* profile,
                               std::unique_ptr<WebContents> main_web_contents,
                               DevToolsUIBindings* bindings,
                               WebContents* inspected_web_contents,
                               bool can_dock)
    : frontend_type_(frontend_type),
      profile_(profile),
      main_web_contents_(main_web_contents.get()),
      toolbox_web_contents_(nullptr),
      bindings_(bindings),
      browser_(nullptr),
      is_docked_(true),
      owned_main_web_contents_(std::move(main_web_contents)),
      can_dock_(can_dock),
      close_on_detach_(true),
      // This initialization allows external front-end to work without changes.
      // We don't wait for docking call, but instead immediately show undocked.
      // Passing "dockSide=undocked" parameter ensures proper UI.
      life_stage_(can_dock ? kNotLoaded : kIsDockedSet),
      action_on_load_(DevToolsToggleAction::NoOp()),
      intercepted_page_beforeunload_(false),
      ready_for_test_(false) {
  // Set up delegate, so we get fully-functional window immediately.
  // It will not appear in UI though until |life_stage_ == kLoadCompleted|.
  main_web_contents_->SetDelegate(this);
  // Bindings take ownership over devtools as its delegate.
  bindings_->SetDelegate(this);
  // DevTools uses PageZoom::Zoom(), so main_web_contents_ requires a
  // ZoomController.
  zoom::ZoomController::CreateForWebContents(main_web_contents_);
  zoom::ZoomController::FromWebContents(main_web_contents_)
      ->SetShowsNotificationBubble(false);
  performance_manager::PerformanceManagerRegistry::GetInstance()
      ->CreatePageNodeForWebContents(main_web_contents_);

  g_devtools_window_instances.Get().push_back(this);

  // There is no inspected_web_contents in case of various workers.
  if (inspected_web_contents)
    inspected_contents_observer_.reset(
        new ObserverWithAccessor(inspected_web_contents));

  // Initialize docked page to be of the right size.
  if (can_dock_ && inspected_web_contents) {
    content::RenderWidgetHostView* inspected_view =
        inspected_web_contents->GetRenderWidgetHostView();
    if (inspected_view && main_web_contents_->GetRenderWidgetHostView()) {
      gfx::Size size = inspected_view->GetViewBounds().size();
      main_web_contents_->GetRenderWidgetHostView()->SetSize(size);
    }
  }

  event_forwarder_.reset(new DevToolsEventForwarder(this));

  // Tag the DevTools main WebContents with its TaskManager specific UserData
  // so that it shows up in the task manager.
  task_manager::WebContentsTags::CreateForDevToolsContents(main_web_contents_);

  std::vector<base::Callback<void(DevToolsWindow*)>> copy(
      g_creation_callbacks.Get());
  for (const auto& callback : copy)
    callback.Run(this);
}

// static
bool DevToolsWindow::AllowDevToolsFor(Profile* profile,
                                      content::WebContents* web_contents) {
  // Don't allow DevTools UI in kiosk mode, because the DevTools UI would be
  // broken there. See https://crbug.com/514551 for context.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode))
    return false;

  return ChromeDevToolsManagerDelegate::AllowInspection(profile, web_contents);
}

// static
DevToolsWindow* DevToolsWindow::Create(
    Profile* profile,
    content::WebContents* inspected_web_contents,
    FrontendType frontend_type,
    const std::string& frontend_url,
    bool can_dock,
    const std::string& settings,
    const std::string& panel,
    bool has_other_clients) {
  if (!AllowDevToolsFor(profile, inspected_web_contents))
    return nullptr;

  if (inspected_web_contents) {
    // Check for a place to dock.
    Browser* browser = nullptr;
    int tab;
    if (!FindInspectedBrowserAndTabIndex(inspected_web_contents, &browser,
                                         &tab) ||
        !browser->is_type_normal()) {
      can_dock = false;
    }
  }

  // Create WebContents with devtools.
  GURL url(GetDevToolsURL(profile, frontend_type, frontend_url, can_dock, panel,
                          has_other_clients));
  std::unique_ptr<WebContents> main_web_contents =
      WebContents::Create(WebContents::CreateParams(profile));
  main_web_contents->GetController().LoadURL(
      DecorateFrontendURL(url), content::Referrer(),
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());
  DevToolsUIBindings* bindings =
      DevToolsUIBindings::ForWebContents(main_web_contents.get());

  if (!bindings)
    return nullptr;
  if (!settings.empty())
    SetPreferencesFromJson(profile, settings);
  return new DevToolsWindow(frontend_type, profile,
                            std::move(main_web_contents), bindings,
                            inspected_web_contents, can_dock);
}

// static
GURL DevToolsWindow::GetDevToolsURL(Profile* profile,
                                    FrontendType frontend_type,
                                    const std::string& frontend_url,
                                    bool can_dock,
                                    const std::string& panel,
                                    bool has_other_clients) {
  std::string url;

  std::string remote_base =
      "?remoteBase=" + DevToolsUI::GetRemoteBaseURL().spec();

  const std::string valid_frontend =
      frontend_url.empty() ? chrome::kChromeUIDevToolsURL : frontend_url;

  // remoteFrontend is here for backwards compatibility only.
  std::string remote_frontend =
      valid_frontend + ((valid_frontend.find("?") == std::string::npos)
                            ? "?remoteFrontend=true"
                            : "&remoteFrontend=true");
  switch (frontend_type) {
    case kFrontendDefault:
      url = kDefaultFrontendURL + remote_base;
      if (can_dock)
        url += "&can_dock=true";
      if (!panel.empty())
        url += "&panel=" + panel;
      break;
    case kFrontendWorker:
      url = kWorkerFrontendURL + remote_base;
      break;
    case kFrontendV8:
      url = kJSFrontendURL + remote_base;
      break;
    case kFrontendNode:
      url = kNodeFrontendURL + remote_base;
      break;
    case kFrontendRemote:
      url = remote_frontend;
      break;
    case kFrontendRemoteWorker:
      // isSharedWorker is here for backwards compatibility only.
      url = remote_frontend + "&isSharedWorker=true";
      break;
  }

  if (has_other_clients)
    url += "&hasOtherClients=true";
  return DevToolsUIBindings::SanitizeFrontendURL(GURL(url));
}

// static
DevToolsWindow* DevToolsWindow::FindDevToolsWindow(
    DevToolsAgentHost* agent_host) {
  if (!agent_host || !g_devtools_window_instances.IsCreated())
    return NULL;
  DevToolsWindows* instances = g_devtools_window_instances.Pointer();
  for (auto it(instances->begin()); it != instances->end(); ++it) {
    if ((*it)->bindings_->IsAttachedTo(agent_host))
      return *it;
  }
  return NULL;
}

// static
DevToolsWindow* DevToolsWindow::AsDevToolsWindow(
    content::WebContents* web_contents) {
  if (!web_contents || !g_devtools_window_instances.IsCreated())
    return NULL;
  DevToolsWindows* instances = g_devtools_window_instances.Pointer();
  for (auto it(instances->begin()); it != instances->end(); ++it) {
    if ((*it)->main_web_contents_ == web_contents)
      return *it;
  }
  return NULL;
}

WebContents* DevToolsWindow::OpenURLFromTab(
    WebContents* source,
    const content::OpenURLParams& params) {
  DCHECK(source == main_web_contents_);
  if (!params.url.SchemeIs(content::kChromeDevToolsScheme)) {
    WebContents* inspected_web_contents = GetInspectedWebContents();
    if (!inspected_web_contents)
      return nullptr;
    content::OpenURLParams modified = params;
    modified.referrer = content::Referrer();
    return inspected_web_contents->OpenURL(modified);
  }
  main_web_contents_->GetController().Reload(content::ReloadType::NORMAL,
                                             false);
  return main_web_contents_;
}

void DevToolsWindow::ActivateContents(WebContents* contents) {
  if (is_docked_) {
    WebContents* inspected_tab = GetInspectedWebContents();
    if (inspected_tab)
      inspected_tab->GetDelegate()->ActivateContents(inspected_tab);
  } else if (browser_) {
    browser_->window()->Activate();
  }
}

void DevToolsWindow::AddNewContents(WebContents* source,
                                    std::unique_ptr<WebContents> new_contents,
                                    const GURL& target_url,
                                    WindowOpenDisposition disposition,
                                    const gfx::Rect& initial_rect,
                                    bool user_gesture,
                                    bool* was_blocked) {
  if (new_contents.get() == toolbox_web_contents_) {
    owned_toolbox_web_contents_ = std::move(new_contents);

    toolbox_web_contents_->SetDelegate(
        new DevToolsToolboxDelegate(toolbox_web_contents_,
                                    inspected_contents_observer_.get()));
    if (main_web_contents_->GetRenderWidgetHostView() &&
        toolbox_web_contents_->GetRenderWidgetHostView()) {
      gfx::Size size =
          main_web_contents_->GetRenderWidgetHostView()->GetViewBounds().size();
      toolbox_web_contents_->GetRenderWidgetHostView()->SetSize(size);
    }
    UpdateBrowserWindow();
    return;
  }

  WebContents* inspected_web_contents = GetInspectedWebContents();
  if (inspected_web_contents) {
    inspected_web_contents->GetDelegate()->AddNewContents(
        source, std::move(new_contents), target_url, disposition, initial_rect,
        user_gesture, was_blocked);
  }
}

void DevToolsWindow::WebContentsCreated(WebContents* source_contents,
                                        int opener_render_process_id,
                                        int opener_render_frame_id,
                                        const std::string& frame_name,
                                        const GURL& target_url,
                                        WebContents* new_contents) {
  if (target_url.SchemeIs(content::kChromeDevToolsScheme) &&
      target_url.path().rfind("toolbox.html") != std::string::npos) {
    CHECK(can_dock_);

    // Ownership will be passed in DevToolsWindow::AddNewContents.
    if (owned_toolbox_web_contents_)
      owned_toolbox_web_contents_.reset();
    toolbox_web_contents_ = new_contents;

    // Tag the DevTools toolbox WebContents with its TaskManager specific
    // UserData so that it shows up in the task manager.
    task_manager::WebContentsTags::CreateForDevToolsContents(
        toolbox_web_contents_);

    // The toolbox holds a placeholder for the inspected WebContents. When the
    // placeholder is resized, a frame is requested. The inspected WebContents
    // is resized when the frame is rendered. Force rendering of the toolbox at
    // all times, to make sure that a frame can be rendered even when the
    // inspected WebContents fully covers the toolbox. https://crbug.com/828307
    toolbox_web_contents_->IncrementCapturerCount(gfx::Size(),
                                                  /* stay_hidden */ false);
  }
}

void DevToolsWindow::CloseContents(WebContents* source) {
  CHECK(is_docked_);
  life_stage_ = kClosing;
  UpdateBrowserWindow();
  // In case of docked main_web_contents_, we own it so delete here.
  // Embedding DevTools window will be deleted as a result of
  // DevToolsUIBindings destruction.
  CHECK(owned_main_web_contents_);
  owned_main_web_contents_.reset();
}

void DevToolsWindow::ContentsZoomChange(bool zoom_in) {
  DCHECK(is_docked_);
  zoom::PageZoom::Zoom(main_web_contents_, zoom_in ? content::PAGE_ZOOM_IN
                                                   : content::PAGE_ZOOM_OUT);
}

void DevToolsWindow::BeforeUnloadFired(WebContents* tab,
                                       bool proceed,
                                       bool* proceed_to_fire_unload) {
  if (!intercepted_page_beforeunload_) {
    // Docked devtools window closed directly.
    if (proceed)
      bindings_->Detach();
    *proceed_to_fire_unload = proceed;
  } else {
    // Inspected page is attempting to close.
    WebContents* inspected_web_contents = GetInspectedWebContents();
    if (proceed) {
      inspected_web_contents->DispatchBeforeUnload(false /* auto_cancel */);
    } else {
      bool should_proceed;
      inspected_web_contents->GetDelegate()->BeforeUnloadFired(
          inspected_web_contents, false, &should_proceed);
      DCHECK(!should_proceed);
    }
    *proceed_to_fire_unload = false;
  }
}

content::KeyboardEventProcessingResult DevToolsWindow::PreHandleKeyboardEvent(
    WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  BrowserWindow* inspected_window = GetInspectedBrowserWindow();
  if (inspected_window) {
    return inspected_window->PreHandleKeyboardEvent(event);
  }
  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}

bool DevToolsWindow::HandleKeyboardEvent(
    WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  if (event.windows_key_code == 0x08) {
    // Do not navigate back in history on Windows (http://crbug.com/74156).
    return true;
  }
  BrowserWindow* inspected_window = GetInspectedBrowserWindow();
  return inspected_window && inspected_window->HandleKeyboardEvent(event);
}

content::JavaScriptDialogManager* DevToolsWindow::GetJavaScriptDialogManager(
    WebContents* source) {
  return javascript_dialogs::AppModalDialogManager::GetInstance();
}

content::ColorChooser* DevToolsWindow::OpenColorChooser(
    WebContents* web_contents,
    SkColor initial_color,
    const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions) {
  return chrome::ShowColorChooser(web_contents, initial_color);
}

void DevToolsWindow::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    std::unique_ptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  FileSelectHelper::RunFileChooser(render_frame_host, std::move(listener),
                                   params);
}

bool DevToolsWindow::PreHandleGestureEvent(
    WebContents* source,
    const blink::WebGestureEvent& event) {
  // Disable pinch zooming.
  return blink::WebInputEvent::IsPinchGestureEventType(event.GetType());
}

void DevToolsWindow::ActivateWindow() {
  if (life_stage_ != kLoadCompleted)
    return;
  if (is_docked_ && GetInspectedBrowserWindow())
    main_web_contents_->Focus();
  else if (!is_docked_ && !browser_->window()->IsActive())
    browser_->window()->Activate();
}

void DevToolsWindow::CloseWindow() {
  DCHECK(is_docked_);
  life_stage_ = kClosing;
  main_web_contents_->DispatchBeforeUnload(false /* auto_cancel */);
}

void DevToolsWindow::Inspect(scoped_refptr<content::DevToolsAgentHost> host) {
  DevToolsWindow::OpenDevToolsWindow(host, profile_);
}

void DevToolsWindow::SetInspectedPageBounds(const gfx::Rect& rect) {
  DevToolsContentsResizingStrategy strategy(rect);
  if (contents_resizing_strategy_.Equals(strategy))
    return;

  contents_resizing_strategy_.CopyFrom(strategy);
  UpdateBrowserWindow();
}

void DevToolsWindow::InspectElementCompleted() {
  if (!inspect_element_start_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("DevTools.InspectElement",
        base::TimeTicks::Now() - inspect_element_start_time_);
    inspect_element_start_time_ = base::TimeTicks();
  }
}

void DevToolsWindow::SetIsDocked(bool dock_requested) {
  if (life_stage_ == kClosing)
    return;

  DCHECK(can_dock_ || !dock_requested);
  if (!can_dock_)
    dock_requested = false;

  bool was_docked = is_docked_;
  is_docked_ = dock_requested;

  if (life_stage_ != kLoadCompleted) {
    // This is a first time call we waited for to initialize.
    life_stage_ = life_stage_ == kOnLoadFired ? kLoadCompleted : kIsDockedSet;
    if (life_stage_ == kLoadCompleted)
      LoadCompleted();
    return;
  }

  if (dock_requested == was_docked)
    return;

  if (dock_requested && !was_docked) {
    // Detach window from the external devtools browser. It will lead to
    // the browser object's close and delete. Remove observer first.
    TabStripModel* tab_strip_model = browser_->tab_strip_model();
    DCHECK(!owned_main_web_contents_);

    // Removing the only WebContents from the tab strip of browser_ will
    // eventually lead to the destruction of browser_ as well, which is why it's
    // okay to just null the raw pointer here.
    browser_ = NULL;

    owned_main_web_contents_ = tab_strip_model->DetachWebContentsAt(
        tab_strip_model->GetIndexOfWebContents(main_web_contents_));
  } else if (!dock_requested && was_docked) {
    UpdateBrowserWindow();
  }

  Show(DevToolsToggleAction::Show());
}

void DevToolsWindow::OpenInNewTab(const std::string& url) {
  GURL fixed_url(url);
  WebContents* inspected_web_contents = GetInspectedWebContents();
  int child_id = content::ChildProcessHost::kInvalidUniqueID;
  if (inspected_web_contents) {
    content::RenderViewHost* render_view_host =
        inspected_web_contents->GetRenderViewHost();
    if (render_view_host)
      child_id = render_view_host->GetProcess()->GetID();
  }
  // Use about:blank instead of an empty GURL. The browser treats an empty GURL
  // as navigating to the home page, which may be privileged (chrome://newtab/).
  if (!content::ChildProcessSecurityPolicy::GetInstance()->CanRequestURL(
          child_id, fixed_url))
    fixed_url = GURL(url::kAboutBlankURL);

  content::OpenURLParams params(fixed_url, content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  if (!inspected_web_contents || !inspected_web_contents->OpenURL(params)) {
    chrome::ScopedTabbedBrowserDisplayer displayer(profile_);
    chrome::AddSelectedTabWithURL(displayer.browser(), fixed_url,
                                  ui::PAGE_TRANSITION_LINK);
  }
}

void DevToolsWindow::SetWhitelistedShortcuts(
    const std::string& message) {
  event_forwarder_->SetWhitelistedShortcuts(message);
}

void DevToolsWindow::SetEyeDropperActive(bool active) {
  WebContents* web_contents = GetInspectedWebContents();
  if (!web_contents)
    return;
  if (active) {
    eye_dropper_.reset(new DevToolsEyeDropper(
        web_contents, base::Bind(&DevToolsWindow::ColorPickedInEyeDropper,
                                 base::Unretained(this))));
  } else {
    eye_dropper_.reset();
  }
}

void DevToolsWindow::ColorPickedInEyeDropper(int r, int g, int b, int a) {
  base::DictionaryValue color;
  color.SetInteger("r", r);
  color.SetInteger("g", g);
  color.SetInteger("b", b);
  color.SetInteger("a", a);
  bindings_->CallClientMethod("DevToolsAPI", "eyeDropperPickedColor", color);
}

void DevToolsWindow::InspectedContentsClosing() {
  if (!close_on_detach_)
    return;
  intercepted_page_beforeunload_ = false;
  life_stage_ = kClosing;
  main_web_contents_->ClosePage();
}

InfoBarService* DevToolsWindow::GetInfoBarService() {
  return is_docked_ ?
      InfoBarService::FromWebContents(GetInspectedWebContents()) :
      InfoBarService::FromWebContents(main_web_contents_);
}

void DevToolsWindow::RenderProcessGone(bool crashed) {
  // Docked DevToolsWindow owns its main_web_contents_ and must delete it.
  // Undocked main_web_contents_ are owned and handled by browser.
  // see crbug.com/369932
  if (is_docked_) {
    CloseContents(main_web_contents_);
  } else if (browser_ && crashed) {
    browser_->window()->Close();
  }
}

void DevToolsWindow::ShowCertificateViewer(const std::string& cert_chain) {
  base::Optional<base::Value> value = base::JSONReader::Read(cert_chain);
  CHECK(value && value->is_list());
  std::vector<std::string> decoded;
  for (const auto& item : value->GetList()) {
    CHECK(item.is_string());
    std::string temp;
    CHECK(base::Base64Decode(item.GetString(), &temp));
    decoded.push_back(std::move(temp));
  }

  std::vector<base::StringPiece> cert_string_piece;
  for (const auto& str : decoded)
    cert_string_piece.push_back(str);
  scoped_refptr<net::X509Certificate> cert =
      net::X509Certificate::CreateFromDERCertChain(cert_string_piece);
  CHECK(cert);

  WebContents* inspected_contents =
      is_docked_ ? GetInspectedWebContents() : main_web_contents_;
  Browser* browser = NULL;
  int tab = 0;
  if (!FindInspectedBrowserAndTabIndex(inspected_contents, &browser, &tab))
    return;
  gfx::NativeWindow parent = browser->window()->GetNativeWindow();
  ::ShowCertificateViewer(inspected_contents, parent, cert.get());
}

void DevToolsWindow::OnLoadCompleted() {
  // First seed inspected tab id for extension APIs.
  WebContents* inspected_web_contents = GetInspectedWebContents();
  if (inspected_web_contents) {
    sessions::SessionTabHelper* session_tab_helper =
        sessions::SessionTabHelper::FromWebContents(inspected_web_contents);
    if (session_tab_helper) {
      bindings_->CallClientMethod(
          "DevToolsAPI", "setInspectedTabId",
          base::Value(session_tab_helper->session_id().id()));
    }
  }

  if (life_stage_ == kClosing)
    return;

  // We could be in kLoadCompleted state already if frontend reloads itself.
  if (life_stage_ != kLoadCompleted) {
    // Load is completed when both kIsDockedSet and kOnLoadFired happened.
    // Here we set kOnLoadFired.
    life_stage_ = life_stage_ == kIsDockedSet ? kLoadCompleted : kOnLoadFired;
  }
  if (life_stage_ == kLoadCompleted)
    LoadCompleted();
}

void DevToolsWindow::ReadyForTest() {
  ready_for_test_ = true;
  if (!ready_for_test_callback_.is_null())
    std::move(ready_for_test_callback_).Run();
}

void DevToolsWindow::ConnectionReady() {
  if (throttle_)
    throttle_->ResumeThrottle();
}

void DevToolsWindow::SetOpenNewWindowForPopups(bool value) {
  open_new_window_for_popups_ = value;
}

void DevToolsWindow::CreateDevToolsBrowser() {
  PrefService* prefs = profile_->GetPrefs();
  if (!prefs->GetDictionary(prefs::kAppWindowPlacement)->HasKey(kDevToolsApp)) {
    // Ensure there is always a default size so that
    // BrowserFrame::InitBrowserFrame can retrieve it later.
    DictionaryPrefUpdate update(prefs, prefs::kAppWindowPlacement);
    base::Value* wp_prefs = update.Get();
    base::Value dev_tools_defaults(base::Value::Type::DICTIONARY);
    dev_tools_defaults.SetIntKey("left", 100);
    dev_tools_defaults.SetIntKey("top", 100);
    dev_tools_defaults.SetIntKey("right", 740);
    dev_tools_defaults.SetIntKey("bottom", 740);
    dev_tools_defaults.SetBoolKey("maximized", false);
    dev_tools_defaults.SetBoolKey("always_on_top", false);
    wp_prefs->SetKey(kDevToolsApp, std::move(dev_tools_defaults));
  }

  browser_ = new Browser(Browser::CreateParams::CreateForDevTools(profile_));
  browser_->tab_strip_model()->AddWebContents(
      std::move(owned_main_web_contents_), -1,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, TabStripModel::ADD_ACTIVE);
  main_web_contents_->SyncRendererPrefs();
}

BrowserWindow* DevToolsWindow::GetInspectedBrowserWindow() {
  Browser* browser = NULL;
  int tab;
  return FindInspectedBrowserAndTabIndex(GetInspectedWebContents(),
                                         &browser, &tab) ?
      browser->window() : NULL;
}

void DevToolsWindow::DoAction(const DevToolsToggleAction& action) {
  switch (action.type()) {
    case DevToolsToggleAction::kInspect:
      bindings_->CallClientMethod("DevToolsAPI", "enterInspectElementMode");
      break;

    case DevToolsToggleAction::kShowElementsPanel:
    case DevToolsToggleAction::kPauseInDebugger:
    case DevToolsToggleAction::kShowConsolePanel:
    case DevToolsToggleAction::kShow:
    case DevToolsToggleAction::kToggle:
      // Do nothing.
      break;

    case DevToolsToggleAction::kReveal: {
      const DevToolsToggleAction::RevealParams* params =
          action.params();
      CHECK(params);
      bindings_->CallClientMethod(
          "DevToolsAPI", "revealSourceLine", base::Value(params->url),
          base::Value(static_cast<int>(params->line_number)),
          base::Value(static_cast<int>(params->column_number)));
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

void DevToolsWindow::UpdateBrowserToolbar() {
  BrowserWindow* inspected_window = GetInspectedBrowserWindow();
  if (inspected_window)
    inspected_window->UpdateToolbar(NULL);
}

void DevToolsWindow::UpdateBrowserWindow() {
  BrowserWindow* inspected_window = GetInspectedBrowserWindow();
  if (inspected_window)
    inspected_window->UpdateDevTools();
}

WebContents* DevToolsWindow::GetInspectedWebContents() {
  return inspected_contents_observer_
             ? inspected_contents_observer_->web_contents()
             : NULL;
}

void DevToolsWindow::LoadCompleted() {
  Show(action_on_load_);
  action_on_load_ = DevToolsToggleAction::NoOp();
  if (!load_completed_callback_.is_null()) {
    std::move(load_completed_callback_).Run();
  }
}

void DevToolsWindow::SetLoadCompletedCallback(base::OnceClosure closure) {
  if (life_stage_ == kLoadCompleted || life_stage_ == kClosing) {
    if (!closure.is_null())
      std::move(closure).Run();
    return;
  }
  load_completed_callback_ = std::move(closure);
}

bool DevToolsWindow::ForwardKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  return event_forwarder_->ForwardEvent(event);
}

bool DevToolsWindow::ReloadInspectedWebContents(bool bypass_cache) {
  // Only route reload via front-end if the agent is attached.
  WebContents* wc = GetInspectedWebContents();
  if (!wc || wc->GetCrashedStatus() != base::TERMINATION_STATUS_STILL_RUNNING)
    return false;
  bindings_->CallClientMethod("DevToolsAPI", "reloadInspectedPage",
                              base::Value(bypass_cache));
  return true;
}

void DevToolsWindow::RegisterModalDialogManager(Browser* browser) {
  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      main_web_contents_);
  web_modal::WebContentsModalDialogManager::FromWebContents(main_web_contents_)
      ->SetDelegate(browser);
}
