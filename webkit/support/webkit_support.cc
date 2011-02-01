// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/support/webkit_support.h"

#include "app/gfx/gl/gl_context.h"
#include "app/gfx/gl/gl_implementation.h"
#include "base/at_exit.h"
#include "base/base64.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/sys_string_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/weak_ptr.h"
#include "grit/webkit_chromium_resources.h"
#include "media/base/filter_collection.h"
#include "media/base/message_loop_factory_impl.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileSystemCallbacks.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginParams.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLError.h"
#include "webkit/appcache/web_application_cache_host_impl.h"
#include "webkit/glue/media/video_renderer_impl.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webkitclient_impl.h"
#include "webkit/glue/webmediaplayer_impl.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/npapi/webplugin_impl.h"
#include "webkit/plugins/npapi/webplugin_page_delegate.h"
#include "webkit/plugins/npapi/webplugininfo.h"
#include "webkit/support/platform_support.h"
#include "webkit/support/test_webplugin_page_delegate.h"
#include "webkit/support/test_webkit_client.h"
#include "webkit/tools/test_shell/simple_database_system.h"
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
  explicit TestEnvironment(bool unit_test_mode) {
    if (!unit_test_mode) {
      at_exit_manager_.reset(new base::AtExitManager);
      InitLogging(false);
    }
    main_message_loop_.reset(new MessageLoopForUI);
    // TestWebKitClient must be instantiated after the MessageLoopForUI.
    webkit_client_.reset(new TestWebKitClient(unit_test_mode));
  }

  ~TestEnvironment() {
    SimpleResourceLoaderBridge::Shutdown();
  }

  TestWebKitClient* webkit_client() const { return webkit_client_.get(); }

#if defined(OS_WIN) || defined(OS_MACOSX)
  void set_theme_engine(WebKit::WebThemeEngine* engine) {
    DCHECK(webkit_client_ != 0);
    webkit_client_->SetThemeEngine(engine);
  }

  WebKit::WebThemeEngine* theme_engine() const {
    return webkit_client_->themeEngine();
  }
#endif

 private:
  scoped_ptr<base::AtExitManager> at_exit_manager_;
  scoped_ptr<MessageLoopForUI> main_message_loop_;
  scoped_ptr<TestWebKitClient> webkit_client_;
};

class WebPluginImplWithPageDelegate
    : public webkit_support::TestWebPluginPageDelegate,
      public base::SupportsWeakPtr<WebPluginImplWithPageDelegate>,
      public webkit::npapi::WebPluginImpl {
 public:
  WebPluginImplWithPageDelegate(WebFrame* frame,
                                const WebPluginParams& params,
                                const FilePath& path,
                                const std::string& mime_type)
      : webkit_support::TestWebPluginPageDelegate(),
        webkit::npapi::WebPluginImpl(
            frame, params, path, mime_type, AsWeakPtr()) {}
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
  // webkit/glue/webmediaplayer_impl.cc checks --enable-openmax.
  // webkit/glue/plugin/plugin_list_posix.cc checks --debug-plugin-loading.
  // webkit/glue/plugin/plugin_list_win.cc checks --old-wmp.
  // If DRT needs these flags, specify them in the following kFixedArguments.
  const char* kFixedArguments[] = {"DumpRenderTree"};
  CommandLine::Init(arraysize(kFixedArguments), kFixedArguments);

  webkit_support::BeforeInitialize(unit_test_mode);
  webkit_support::test_environment = new TestEnvironment(unit_test_mode);
  webkit_support::AfterInitialize(unit_test_mode);
  if (!unit_test_mode) {
    // Load ICU data tables.  This has to run after TestEnvironment is created
    // because on Linux, we need base::AtExitManager.
    icu_util::Initialize();
  }
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

WebKit::WebKitClient* GetWebKitClient() {
  DCHECK(test_environment);
  return test_environment->webkit_client();
}

WebPlugin* CreateWebPlugin(WebFrame* frame,
                           const WebPluginParams& params) {
  const bool kAllowWildcard = true;
  webkit::npapi::WebPluginInfo info;
  std::string actual_mime_type;
  if (!webkit::npapi::PluginList::Singleton()->GetPluginInfo(
          params.url, params.mimeType.utf8(), kAllowWildcard, &info,
          &actual_mime_type) || !webkit::npapi::IsPluginEnabled(info)) {
    return NULL;
  }

  return new WebPluginImplWithPageDelegate(
      frame, params, info.path, actual_mime_type);
}

WebKit::WebMediaPlayer* CreateMediaPlayer(WebFrame* frame,
                                          WebMediaPlayerClient* client) {
  scoped_ptr<media::MessageLoopFactory> message_loop_factory(
      new media::MessageLoopFactoryImpl());

  scoped_ptr<media::FilterCollection> collection(
      new media::FilterCollection());

  scoped_refptr<webkit_glue::VideoRendererImpl> video_renderer(
      new webkit_glue::VideoRendererImpl(false));
  collection->AddVideoRenderer(video_renderer);

  scoped_ptr<webkit_glue::WebMediaPlayerImpl> result(
      new webkit_glue::WebMediaPlayerImpl(client,
                                          collection.release(),
                                          message_loop_factory.release()));
  if (!result->Initialize(frame, false, video_renderer)) {
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
  return WebKit::WebString::fromUTF8(WideToUTF8(path.ToWStringHack()).c_str());
}

void SetUpGLBindings(GLBindingPreferences bindingPref) {
  switch(bindingPref) {
    case GL_BINDING_DEFAULT:
      gfx::GLContext::InitializeOneOff();
      break;
    case GL_BINDING_SOFTWARE_RENDERER:
      gfx::InitializeGLBindings(gfx::kGLImplementationOSMesaGL);
      break;
    default:
      NOTREACHED();
  }
}

void RegisterMockedURL(const WebKit::WebURL& url,
                     const WebKit::WebURLResponse& response,
                     const WebKit::WebString& file_path) {
  test_environment->webkit_client()->url_loader_factory()->
      RegisterURL(url, response, file_path);
}

void UnregisterMockedURL(const WebKit::WebURL& url) {
  test_environment->webkit_client()->url_loader_factory()->UnregisterURL(url);
}

void UnregisterAllMockedURLs() {
  test_environment->webkit_client()->url_loader_factory()->UnregisterAllURLs();
}

void ServeAsynchronousMockedRequests() {
  test_environment->webkit_client()->url_loader_factory()->
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

int64 GetCurrentTimeInMillisecond() {
  return base::TimeTicks::Now().ToInternalValue()
      / base::Time::kMicrosecondsPerMillisecond;
}

std::string EscapePath(const std::string& path) {
  return ::EscapePath(path);
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
WebCString GetDevToolsDebuggerScriptSource() {
  base::StringPiece debuggerScriptJS = webkit_glue::GetDataResource(
      IDR_DEVTOOLS_DEBUGGER_SCRIPT_JS);
  return WebCString(debuggerScriptJS.as_string().c_str());
}

WebURL GetDevToolsPathAsURL() {
  FilePath dirExe;
  if (!webkit_glue::GetExeDirectory(&dirExe)) {
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
      test_environment->webkit_client()->fileSystem());
  fileSystem->OpenFileSystem(frame, type, size, create, callbacks);
}

}  // namespace webkit_support
