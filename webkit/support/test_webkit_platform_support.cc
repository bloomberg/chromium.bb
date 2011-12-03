// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/support/test_webkit_platform_support.h"

#include "base/file_util.h"
#include "base/metrics/stats_counters.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "media/base/media.h"
#include "net/base/cookie_monster.h"
#include "net/http/http_cache.h"
#include "net/test/test_server.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebAudioDevice.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCache.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDatabase.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebFileSystem.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebGamepads.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBFactory.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBKey.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBKeyPath.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRuntimeFeatures.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptController.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityPolicy.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSerializedScriptValue.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageArea.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageEventDispatcher.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageNamespace.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "v8/include/v8.h"
#include "webkit/appcache/web_application_cache_host_impl.h"
#include "webkit/database/vfs_backend.h"
#include "webkit/extensions/v8/gc_extension.h"
#include "webkit/glue/simple_webmimeregistry_impl.h"
#include "webkit/glue/webclipboard_impl.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webkitplatformsupport_impl.h"
#include "webkit/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"
#include "webkit/gpu/webgraphicscontext3d_in_process_impl.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/support/simple_database_system.h"
#include "webkit/support/test_webmessageportchannel.h"
#include "webkit/support/webkit_support.h"
#include "webkit/support/weburl_loader_mock_factory.h"
#include "webkit/support/web_audio_device_mock.h"
#include "webkit/tools/test_shell/mock_webclipboard_impl.h"
#include "webkit/tools/test_shell/simple_appcache_system.h"
#include "webkit/tools/test_shell/simple_file_system.h"
#include "webkit/tools/test_shell/simple_socket_stream_bridge.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "webkit/tools/test_shell/simple_webcookiejar_impl.h"
#include "webkit/tools/test_shell/test_shell_request_context.h"
#include "webkit/tools/test_shell/test_shell_webblobregistry_impl.h"

#if defined(OS_WIN)
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/win/WebThemeEngine.h"
#include "webkit/tools/test_shell/test_shell_webthemeengine.h"
#elif defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#elif defined(OS_POSIX) && !defined(OS_ANDROID)
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/linux/WebThemeEngine.h"
#endif

using WebKit::WebScriptController;

TestWebKitPlatformSupport::TestWebKitPlatformSupport(bool unit_test_mode)
      : unit_test_mode_(unit_test_mode) {
  v8::V8::SetCounterFunction(base::StatsTable::FindLocation);

  WebKit::initialize(this);
  WebKit::setLayoutTestMode(true);
  WebKit::WebSecurityPolicy::registerURLSchemeAsLocal(
      WebKit::WebString::fromUTF8("test-shell-resource"));
  WebKit::WebSecurityPolicy::registerURLSchemeAsNoAccess(
      WebKit::WebString::fromUTF8("test-shell-resource"));
  WebScriptController::enableV8SingleThreadMode();
  WebKit::WebRuntimeFeatures::enableSockets(true);
  WebKit::WebRuntimeFeatures::enableApplicationCache(true);
  WebKit::WebRuntimeFeatures::enableDatabase(true);
  WebKit::WebRuntimeFeatures::enableDataTransferItems(true);
  WebKit::WebRuntimeFeatures::enablePushState(true);
  WebKit::WebRuntimeFeatures::enableNotifications(true);
  WebKit::WebRuntimeFeatures::enableTouch(true);
  WebKit::WebRuntimeFeatures::enableGamepad(true);

  // Load libraries for media and enable the media player.
  bool enable_media = false;
  FilePath module_path;
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

#if defined(OS_WIN)
  // Ensure we pick up the default theme engine.
  SetThemeEngine(NULL);
#endif

  net::HttpCache::Mode cache_mode = net::HttpCache::NORMAL;
  net::CookieMonster::EnableFileScheme();

  // Initializing with a default context, which means no on-disk cookie DB,
  // and no support for directory listings.
  SimpleResourceLoaderBridge::Init(FilePath(), cache_mode, true);

  // Test shell always exposes the GC.
  webkit_glue::SetJavaScriptFlags(" --expose-gc");
  // Expose GCController to JavaScript.
  WebScriptController::registerExtension(extensions_v8::GCExtension::Get());
}

TestWebKitPlatformSupport::~TestWebKitPlatformSupport() {
  if (RunningOnValgrind())
    WebKit::WebCache::clear();
  WebKit::shutdown();
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

bool TestWebKitPlatformSupport::sandboxEnabled() {
  return true;
}

WebKit::WebKitPlatformSupport::FileHandle
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
  // We use different strings for form validation messages.
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
  return WebKit::WebStorageNamespace::createLocalStorageNamespace(path, quota);
}

void TestWebKitPlatformSupport::dispatchStorageEvent(
    const WebKit::WebString& key,
    const WebKit::WebString& old_value, const WebKit::WebString& new_value,
    const WebKit::WebString& origin, const WebKit::WebURL& url,
    bool is_local_storage) {
  // The event is dispatched by the proxy.
}

WebKit::WebIDBFactory* TestWebKitPlatformSupport::idbFactory() {
  return WebKit::WebIDBFactory::create();
}

void TestWebKitPlatformSupport::createIDBKeysFromSerializedValuesAndKeyPath(
      const WebKit::WebVector<WebKit::WebSerializedScriptValue>& values,
      const WebKit::WebString& keyPath,
      WebKit::WebVector<WebKit::WebIDBKey>& keys_out) {
  WebKit::WebVector<WebKit::WebIDBKey> keys(values.size());
  for (size_t i = 0; i < values.size(); ++i) {
    keys[i] = WebKit::WebIDBKey::createFromValueAndKeyPath(
        values[i], WebKit::WebIDBKeyPath::create(keyPath));
  }
  keys_out.swap(keys);
}

WebKit::WebSerializedScriptValue
TestWebKitPlatformSupport::injectIDBKeyIntoSerializedValue(
    const WebKit::WebIDBKey& key,
    const WebKit::WebSerializedScriptValue& value,
    const WebKit::WebString& keyPath) {
  return WebKit::WebIDBKey::injectIDBKeyIntoSerializedValue(
      key, value, WebKit::WebIDBKeyPath::create(keyPath));
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

WebKit::WebSharedWorkerRepository*
TestWebKitPlatformSupport::sharedWorkerRepository() {
  return NULL;
}

WebKit::WebGraphicsContext3D*
TestWebKitPlatformSupport::createGraphicsContext3D() {
  switch (webkit_support::GetGraphicsContext3DImplementation()) {
    case webkit_support::IN_PROCESS:
      return new webkit::gpu::WebGraphicsContext3DInProcessImpl(
          gfx::kNullPluginWindow, NULL);
    case webkit_support::IN_PROCESS_COMMAND_BUFFER:
      return new webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl();
    default:
      CHECK(false) << "Unknown GraphicsContext3D Implementation";
      return NULL;
  }
}

double TestWebKitPlatformSupport::audioHardwareSampleRate() {
  return 44100.0;
}

size_t TestWebKitPlatformSupport::audioHardwareBufferSize() {
  return 128;
}

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
  // Don't load the forked npapi_layout_test_plugin in DRT, we only want to
  // use the upstream version TestNetscapePlugIn.
  const FilePath::StringType kPluginBlackList[] = {
    FILE_PATH_LITERAL("npapi_layout_test_plugin.dll"),
    FILE_PATH_LITERAL("WebKitTestNetscapePlugIn.plugin"),
    FILE_PATH_LITERAL("libnpapi_layout_test_plugin.so"),
  };
  for (int i = plugins->size() - 1; i >= 0; --i) {
    webkit::WebPluginInfo plugin_info = plugins->at(i);
    for (size_t j = 0; j < arraysize(kPluginBlackList); ++j) {
      if (plugin_info.path.BaseName() == FilePath(kPluginBlackList[j])) {
        plugins->erase(plugins->begin() + i);
      }
    }
  }
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
