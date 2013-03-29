// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/support/test_webkit_platform_support.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/stats_counters.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "cc/base/thread_impl.h"
#include "cc/output/context_provider.h"
#include "media/base/media.h"
#include "net/cookies/cookie_monster.h"
#include "net/http/http_cache.h"
#include "net/test/test_server.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebAudioDevice.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebData.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFileSystem.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGamepads.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebStorageArea.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebStorageNamespace.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDatabase.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRuntimeFeatures.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptController.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityPolicy.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageEventDispatcher.h"
#include "v8/include/v8.h"
#include "webkit/appcache/web_application_cache_host_impl.h"
#include "webkit/compositor_bindings/web_compositor_support_impl.h"
#include "webkit/compositor_bindings/web_layer_tree_view_impl_for_testing.h"
#include "webkit/database/vfs_backend.h"
#include "webkit/glue/simple_webmimeregistry_impl.h"
#include "webkit/glue/webclipboard_impl.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webkitplatformsupport_impl.h"
#include "webkit/gpu/test_context_provider_factory.h"
#include "webkit/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/support/gc_extension.h"
#include "webkit/support/simple_database_system.h"
#include "webkit/support/test_webmessageportchannel.h"
#include "webkit/support/web_audio_device_mock.h"
#include "webkit/support/web_gesture_curve_mock.h"
#include "webkit/support/webkit_support.h"
#include "webkit/support/weburl_loader_mock_factory.h"
#include "webkit/tools/test_shell/mock_webclipboard_impl.h"
#include "webkit/tools/test_shell/simple_appcache_system.h"
#include "webkit/tools/test_shell/simple_file_system.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "webkit/tools/test_shell/simple_socket_stream_bridge.h"
#include "webkit/tools/test_shell/simple_webcookiejar_impl.h"
#include "webkit/tools/test_shell/test_shell_request_context.h"
#include "webkit/tools/test_shell/test_shell_webblobregistry_impl.h"

#if defined(OS_WIN)
#include "third_party/WebKit/Source/Platform/chromium/public/win/WebThemeEngine.h"
#include "webkit/tools/test_shell/test_shell_webthemeengine.h"
#elif defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

using WebKit::WebScriptController;
using webkit::WebLayerTreeViewImplForTesting;

TestWebKitPlatformSupport::TestWebKitPlatformSupport(bool unit_test_mode,
    WebKit::Platform* shadow_platform_delegate)
    : unit_test_mode_(unit_test_mode),
      shadow_platform_delegate_(shadow_platform_delegate),
      threaded_compositing_enabled_(false) {
  v8::V8::SetCounterFunction(base::StatsTable::FindLocation);

  WebKit::initialize(this);
  WebKit::setLayoutTestMode(true);
  WebKit::WebSecurityPolicy::registerURLSchemeAsLocal(
      WebKit::WebString::fromUTF8("test-shell-resource"));
  WebKit::WebSecurityPolicy::registerURLSchemeAsNoAccess(
      WebKit::WebString::fromUTF8("test-shell-resource"));
  WebKit::WebSecurityPolicy::registerURLSchemeAsDisplayIsolated(
      WebKit::WebString::fromUTF8("test-shell-resource"));
  WebKit::WebSecurityPolicy::registerURLSchemeAsEmptyDocument(
      WebKit::WebString::fromUTF8("test-shell-resource"));
  WebScriptController::enableV8SingleThreadMode();
  WebKit::WebRuntimeFeatures::enableSockets(true);
  WebKit::WebRuntimeFeatures::enableApplicationCache(true);
  WebKit::WebRuntimeFeatures::enableDatabase(true);
  WebKit::WebRuntimeFeatures::enableDataTransferItems(true);
  WebKit::WebRuntimeFeatures::enableNotifications(true);
  WebKit::WebRuntimeFeatures::enableTouch(true);
  WebKit::WebRuntimeFeatures::enableGamepad(true);

  // Load libraries for media and enable the media player.
  bool enable_media = false;
  base::FilePath module_path;
  if (PathService::Get(base::DIR_MODULE, &module_path)) {
#if defined(OS_MACOSX)
    if (base::mac::AmIBundled())
      module_path = module_path.DirName().DirName().DirName();
#endif
    if (media::InitializeMediaLibrary(module_path))
      enable_media = true;
  }
  WebKit::WebRuntimeFeatures::enableMediaPlayer(enable_media);
  LOG_IF(WARNING, !enable_media) << "Failed to initialize the media library.\n";

  // TODO(joth): Make a dummy geolocation service implemenation for
  // test_shell, and set this to true. http://crbug.com/36451
  WebKit::WebRuntimeFeatures::enableGeolocation(false);

  // Construct and initialize an appcache system for this scope.
  // A new empty temp directory is created to house any cached
  // content during the run. Upon exit that directory is deleted.
  // If we can't create a tempdir, we'll use in-memory storage.
  if (!appcache_dir_.CreateUniqueTempDir()) {
    LOG(WARNING) << "Failed to create a temp dir for the appcache, "
                    "using in-memory storage.";
    DCHECK(appcache_dir_.path().empty());
  }
  SimpleAppCacheSystem::InitializeOnUIThread(appcache_dir_.path());

  WebKit::WebDatabase::setObserver(&database_system_);

  blob_registry_ = new TestShellWebBlobRegistryImpl();

  file_utilities_.set_sandbox_enabled(false);

  if (!file_system_root_.CreateUniqueTempDir()) {
    LOG(WARNING) << "Failed to create a temp dir for the filesystem."
                    "FileSystem feature will be disabled.";
    DCHECK(file_system_root_.path().empty());
  }

  {
    // Initialize the hyphen library with a sample dictionary.
    base::FilePath path = webkit_support::GetChromiumRootDirFilePath();
    path = path.Append(FILE_PATH_LITERAL("third_party/hyphen/hyph_en_US.dic"));
    base::PlatformFile dict_file = base::CreatePlatformFile(
        path,
        base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ,
        NULL, NULL);
    hyphenator_.LoadDictionary(dict_file);
  }

#if defined(OS_WIN)
  // Ensure we pick up the default theme engine.
  SetThemeEngine(NULL);
#endif

  net::HttpCache::Mode cache_mode = net::HttpCache::NORMAL;
  net::CookieMonster::EnableFileScheme();

  // Initializing with a default context, which means no on-disk cookie DB,
  // and no support for directory listings.
  SimpleResourceLoaderBridge::Init(base::FilePath(), cache_mode, true);

  // Test shell always exposes the GC.
  webkit_glue::SetJavaScriptFlags(" --expose-gc");
  // Expose GCController to JavaScript.
  WebScriptController::registerExtension(extensions_v8::GCExtension::Get());
}

TestWebKitPlatformSupport::~TestWebKitPlatformSupport() {
}

WebKit::WebMimeRegistry* TestWebKitPlatformSupport::mimeRegistry() {
  return &mime_registry_;
}

WebKit::WebClipboard* TestWebKitPlatformSupport::clipboard() {
  // Mock out clipboard calls so that tests don't mess
  // with each other's copies/pastes when running in parallel.
  return &mock_clipboard_;
}

WebKit::WebFileUtilities* TestWebKitPlatformSupport::fileUtilities() {
  return &file_utilities_;
}

WebKit::WebSandboxSupport* TestWebKitPlatformSupport::sandboxSupport() {
  return NULL;
}

WebKit::WebCookieJar* TestWebKitPlatformSupport::cookieJar() {
  return &cookie_jar_;
}

WebKit::WebBlobRegistry* TestWebKitPlatformSupport::blobRegistry() {
  return blob_registry_.get();
}

WebKit::WebFileSystem* TestWebKitPlatformSupport::fileSystem() {
  return &file_system_;
}

WebKit::WebHyphenator* TestWebKitPlatformSupport::hyphenator() {
  return &hyphenator_;
}

bool TestWebKitPlatformSupport::sandboxEnabled() {
  return true;
}

WebKit::Platform::FileHandle
TestWebKitPlatformSupport::databaseOpenFile(
    const WebKit::WebString& vfs_file_name, int desired_flags) {
  return SimpleDatabaseSystem::GetInstance()->OpenFile(
      vfs_file_name, desired_flags);
}

int TestWebKitPlatformSupport::databaseDeleteFile(
    const WebKit::WebString& vfs_file_name, bool sync_dir) {
  return SimpleDatabaseSystem::GetInstance()->DeleteFile(
      vfs_file_name, sync_dir);
}

long TestWebKitPlatformSupport::databaseGetFileAttributes(
    const WebKit::WebString& vfs_file_name) {
  return SimpleDatabaseSystem::GetInstance()->GetFileAttributes(
      vfs_file_name);
}

long long TestWebKitPlatformSupport::databaseGetFileSize(
    const WebKit::WebString& vfs_file_name) {
  return SimpleDatabaseSystem::GetInstance()->GetFileSize(vfs_file_name);
}

long long TestWebKitPlatformSupport::databaseGetSpaceAvailableForOrigin(
    const WebKit::WebString& origin_identifier) {
  return SimpleDatabaseSystem::GetInstance()->GetSpaceAvailable(
      origin_identifier);
}

unsigned long long TestWebKitPlatformSupport::visitedLinkHash(
    const char* canonicalURL, size_t length) {
  return 0;
}

bool TestWebKitPlatformSupport::isLinkVisited(unsigned long long linkHash) {
  return false;
}

WebKit::WebMessagePortChannel*
TestWebKitPlatformSupport::createMessagePortChannel() {
  return new TestWebMessagePortChannel();
}

void TestWebKitPlatformSupport::prefetchHostName(const WebKit::WebString&) {
}

WebKit::WebURLLoader* TestWebKitPlatformSupport::createURLLoader() {
  if (!unit_test_mode_)
    return webkit_glue::WebKitPlatformSupportImpl::createURLLoader();
  return url_loader_factory_.CreateURLLoader(
      webkit_glue::WebKitPlatformSupportImpl::createURLLoader());
}

WebKit::WebData TestWebKitPlatformSupport::loadResource(const char* name) {
  if (!strcmp(name, "deleteButton")) {
    // Create a red 30x30 square.
    const char red_square[] =
        "\x89\x50\x4e\x47\x0d\x0a\x1a\x0a\x00\x00\x00\x0d\x49\x48\x44\x52"
        "\x00\x00\x00\x1e\x00\x00\x00\x1e\x04\x03\x00\x00\x00\xc9\x1e\xb3"
        "\x91\x00\x00\x00\x30\x50\x4c\x54\x45\x00\x00\x00\x80\x00\x00\x00"
        "\x80\x00\x80\x80\x00\x00\x00\x80\x80\x00\x80\x00\x80\x80\x80\x80"
        "\x80\xc0\xc0\xc0\xff\x00\x00\x00\xff\x00\xff\xff\x00\x00\x00\xff"
        "\xff\x00\xff\x00\xff\xff\xff\xff\xff\x7b\x1f\xb1\xc4\x00\x00\x00"
        "\x09\x70\x48\x59\x73\x00\x00\x0b\x13\x00\x00\x0b\x13\x01\x00\x9a"
        "\x9c\x18\x00\x00\x00\x17\x49\x44\x41\x54\x78\x01\x63\x98\x89\x0a"
        "\x18\x50\xb9\x33\x47\xf9\xa8\x01\x32\xd4\xc2\x03\x00\x33\x84\x0d"
        "\x02\x3a\x91\xeb\xa5\x00\x00\x00\x00\x49\x45\x4e\x44\xae\x42\x60"
        "\x82";
    return WebKit::WebData(red_square, arraysize(red_square));
  }
  return webkit_glue::WebKitPlatformSupportImpl::loadResource(name);
}

WebKit::WebString TestWebKitPlatformSupport::queryLocalizedString(
    WebKit::WebLocalizedString::Name name) {
  // Returns messages same as WebKit's in DRT.
  // We use different strings for form validation messages and page popup UI
  // strings.
  switch (name) {
    case WebKit::WebLocalizedString::ValidationValueMissing:
    case WebKit::WebLocalizedString::ValidationValueMissingForCheckbox:
    case WebKit::WebLocalizedString::ValidationValueMissingForFile:
    case WebKit::WebLocalizedString::ValidationValueMissingForMultipleFile:
    case WebKit::WebLocalizedString::ValidationValueMissingForRadio:
    case WebKit::WebLocalizedString::ValidationValueMissingForSelect:
      return ASCIIToUTF16("value missing");
    case WebKit::WebLocalizedString::ValidationTypeMismatch:
    case WebKit::WebLocalizedString::ValidationTypeMismatchForEmail:
    case WebKit::WebLocalizedString::ValidationTypeMismatchForMultipleEmail:
    case WebKit::WebLocalizedString::ValidationTypeMismatchForURL:
      return ASCIIToUTF16("type mismatch");
    case WebKit::WebLocalizedString::ValidationPatternMismatch:
      return ASCIIToUTF16("pattern mismatch");
    case WebKit::WebLocalizedString::ValidationTooLong:
      return ASCIIToUTF16("too long");
    case WebKit::WebLocalizedString::ValidationRangeUnderflow:
      return ASCIIToUTF16("range underflow");
    case WebKit::WebLocalizedString::ValidationRangeOverflow:
      return ASCIIToUTF16("range overflow");
    case WebKit::WebLocalizedString::ValidationStepMismatch:
      return ASCIIToUTF16("step mismatch");
    case WebKit::WebLocalizedString::OtherDateLabel:
      return ASCIIToUTF16("<<OtherDateLabel>>");
    case WebKit::WebLocalizedString::OtherMonthLabel:
      return ASCIIToUTF16("<<OtherMonthLabel>>");
    case WebKit::WebLocalizedString::OtherTimeLabel:
      return ASCIIToUTF16("<<OtherTimeLabel>>");
    case WebKit::WebLocalizedString::OtherWeekLabel:
      return ASCIIToUTF16("<<OtherWeekLabel>>");
    case WebKit::WebLocalizedString::CalendarClear:
      return ASCIIToUTF16("<<CalendarClear>>");
    case WebKit::WebLocalizedString::CalendarToday:
      return ASCIIToUTF16("<<CalendarToday>>");
    case WebKit::WebLocalizedString::ThisMonthButtonLabel:
      return ASCIIToUTF16("<<ThisMonthLabel>>");
    case WebKit::WebLocalizedString::ThisWeekButtonLabel:
      return ASCIIToUTF16("<<ThisWeekLabel>>");
    default:
      return WebKitPlatformSupportImpl::queryLocalizedString(name);
  }
}

WebKit::WebString TestWebKitPlatformSupport::queryLocalizedString(
    WebKit::WebLocalizedString::Name name, const WebKit::WebString& value) {
  if (name == WebKit::WebLocalizedString::ValidationRangeUnderflow)
    return ASCIIToUTF16("range underflow");
  if (name == WebKit::WebLocalizedString::ValidationRangeOverflow)
    return ASCIIToUTF16("range overflow");
  return WebKitPlatformSupportImpl::queryLocalizedString(name, value);
}

WebKit::WebString TestWebKitPlatformSupport::queryLocalizedString(
    WebKit::WebLocalizedString::Name name,
    const WebKit::WebString& value1,
    const WebKit::WebString& value2) {
  if (name == WebKit::WebLocalizedString::ValidationTooLong)
    return ASCIIToUTF16("too long");
  if (name == WebKit::WebLocalizedString::ValidationStepMismatch)
    return ASCIIToUTF16("step mismatch");
  return WebKitPlatformSupportImpl::queryLocalizedString(name, value1, value2);
}

WebKit::WebString TestWebKitPlatformSupport::defaultLocale() {
  return ASCIIToUTF16("en-US");
}

WebKit::WebStorageNamespace*
TestWebKitPlatformSupport::createLocalStorageNamespace(
    const WebKit::WebString& path, unsigned quota) {
  return dom_storage_system_.CreateLocalStorageNamespace();
}

#if defined(OS_WIN) || defined(OS_MACOSX)
void TestWebKitPlatformSupport::SetThemeEngine(WebKit::WebThemeEngine* engine) {
  active_theme_engine_ = engine ?
      engine : WebKitPlatformSupportImpl::themeEngine();
}

WebKit::WebThemeEngine* TestWebKitPlatformSupport::themeEngine() {
  return active_theme_engine_;
}
#endif

WebKit::WebGraphicsContext3D*
TestWebKitPlatformSupport::createOffscreenGraphicsContext3D(
    const WebKit::WebGraphicsContext3D::Attributes& attributes) {
  using webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl;
  return new WebGraphicsContext3DInProcessCommandBufferImpl(attributes);
}

WebKit::WebGraphicsContext3D*
TestWebKitPlatformSupport::sharedOffscreenGraphicsContext3D() {
  main_thread_contexts_ =
      webkit::gpu::TestContextProviderFactory::GetInstance()->
          OffscreenContextProviderForMainThread();
  if (!main_thread_contexts_)
    return NULL;
  return main_thread_contexts_->Context3d();
}

GrContext* TestWebKitPlatformSupport::sharedOffscreenGrContext() {
  if (!main_thread_contexts_)
    return NULL;
  return main_thread_contexts_->GrContext();
}

bool TestWebKitPlatformSupport::canAccelerate2dCanvas() {
  // We supply an OS-MESA based context for accelarated 2d
  // canvas, which should always work.
  return true;
}

bool TestWebKitPlatformSupport::isThreadedCompositingEnabled() {
  return threaded_compositing_enabled_;
}

double TestWebKitPlatformSupport::audioHardwareSampleRate() {
  return 44100.0;
}

size_t TestWebKitPlatformSupport::audioHardwareBufferSize() {
  return 128;
}

WebKit::WebAudioDevice* TestWebKitPlatformSupport::createAudioDevice(
    size_t bufferSize, unsigned numberOfInputChannels,
    unsigned numberOfChannels, double sampleRate,
    WebKit::WebAudioDevice::RenderCallback*,
    const WebKit::WebString& input_device_id) {
  return new WebAudioDeviceMock(sampleRate);
}

// TODO(crogers): remove once WebKit switches to new API.
WebKit::WebAudioDevice* TestWebKitPlatformSupport::createAudioDevice(
    size_t bufferSize, unsigned numberOfInputChannels,
    unsigned numberOfChannels, double sampleRate,
    WebKit::WebAudioDevice::RenderCallback*) {
  return new WebAudioDeviceMock(sampleRate);
}

// TODO(crogers): remove once WebKit switches to new API.
WebKit::WebAudioDevice* TestWebKitPlatformSupport::createAudioDevice(
    size_t bufferSize, unsigned numberOfChannels, double sampleRate,
    WebKit::WebAudioDevice::RenderCallback*) {
  return new WebAudioDeviceMock(sampleRate);
}

void TestWebKitPlatformSupport::sampleGamepads(WebKit::WebGamepads& data) {
  data = gamepad_data_;
}

void TestWebKitPlatformSupport::setGamepadData(
    const WebKit::WebGamepads& data) {
  gamepad_data_ = data;
}

void TestWebKitPlatformSupport::GetPlugins(
    bool refresh, std::vector<webkit::WebPluginInfo>* plugins) {
  if (refresh)
    webkit::npapi::PluginList::Singleton()->RefreshPlugins();
  webkit::npapi::PluginList::Singleton()->GetPlugins(plugins);
}

webkit_glue::ResourceLoaderBridge*
TestWebKitPlatformSupport::CreateResourceLoader(
    const webkit_glue::ResourceLoaderBridge::RequestInfo& request_info) {
  return SimpleResourceLoaderBridge::Create(request_info);
}

webkit_glue::WebSocketStreamHandleBridge*
TestWebKitPlatformSupport::CreateWebSocketBridge(
    WebKit::WebSocketStreamHandle* handle,
    webkit_glue::WebSocketStreamHandleDelegate* delegate) {
  return SimpleSocketStreamBridge::Create(handle, delegate);
}

WebKit::WebMediaStreamCenter*
TestWebKitPlatformSupport::createMediaStreamCenter(
    WebKit::WebMediaStreamCenterClient* client) {
  if (shadow_platform_delegate_)
    return shadow_platform_delegate_->createMediaStreamCenter(client);

  return webkit_glue::WebKitPlatformSupportImpl::createMediaStreamCenter(
      client);
}

WebKit::WebRTCPeerConnectionHandler*
TestWebKitPlatformSupport::createRTCPeerConnectionHandler(
    WebKit::WebRTCPeerConnectionHandlerClient* client) {
  if (shadow_platform_delegate_)
    return shadow_platform_delegate_->createRTCPeerConnectionHandler(client);

  return webkit_glue::WebKitPlatformSupportImpl::createRTCPeerConnectionHandler(
      client);
}

WebKit::WebGestureCurve* TestWebKitPlatformSupport::createFlingAnimationCurve(
    int device_source,
    const WebKit::WebFloatPoint& velocity,
    const WebKit::WebSize& cumulative_scroll) {
  // Caller will retain and release.
  return new WebGestureCurveMock(velocity, cumulative_scroll);
}

WebKit::WebUnitTestSupport* TestWebKitPlatformSupport::unitTestSupport() {
  return this;
}

void TestWebKitPlatformSupport::registerMockedURL(
    const WebKit::WebURL& url,
    const WebKit::WebURLResponse& response,
    const WebKit::WebString& file_path) {
  url_loader_factory_.RegisterURL(url, response, file_path);
}

void TestWebKitPlatformSupport::registerMockedErrorURL(
    const WebKit::WebURL& url,
    const WebKit::WebURLResponse& response,
    const WebKit::WebURLError& error) {
  url_loader_factory_.RegisterErrorURL(url, response, error);
}

void TestWebKitPlatformSupport::unregisterMockedURL(const WebKit::WebURL& url) {
  url_loader_factory_.UnregisterURL(url);
}

void TestWebKitPlatformSupport::unregisterAllMockedURLs() {
  url_loader_factory_.UnregisterAllURLs();
}

void TestWebKitPlatformSupport::serveAsynchronousMockedRequests() {
  url_loader_factory_.ServeAsynchronousRequests();
}

WebKit::WebString TestWebKitPlatformSupport::webKitRootDir() {
  return webkit_support::GetWebKitRootDir();
}


WebKit::WebLayerTreeView*
    TestWebKitPlatformSupport::createLayerTreeViewForTesting() {
  scoped_ptr<WebLayerTreeViewImplForTesting> view(
      new WebLayerTreeViewImplForTesting(
          webkit_support::FAKE_CONTEXT, NULL));

  if (!view->initialize(scoped_ptr<cc::Thread>()))
    return NULL;
  return view.release();
}

WebKit::WebLayerTreeView*
    TestWebKitPlatformSupport::createLayerTreeViewForTesting(
        TestViewType type) {
  DCHECK_EQ(TestViewTypeUnitTest, type);
  return createLayerTreeViewForTesting();
}

