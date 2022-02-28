// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_content_browser_client.h"

#include <stddef.h>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/cxx17_backports.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/base/switches.h"
#include "components/metrics/client_info.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/variations/platform_field_trials.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/safe_seed_manager.h"
#include "components/variations/service/variations_field_trial_creator.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/service/variations_service_client.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/login_delegate.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switch_dependent_feature_overrides.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/user_agent.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_browser_main_parts.h"
#include "content/shell/browser/shell_devtools_manager_delegate.h"
#include "content/shell/browser/shell_paths.h"
#include "content/shell/browser/shell_quota_permission_context.h"
#include "content/shell/browser/shell_web_contents_view_delegate_creator.h"
#include "content/shell/common/shell_controller.test-mojom.h"
#include "content/shell/common/shell_switches.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/mojom/media_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/ssl/client_cert_identity.h"
#include "services/device/public/cpp/geolocation/location_system_permission_status.h"
#include "services/network/public/mojom/ct_log_info.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "url/gurl.h"
#include "url/origin.h"

#if defined(OS_ANDROID)
#include "base/android/apk_assets.h"
#include "base/android/path_utils.h"
#include "components/variations/android/variations_seed_bridge.h"
#include "content/shell/android/shell_descriptors.h"
#endif

#if defined(OS_ANDROID)
#include "components/crash/content/browser/crash_handler_host_linux.h"
#endif

#if defined(OS_MAC)
#include "services/device/public/cpp/test/fake_geolocation_manager.h"
#endif

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
#include "components/crash/core/app/crash_switches.h"
#include "components/crash/core/app/crashpad.h"
#include "content/public/common/content_descriptors.h"
#endif

#if BUILDFLAG(ENABLE_CAST_RENDERER)
#include "media/mojo/services/media_service_factory.h"  // nogncheck
#endif

namespace content {

namespace {

ShellContentBrowserClient* g_browser_client = nullptr;

bool g_enable_expect_ct_for_testing = false;

#if defined(OS_ANDROID)
int GetCrashSignalFD(const base::CommandLine& command_line) {
  return crashpad::CrashHandlerHost::Get()->GetDeathSignalSocket();
}
#elif defined(OS_LINUX) || defined(OS_CHROMEOS)
int GetCrashSignalFD(const base::CommandLine& command_line) {
  int fd;
  pid_t pid;
  return crash_reporter::GetHandlerSocket(&fd, &pid) ? fd : -1;
}
#endif

class ShellControllerImpl : public mojom::ShellController {
 public:
  ShellControllerImpl() = default;
  ~ShellControllerImpl() override = default;

  // mojom::ShellController:
  void GetSwitchValue(const std::string& name,
                      GetSwitchValueCallback callback) override {
    const auto& command_line = *base::CommandLine::ForCurrentProcess();
    if (command_line.HasSwitch(name))
      std::move(callback).Run(command_line.GetSwitchValueASCII(name));
    else
      std::move(callback).Run(absl::nullopt);
  }

  void ExecuteJavaScript(const std::u16string& script,
                         ExecuteJavaScriptCallback callback) override {
    CHECK(!Shell::windows().empty());
    WebContents* contents = Shell::windows()[0]->web_contents();
    contents->GetMainFrame()->ExecuteJavaScriptForTests(script,
                                                        std::move(callback));
  }

  void ShutDown() override { Shell::Shutdown(); }
};

// TODO(crbug/1219642): Consider not needing VariationsServiceClient just to use
// VariationsFieldTrialCreator.
class ShellVariationsServiceClient
    : public variations::VariationsServiceClient {
 public:
  ShellVariationsServiceClient() = default;
  ~ShellVariationsServiceClient() override = default;

  // variations::VariationsServiceClient:
  base::Version GetVersionForSimulation() override { return base::Version(); }
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return nullptr;
  }
  network_time::NetworkTimeTracker* GetNetworkTimeTracker() override {
    return nullptr;
  }
  version_info::Channel GetChannel() override {
    return version_info::Channel::UNKNOWN;
  }
  bool OverridesRestrictParameter(std::string* parameter) override {
    return false;
  }
  bool IsEnterprise() override { return false; }
};

// Returns the reduced user agent string for the content shell.
std::string GetShellReducedUserAgent() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return content::GetReducedUserAgent(
      command_line->HasSwitch(switches::kUseMobileUserAgent),
      CONTENT_SHELL_MAJOR_VERSION);
}

}  // namespace

class ShellContentBrowserClient::ShellFieldTrials
    : public variations::PlatformFieldTrials {
 public:
  ShellFieldTrials() = default;
  ~ShellFieldTrials() override = default;

  // variations::PlatformFieldTrials:
  void SetUpFieldTrials() override {}
  void SetUpFeatureControllingFieldTrials(
      bool has_seed,
      const base::FieldTrial::EntropyProvider* low_entropy_provider,
      base::FeatureList* feature_list) override {}
};

std::string GetShellUserAgent() {
  if (base::FeatureList::IsEnabled(blink::features::kReduceUserAgent))
    return GetShellReducedUserAgent();

  std::string product = "Chrome/" CONTENT_SHELL_VERSION;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kUseMobileUserAgent))
    product += " Mobile";
  return BuildUserAgentFromProduct(product);
}

std::string GetShellLanguage() {
  return "en-us,en";
}

blink::UserAgentMetadata GetShellUserAgentMetadata() {
  blink::UserAgentMetadata metadata;

  metadata.brand_version_list.emplace_back("content_shell",
                                           CONTENT_SHELL_MAJOR_VERSION);
  metadata.brand_full_version_list.emplace_back("content_shell",
                                                CONTENT_SHELL_VERSION);
  metadata.full_version = CONTENT_SHELL_VERSION;
  metadata.platform = "Unknown";
  metadata.architecture = BuildCpuInfo();
  metadata.model = BuildModelInfo();

  metadata.bitness = GetLowEntropyCpuBitness();

  return metadata;
}

// static
bool ShellContentBrowserClient::allow_any_cors_exempt_header_for_browser_ =
    false;

ShellContentBrowserClient* ShellContentBrowserClient::Get() {
  return g_browser_client;
}

ShellContentBrowserClient::ShellContentBrowserClient() {
  DCHECK(!g_browser_client);
#if defined(OS_MAC)
  location_manager_ = std::make_unique<device::FakeGeolocationManager>();
  location_manager_->SetSystemPermission(
      device::LocationSystemPermissionStatus::kAllowed);
#endif
  g_browser_client = this;
}

ShellContentBrowserClient::~ShellContentBrowserClient() {
  g_browser_client = nullptr;
}

std::unique_ptr<BrowserMainParts>
ShellContentBrowserClient::CreateBrowserMainParts(
    MainFunctionParams parameters) {
  auto browser_main_parts =
      std::make_unique<ShellBrowserMainParts>(std::move(parameters));

  shell_browser_main_parts_ = browser_main_parts.get();

  return browser_main_parts;
}

bool ShellContentBrowserClient::IsHandledURL(const GURL& url) {
  if (!url.is_valid())
    return false;
  static const char* const kProtocolList[] = {
      url::kHttpScheme, url::kHttpsScheme,        url::kWsScheme,
      url::kWssScheme,  url::kBlobScheme,         url::kFileSystemScheme,
      kChromeUIScheme,  kChromeUIUntrustedScheme, kChromeDevToolsScheme,
      url::kDataScheme, url::kFileScheme,
  };
  for (const char* supported_protocol : kProtocolList) {
    if (url.scheme_piece() == supported_protocol)
      return true;
  }
  return false;
}

void ShellContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  static const char* kForwardSwitches[] = {
#if defined(OS_MAC)
    // Needed since on Mac, content_browsertests doesn't use
    // content_test_launcher.cc and instead uses shell_main.cc. So give a signal
    // to shell_main.cc that it's a browser test.
    switches::kBrowserTest,
#endif
    switches::kCrashDumpsDir,
    switches::kEnableCrashReporter,
    switches::kExposeInternalsForTesting,
    switches::kRunWebTests,
  };

  command_line->CopySwitchesFrom(*base::CommandLine::ForCurrentProcess(),
                                 kForwardSwitches,
                                 base::size(kForwardSwitches));

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableCrashReporter)) {
    int fd;
    pid_t pid;
    if (crash_reporter::GetHandlerSocket(&fd, &pid)) {
      command_line->AppendSwitchASCII(
          crash_reporter::switches::kCrashpadHandlerPid,
          base::NumberToString(pid));
    }
  }
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)
}

device::GeolocationManager* ShellContentBrowserClient::GetGeolocationManager() {
#if defined(OS_MAC)
  return location_manager_.get();
#else
  return nullptr;
#endif
}

std::string ShellContentBrowserClient::GetAcceptLangs(BrowserContext* context) {
  return GetShellLanguage();
}

std::string ShellContentBrowserClient::GetDefaultDownloadName() {
  return "download";
}

WebContentsViewDelegate* ShellContentBrowserClient::GetWebContentsViewDelegate(
    WebContents* web_contents) {
  performance_manager::PerformanceManagerRegistry::GetInstance()
      ->MaybeCreatePageNodeForWebContents(web_contents);
  return CreateShellWebContentsViewDelegate(web_contents);
}

scoped_refptr<content::QuotaPermissionContext>
ShellContentBrowserClient::CreateQuotaPermissionContext() {
  return new ShellQuotaPermissionContext();
}

GeneratedCodeCacheSettings
ShellContentBrowserClient::GetGeneratedCodeCacheSettings(
    content::BrowserContext* context) {
  // If we pass 0 for size, disk_cache will pick a default size using the
  // heuristics based on available disk size. These are implemented in
  // disk_cache::PreferredCacheSize in net/disk_cache/cache_util.cc.
  return GeneratedCodeCacheSettings(true, 0, context->GetPath());
}

base::OnceClosure ShellContentBrowserClient::SelectClientCertificate(
    WebContents* web_contents,
    net::SSLCertRequestInfo* cert_request_info,
    net::ClientCertIdentityList client_certs,
    std::unique_ptr<ClientCertificateDelegate> delegate) {
  if (select_client_certificate_callback_)
    return std::move(select_client_certificate_callback_)
        .Run(web_contents, cert_request_info, std::move(client_certs),
             std::move(delegate));
  return base::OnceClosure();
}

SpeechRecognitionManagerDelegate*
ShellContentBrowserClient::CreateSpeechRecognitionManagerDelegate() {
  return new ShellSpeechRecognitionManagerDelegate();
}

void ShellContentBrowserClient::OverrideWebkitPrefs(
    WebContents* web_contents,
    blink::web_pref::WebPreferences* prefs) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceDarkMode)) {
    prefs->preferred_color_scheme = blink::mojom::PreferredColorScheme::kDark;
  } else {
    prefs->preferred_color_scheme = blink::mojom::PreferredColorScheme::kLight;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceHighContrast)) {
    prefs->preferred_contrast = blink::mojom::PreferredContrast::kMore;
  } else {
    prefs->preferred_contrast = blink::mojom::PreferredContrast::kNoPreference;
  }

  if (override_web_preferences_callback_)
    override_web_preferences_callback_.Run(prefs);
}

base::FilePath ShellContentBrowserClient::GetFontLookupTableCacheDir() {
  return browser_context()->GetPath().Append(
      FILE_PATH_LITERAL("FontLookupTableCache"));
}

std::unique_ptr<content::DevToolsManagerDelegate>
ShellContentBrowserClient::CreateDevToolsManagerDelegate() {
  return std::make_unique<ShellDevToolsManagerDelegate>(browser_context());
}

void ShellContentBrowserClient::ExposeInterfacesToRenderer(
    service_manager::BinderRegistry* registry,
    blink::AssociatedInterfaceRegistry* associated_registry,
    RenderProcessHost* render_process_host) {
  performance_manager::PerformanceManagerRegistry::GetInstance()
      ->CreateProcessNodeAndExposeInterfacesToRendererProcess(
          registry, render_process_host);
}

mojo::Remote<::media::mojom::MediaService>
ShellContentBrowserClient::RunSecondaryMediaService() {
  mojo::Remote<::media::mojom::MediaService> remote;
#if BUILDFLAG(ENABLE_CAST_RENDERER)
  static base::SequenceLocalStorageSlot<std::unique_ptr<::media::MediaService>>
      service;
  service.emplace(::media::CreateMediaServiceForTesting(
      remote.BindNewPipeAndPassReceiver()));
#endif
  return remote;
}

void ShellContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
    RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<RenderFrameHost*>* map) {
  performance_manager::PerformanceManagerRegistry::GetInstance()
      ->ExposeInterfacesToRenderFrame(map);
}

void ShellContentBrowserClient::OpenURL(
    SiteInstance* site_instance,
    const OpenURLParams& params,
    base::OnceCallback<void(WebContents*)> callback) {
  std::move(callback).Run(
      Shell::CreateNewWindow(site_instance->GetBrowserContext(), params.url,
                             nullptr, gfx::Size())
          ->web_contents());
}

std::vector<std::unique_ptr<NavigationThrottle>>
ShellContentBrowserClient::CreateThrottlesForNavigation(
    NavigationHandle* navigation_handle) {
  std::vector<std::unique_ptr<NavigationThrottle>> empty_throttles;
  if (create_throttles_for_navigation_callback_)
    return create_throttles_for_navigation_callback_.Run(navigation_handle);
  return empty_throttles;
}

std::unique_ptr<LoginDelegate> ShellContentBrowserClient::CreateLoginDelegate(
    const net::AuthChallengeInfo& auth_info,
    content::WebContents* web_contents,
    const content::GlobalRequestID& request_id,
    bool is_request_for_primary_main_frame,
    const GURL& url,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    bool first_auth_attempt,
    LoginAuthRequiredCallback auth_required_callback) {
  if (!login_request_callback_.is_null()) {
    std::move(login_request_callback_).Run(is_request_for_primary_main_frame);
  }
  return nullptr;
}

base::DictionaryValue ShellContentBrowserClient::GetNetLogConstants() {
  base::DictionaryValue client_constants;
  client_constants.SetString("name", "content_shell");
  base::CommandLine::StringType command_line =
      base::CommandLine::ForCurrentProcess()->GetCommandLineString();
#if defined(OS_WIN)
  client_constants.SetString("command_line", base::WideToUTF8(command_line));
#else
  client_constants.SetString("command_line", command_line);
#endif
  base::DictionaryValue constants;
  constants.SetKey("clientInfo", std::move(client_constants));
  return constants;
}

base::FilePath
ShellContentBrowserClient::GetSandboxedStorageServiceDataDirectory() {
  return browser_context()->GetPath();
}

std::string ShellContentBrowserClient::GetUserAgent() {
  return GetShellUserAgent();
}

std::string ShellContentBrowserClient::GetReducedUserAgent() {
  return GetShellReducedUserAgent();
}

blink::UserAgentMetadata ShellContentBrowserClient::GetUserAgentMetadata() {
  return GetShellUserAgentMetadata();
}

void ShellContentBrowserClient::OverrideURLLoaderFactoryParams(
    BrowserContext* browser_context,
    const url::Origin& origin,
    bool is_for_isolated_world,
    network::mojom::URLLoaderFactoryParams* factory_params) {
  if (url_loader_factory_params_callback_) {
    url_loader_factory_params_callback_.Run(factory_params, origin,
                                            is_for_isolated_world);
  }
}

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
void ShellContentBrowserClient::GetAdditionalMappedFilesForChildProcess(
    const base::CommandLine& command_line,
    int child_process_id,
    content::PosixFileDescriptorInfo* mappings) {
#if defined(OS_ANDROID)
  mappings->ShareWithRegion(
      kShellPakDescriptor,
      base::GlobalDescriptors::GetInstance()->Get(kShellPakDescriptor),
      base::GlobalDescriptors::GetInstance()->GetRegion(kShellPakDescriptor));
#endif
  int crash_signal_fd = GetCrashSignalFD(command_line);
  if (crash_signal_fd >= 0) {
    mappings->Share(kCrashDumpSignal, crash_signal_fd);
  }
}
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)

void ShellContentBrowserClient::ConfigureNetworkContextParams(
    BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path,
    network::mojom::NetworkContextParams* network_context_params,
    cert_verifier::mojom::CertVerifierCreationParams*
        cert_verifier_creation_params) {
  ConfigureNetworkContextParamsForShell(context, network_context_params,
                                        cert_verifier_creation_params);
}

std::vector<base::FilePath>
ShellContentBrowserClient::GetNetworkContextsParentDirectory() {
  return {browser_context()->GetPath()};
}

void ShellContentBrowserClient::BindBrowserControlInterface(
    mojo::ScopedMessagePipeHandle pipe) {
  if (!pipe.is_valid())
    return;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ShellControllerImpl>(),
      mojo::PendingReceiver<mojom::ShellController>(std::move(pipe)));
}

ShellBrowserContext* ShellContentBrowserClient::browser_context() {
  return shell_browser_main_parts_->browser_context();
}

ShellBrowserContext*
ShellContentBrowserClient::off_the_record_browser_context() {
  return shell_browser_main_parts_->off_the_record_browser_context();
}

void ShellContentBrowserClient::set_enable_expect_ct_for_testing(
    bool enable_expect_ct_for_testing) {
  g_enable_expect_ct_for_testing = enable_expect_ct_for_testing;
}

void ShellContentBrowserClient::ConfigureNetworkContextParamsForShell(
    BrowserContext* context,
    network::mojom::NetworkContextParams* context_params,
    cert_verifier::mojom::CertVerifierCreationParams*
        cert_verifier_creation_params) {
  context_params->allow_any_cors_exempt_header_for_browser =
      allow_any_cors_exempt_header_for_browser_;
  context_params->user_agent = GetUserAgent();
  context_params->accept_language = GetAcceptLangs(context);
  auto exempt_header =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          "cors_exempt_header_list");
  if (!exempt_header.empty())
    context_params->cors_exempt_header_list.push_back(exempt_header);

  if (g_enable_expect_ct_for_testing) {
    context_params->enforce_chrome_ct_policy = true;
    context_params->enable_expect_ct_reporting = true;
  }
}

void ShellContentBrowserClient::GetHyphenationDictionary(
    base::OnceCallback<void(const base::FilePath&)> callback) {
  // If we have the source tree, return the dictionary files in the tree.
  base::FilePath dir;
  if (base::PathService::Get(base::DIR_SOURCE_ROOT, &dir)) {
    dir = dir.AppendASCII("third_party")
              .AppendASCII("hyphenation-patterns")
              .AppendASCII("hyb");
    std::move(callback).Run(dir);
  }
  // No need to callback if there were no dictionaries.
}

bool ShellContentBrowserClient::HasErrorPage(int http_status_code) {
  return http_status_code >= 400 && http_status_code < 600;
}

void ShellContentBrowserClient::CreateFeatureListAndFieldTrials() {
  local_state_ = CreateLocalState();
  SetUpFieldTrials();
  // Schedule a Local State write since the above function resulted in some
  // prefs being updated.
  local_state_->CommitPendingWrite();
}

std::unique_ptr<PrefService> ShellContentBrowserClient::CreateLocalState() {
  auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();

  metrics::MetricsService::RegisterPrefs(pref_registry.get());
  variations::VariationsService::RegisterPrefs(pref_registry.get());

  base::FilePath path;
  CHECK(base::PathService::Get(SHELL_DIR_USER_DATA, &path));
  path = path.AppendASCII("Local State");

  PrefServiceFactory pref_service_factory;
  pref_service_factory.set_user_prefs(
      base::MakeRefCounted<JsonPrefStore>(path));

  return pref_service_factory.Create(pref_registry);
}

void ShellContentBrowserClient::SetUpFieldTrials() {
  metrics::TestEnabledStateProvider enabled_state_provider(/*consent=*/false,
                                                           /*enabled=*/false);
  base::FilePath path;
  base::PathService::Get(SHELL_DIR_USER_DATA, &path);
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager =
      metrics::MetricsStateManager::Create(
          local_state_.get(), &enabled_state_provider, std::wstring(),
          path.AppendASCII("Local State"));
  metrics_state_manager->InstantiateFieldTrialList(
      cc::switches::kEnableGpuBenchmarking);

  std::vector<std::string> variation_ids;
  auto feature_list = std::make_unique<base::FeatureList>();

  field_trials_ = std::make_unique<ShellFieldTrials>();

  std::unique_ptr<variations::SeedResponse> initial_seed;
#if defined(OS_ANDROID)
  if (!local_state_->HasPrefPath(variations::prefs::kVariationsSeedSignature)) {
    DVLOG(1) << "Importing first run seed from Java preferences.";
    initial_seed = variations::android::GetVariationsFirstRunSeed();
  }
#endif

  ShellVariationsServiceClient variations_service_client;
  variations::VariationsFieldTrialCreator field_trial_creator(
      &variations_service_client,
      std::make_unique<variations::VariationsSeedStore>(
          local_state_.get(), std::move(initial_seed),
          /*signature_verification_enabled=*/true),
      variations::UIStringOverrider());

  variations::SafeSeedManager safe_seed_manager(local_state_.get());

  // Since this is a test-only code path, some arguments to SetUpFieldTrials are
  // null.
  // TODO(crbug/1248066): Consider passing a low entropy provider and source.
  field_trial_creator.SetUpFieldTrials(
      variation_ids,
      content::GetSwitchDependentFeatureOverrides(
          *base::CommandLine::ForCurrentProcess()),
      /*low_entropy_provider=*/nullptr, std::move(feature_list),
      metrics_state_manager.get(), field_trials_.get(), &safe_seed_manager,
      /*low_entropy_source_value=*/absl::nullopt);
}

void ShellContentBrowserClient::OnNetworkServiceCreated(
    network::mojom::NetworkService* network_service) {
  // Explicitly configure Certificate Transparency with no logs, but with
  // a fresh enough log update time that policy enforcement will still be
  // run. This does not use base::GetBuildTime(), as that may cause certain
  // checks to be disabled if too far in the past. Callers that set
  // `g_enable_expect_ct_reporting` are expected to simulate CT verification
  // using `MockCertVerifier` (otherwise CT validation would fail due to the
  // empty log list).
  if (g_enable_expect_ct_for_testing) {
    network_service->UpdateCtLogList(
        std::vector<network::mojom::CTLogInfoPtr>(), base::Time::Now());
  }
}

}  // namespace content
