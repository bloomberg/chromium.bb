// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/support/webkit_support.h"

#include "base/at_exit.h"
#include "base/base64.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/scoped_temp_dir.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/sys_string_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/url_util.h"
#include "grit/webkit_chromium_resources.h"
#include "media/base/filter_collection.h"
#include "media/base/media_log.h"
#include "media/base/message_loop_factory_impl.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileSystemCallbacks.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginParams.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLError.h"
#if defined(TOOLKIT_USES_GTK)
#include "ui/base/keycodes/keyboard_code_conversion_gtk.h"
#endif
#include "ui/gfx/gl/gl_context.h"
#include "ui/gfx/gl/gl_implementation.h"
#include "ui/gfx/gl/gl_surface.h"
#include "webkit/appcache/web_application_cache_host_impl.h"
#include "webkit/glue/user_agent.h"
#include "webkit/glue/webkit_constants.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webkitplatformsupport_impl.h"
#include "webkit/media/video_renderer_impl.h"
#include "webkit/media/webmediaplayer_impl.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/npapi/webplugin_impl.h"
#include "webkit/plugins/npapi/webplugin_page_delegate.h"
#include "webkit/plugins/webplugininfo.h"
#include "webkit/support/platform_support.h"
#include "webkit/support/simple_database_system.h"
#include "webkit/support/test_webkit_platform_support.h"
#include "webkit/support/test_webplugin_page_delegate.h"
#include "webkit/tools/test_shell/simple_file_system.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"

using WebKit::WebCString;
using WebKit::WebDevToolsAgentClient;
using WebKit::WebFileSystem;
using WebKit::WebFileSystemCallbacks;
using WebKit::WebFrame;
using WebKit::WebMediaPlayerClient;
using WebKit::WebPlugin;
using WebKit::WebPluginParams;
using WebKit::WebString;
using WebKit::WebURL;

namespace {

// All fatal log messages (e.g. DCHECK failures) imply unit test failures
void UnitTestAssertHandler(const std::string& str) {
  FAIL() << str;
}

void InitLogging(bool enable_gp_fault_error_box) {
  logging::SetLogAssertHandler(UnitTestAssertHandler);

#if defined(OS_WIN)
  if (!::IsDebuggerPresent()) {
    UINT new_flags = SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX;
    if (!enable_gp_fault_error_box)
      new_flags |= SEM_NOGPFAULTERRORBOX;

    // Preserve existing error mode, as discussed at
    // http://blogs.msdn.com/oldnewthing/archive/2004/07/27/198410.aspx
    UINT existing_flags = SetErrorMode(new_flags);
    SetErrorMode(existing_flags | new_flags);
  }
#endif

  FilePath log_filename;
  PathService::Get(base::DIR_EXE, &log_filename);
  log_filename = log_filename.AppendASCII("DumpRenderTree.log");
  logging::InitLogging(
      log_filename.value().c_str(),
      // Only log to a file. This prevents debugging output from disrupting
      // whether or not we pass.
      logging::LOG_ONLY_TO_FILE,
      // We might have multiple DumpRenderTree processes going at once.
      logging::LOCK_LOG_FILE,
      logging::DELETE_OLD_LOG_FILE,
      logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);

  // We want process and thread IDs because we may have multiple processes.
  const bool kProcessId = true;
  const bool kThreadId = true;
  const bool kTimestamp = true;
  const bool kTickcount = true;
  logging::SetLogItems(kProcessId, kThreadId, !kTimestamp, kTickcount);
}

class TestEnvironment {
 public:
#if defined(OS_ANDROID)
  // Android UI message loop goes through Java, so don't use it in tests.
  typedef MessageLoop MessageLoopType;
#else
  typedef MessageLoopForUI MessageLoopType;
#endif

  explicit TestEnvironment(bool unit_test_mode) {
    if (!unit_test_mode) {
      at_exit_manager_.reset(new base::AtExitManager);
      InitLogging(false);
    }
    main_message_loop_.reset(new MessageLoopType);
    // TestWebKitPlatformSupport must be instantiated after MessageLoopType.
    webkit_platform_support_.reset(
      new TestWebKitPlatformSupport(unit_test_mode));
  }

  ~TestEnvironment() {
    SimpleResourceLoaderBridge::Shutdown();
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

 private:
  scoped_ptr<base::AtExitManager> at_exit_manager_;
  scoped_ptr<MessageLoopType> main_message_loop_;
  scoped_ptr<TestWebKitPlatformSupport> webkit_platform_support_;
};

class WebPluginImplWithPageDelegate
    : public webkit_support::TestWebPluginPageDelegate,
      public base::SupportsWeakPtr<WebPluginImplWithPageDelegate>,
      public webkit::npapi::WebPluginImpl {
 public:
  WebPluginImplWithPageDelegate(WebFrame* frame,
                                const WebPluginParams& params,
                                const FilePath& path)
      : webkit_support::TestWebPluginPageDelegate(),
        webkit::npapi::WebPluginImpl(frame, params, path, AsWeakPtr()) {}
  virtual ~WebPluginImplWithPageDelegate() {}
 private:
  DISALLOW_COPY_AND_ASSIGN(WebPluginImplWithPageDelegate);
};

FilePath GetWebKitRootDirFilePath() {
  FilePath basePath;
  PathService::Get(base::DIR_SOURCE_ROOT, &basePath);
  if (file_util::PathExists(basePath.Append(FILE_PATH_LITERAL("chrome")))) {
    return basePath.Append(FILE_PATH_LITERAL("third_party/WebKit"));
  } else {
    // WebKit/Source/WebKit/chromium/ -> WebKit/
    return basePath.Append(FILE_PATH_LITERAL("../../.."));
  }
}

class WebKitClientMessageLoopImpl
    : public WebDevToolsAgentClient::WebKitClientMessageLoop {
 public:
  WebKitClientMessageLoopImpl() : message_loop_(MessageLoop::current()) {}
  virtual ~WebKitClientMessageLoopImpl() {
    message_loop_ = NULL;
  }
  virtual void run() {
    bool old_state = message_loop_->NestableTasksAllowed();
    message_loop_->SetNestableTasksAllowed(true);
    message_loop_->Run();
    message_loop_->SetNestableTasksAllowed(old_state);
  }
  virtual void quitNow() {
    message_loop_->QuitNow();
  }
 private:
  MessageLoop* message_loop_;
};

// An wrapper object for giving TaskAdaptor ref-countability,
// which NewRunnableMethod() requires.
class TaskAdaptorHolder : public CancelableTask {
 public:
  explicit TaskAdaptorHolder(webkit_support::TaskAdaptor* adaptor)
      : adaptor_(adaptor) {
  }

  virtual void Run() {
    adaptor_->Run();
  }

  virtual void Cancel() {
    adaptor_.reset();
  }

 private:
  scoped_ptr<webkit_support::TaskAdaptor> adaptor_;
};

webkit_support::GraphicsContext3DImplementation
    g_graphics_context_3d_implementation =
        webkit_support::IN_PROCESS_COMMAND_BUFFER;

}  // namespace

namespace webkit_support {

static TestEnvironment* test_environment;

static void SetUpTestEnvironmentImpl(bool unit_test_mode) {
  base::EnableInProcessStackDumping();
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
  BeforeInitialize(unit_test_mode);
  test_environment = new TestEnvironment(unit_test_mode);
  AfterInitialize(unit_test_mode);
  if (!unit_test_mode) {
    // Load ICU data tables.  This has to run after TestEnvironment is created
    // because on Linux, we need base::AtExitManager.
    icu_util::Initialize();
  }
  webkit_glue::SetUserAgent(webkit_glue::BuildUserAgentFromProduct(
      "DumpRenderTree/0.0.0.0"), false);
}

void SetUpTestEnvironment() {
  SetUpTestEnvironmentImpl(false);
}

void SetUpTestEnvironmentForUnitTests() {
  SetUpTestEnvironmentImpl(true);
}

void TearDownTestEnvironment() {
  // Flush any remaining messages before we kill ourselves.
  // http://code.google.com/p/chromium/issues/detail?id=9500
  MessageLoop::current()->RunAllPending();

  BeforeShutdown();
  WebKit::shutdown();
  delete test_environment;
  test_environment = NULL;
  AfterShutdown();
  logging::CloseLogFile();
}

WebKit::WebKitPlatformSupport* GetWebKitPlatformSupport() {
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

WebKit::WebMediaPlayer* CreateMediaPlayer(WebFrame* frame,
                                          WebMediaPlayerClient* client) {
  scoped_ptr<media::MessageLoopFactory> message_loop_factory(
      new media::MessageLoopFactoryImpl());

  scoped_ptr<media::FilterCollection> collection(
      new media::FilterCollection());

  scoped_ptr<webkit_media::WebMediaPlayerImpl> result(
      new webkit_media::WebMediaPlayerImpl(
          client,
          base::WeakPtr<webkit_media::WebMediaPlayerDelegate>(),
          collection.release(),
          message_loop_factory.release(),
          NULL,
          new media::MediaLog()));
  if (!result->Initialize(frame, false)) {
    return NULL;
  }
  return result.release();
}

WebKit::WebApplicationCacheHost* CreateApplicationCacheHost(
    WebFrame*, WebKit::WebApplicationCacheHostClient* client) {
  return SimpleAppCacheSystem::CreateApplicationCacheHost(client);
}

WebKit::WebString GetWebKitRootDir() {
  FilePath path = GetWebKitRootDirFilePath();
  std::string path_ascii = path.MaybeAsASCII();
  CHECK(!path_ascii.empty());
  return WebKit::WebString::fromUTF8(path_ascii.c_str());
}

void SetUpGLBindings(GLBindingPreferences bindingPref) {
  switch(bindingPref) {
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

void SetGraphicsContext3DImplementation(GraphicsContext3DImplementation impl) {
  g_graphics_context_3d_implementation = impl;
}

GraphicsContext3DImplementation GetGraphicsContext3DImplementation() {
  return g_graphics_context_3d_implementation;
}

void RegisterMockedURL(const WebKit::WebURL& url,
                     const WebKit::WebURLResponse& response,
                     const WebKit::WebString& file_path) {
  test_environment->webkit_platform_support()->url_loader_factory()->
      RegisterURL(url, response, file_path);
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

// Wrapper for debug_util
bool BeingDebugged() {
  return base::debug::BeingDebugged();
}

// Wrappers for MessageLoop

void RunMessageLoop() {
  MessageLoop::current()->Run();
}

void QuitMessageLoop() {
  MessageLoop::current()->Quit();
}

void RunAllPendingMessages() {
  MessageLoop::current()->RunAllPending();
}

bool MessageLoopNestableTasksAllowed() {
  return MessageLoop::current()->NestableTasksAllowed();
}

void MessageLoopSetNestableTasksAllowed(bool allowed) {
  MessageLoop::current()->SetNestableTasksAllowed(allowed);
}

void DispatchMessageLoop() {
  MessageLoop* current = MessageLoop::current();
  bool old_state = current->NestableTasksAllowed();
  current->SetNestableTasksAllowed(true);
  current->RunAllPending();
  current->SetNestableTasksAllowed(old_state);
}

WebDevToolsAgentClient::WebKitClientMessageLoop* CreateDevToolsMessageLoop() {
  return new WebKitClientMessageLoopImpl();
}

void PostDelayedTask(void (*func)(void*), void* context, int64 delay_ms) {
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, NewRunnableFunction(func, context), delay_ms);
}

void PostDelayedTask(TaskAdaptor* task, int64 delay_ms) {
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, new TaskAdaptorHolder(task), delay_ms);
}

// Wrappers for FilePath and file_util

WebString GetAbsoluteWebStringFromUTF8Path(const std::string& utf8_path) {
#if defined(OS_WIN)
  FilePath path(UTF8ToWide(utf8_path));
  file_util::AbsolutePath(&path);
  return WebString(path.value());
#else
  FilePath path(base::SysWideToNativeMB(base::SysUTF8ToWide(utf8_path)));
  file_util::AbsolutePath(&path);
  return WideToUTF16(base::SysNativeMBToWide(path.value()));
#endif
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
  return net::FilePathToFileURL(FilePath(wide_path_or_url));
#else
  return net::FilePathToFileURL(FilePath(path_or_url_in_nativemb));
#endif
}

WebURL RewriteLayoutTestsURL(const std::string& utf8_url) {
  const char kPrefix[] = "file:///tmp/LayoutTests/";
  const int kPrefixLen = arraysize(kPrefix) - 1;

  if (utf8_url.compare(0, kPrefixLen, kPrefix, kPrefixLen))
    return WebURL(GURL(utf8_url));

  FilePath replacePath =
      GetWebKitRootDirFilePath().Append(FILE_PATH_LITERAL("LayoutTests/"));
  CHECK(file_util::PathExists(replacePath)) << replacePath.value() <<
      " (re-written from " << utf8_url << ") does not exit";
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
  FilePath local_path;
  return net::FileURLToFilePath(fileUrl, &local_path)
      && file_util::SetCurrentDirectory(local_path.DirName());
}

WebURL LocalFileToDataURL(const WebURL& fileUrl) {
  FilePath local_path;
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

// A wrapper object for exporting ScopedTempDir to be used
// by webkit layout tests.
class ScopedTempDirectoryInternal : public ScopedTempDirectory {
 public:
   virtual bool CreateUniqueTempDir() {
     return tempDirectory_.CreateUniqueTempDir();
   }

   virtual std::string path() const {
     return tempDirectory_.path().MaybeAsASCII();
   }

 private:
   ScopedTempDir tempDirectory_;
};

ScopedTempDirectory* CreateScopedTempDirectory() {
  return new ScopedTempDirectoryInternal();
}

int64 GetCurrentTimeInMillisecond() {
  return base::TimeTicks::Now().ToInternalValue()
      / base::Time::kMicrosecondsPerMillisecond;
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
  } else
    DLOG(WARNING) << "Unknown error domain";

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

// Bridge for SimpleDatabaseSystem

void SetDatabaseQuota(int quota) {
  SimpleDatabaseSystem::GetInstance()->SetDatabaseQuota(quota);
}

void ClearAllDatabases() {
  SimpleDatabaseSystem::GetInstance()->ClearAllDatabases();
}

// Bridge for SimpleResourceLoaderBridge

void SetAcceptAllCookies(bool accept) {
  SimpleResourceLoaderBridge::SetAcceptAllCookies(accept);
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

// DevTools
WebURL GetDevToolsPathAsURL() {
  FilePath dirExe;
  if (!PathService::Get(base::DIR_EXE, &dirExe)) {
      DCHECK(false);
      return WebURL();
  }
  FilePath devToolsPath = dirExe.AppendASCII(
      "resources/inspector/devtools.html");
  return net::FilePathToFileURL(devToolsPath);
}

// FileSystem
void OpenFileSystem(WebFrame* frame, WebFileSystem::Type type,
    long long size, bool create, WebFileSystemCallbacks* callbacks) {
  SimpleFileSystem* fileSystem = static_cast<SimpleFileSystem*>(
      test_environment->webkit_platform_support()->fileSystem());
  fileSystem->OpenFileSystem(frame, type, size, create, callbacks);
}

// Keyboard code
#if defined(TOOLKIT_USES_GTK)
int NativeKeyCodeForWindowsKeyCode(int keycode, bool shift) {
  ui::KeyboardCode code = static_cast<ui::KeyboardCode>(keycode);
  return ui::GdkNativeKeyCodeForWindowsKeyCode(code, shift);
}
#endif

// Timers
double GetForegroundTabTimerInterval() {
  return webkit_glue::kForegroundTabTimerInterval;
}

// Logging
void EnableWebCoreLogChannels(const std::string& channels) {
  webkit_glue::EnableWebCoreLogChannels(channels);
}

void SetGamepadData(const WebKit::WebGamepads& pads) {
  test_environment->webkit_platform_support()->setGamepadData(pads);
}

}  // namespace webkit_support
