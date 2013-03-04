// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/support/webkit_support.h"

#include "base/at_exit.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/debug/stack_trace.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/sys_string_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "cc/thread_impl.h"
#include "googleurl/src/url_util.h"
#include "grit/webkit_chromium_resources.h"
#include "media/base/filter_collection.h"
#include "media/base/media_log.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCache.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileSystemCallbacks.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginParams.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageNamespace.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#if defined(TOOLKIT_GTK)
#include "ui/base/keycodes/keyboard_code_conversion_gtk.h"
#endif
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "webkit/appcache/web_application_cache_host_impl.h"
#include "webkit/base/file_path_string_conversions.h"
#include "webkit/compositor_bindings/web_compositor_support_impl.h"
#include "webkit/compositor_bindings/web_layer_tree_view_impl_for_testing.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/glue/webkit_constants.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webkitplatformsupport_impl.h"
#include "webkit/glue/webthread_impl.h"
#include "webkit/glue/weburlrequest_extradata_impl.h"
#include "webkit/gpu/test_context_provider_factory.h"
#include "webkit/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"
#include "webkit/gpu/webgraphicscontext3d_in_process_impl.h"
#if defined(OS_ANDROID)
#include "webkit/media/android/media_player_bridge_manager_impl.h"
#include "webkit/media/android/webmediaplayer_in_process_android.h"
#include "webkit/media/android/webmediaplayer_manager_android.h"
#endif
#include "webkit/media/media_stream_client.h"
#include "webkit/media/webmediaplayer_impl.h"
#include "webkit/media/webmediaplayer_ms.h"
#include "webkit/media/webmediaplayer_params.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/npapi/webplugin_impl.h"
#include "webkit/plugins/npapi/webplugin_page_delegate.h"
#include "webkit/plugins/webplugininfo.h"
#include "webkit/support/platform_support.h"
#include "webkit/support/simple_database_system.h"
#include "webkit/support/test_webidbfactory.h"
#include "webkit/support/test_webkit_platform_support.h"
#include "webkit/support/test_webplugin_page_delegate.h"
#include "webkit/tools/test_shell/simple_appcache_system.h"
#include "webkit/tools/test_shell/simple_dom_storage_system.h"
#include "webkit/tools/test_shell/simple_file_system.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "webkit/user_agent/user_agent.h"
#include "webkit/user_agent/user_agent_util.h"

#if defined(OS_ANDROID)
#include "base/test/test_support_android.h"
#include "webkit/support/test_stream_texture_factory_android.h"
#endif

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
using webkit::gpu::WebGraphicsContext3DInProcessImpl;
using webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl;

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
  }
#endif

#if defined(OS_ANDROID)
  // On Android we expect the log to appear in logcat.
  base::InitAndroidTestLogging();
#else
  base::FilePath log_filename;
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
#endif  // else defined(OS_ANDROID)
}

class TestEnvironment {
 public:
#if defined(OS_ANDROID)
  // Android UI message loop goes through Java, so don't use it in tests.
  typedef MessageLoop MessageLoopType;
#else
  typedef MessageLoopForUI MessageLoopType;
#endif

  TestEnvironment(bool unit_test_mode,
                  base::AtExitManager* existing_at_exit_manager,
                  WebKit::Platform* shadow_platform_delegate) {
    if (unit_test_mode) {
      logging::SetLogAssertHandler(UnitTestAssertHandler);
    } else {
      // The existing_at_exit_manager must be not NULL.
      at_exit_manager_.reset(existing_at_exit_manager);
      InitLogging();
    }
    main_message_loop_.reset(new MessageLoopType);

    // TestWebKitPlatformSupport must be instantiated after MessageLoopType.
    webkit_platform_support_.reset(
        new TestWebKitPlatformSupport(unit_test_mode,
                                      shadow_platform_delegate));

    idb_factory_.reset(new TestWebIDBFactory());
    WebKit::setIDBFactory(idb_factory_.get());

#if defined(OS_ANDROID)
    // Make sure we have enough decoding resources for layout tests.
    // The current maximum number of media elements in a layout test is 8.
    media_bridge_manager_.reset(
        new webkit_media::MediaPlayerBridgeManagerImpl(8));
    media_player_manager_.reset(
        new webkit_media::WebMediaPlayerManagerAndroid());
#endif
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

  webkit_media::WebMediaPlayerManagerAndroid* media_player_manager() {
    return media_player_manager_.get();
  }

  webkit_media::MediaPlayerBridgeManagerImpl* media_bridge_manager() {
    return media_bridge_manager_.get();
  }
#endif

 private:
  // Data member at_exit_manager_ will take the ownership of the input
  // AtExitManager and manage its lifecycle.
  scoped_ptr<base::AtExitManager> at_exit_manager_;
  scoped_ptr<MessageLoopType> main_message_loop_;
  scoped_ptr<TestWebKitPlatformSupport> webkit_platform_support_;
  scoped_ptr<TestWebIDBFactory> idb_factory_;

#if defined(OS_ANDROID)
  base::FilePath mock_current_directory_;
  scoped_ptr<webkit_media::WebMediaPlayerManagerAndroid> media_player_manager_;
  scoped_ptr<webkit_media::MediaPlayerBridgeManagerImpl> media_bridge_manager_;
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
  CHECK(file_util::AbsolutePath(&basePath));
  // We're in a WebKit-only, make-build, so the DIR_SOURCE_ROOT is already the
  // WebKit root. That, or we have no idea where we are.
  return basePath;
}

class WebKitClientMessageLoopImpl
    : public WebDevToolsAgentClient::WebKitClientMessageLoop {
 public:
  WebKitClientMessageLoopImpl() : message_loop_(MessageLoop::current()) {}
  virtual ~WebKitClientMessageLoopImpl() {
    message_loop_ = NULL;
  }
  virtual void run() {
    MessageLoop::ScopedNestableTaskAllower allow(message_loop_);
    message_loop_->Run();
  }
  virtual void quitNow() {
    message_loop_->QuitNow();
  }
 private:
  MessageLoop* message_loop_;
};

webkit_support::GraphicsContext3DImplementation
    g_graphics_context_3d_implementation =
        webkit_support::IN_PROCESS_COMMAND_BUFFER;

TestEnvironment* test_environment;

void SetUpTestEnvironmentImpl(bool unit_test_mode,
                              WebKit::Platform* shadow_platform_delegate) {
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
  base::AtExitManager* at_exit_manager = NULL;
  // In Android DumpRenderTree, AtExitManager is created in
  // testing/android/native_test_wrapper.cc before main() is called.
#if !defined(OS_ANDROID)
  // Some initialization code may use a AtExitManager before initializing
  // TestEnvironment, so we create a AtExitManager early and pass its ownership
  // to TestEnvironment.
  if (!unit_test_mode)
    at_exit_manager = new base::AtExitManager;
#endif
  webkit_support::BeforeInitialize(unit_test_mode);
  test_environment = new TestEnvironment(unit_test_mode, at_exit_manager,
                                         shadow_platform_delegate);
  webkit_support::AfterInitialize(unit_test_mode);
  if (!unit_test_mode) {
    // Load ICU data tables.  This has to run after TestEnvironment is created
    // because on Linux, we need base::AtExitManager.
    icu_util::Initialize();
  }
  webkit_glue::SetUserAgent(webkit_glue::BuildUserAgentFromProduct(
      "DumpRenderTree/0.0.0.0"), false);
}

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

void SetUpTestEnvironment() {
  SetUpTestEnvironment(NULL);
}

void SetUpTestEnvironmentForUnitTests() {
  SetUpTestEnvironmentForUnitTests(NULL);
}

void SetUpTestEnvironment(WebKit::Platform* shadow_platform_delegate) {
  SetUpTestEnvironmentImpl(false, shadow_platform_delegate);
}

void SetUpTestEnvironmentForUnitTests(
    WebKit::Platform* shadow_platform_delegate) {
  SetUpTestEnvironmentImpl(true, shadow_platform_delegate);
}

void TearDownTestEnvironment() {
  // Flush any remaining messages before we kill ourselves.
  // http://code.google.com/p/chromium/issues/detail?id=9500
  MessageLoop::current()->RunUntilIdle();

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

WebKit::WebMediaPlayer* CreateMediaPlayer(
    WebFrame* frame,
    const WebURL& url,
    WebMediaPlayerClient* client,
    webkit_media::MediaStreamClient* media_stream_client) {
  if (media_stream_client && media_stream_client->IsMediaStream(url)) {
    return new webkit_media::WebMediaPlayerMS(
        frame,
        client,
        base::WeakPtr<webkit_media::WebMediaPlayerDelegate>(),
        media_stream_client,
        new media::MediaLog());
  }

#if defined(OS_ANDROID)
  return new webkit_media::WebMediaPlayerInProcessAndroid(
      frame,
      client,
      GetWebKitPlatformSupport()->cookieJar(),
      test_environment->media_player_manager(),
      test_environment->media_bridge_manager(),
      new webkit_support::TestStreamTextureFactory(),
      true);
#else
  webkit_media::WebMediaPlayerParams params(
      NULL, NULL, new media::MediaLog());
  return new webkit_media::WebMediaPlayerImpl(
      frame,
      client,
      base::WeakPtr<webkit_media::WebMediaPlayerDelegate>(),
      params);
#endif
}

WebKit::WebMediaPlayer* CreateMediaPlayer(
    WebFrame* frame,
    const WebURL& url,
    WebMediaPlayerClient* client) {
  return CreateMediaPlayer(frame, url, client, NULL);
}

#if defined(OS_ANDROID)
void ReleaseMediaResources() {
  test_environment->media_player_manager()->ReleaseMediaResources();
}
#endif

WebKit::WebApplicationCacheHost* CreateApplicationCacheHost(
    WebFrame*, WebKit::WebApplicationCacheHostClient* client) {
  return SimpleAppCacheSystem::CreateApplicationCacheHost(client);
}

WebKit::WebStorageNamespace* CreateSessionStorageNamespace(unsigned quota) {
  return SimpleDomStorageSystem::instance().CreateSessionStorageNamespace();
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
  webkit::gpu::TestContextProviderFactory::SetUpFactoryForTesting(
      g_graphics_context_3d_implementation);
}

void SetGraphicsContext3DImplementation(GraphicsContext3DImplementation impl) {
  g_graphics_context_3d_implementation = impl;
}

GraphicsContext3DImplementation GetGraphicsContext3DImplementation() {
  return g_graphics_context_3d_implementation;
}

WebKit::WebGraphicsContext3D* CreateGraphicsContext3D(
    const WebKit::WebGraphicsContext3D::Attributes& attributes,
    WebKit::WebView* web_view) {
  switch (webkit_support::GetGraphicsContext3DImplementation()) {
    case webkit_support::IN_PROCESS:
      return WebGraphicsContext3DInProcessImpl::CreateForWebView(
          attributes, true /* direct */);
    case webkit_support::IN_PROCESS_COMMAND_BUFFER: {
      scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl> context(
          new WebGraphicsContext3DInProcessCommandBufferImpl());
      if (!context->Initialize(attributes, NULL))
        return NULL;
      return context.release();
    }
  }
  NOTREACHED();
  return NULL;
}

static WebKit::WebLayerTreeView* CreateLayerTreeView(
    LayerTreeViewType type,
    DRTLayerTreeViewClient* client,
    scoped_ptr<cc::Thread> compositor_thread) {
  scoped_ptr<WebKit::WebLayerTreeViewImplForTesting> view(
      new WebKit::WebLayerTreeViewImplForTesting(type, client));

  if (!view->initialize(compositor_thread.Pass()))
    return NULL;
  return view.release();
}

WebKit::WebLayerTreeView* CreateLayerTreeView(
    LayerTreeViewType type,
    DRTLayerTreeViewClient* client,
    WebKit::WebThread* thread) {
  scoped_ptr<cc::Thread> compositor_thread;
  if (thread)
    compositor_thread = cc::ThreadImpl::createForDifferentThread(
        static_cast<webkit_glue::WebThreadImpl*>(thread)->
        message_loop()->message_loop_proxy());
  return CreateLayerTreeView(type, client, compositor_thread.Pass());
}

// DEPRECATED. TODO(jamesr): Remove these three after fixing WebKit callers.
static WebKit::WebLayerTreeView* CreateLayerTreeView(
    LayerTreeViewType type,
    DRTLayerTreeViewClient* client) {
  scoped_ptr<cc::Thread> compositor_thread;

  webkit::WebCompositorSupportImpl* compositor_support_impl =
      test_environment->webkit_platform_support()->compositor_support_impl();
  if (compositor_support_impl->compositor_thread_message_loop_proxy())
    compositor_thread = cc::ThreadImpl::createForDifferentThread(
        compositor_support_impl->compositor_thread_message_loop_proxy());
  return CreateLayerTreeView(type, client, compositor_thread.Pass());
}
WebKit::WebLayerTreeView* CreateLayerTreeViewSoftware(
    DRTLayerTreeViewClient* client) {
  return CreateLayerTreeView(SOFTWARE_CONTEXT, client);
}
WebKit::WebLayerTreeView* CreateLayerTreeView3d(
    DRTLayerTreeViewClient* client) {
  return CreateLayerTreeView(MESA_CONTEXT, client);
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
  MessageLoop::current()->Run();
}

void QuitMessageLoop() {
  MessageLoop::current()->Quit();
}

void QuitMessageLoopNow() {
  MessageLoop::current()->QuitNow();
}

void RunAllPendingMessages() {
  MessageLoop::current()->RunUntilIdle();
}

bool MessageLoopNestableTasksAllowed() {
  return MessageLoop::current()->NestableTasksAllowed();
}

void MessageLoopSetNestableTasksAllowed(bool allowed) {
  MessageLoop::current()->SetNestableTasksAllowed(allowed);
}

void DispatchMessageLoop() {
  MessageLoop* current = MessageLoop::current();
  MessageLoop::ScopedNestableTaskAllower allow(current);
  current->RunUntilIdle();
}

WebDevToolsAgentClient::WebKitClientMessageLoop* CreateDevToolsMessageLoop() {
  return new WebKitClientMessageLoopImpl();
}

void PostDelayedTask(void (*func)(void*), void* context, int64 delay_ms) {
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(func, context),
      base::TimeDelta::FromMilliseconds(delay_ms));
}

void PostDelayedTask(TaskAdaptor* task, int64 delay_ms) {
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&TaskAdaptor::Run, base::Owned(task)),
      base::TimeDelta::FromMilliseconds(delay_ms));
}

// Wrappers for FilePath and file_util

WebString GetAbsoluteWebStringFromUTF8Path(const std::string& utf8_path) {
#if defined(OS_WIN)
  base::FilePath path(UTF8ToWide(utf8_path));
  file_util::AbsolutePath(&path);
  return WebString(path.value());
#else
  base::FilePath path(base::SysWideToNativeMB(base::SysUTF8ToWide(utf8_path)));
#if defined(OS_ANDROID)
  if (WebKit::layoutTestMode()) {
    // See comment of TestEnvironment::set_mock_current_directory().
    if (!path.IsAbsolute()) {
      // Not using FilePath::Append() because it can't handle '..' in path.
      DCHECK(test_environment);
      GURL base_url = net::FilePathToFileURL(
          test_environment->mock_current_directory()
              .Append(FILE_PATH_LITERAL("foo")));
      net::FileURLToFilePath(base_url.Resolve(path.value()), &path);
    }
  } else {
    file_util::AbsolutePath(&path);
  }
#else
  file_util::AbsolutePath(&path);
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
  file_util::AbsolutePath(&path);
  return net::FilePathToFileURL(path);
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

// FileSystem
void OpenFileSystem(WebFrame* frame, WebFileSystem::Type type,
    long long size, bool create, WebFileSystemCallbacks* callbacks) {
  SimpleFileSystem* fileSystem = static_cast<SimpleFileSystem*>(
      test_environment->webkit_platform_support()->fileSystem());
  fileSystem->OpenFileSystem(frame, type, size, create, callbacks);
}

void DeleteFileSystem(WebFrame* frame, WebFileSystem::Type type,
                      WebFileSystemCallbacks* callbacks) {
  SimpleFileSystem* fileSystem = static_cast<SimpleFileSystem*>(
      test_environment->webkit_platform_support()->fileSystem());
  fileSystem->DeleteFileSystem(frame, type, callbacks);
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
