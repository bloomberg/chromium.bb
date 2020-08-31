// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"

#include "base/time/time.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host_queue.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

namespace {

enum class BackgroundType {
  kPersistentPage,
  kLazyPage,
  kWorker,
};

}  // namespace

// Yes, this is a test for a test class. It exercises the inner workings of
// ExtensionBrowserTest itself.
using ExtensionBrowserTestBrowserTest = ExtensionBrowserTest;

class MultiBackgroundExtensionBrowserTestBrowserTest
    : public ExtensionBrowserTestBrowserTest,
      public testing::WithParamInterface<BackgroundType> {};

IN_PROC_BROWSER_TEST_P(MultiBackgroundExtensionBrowserTestBrowserTest,
                       LoadExtensionWaitsForBackgroundPageToBeReady) {
  // We add a custom delay here to force the background page of the extension to
  // load a little later; this helps ensure we are properly waiting on it in the
  // LoadExtension() method.
  ExtensionHostQueue::GetInstance().SetCustomDelayForTesting(
      base::TimeDelta::FromSeconds(1));

  constexpr char kPersistentBackgroundPage[] =
      R"("scripts": ["background.js"])";
  constexpr char kLazyBackgroundPage[] =
      R"("scripts": ["background.js"], "persistent": false)";
  constexpr char kWorkerBackground[] = R"("service_worker": "background.js")";

  const char* background_key = nullptr;
  int manifest_version = 2;
  base::Optional<ScopedCurrentChannel> channel_override;
  switch (GetParam()) {
    case BackgroundType::kPersistentPage:
      background_key = kPersistentBackgroundPage;
      break;
    case BackgroundType::kLazyPage:
      background_key = kLazyBackgroundPage;
      break;
    case BackgroundType::kWorker:
      background_key = kWorkerBackground;
      // Worker-based background scripts are channel-restricted, and available
      // in manifest v3.
      channel_override.emplace(version_info::Channel::UNKNOWN);
      manifest_version = 3;
      break;
  }

  TestExtensionDir test_dir;
  constexpr char kManifestTemplate[] =
      R"({
           "name": "Test Extension",
           "version": "0.1",
           "manifest_version": %d,
           "background": {
             %s
           }
         })";
  test_dir.WriteManifest(
      base::StringPrintf(kManifestTemplate, manifest_version, background_key));
  constexpr char kBackgroundJs[] =
      R"(chrome.tabs.onCreated.addListener(() => {});)";
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  EventRouter* event_router = EventRouter::Get(profile());
  EXPECT_TRUE(event_router->ExtensionHasEventListener(extension->id(),
                                                      "tabs.onCreated"));
  ExtensionHostQueue::GetInstance().SetCustomDelayForTesting(
      base::TimeDelta::FromSeconds(0));
}

// TODO(devlin): Add support for ServiceWorker-based extensions here as well.
// Currently, we have no good way to wait for the ServiceWorker to be ready.
INSTANTIATE_TEST_SUITE_P(All,
                         MultiBackgroundExtensionBrowserTestBrowserTest,
                         testing::Values(BackgroundType::kPersistentPage,
                                         BackgroundType::kLazyPage));

}  // namespace extensions
