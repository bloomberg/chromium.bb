// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/support/test_webkit_platform_support.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/stats_counters.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "media/base/media.h"
#include "net/cookies/cookie_monster.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebFileSystem.h"
#include "third_party/WebKit/public/platform/WebStorageArea.h"
#include "third_party/WebKit/public/platform/WebStorageNamespace.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebDatabase.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/web/WebScriptController.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"
#include "third_party/WebKit/public/web/WebStorageEventDispatcher.h"
#include "v8/include/v8.h"
#include "webkit/browser/database/vfs_backend.h"
#include "webkit/child/webkitplatformsupport_impl.h"
#include "webkit/glue/simple_webmimeregistry_impl.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/renderer/compositor_bindings/web_compositor_support_impl.h"
#include "webkit/support/mock_webclipboard_impl.h"
#include "webkit/support/web_gesture_curve_mock.h"
#include "webkit/support/web_layer_tree_view_impl_for_testing.h"
#include "webkit/support/weburl_loader_mock_factory.h"

#if defined(OS_WIN)
#include "third_party/WebKit/public/platform/win/WebThemeEngine.h"
#elif defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

using WebKit::WebScriptController;
using webkit::WebLayerTreeViewImplForTesting;

TestWebKitPlatformSupport::TestWebKitPlatformSupport() {
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
  WebKit::WebRuntimeFeatures::enableApplicationCache(true);
  WebKit::WebRuntimeFeatures::enableDatabase(true);
  WebKit::WebRuntimeFeatures::enableNotifications(true);
  WebKit::WebRuntimeFeatures::enableTouch(true);

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

  net::CookieMonster::EnableFileScheme();

  // Test shell always exposes the GC.
  webkit_glue::SetJavaScriptFlags(" --expose-gc");
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

WebKit::WebIDBFactory* TestWebKitPlatformSupport::idbFactory() {
  NOTREACHED() <<
      "IndexedDB cannot be tested with in-process harnesses.";
  return NULL;
}

WebKit::WebURLLoader* TestWebKitPlatformSupport::createURLLoader() {
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
  // Returns placeholder strings to check if they are correctly localized.
  switch (name) {
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
    case WebKit::WebLocalizedString::WeekFormatTemplate:
      return ASCIIToUTF16("Week $2, $1");
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

#if defined(OS_WIN) || defined(OS_MACOSX)
void TestWebKitPlatformSupport::SetThemeEngine(WebKit::WebThemeEngine* engine) {
  active_theme_engine_ = engine ?
      engine : WebKitPlatformSupportImpl::themeEngine();
}

WebKit::WebThemeEngine* TestWebKitPlatformSupport::themeEngine() {
  return active_theme_engine_;
}
#endif

WebKit::WebCompositorSupport*
TestWebKitPlatformSupport::compositorSupport() {
  return &compositor_support_;
}

base::string16 TestWebKitPlatformSupport::GetLocalizedString(int message_id) {
  return base::string16();
}

base::StringPiece TestWebKitPlatformSupport::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) {
  return base::StringPiece();
}

webkit_glue::ResourceLoaderBridge*
TestWebKitPlatformSupport::CreateResourceLoader(
    const webkit_glue::ResourceLoaderBridge::RequestInfo& request_info) {
  NOTREACHED();
  return NULL;
}

webkit_glue::WebSocketStreamHandleBridge*
TestWebKitPlatformSupport::CreateWebSocketBridge(
    WebKit::WebSocketStreamHandle* handle,
    webkit_glue::WebSocketStreamHandleDelegate* delegate) {
  NOTREACHED();
  return NULL;
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
  base::FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.Append(FILE_PATH_LITERAL("third_party/WebKit"));
  path = base::MakeAbsoluteFilePath(path);
  CHECK(!path.empty());
  std::string path_ascii = path.MaybeAsASCII();
  CHECK(!path_ascii.empty());
  return WebKit::WebString::fromUTF8(path_ascii.c_str());
}


WebKit::WebLayerTreeView*
    TestWebKitPlatformSupport::createLayerTreeViewForTesting() {
  scoped_ptr<WebLayerTreeViewImplForTesting> view(
      new WebLayerTreeViewImplForTesting());

  if (!view->Initialize())
    return NULL;
  return view.release();
}

WebKit::WebLayerTreeView*
    TestWebKitPlatformSupport::createLayerTreeViewForTesting(
        TestViewType type) {
  DCHECK_EQ(TestViewTypeUnitTest, type);
  return createLayerTreeViewForTesting();
}

WebKit::WebData TestWebKitPlatformSupport::readFromFile(
    const WebKit::WebString& path) {
  base::FilePath file_path = base::FilePath::FromUTF16Unsafe(path);

  std::string buffer;
  file_util::ReadFileToString(file_path, &buffer);

  return WebKit::WebData(buffer.data(), buffer.size());
}
