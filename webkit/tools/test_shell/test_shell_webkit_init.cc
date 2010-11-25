// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_shell_webkit_init.h"

#include "base/metrics/stats_counters.h"
#include "base/path_service.h"
#include "media/base/media.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCache.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDatabase.h"
#include "third_party/WebKit/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/WebKit/chromium/public/WebRuntimeFeatures.h"
#include "third_party/WebKit/WebKit/chromium/public/WebScriptController.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSecurityPolicy.h"
#include "webkit/extensions/v8/gears_extension.h"
#include "webkit/tools/test_shell/test_shell.h"

#if defined(OS_WIN)
#include "webkit/tools/test_shell/test_shell_webthemeengine.h"
#endif

TestShellWebKitInit::TestShellWebKitInit(bool layout_test_mode) {
  v8::V8::SetCounterFunction(base::StatsTable::FindLocation);

  WebKit::initialize(this);
  WebKit::setLayoutTestMode(layout_test_mode);
  WebKit::WebSecurityPolicy::registerURLSchemeAsLocal(
      WebKit::WebString::fromUTF8("test-shell-resource"));
  WebKit::WebSecurityPolicy::registerURLSchemeAsNoAccess(
      WebKit::WebString::fromUTF8("test-shell-resource"));
  WebKit::WebScriptController::enableV8SingleThreadMode();
  WebKit::WebScriptController::registerExtension(
      extensions_v8::GearsExtension::Get());
  WebKit::WebRuntimeFeatures::enableSockets(true);
  WebKit::WebRuntimeFeatures::enableApplicationCache(true);
  WebKit::WebRuntimeFeatures::enableDatabase(true);
  WebKit::WebRuntimeFeatures::enableWebGL(true);
  WebKit::WebRuntimeFeatures::enablePushState(true);
  WebKit::WebRuntimeFeatures::enableNotifications(true);
  WebKit::WebRuntimeFeatures::enableTouch(true);
  WebKit::WebRuntimeFeatures::enableIndexedDatabase(true);
  WebKit::WebRuntimeFeatures::enableSpeechInput(true);
  WebKit::WebRuntimeFeatures::enableFileSystem(true);

  // TODO(hwennborg): Enable this once the implementation supports it.
  WebKit::WebRuntimeFeatures::enableDeviceMotion(false);
  WebKit::WebRuntimeFeatures::enableDeviceOrientation(true);

  // Load libraries for media and enable the media player.
  FilePath module_path;
  WebKit::WebRuntimeFeatures::enableMediaPlayer(
      PathService::Get(base::DIR_MODULE, &module_path) &&
      media::InitializeMediaLibrary(module_path));

  WebKit::WebRuntimeFeatures::enableGeolocation(true);

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

  // Restrict the supported media types when running in layout test mode.
  if (layout_test_mode)
    mime_registry_.reset(new TestShellWebMimeRegistryImpl());
  else
    mime_registry_.reset(new webkit_glue::SimpleWebMimeRegistryImpl());

#if defined(OS_WIN)
  // Ensure we pick up the default theme engine.
  SetThemeEngine(NULL);
#endif
}

TestShellWebKitInit::~TestShellWebKitInit() {
  if (RunningOnValgrind())
    WebKit::WebCache::clear();
  WebKit::shutdown();
}

WebKit::WebClipboard* TestShellWebKitInit::clipboard() {
  // Mock out clipboard calls in layout test mode so that tests don't mess
  // with each other's copies/pastes when running in parallel.
  if (TestShell::layout_test_mode()) {
    return &mock_clipboard_;
  } else {
    return &real_clipboard_;
  }
}

WebKit::WebData TestShellWebKitInit::loadResource(const char* name) {
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
  return webkit_glue::WebKitClientImpl::loadResource(name);
}

WebKit::WebString TestShellWebKitInit::queryLocalizedString(
    WebKit::WebLocalizedString::Name name) {
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
      return WebKitClientImpl::queryLocalizedString(name);
  }
}

WebKit::WebString TestShellWebKitInit::queryLocalizedString(
    WebKit::WebLocalizedString::Name name, const WebKit::WebString& value) {
  if (name == WebKit::WebLocalizedString::ValidationRangeUnderflow)
    return ASCIIToUTF16("range underflow");
  if (name == WebKit::WebLocalizedString::ValidationRangeOverflow)
    return ASCIIToUTF16("range overflow");
  return WebKitClientImpl::queryLocalizedString(name, value);
}

WebKit::WebString TestShellWebKitInit::queryLocalizedString(
    WebKit::WebLocalizedString::Name name,
    const WebKit::WebString& value1,
    const WebKit::WebString& value2) {
  if (name == WebKit::WebLocalizedString::ValidationTooLong)
    return ASCIIToUTF16("too long");
  if (name == WebKit::WebLocalizedString::ValidationStepMismatch)
    return ASCIIToUTF16("step mismatch");
  return WebKitClientImpl::queryLocalizedString(name, value1, value2);
}

