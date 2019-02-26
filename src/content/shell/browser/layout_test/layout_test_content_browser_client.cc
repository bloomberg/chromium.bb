// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_content_browser_client.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/pattern.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/login_delegate.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/shell/browser/layout_test/blink_test_controller.h"
#include "content/shell/browser/layout_test/fake_bluetooth_chooser.h"
#include "content/shell/browser/layout_test/layout_test_bluetooth_fake_adapter_setter_impl.h"
#include "content/shell/browser/layout_test/layout_test_browser_context.h"
#include "content/shell/browser/layout_test/layout_test_browser_main_parts.h"
#include "content/shell/browser/layout_test/layout_test_message_filter.h"
#include "content/shell/browser/layout_test/mojo_layout_test_helper.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/common/layout_test/layout_test_switches.h"
#include "content/shell/common/shell_messages.h"
#include "content/shell/renderer/web_test/blink_test_helpers.h"
#include "content/test/mock_clipboard_host.h"
#include "content/test/mock_platform_notification_service.h"
#include "device/bluetooth/test/fake_bluetooth.h"
#include "gpu/config/gpu_switches.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "url/origin.h"

namespace content {
namespace {

LayoutTestContentBrowserClient* g_layout_test_browser_client;

void BindLayoutTestHelper(mojom::MojoLayoutTestHelperRequest request,
                          RenderFrameHost* render_frame_host) {
  MojoLayoutTestHelper::Create(std::move(request));
}

class TestOverlayWindow : public OverlayWindow {
 public:
  TestOverlayWindow() = default;
  ~TestOverlayWindow() override{};

  static std::unique_ptr<OverlayWindow> Create(
      PictureInPictureWindowController* controller) {
    return std::unique_ptr<OverlayWindow>(new TestOverlayWindow());
  }

  bool IsActive() const override { return false; }
  void Close() override {}
  void Show() override {}
  void Hide() override {}
  void SetPictureInPictureCustomControls(
      const std::vector<blink::PictureInPictureControlInfo>& controls)
      override {}
  bool IsVisible() const override { return false; }
  bool IsAlwaysOnTop() const override { return false; }
  ui::Layer* GetLayer() override { return nullptr; }
  gfx::Rect GetBounds() const override { return gfx::Rect(); }
  void UpdateVideoSize(const gfx::Size& natural_size) override {}
  void SetPlaybackState(PlaybackState playback_state) override {}
  void SetAlwaysHidePlayPauseButton(bool is_visible) override {}
  ui::Layer* GetWindowBackgroundLayer() override { return nullptr; }
  ui::Layer* GetVideoLayer() override { return nullptr; }
  gfx::Rect GetVideoBounds() override { return gfx::Rect(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestOverlayWindow);
};

}  // namespace

LayoutTestContentBrowserClient::LayoutTestContentBrowserClient()
    : mock_platform_notification_service_(
          std::make_unique<MockPlatformNotificationService>()) {
  DCHECK(!g_layout_test_browser_client);

  g_layout_test_browser_client = this;
}

LayoutTestContentBrowserClient::~LayoutTestContentBrowserClient() {
  g_layout_test_browser_client = nullptr;
}

LayoutTestContentBrowserClient* LayoutTestContentBrowserClient::Get() {
  return g_layout_test_browser_client;
}

LayoutTestBrowserContext*
LayoutTestContentBrowserClient::GetLayoutTestBrowserContext() {
  return static_cast<LayoutTestBrowserContext*>(browser_context());
}

void LayoutTestContentBrowserClient::SetPopupBlockingEnabled(
    bool block_popups) {
  block_popups_ = block_popups;
}

void LayoutTestContentBrowserClient::ResetMockClipboardHost() {
  if (mock_clipboard_host_)
    mock_clipboard_host_->Reset();
}

std::unique_ptr<FakeBluetoothChooser>
LayoutTestContentBrowserClient::GetNextFakeBluetoothChooser() {
  return std::move(next_fake_bluetooth_chooser_);
}

void LayoutTestContentBrowserClient::RenderProcessWillLaunch(
    RenderProcessHost* host,
    service_manager::mojom::ServiceRequest* service_request) {
  ShellContentBrowserClient::RenderProcessWillLaunch(host, service_request);

  StoragePartition* partition =
      BrowserContext::GetDefaultStoragePartition(browser_context());
  host->AddFilter(new LayoutTestMessageFilter(
      host->GetID(), partition->GetDatabaseTracker(),
      partition->GetQuotaManager(), partition->GetURLRequestContext(),
      partition->GetNetworkContext()));
}

void LayoutTestContentBrowserClient::ExposeInterfacesToRenderer(
    service_manager::BinderRegistry* registry,
    blink::AssociatedInterfaceRegistry* associated_registry,
    RenderProcessHost* render_process_host) {
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner =
      base::CreateSingleThreadTaskRunnerWithTraits(
          {content::BrowserThread::UI});
  registry->AddInterface(
      base::BindRepeating(&LayoutTestBluetoothFakeAdapterSetterImpl::Create),
      ui_task_runner);

  registry->AddInterface(base::BindRepeating(&bluetooth::FakeBluetooth::Create),
                         ui_task_runner);
  // This class outlives |render_process_host|, which owns |registry|. Since
  // any binders will not be called after |registry| is deleted
  // and |registry| is outlived by this class, it is safe to use
  // base::Unretained in all binders.
  registry->AddInterface(
      base::BindRepeating(
          &LayoutTestContentBrowserClient::CreateFakeBluetoothChooser,
          base::Unretained(this)),
      ui_task_runner);
  registry->AddInterface(base::BindRepeating(&MojoLayoutTestHelper::Create));
  registry->AddInterface(
      base::BindRepeating(&LayoutTestContentBrowserClient::BindClipboardHost,
                          base::Unretained(this)),
      ui_task_runner);
}

void LayoutTestContentBrowserClient::BindClipboardHost(
    blink::mojom::ClipboardHostRequest request) {
  if (!mock_clipboard_host_)
    mock_clipboard_host_ = std::make_unique<MockClipboardHost>();
  mock_clipboard_host_->Bind(std::move(request));
}

void LayoutTestContentBrowserClient::OverrideWebkitPrefs(
    RenderViewHost* render_view_host,
    WebPreferences* prefs) {
  if (BlinkTestController::Get())
    BlinkTestController::Get()->OverrideWebkitPrefs(prefs);
}

void LayoutTestContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  command_line->AppendSwitch(switches::kRunWebTests);
  ShellContentBrowserClient::AppendExtraCommandLineSwitches(command_line,
                                                            child_process_id);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAlwaysUseComplexText)) {
    command_line->AppendSwitch(switches::kAlwaysUseComplexText);
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableFontAntialiasing)) {
    command_line->AppendSwitch(switches::kEnableFontAntialiasing);
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kStableReleaseMode)) {
    command_line->AppendSwitch(switches::kStableReleaseMode);
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableLeakDetection)) {
    command_line->AppendSwitchASCII(
        switches::kEnableLeakDetection,
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kEnableLeakDetection));
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableDisplayCompositorPixelDump)) {
    command_line->AppendSwitch(switches::kEnableDisplayCompositorPixelDump);
  }
}

BrowserMainParts* LayoutTestContentBrowserClient::CreateBrowserMainParts(
    const MainFunctionParams& parameters) {
  set_browser_main_parts(new LayoutTestBrowserMainParts(parameters));
  return shell_browser_main_parts();
}

void LayoutTestContentBrowserClient::GetQuotaSettings(
    BrowserContext* context,
    StoragePartition* partition,
    storage::OptionalQuotaSettingsCallback callback) {
  // The 1GB limit is intended to give a large headroom to tests that need to
  // build up a large data set and issue many concurrent reads or writes.
  std::move(callback).Run(storage::GetHardCodedSettings(1024 * 1024 * 1024));
}

std::unique_ptr<OverlayWindow>
LayoutTestContentBrowserClient::CreateWindowForPictureInPicture(
    PictureInPictureWindowController* controller) {
  return TestOverlayWindow::Create(controller);
}

std::vector<url::Origin>
LayoutTestContentBrowserClient::GetOriginsRequiringDedicatedProcess() {
  // Unconditionally (with and without --site-per-process) isolate some origins
  // that may be used by tests that only make sense in presence of an OOPIF.
  std::vector<std::string> origins_to_isolate = {
      "http://devtools.oopif.test:8000/", "http://devtools.oopif.test:8003/",
      "http://devtools-extensions.oopif.test:8000/",
      "https://devtools.oopif.test:8443/",
  };

  // On platforms with strict Site Isolation, the also isolate WPT origins for
  // additional OOPIF coverage.
  //
  // Don't isolate WPT origins on
  // 1) platforms where strict Site Isolation is not the default.
  // 2) in layout tests under virtual/not-site-per-process where
  //    --disable-site-isolation-trials switch is used.
  if (SiteIsolationPolicy::UseDedicatedProcessesForAllSites()) {
    // The list of hostnames below is based on
    // https://web-platform-tests.org/writing-tests/server-features.html
    const char* kWptHostnames[] = {
        "www.web-platform.test",
        "www1.web-platform.test",
        "www2.web-platform.test",
        "xn--n8j6ds53lwwkrqhv28a.web-platform.test",
        "xn--lve-6lad.web-platform.test",
    };

    // The list of schemes and ports below is based on
    // third_party/blink/tools/blinkpy/third_party/wpt/wpt.config.json
    const char* kOriginTemplates[] = {
        "http://%s:8001/", "http://%s:8081/", "https://%s:8444/",
    };

    origins_to_isolate.reserve(origins_to_isolate.size() +
                               base::size(kWptHostnames) *
                                   base::size(kOriginTemplates));
    for (const char* kWptHostname : kWptHostnames) {
      for (const char* kOriginTemplate : kOriginTemplates) {
        std::string origin = base::StringPrintf(kOriginTemplate, kWptHostname);
        origins_to_isolate.push_back(origin);
      }
    }
  }

  // Translate std::vector<std::string> into std::vector<url::Origin>.
  std::vector<url::Origin> result;
  result.reserve(origins_to_isolate.size());
  for (const std::string& s : origins_to_isolate)
    result.push_back(url::Origin::Create(GURL(s)));
  return result;
}

PlatformNotificationService*
LayoutTestContentBrowserClient::GetPlatformNotificationService() {
  return mock_platform_notification_service_.get();
}

bool LayoutTestContentBrowserClient::CanCreateWindow(
    content::RenderFrameHost* opener,
    const GURL& opener_url,
    const GURL& opener_top_level_frame_url,
    const GURL& source_origin,
    content::mojom::WindowContainerType container_type,
    const GURL& target_url,
    const content::Referrer& referrer,
    const std::string& frame_name,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& features,
    bool user_gesture,
    bool opener_suppressed,
    bool* no_javascript_access) {
  *no_javascript_access = false;
  return !block_popups_ || user_gesture;
}

bool LayoutTestContentBrowserClient::CanIgnoreCertificateErrorIfNeeded() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kRunWebTests);
}

void LayoutTestContentBrowserClient::ExposeInterfacesToFrame(
    service_manager::BinderRegistryWithArgs<content::RenderFrameHost*>*
        registry) {
  registry->AddInterface(base::Bind(&BindLayoutTestHelper));
}

scoped_refptr<LoginDelegate>
LayoutTestContentBrowserClient::CreateLoginDelegate(
    net::AuthChallengeInfo* auth_info,
    content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
    const content::GlobalRequestID& request_id,
    bool is_main_frame,
    const GURL& url,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    bool first_auth_attempt,
    LoginAuthRequiredCallback auth_required_callback) {
  return nullptr;
}

// private
void LayoutTestContentBrowserClient::CreateFakeBluetoothChooser(
    mojom::FakeBluetoothChooserRequest request) {
  DCHECK(!next_fake_bluetooth_chooser_);
  next_fake_bluetooth_chooser_ =
      FakeBluetoothChooser::Create(std::move(request));
}

}  // namespace content
