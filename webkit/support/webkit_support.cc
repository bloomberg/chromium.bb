// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/support/webkit_support.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/debug/stack_trace.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time.h"
#include "googleurl/src/url_util.h"
#include "grit/webkit_chromium_resources.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebCache.h"
#include "third_party/WebKit/public/web/WebFileSystemCallbacks.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/WebKit/public/platform/WebStorageNamespace.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#if defined(TOOLKIT_GTK)
#include "ui/base/keycodes/keyboard_code_conversion_gtk.h"
#endif
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "webkit/base/file_path_string_conversions.h"
#include "webkit/browser/fileapi/isolated_context.h"
#include "webkit/child/webthread_impl.h"
#include "webkit/common/gpu/test_context_provider_factory.h"
#include "webkit/common/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"
#include "webkit/common/user_agent/user_agent.h"
#include "webkit/common/user_agent/user_agent_util.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webkitplatformsupport_impl.h"
#include "webkit/glue/weburlrequest_extradata_impl.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/npapi/webplugin_impl.h"
#include "webkit/plugins/npapi/webplugin_page_delegate.h"
#include "webkit/plugins/webplugininfo.h"
#include "webkit/renderer/appcache/web_application_cache_host_impl.h"
#include "webkit/renderer/compositor_bindings/web_compositor_support_impl.h"
#include "webkit/support/platform_support.h"
#include "webkit/support/test_webkit_platform_support.h"
#include "webkit/support/test_webplugin_page_delegate.h"
#include "webkit/support/web_layer_tree_view_impl_for_testing.h"

#if defined(OS_ANDROID)
#include "base/test/test_support_android.h"
#endif

using WebKit::WebCString;
using WebKit::WebDevToolsAgentClient;
using WebKit::WebFileSystem;
using WebKit::WebFileSystemCallbacks;
using WebKit::WebFrame;
using WebKit::WebPlugin;
using WebKit::WebPluginParams;
using WebKit::WebString;
using WebKit::WebURL;

namespace {

// All fatal log messages (e.g. DCHECK failures) imply unit test failures
void UnitTestAssertHandler(const std::string& str) {
  FAIL() << str;
}

void InitLogging() {
#if defined(OS_WIN)
  if (!::IsDebuggerPresent()) {
    UINT new_flags = SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX
        | SEM_NOGPFAULTERRORBOX;

    // Preserve existing error mode, as discussed at
    // http://blogs.msdn.com/oldnewthing/archive/2004/07/27/198410.aspx
    UINT existing_flags = SetErrorMode(new_flags);
    SetErrorMode(existing_flags | new_flags);

    // Don't pop up dialog on assertion failure, log to stdout instead.
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDOUT);
  }
#endif

#if defined(OS_ANDROID)
  // On Android we expect the log to appear in logcat.
  base::InitAndroidTestLogging();
#else
  base::FilePath log_filename;
  PathService::Get(base::DIR_EXE, &log_filename);
  log_filename = log_filename.AppendASCII("DumpRenderTree.log");
  logging::LoggingSettings settings;
  // Only log to a file. This prevents debugging output from disrupting
  // whether or not we pass.
  settings.logging_dest = logging::LOG_TO_FILE;
  settings.log_file = log_filename.value().c_str();
  settings.delete_old = logging::DELETE_OLD_LOG_FILE;
  logging::InitLogging(settings);

  // We want process and thread IDs because we may have multiple processes.
  const bool kProcessId = true;
  const bool kThreadId = true;
  const bool kTimestamp = true;
  const bool kTickcount = true;
  logging::SetLogItems(kProcessId, kThreadId, !kTimestamp, kTickcount);
#endif  // else defined(OS_ANDROID)
}

class TestEnvironment {
 public:
#if defined(OS_ANDROID)
  // Android UI message loop goes through Java, so don't use it in tests.
  typedef base::MessageLoop MessageLoopType;
#else
  typedef base::MessageLoopForUI MessageLoopType;
#endif

  TestEnvironment() {
    logging::SetLogAssertHandler(UnitTestAssertHandler);
    main_message_loop_.reset(new MessageLoopType);

    // TestWebKitPlatformSupport must be instantiated after MessageLoopType.
    webkit_platform_support_.reset(new TestWebKitPlatformSupport);
  }

  TestWebKitPlatformSupport* webkit_platform_support() const {
    return webkit_platform_support_.get();
  }

#if defined(OS_WIN) || defined(OS_MACOSX)
  void set_theme_engine(WebKit::WebThemeEngine* engine) {
    DCHECK(webkit_platform_support_ != 0);
    webkit_platform_support_->SetThemeEngine(engine);
  }

  WebKit::WebThemeEngine* theme_engine() const {
    return webkit_platform_support_->themeEngine();
  }
#endif

#if defined(OS_ANDROID)
  // On Android under layout test mode, we mock the current directory
  // in SetCurrentDirectoryForFileURL() and GetAbsoluteWebStringFromUTF8Path(),
  // as the directory might not exist on the device because we are using
  // file-over-http bridge.
  void set_mock_current_directory(const base::FilePath& directory) {
    mock_current_directory_ = directory;
  }

  base::FilePath mock_current_directory() const {
    return mock_current_directory_;
  }
#endif

 private:
  scoped_ptr<MessageLoopType> main_message_loop_;
  scoped_ptr<TestWebKitPlatformSupport> webkit_platform_support_;

#if defined(OS_ANDROID)
  base::FilePath mock_current_directory_;
#endif
};

class WebPluginImplWithPageDelegate
    : public webkit_support::TestWebPluginPageDelegate,
      public base::SupportsWeakPtr<WebPluginImplWithPageDelegate>,
      public webkit::npapi::WebPluginImpl {
 public:
  WebPluginImplWithPageDelegate(WebFrame* frame,
                                const WebPluginParams& params,
                                const base::FilePath& path)
      : webkit_support::TestWebPluginPageDelegate(),
        webkit::npapi::WebPluginImpl(frame, params, path, AsWeakPtr()) {}
  virtual ~WebPluginImplWithPageDelegate() {}
 private:
  DISALLOW_COPY_AND_ASSIGN(WebPluginImplWithPageDelegate);
};

base::FilePath GetWebKitRootDirFilePath() {
  base::FilePath basePath;
  PathService::Get(base::DIR_SOURCE_ROOT, &basePath);
  if (file_util::PathExists(
          basePath.Append(FILE_PATH_LITERAL("third_party/WebKit")))) {
    // We're in a WebKit-in-chrome checkout.
    basePath = basePath.Append(FILE_PATH_LITERAL("third_party/WebKit"));
  } else if (file_util::PathExists(
          basePath.Append(FILE_PATH_LITERAL("chromium")))) {
    // We're in a WebKit-only checkout on Windows.
    basePath = basePath.Append(FILE_PATH_LITERAL("../.."));
  } else if (file_util::PathExists(
          basePath.Append(FILE_PATH_LITERAL("webkit/support")))) {
    // We're in a WebKit-only/xcodebuild checkout on Mac
    basePath = basePath.Append(FILE_PATH_LITERAL("../../.."));
  }
  basePath = base::MakeAbsoluteFilePath(basePath);
  CHECK(!basePath.empty());
  // We're in a WebKit-only, make-build, so the DIR_SOURCE_ROOT is already the
  // WebKit root. That, or we have no idea where we are.
  return basePath;
}

class WebKitClientMessageLoopImpl
    : public WebDevToolsAgentClient::WebKitClientMessageLoop {
 public:
  WebKitClientMessageLoopImpl() : message_loop_(base::MessageLoop::current()) {}
  virtual ~WebKitClientMessageLoopImpl() { message_loop_ = NULL; }
  virtual void run() {
    base::MessageLoop::ScopedNestableTaskAllower allow(message_loop_);
    message_loop_->Run();
  }
  virtual void quitNow() {
    message_loop_->QuitNow();
  }
 private:
  base::MessageLoop* message_loop_;
};

TestEnvironment* test_environment;

}  // namespace

namespace webkit_support {

base::FilePath GetChromiumRootDirFilePath() {
  base::FilePath basePath;
  PathService::Get(base::DIR_SOURCE_ROOT, &basePath);
  if (file_util::PathExists(
          basePath.Append(FILE_PATH_LITERAL("third_party/WebKit")))) {
    // We're in a WebKit-in-chrome checkout.
    return basePath;
  }
  return GetWebKitRootDirFilePath()
         .Append(FILE_PATH_LITERAL("Source/WebKit/chromium"));
}

void SetUpTestEnvironmentForUnitTests() {
  base::debug::EnableInProcessStackDumping();
  base::EnableTerminationOnHeapCorruption();

  // Initialize the singleton CommandLine with fixed values.  Some code refer to
  // CommandLine::ForCurrentProcess().  We don't use the actual command-line
  // arguments of DRT to avoid unexpected behavior change.
  //
  // webkit/glue/plugin/plugin_list_posix.cc checks --debug-plugin-loading.
  // webkit/glue/plugin/plugin_list_win.cc checks --old-wmp.
  // If DRT needs these flags, specify them in the following kFixedArguments.
  const char* kFixedArguments[] = {"DumpRenderTree"};
  CommandLine::Init(arraysize(kFixedArguments), kFixedArguments);

  // Explicitly initialize the GURL library before spawning any threads.
  // Otherwise crash may happend when different threads try to create a GURL
  // at same time.
  url_util::Initialize();
  webkit_support::BeforeInitialize();
  test_environment = new TestEnvironment;
  webkit_support::AfterInitialize();
  webkit_glue::SetUserAgent(webkit_glue::BuildUserAgentFromProduct(
      "DumpRenderTree/0.0.0.0"), false);
}

void TearDownTestEnvironment() {
  // Flush any remaining messages before we kill ourselves.
  // http://code.google.com/p/chromium/issues/detail?id=9500
  base::RunLoop().RunUntilIdle();

  BeforeShutdown();
  if (RunningOnValgrind())
    WebKit::WebCache::clear();
  WebKit::shutdown();
  delete test_environment;
  test_environment = NULL;
  AfterShutdown();
  logging::CloseLogFile();
}

WebKit::Platform* GetWebKitPlatformSupport() {
  DCHECK(test_environment);
  return test_environment->webkit_platform_support();
}

WebPlugin* CreateWebPlugin(WebFrame* frame,
                           const WebPluginParams& params) {
  const bool kAllowWildcard = true;
  std::vector<webkit::WebPluginInfo> plugins;
  std::vector<std::string> mime_types;
  webkit::npapi::PluginList::Singleton()->GetPluginInfoArray(
      params.url, params.mimeType.utf8(), kAllowWildcard,
      NULL, &plugins, &mime_types);
  if (plugins.empty())
    return NULL;

  WebPluginParams new_params = params;
  new_params.mimeType = WebString::fromUTF8(mime_types.front());
  return new WebPluginImplWithPageDelegate(
      frame, new_params, plugins.front().path);
}

WebKit::WebString GetWebKitRootDir() {
  base::FilePath path = GetWebKitRootDirFilePath();
  std::string path_ascii = path.MaybeAsASCII();
  CHECK(!path_ascii.empty());
  return WebKit::WebString::fromUTF8(path_ascii.c_str());
}

void SetUpGLBindings(GLBindingPreferences bindingPref) {
  switch (bindingPref) {
    case GL_BINDING_DEFAULT:
      gfx::GLSurface::InitializeOneOff();
      break;
    case GL_BINDING_SOFTWARE_RENDERER:
      gfx::InitializeGLBindings(gfx::kGLImplementationOSMesaGL);
      break;
    default:
      NOTREACHED();
  }
}

WebKit::WebLayerTreeView* CreateLayerTreeView(
    LayerTreeViewType type,
    DRTLayerTreeViewClient* client,
    WebKit::WebThread* thread) {
  DCHECK(!thread);

  scoped_ptr<webkit::WebLayerTreeViewImplForTesting> view(
      new webkit::WebLayerTreeViewImplForTesting(type, client));

  if (!view->Initialize())
    return NULL;
  return view.release();
}

void RegisterMockedURL(const WebKit::WebURL& url,
                     const WebKit::WebURLResponse& response,
                     const WebKit::WebString& file_path) {
  test_environment->webkit_platform_support()->url_loader_factory()->
      RegisterURL(url, response, file_path);
}

void RegisterMockedErrorURL(const WebKit::WebURL& url,
                            const WebKit::WebURLResponse& response,
                            const WebKit::WebURLError& error) {
  test_environment->webkit_platform_support()->url_loader_factory()->
      RegisterErrorURL(url, response, error);
}

void UnregisterMockedURL(const WebKit::WebURL& url) {
  test_environment->webkit_platform_support()->url_loader_factory()->
    UnregisterURL(url);
}

void UnregisterAllMockedURLs() {
  test_environment->webkit_platform_support()->url_loader_factory()->
    UnregisterAllURLs();
}

void ServeAsynchronousMockedRequests() {
  test_environment->webkit_platform_support()->url_loader_factory()->
      ServeAsynchronousRequests();
}

WebKit::WebURLRequest GetLastHandledAsynchronousMockedRequest() {
  return test_environment->webkit_platform_support()->url_loader_factory()->
      GetLastHandledAsynchronousRequest();
}

// Wrapper for debug_util
bool BeingDebugged() {
  return base::debug::BeingDebugged();
}

// Wrappers for MessageLoop

void RunMessageLoop() {
  base::MessageLoop::current()->Run();
}

void QuitMessageLoop() {
  base::MessageLoop::current()->Quit();
}

void QuitMessageLoopNow() {
  base::MessageLoop::current()->QuitNow();
}

void RunAllPendingMessages() {
  base::RunLoop().RunUntilIdle();
}

bool MessageLoopNestableTasksAllowed() {
  return base::MessageLoop::current()->NestableTasksAllowed();
}

void MessageLoopSetNestableTasksAllowed(bool allowed) {
  base::MessageLoop::current()->SetNestableTasksAllowed(allowed);
}

void DispatchMessageLoop() {
  base::MessageLoop* current = base::MessageLoop::current();
  base::MessageLoop::ScopedNestableTaskAllower allow(current);
  base::RunLoop().RunUntilIdle();
}

WebDevToolsAgentClient::WebKitClientMessageLoop* CreateDevToolsMessageLoop() {
  return new WebKitClientMessageLoopImpl();
}

void PostDelayedTask(void (*func)(void*), void* context, int64 delay_ms) {
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(func, context),
      base::TimeDelta::FromMilliseconds(delay_ms));
}

void PostDelayedTask(TaskAdaptor* task, int64 delay_ms) {
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&TaskAdaptor::Run, base::Owned(task)),
      base::TimeDelta::FromMilliseconds(delay_ms));
}

// Wrappers for FilePath and file_util

WebString GetAbsoluteWebStringFromUTF8Path(const std::string& utf8_path) {
  // WebKit is happy with a relative path if the absolute path doesn't exist,
  // so we need to check the result of MakeAbsoluteFilePath().
#if defined(OS_WIN)
  base::FilePath orig_path(UTF8ToWide(utf8_path));
  base::FilePath path = base::MakeAbsoluteFilePath(orig_path);
  return WebString(path.empty() ? orig_path.value() : path.value());
#else
  base::FilePath orig_path(
      base::SysWideToNativeMB(base::SysUTF8ToWide(utf8_path)));
  base::FilePath path;
#if defined(OS_ANDROID)
  if (WebKit::layoutTestMode()) {
    // See comment of TestEnvironment::set_mock_current_directory().
    if (!orig_path.IsAbsolute()) {
      // Not using FilePath::Append() because it can't handle '..' in orig_path.
      DCHECK(test_environment);
      GURL base_url = net::FilePathToFileURL(
          test_environment->mock_current_directory()
              .Append(FILE_PATH_LITERAL("foo")));
      net::FileURLToFilePath(base_url.Resolve(orig_path.value()), &path);
    }
  } else {
    path = base::MakeAbsoluteFilePath(orig_path);
    if (path.empty())
      path = orig_path;
  }
#else
  path = base::MakeAbsoluteFilePath(orig_path);
  if (path.empty())
    path = orig_path;
#endif  // else defined(OS_ANDROID)
  return WideToUTF16(base::SysNativeMBToWide(path.value()));
#endif  // else defined(OS_WIN)
}

WebURL CreateURLForPathOrURL(const std::string& path_or_url_in_nativemb) {
  // NativeMB to UTF-8
  std::wstring wide_path_or_url
      = base::SysNativeMBToWide(path_or_url_in_nativemb);
  std::string path_or_url_in_utf8 = WideToUTF8(wide_path_or_url);

  GURL url(path_or_url_in_utf8);
  if (url.is_valid() && url.has_scheme())
    return WebURL(url);
#if defined(OS_WIN)
  base::FilePath path(wide_path_or_url);
#else
  base::FilePath path(path_or_url_in_nativemb);
#endif
  return net::FilePathToFileURL(base::MakeAbsoluteFilePath(path));
}

WebURL RewriteLayoutTestsURL(const std::string& utf8_url) {
  const char kPrefix[] = "file:///tmp/LayoutTests/";
  const int kPrefixLen = arraysize(kPrefix) - 1;

  if (utf8_url.compare(0, kPrefixLen, kPrefix, kPrefixLen))
    return WebURL(GURL(utf8_url));

  base::FilePath replacePath =
      GetWebKitRootDirFilePath().Append(FILE_PATH_LITERAL("LayoutTests/"));

  // On Android, the file is actually accessed through file-over-http. Disable
  // the following CHECK because the file is unlikely to exist on the device.
#if !defined(OS_ANDROID)
  CHECK(file_util::PathExists(replacePath)) << replacePath.value() <<
      " (re-written from " << utf8_url << ") does not exit";
#endif

#if defined(OS_WIN)
  std::string utf8_path = WideToUTF8(replacePath.value());
#else
  std::string utf8_path
      = WideToUTF8(base::SysNativeMBToWide(replacePath.value()));
#endif
  std::string newUrl = std::string("file://") + utf8_path
      + utf8_url.substr(kPrefixLen);
  return WebURL(GURL(newUrl));
}

bool SetCurrentDirectoryForFileURL(const WebKit::WebURL& fileUrl) {
  base::FilePath local_path;
  if (!net::FileURLToFilePath(fileUrl, &local_path))
    return false;
#if defined(OS_ANDROID)
  if (WebKit::layoutTestMode()) {
    // See comment of TestEnvironment::set_mock_current_directory().
    DCHECK(test_environment);
    base::FilePath directory = local_path.DirName();
    test_environment->set_mock_current_directory(directory);
    // Still try to actually change the directory, but ignore any error.
    // For a few tests that need to access resources directly as files
    // (e.g. blob tests) we still push the resources and need to chdir there.
    file_util::SetCurrentDirectory(directory);
    return true;
  }
#endif
  return file_util::SetCurrentDirectory(local_path.DirName());
}

WebURL LocalFileToDataURL(const WebURL& fileUrl) {
  base::FilePath local_path;
  if (!net::FileURLToFilePath(fileUrl, &local_path))
    return WebURL();

  std::string contents;
  if (!file_util::ReadFileToString(local_path, &contents))
    return WebURL();

  std::string contents_base64;
  if (!base::Base64Encode(contents, &contents_base64))
    return WebURL();

  const char kDataUrlPrefix[] = "data:text/css;charset=utf-8;base64,";
  return WebURL(GURL(kDataUrlPrefix + contents_base64));
}

// A wrapper object for exporting base::ScopedTempDir to be used
// by webkit layout tests.
class ScopedTempDirectoryInternal : public ScopedTempDirectory {
 public:
  virtual bool CreateUniqueTempDir() OVERRIDE {
    return tempDirectory_.CreateUniqueTempDir();
  }

  virtual std::string path() const OVERRIDE {
    return tempDirectory_.path().MaybeAsASCII();
  }

 private:
  base::ScopedTempDir tempDirectory_;
};

ScopedTempDirectory* CreateScopedTempDirectory() {
  return new ScopedTempDirectoryInternal();
}

int64 GetCurrentTimeInMillisecond() {
  return base::TimeDelta(base::Time::Now() -
                         base::Time::UnixEpoch()).ToInternalValue() /
         base::Time::kMicrosecondsPerMillisecond;
}

std::string EscapePath(const std::string& path) {
  return net::EscapePath(path);
}

std::string MakeURLErrorDescription(const WebKit::WebURLError& error) {
  std::string domain = error.domain.utf8();
  int code = error.reason;

  if (domain == net::kErrorDomain) {
    domain = "NSURLErrorDomain";
    switch (error.reason) {
    case net::ERR_ABORTED:
      code = -999;  // NSURLErrorCancelled
      break;
    case net::ERR_UNSAFE_PORT:
      // Our unsafe port checking happens at the network stack level, but we
      // make this translation here to match the behavior of stock WebKit.
      domain = "WebKitErrorDomain";
      code = 103;
      break;
    case net::ERR_ADDRESS_INVALID:
    case net::ERR_ADDRESS_UNREACHABLE:
    case net::ERR_NETWORK_ACCESS_DENIED:
      code = -1004;  // NSURLErrorCannotConnectToHost
      break;
    }
  } else {
    DLOG(WARNING) << "Unknown error domain";
  }

  return base::StringPrintf("<NSError domain %s, code %d, failing URL \"%s\">",
      domain.c_str(), code, error.unreachableURL.spec().data());
}

WebKit::WebURLError CreateCancelledError(const WebKit::WebURLRequest& request) {
  WebKit::WebURLError error;
  error.domain = WebKit::WebString::fromUTF8(net::kErrorDomain);
  error.reason = net::ERR_ABORTED;
  error.unreachableURL = request.url();
  return error;
}

WebKit::WebURLRequest::ExtraData* CreateWebURLRequestExtraData(
    WebKit::WebReferrerPolicy referrer_policy) {
  return new webkit_glue::WebURLRequestExtraDataImpl(referrer_policy,
                                                     WebKit::WebString());
}

// Theme engine
#if defined(OS_WIN) || defined(OS_MACOSX)

void SetThemeEngine(WebKit::WebThemeEngine* engine) {
  DCHECK(test_environment);
  test_environment->set_theme_engine(engine);
}

WebKit::WebThemeEngine* GetThemeEngine() {
  DCHECK(test_environment);
  return test_environment->theme_engine();
}

#endif

// DevTools frontend path for inspector LayoutTests.
WebURL GetDevToolsPathAsURL() {
  base::FilePath dirExe;
  if (!PathService::Get(base::DIR_EXE, &dirExe)) {
      DCHECK(false);
      return WebURL();
  }
#if defined(OS_MACOSX)
  dirExe = dirExe.AppendASCII("../../..");
#endif
  base::FilePath devToolsPath = dirExe.AppendASCII(
      "resources/inspector/devtools.html");
  return net::FilePathToFileURL(devToolsPath);
}

WebKit::WebString RegisterIsolatedFileSystem(
    const WebKit::WebVector<WebKit::WebString>& filenames) {
  fileapi::IsolatedContext::FileInfoSet files;
  for (size_t i = 0; i < filenames.size(); ++i) {
    base::FilePath path = webkit_base::WebStringToFilePath(filenames[i]);
    files.AddPath(path, NULL);
  }
  std::string filesystemId =
      fileapi::IsolatedContext::GetInstance()->RegisterDraggedFileSystem(files);
  return UTF8ToUTF16(filesystemId);
}

// Keyboard code
#if defined(TOOLKIT_GTK)
int NativeKeyCodeForWindowsKeyCode(int keycode, bool shift) {
  ui::KeyboardCode code = static_cast<ui::KeyboardCode>(keycode);
  return ui::GdkNativeKeyCodeForWindowsKeyCode(code, shift);
}
#endif

// Logging
void EnableWebCoreLogChannels(const std::string& channels) {
  webkit_glue::EnableWebCoreLogChannels(channels);
}

void SetGamepadData(const WebKit::WebGamepads& pads) {
  test_environment->webkit_platform_support()->setGamepadData(pads);
}

}  // namespace webkit_support
