// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/context.h"

#include "libcef/browser/browser_info_manager.h"
#include "libcef/browser/request_context_impl.h"
#include "libcef/browser/thread_util.h"
#include "libcef/browser/trace_subscriber.h"
#include "libcef/common/cef_switches.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/threading/thread_restrictions.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_WIN)
#include "base/strings/utf_string_conversions.h"
#include "chrome/chrome_elf/chrome_elf_main.h"
#include "chrome/install_static/initialize_from_primary_module.h"
#endif

namespace {

CefContext* g_context = nullptr;

#if DCHECK_IS_ON()
// When the process terminates check if CefShutdown() has been called.
class CefShutdownChecker {
 public:
  ~CefShutdownChecker() { DCHECK(!g_context) << "CefShutdown was not called"; }
} g_shutdown_checker;
#endif  // DCHECK_IS_ON()

#if defined(OS_WIN)
#if defined(ARCH_CPU_X86_64)
// VS2013 only checks the existence of FMA3 instructions, not the enabled-ness
// of them at the OS level (this is fixed in VS2015). We force off usage of
// FMA3 instructions in the CRT to avoid using that path and hitting illegal
// instructions when running on CPUs that support FMA3, but OSs that don't.
void DisableFMA3() {
  static bool disabled = false;
  if (disabled)
    return;
  disabled = true;
  _set_FMA3_enable(0);
}
#endif  // defined(ARCH_CPU_X86_64)

// Transfer state from chrome_elf.dll to the libcef.dll. Accessed when
// loading chrome://system.
void InitInstallDetails() {
  static bool initialized = false;
  if (initialized)
    return;
  initialized = true;
  install_static::InitializeFromPrimaryModule();
}

// Signal chrome_elf to initialize crash reporting, rather than doing it in
// DllMain. See https://crbug.com/656800 for details.
void InitCrashReporter() {
  static bool initialized = false;
  if (initialized)
    return;
  initialized = true;
  SignalInitializeCrashReporting();
}
#endif  // defined(OS_WIN)

bool GetColor(const cef_color_t cef_in, bool is_windowless, SkColor* sk_out) {
  // Windowed browser colors must be fully opaque.
  if (!is_windowless && CefColorGetA(cef_in) != SK_AlphaOPAQUE)
    return false;

  // Windowless browser colors may be fully transparent.
  if (is_windowless && CefColorGetA(cef_in) == SK_AlphaTRANSPARENT) {
    *sk_out = SK_ColorTRANSPARENT;
    return true;
  }

  // Ignore the alpha component.
  *sk_out = SkColorSetRGB(CefColorGetR(cef_in), CefColorGetG(cef_in),
                          CefColorGetB(cef_in));
  return true;
}

// Convert |path_str| to a normalized FilePath.
base::FilePath NormalizePath(const cef_string_t& path_str,
                             const char* name,
                             bool* has_error = nullptr) {
  if (has_error)
    *has_error = false;

  base::FilePath path = base::FilePath(CefString(&path_str));
  if (path.EndsWithSeparator()) {
    // Remove the trailing separator because it will interfere with future
    // equality checks.
    path = path.StripTrailingSeparators();
  }

  if (!path.empty() && !path.IsAbsolute()) {
    LOG(ERROR) << "The " << name << " directory (" << path.value()
               << ") is not an absolute path. Defaulting to empty.";
    if (has_error)
      *has_error = true;
    path = base::FilePath();
  }

  return path;
}

void SetPath(cef_string_t& path_str, const base::FilePath& path) {
#if defined(OS_WIN)
  CefString(&path_str).FromWString(path.value());
#else
  CefString(&path_str).FromString(path.value());
#endif
}

// Convert |path_str| to a normalized FilePath and update the |path_str| value.
base::FilePath NormalizePathAndSet(cef_string_t& path_str, const char* name) {
  const base::FilePath& path = NormalizePath(path_str, name);
  SetPath(path_str, path);
  return path;
}

// Verify that |cache_path| is valid and create it if necessary.
bool ValidateCachePath(const base::FilePath& cache_path,
                       const base::FilePath& root_cache_path) {
  if (cache_path.empty())
    return true;

  if (!root_cache_path.empty() && root_cache_path != cache_path &&
      !root_cache_path.IsParent(cache_path)) {
    LOG(ERROR) << "The cache_path directory (" << cache_path.value()
               << ") is not a child of the root_cache_path directory ("
               << root_cache_path.value() << ")";
    return false;
  }

  base::ThreadRestrictions::ScopedAllowIO allow_io;
  if (!base::DirectoryExists(cache_path) &&
      !base::CreateDirectory(cache_path)) {
    LOG(ERROR) << "The cache_path directory (" << cache_path.value()
               << ") could not be created.";
    return false;
  }

  return true;
}

// Like NormalizePathAndSet but with additional checks specific to the
// cache_path value.
base::FilePath NormalizeCachePathAndSet(cef_string_t& path_str,
                                        const base::FilePath& root_cache_path) {
  bool has_error = false;
  base::FilePath path = NormalizePath(path_str, "cache_path", &has_error);
  if (has_error || !ValidateCachePath(path, root_cache_path)) {
    LOG(ERROR) << "The cache_path is invalid. Defaulting to in-memory storage.";
    path = base::FilePath();
  }
  SetPath(path_str, path);
  return path;
}

}  // namespace

int CefExecuteProcess(const CefMainArgs& args,
                      CefRefPtr<CefApp> application,
                      void* windows_sandbox_info) {
#if defined(OS_WIN)
#if defined(ARCH_CPU_X86_64)
  DisableFMA3();
#endif
  InitInstallDetails();
  InitCrashReporter();
#endif

  return CefMainRunner::RunAsHelperProcess(args, application,
                                           windows_sandbox_info);
}

bool CefInitialize(const CefMainArgs& args,
                   const CefSettings& settings,
                   CefRefPtr<CefApp> application,
                   void* windows_sandbox_info) {
#if defined(OS_WIN)
#if defined(ARCH_CPU_X86_64)
  DisableFMA3();
#endif
  InitInstallDetails();
  InitCrashReporter();
#endif

  // Return true if the global context already exists.
  if (g_context)
    return true;

  if (settings.size != sizeof(cef_settings_t)) {
    NOTREACHED() << "invalid CefSettings structure size";
    return false;
  }

  // Create the new global context object.
  g_context = new CefContext();

  // Initialize the global context.
  return g_context->Initialize(args, settings, application,
                               windows_sandbox_info);
}

void CefShutdown() {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  // Must always be called on the same thread as Initialize.
  if (!g_context->OnInitThread()) {
    NOTREACHED() << "called on invalid thread";
    return;
  }

  // Shut down the global context. This will block until shutdown is complete.
  g_context->Shutdown();

  // Delete the global context object.
  delete g_context;
  g_context = nullptr;
}

void CefDoMessageLoopWork() {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  // Must always be called on the same thread as Initialize.
  if (!g_context->OnInitThread()) {
    NOTREACHED() << "called on invalid thread";
    return;
  }

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

void CefRunMessageLoop() {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  // Must always be called on the same thread as Initialize.
  if (!g_context->OnInitThread()) {
    NOTREACHED() << "called on invalid thread";
    return;
  }

  g_context->RunMessageLoop();
}

void CefQuitMessageLoop() {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  // Must always be called on the same thread as Initialize.
  if (!g_context->OnInitThread()) {
    NOTREACHED() << "called on invalid thread";
    return;
  }

  g_context->QuitMessageLoop();
}

void CefSetOSModalLoop(bool osModalLoop) {
#if defined(OS_WIN)
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  if (CEF_CURRENTLY_ON_UIT())
    base::CurrentThread::Get()->set_os_modal_loop(osModalLoop);
  else
    CEF_POST_TASK(CEF_UIT, base::Bind(CefSetOSModalLoop, osModalLoop));
#endif  // defined(OS_WIN)
}

// CefContext

CefContext::CefContext()
    : initialized_(false), shutting_down_(false), init_thread_id_(0) {}

CefContext::~CefContext() {}

// static
CefContext* CefContext::Get() {
  return g_context;
}

bool CefContext::Initialize(const CefMainArgs& args,
                            const CefSettings& settings,
                            CefRefPtr<CefApp> application,
                            void* windows_sandbox_info) {
  init_thread_id_ = base::PlatformThread::CurrentId();
  settings_ = settings;
  application_ = application;

#if !(defined(OS_WIN) || defined(OS_LINUX))
  if (settings.multi_threaded_message_loop) {
    NOTIMPLEMENTED() << "multi_threaded_message_loop is not supported.";
    return false;
  }
#endif

#if defined(OS_WIN)
  // Signal Chrome Elf that Chrome has begun to start.
  SignalChromeElf();
#endif

  const base::FilePath& root_cache_path =
      NormalizePathAndSet(settings_.root_cache_path, "root_cache_path");
  const base::FilePath& cache_path =
      NormalizeCachePathAndSet(settings_.cache_path, root_cache_path);
  if (root_cache_path.empty() && !cache_path.empty()) {
    CefString(&settings_.root_cache_path) = cache_path.value();
  }

  // All other paths that need to be normalized.
  NormalizePathAndSet(settings_.browser_subprocess_path,
                      "browser_subprocess_path");
  NormalizePathAndSet(settings_.framework_dir_path, "framework_dir_path");
  NormalizePathAndSet(settings_.main_bundle_path, "main_bundle_path");
  NormalizePathAndSet(settings_.user_data_path, "user_data_path");
  NormalizePathAndSet(settings_.resources_dir_path, "resources_dir_path");
  NormalizePathAndSet(settings_.locales_dir_path, "locales_dir_path");

  browser_info_manager_.reset(new CefBrowserInfoManager);

  main_runner_.reset(new CefMainRunner(settings_.multi_threaded_message_loop,
                                       settings_.external_message_pump));
  return main_runner_->Initialize(
      &settings_, application, args, windows_sandbox_info, &initialized_,
      base::BindOnce(&CefContext::OnContextInitialized,
                     base::Unretained(this)));
}

void CefContext::RunMessageLoop() {
  // Must always be called on the same thread as Initialize.
  DCHECK(OnInitThread());

  // Blocks until QuitMessageLoop() is called.
  main_runner_->RunMessageLoop();
}

void CefContext::QuitMessageLoop() {
  // Must always be called on the same thread as Initialize.
  DCHECK(OnInitThread());

  main_runner_->QuitMessageLoop();
}

void CefContext::Shutdown() {
  // Must always be called on the same thread as Initialize.
  DCHECK(OnInitThread());

  shutting_down_ = true;

  main_runner_->Shutdown(
      base::BindOnce(&CefContext::ShutdownOnUIThread, base::Unretained(this)),
      base::BindOnce(&CefContext::FinalizeShutdown, base::Unretained(this)));
}

bool CefContext::OnInitThread() {
  return (base::PlatformThread::CurrentId() == init_thread_id_);
}

SkColor CefContext::GetBackgroundColor(
    const CefBrowserSettings* browser_settings,
    cef_state_t windowless_state) const {
  bool is_windowless = windowless_state == STATE_ENABLED
                           ? true
                           : (windowless_state == STATE_DISABLED
                                  ? false
                                  : !!settings_.windowless_rendering_enabled);

  // Default to opaque white if no acceptable color values are found.
  SkColor sk_color = SK_ColorWHITE;

  if (!browser_settings ||
      !GetColor(browser_settings->background_color, is_windowless, &sk_color)) {
    GetColor(settings_.background_color, is_windowless, &sk_color);
  }
  return sk_color;
}

CefTraceSubscriber* CefContext::GetTraceSubscriber() {
  CEF_REQUIRE_UIT();
  if (shutting_down_)
    return nullptr;
  if (!trace_subscriber_.get())
    trace_subscriber_.reset(new CefTraceSubscriber());
  return trace_subscriber_.get();
}

void CefContext::PopulateGlobalRequestContextSettings(
    CefRequestContextSettings* settings) {
  CefRefPtr<CefCommandLine> command_line =
      CefCommandLine::GetGlobalCommandLine();

  // This value was already normalized in Initialize.
  CefString(&settings->cache_path) = CefString(&settings_.cache_path);

  settings->persist_session_cookies =
      settings_.persist_session_cookies ||
      command_line->HasSwitch(switches::kPersistSessionCookies);
  settings->persist_user_preferences =
      settings_.persist_user_preferences ||
      command_line->HasSwitch(switches::kPersistUserPreferences);
  settings->ignore_certificate_errors =
      settings_.ignore_certificate_errors ||
      command_line->HasSwitch(switches::kIgnoreCertificateErrors);

  CefString(&settings->cookieable_schemes_list) =
      CefString(&settings_.cookieable_schemes_list);
  settings->cookieable_schemes_exclude_defaults =
      settings_.cookieable_schemes_exclude_defaults;
}

void CefContext::NormalizeRequestContextSettings(
    CefRequestContextSettings* settings) {
  // The |root_cache_path| value was already normalized in Initialize.
  const base::FilePath& root_cache_path = CefString(&settings_.root_cache_path);
  NormalizeCachePathAndSet(settings->cache_path, root_cache_path);
}

void CefContext::AddObserver(Observer* observer) {
  CEF_REQUIRE_UIT();
  observers_.AddObserver(observer);
}

void CefContext::RemoveObserver(Observer* observer) {
  CEF_REQUIRE_UIT();
  observers_.RemoveObserver(observer);
}

bool CefContext::HasObserver(Observer* observer) const {
  CEF_REQUIRE_UIT();
  return observers_.HasObserver(observer);
}

void CefContext::OnContextInitialized() {
  CEF_REQUIRE_UIT();

  if (application_) {
    // Notify the handler after the global browser context has initialized.
    CefRefPtr<CefRequestContext> request_context =
        CefRequestContext::GetGlobalContext();
    auto impl = static_cast<CefRequestContextImpl*>(request_context.get());
    impl->ExecuteWhenBrowserContextInitialized(base::BindOnce(
        [](CefRefPtr<CefApp> app) {
          CefRefPtr<CefBrowserProcessHandler> handler =
              app->GetBrowserProcessHandler();
          if (handler)
            handler->OnContextInitialized();
        },
        application_));
  }
}

void CefContext::ShutdownOnUIThread() {
  CEF_REQUIRE_UIT();

  browser_info_manager_->DestroyAllBrowsers();

  for (auto& observer : observers_)
    observer.OnContextDestroyed();

  if (trace_subscriber_.get())
    trace_subscriber_.reset(nullptr);
}

void CefContext::FinalizeShutdown() {
  browser_info_manager_.reset(nullptr);
  application_ = nullptr;
}
