// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/test/extension_background_page_waiter.h"

#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

ExtensionBackgroundPageWaiter::ExtensionBackgroundPageWaiter(
    content::BrowserContext* browser_context,
    const Extension& extension)
    : browser_context_(browser_context),
      extension_(base::WrapRefCounted(&extension)) {
  std::string reason;
  if (!CanWaitFor(*extension_, reason))
    ADD_FAILURE() << "Attempting to wait for an invalid extension: " << reason;

  if (browser_context_->IsOffTheRecord() &&
      !IncognitoInfo::IsSplitMode(extension_.get())) {
    ADD_FAILURE() << "Trying to wait for an incognito background page from a "
                  << "spanning mode extension. Use the on-the-record context.";
  }
}

// static
bool ExtensionBackgroundPageWaiter::CanWaitFor(const Extension& extension,
                                               std::string& reason_out) {
  if (BackgroundInfo::IsServiceWorkerBased(&extension)) {
    reason_out =
        "ExtensionBackgroundPageWaiter does not currently support "
        "Service Worker-based extensions.";
    return false;
  }

  if (extension.is_hosted_app()) {
    // Little known fact: hosted apps can have background pages. They are
    // handled separately in BackgroundContents[Service], and don't use the
    // same infrastructure. They're also deprecated. Don't worry about them.
    // (If we see flakiness in loading hosted apps, we could potentially
    // rejigger this to accommodate for them as well, but it's unclear if it's
    // a problem that needs solving.)
    reason_out = "ExtensionBackgroundPageWaiter does not support hosted apps.";
    return false;
  }

  if (!BackgroundInfo::HasBackgroundPage(&extension)) {
    reason_out = "Extension has no background page.";
    return false;
  }

  return true;
}

ExtensionBackgroundPageWaiter::~ExtensionBackgroundPageWaiter() = default;

void ExtensionBackgroundPageWaiter::WaitForBackgroundInitialized() {
  ProcessManager* process_manager = ProcessManager::Get(browser_context_);
  ExtensionHost* extension_host =
      process_manager->GetBackgroundHostForExtension(extension_->id());

  // Note: This duplicates a bit of logic from WaitForBackgroundOpen(). We
  // can't just put the lazy background check from below first and then call
  // WaitForBackgroundOpen() directly because we want to accommodate for the
  // case of a lazy background page that is active and is still completing its
  // first load, but has registered some event listeners.
  if (extension_host) {
    if (extension_host->has_loaded_once()) {
      // The background host exists and has loaded; we're done.
      return;
    }

    // Wait for the background to finish loading.
    ExtensionHostTestHelper host_helper(browser_context_, extension_->id());
    host_helper.RestrictToHost(extension_host);
    host_helper.WaitForHostCompletedFirstLoad();
    return;
  }

  // There isn't a currently-active background page.

  // If the extension has a lazy background page, it's possible that it's
  // already been loaded and unloaded, which we consider to be initialized.  As
  // a proxy for this, check if there are registered events. This isn't a
  // perfect solution, because
  // a) We might be waiting on a subsequent background page load, and
  // b) The extension might not register any events (which would normally be
  //    a bug in event page-based extensions, but not always).
  // But, it's a decent proxy for now.
  if (BackgroundInfo::HasLazyBackgroundPage(extension_.get()) &&
      EventRouter::Get(browser_context_)
          ->HasRegisteredEvents(extension_->id())) {
    return;
  }

  // Otherwise, wait for a new background page to initialize.
  WaitForBackgroundOpen();
}

void ExtensionBackgroundPageWaiter::WaitForBackgroundOpen() {
  ProcessManager* process_manager = ProcessManager::Get(browser_context_);
  ExtensionHost* extension_host =
      process_manager->GetBackgroundHostForExtension(extension_->id());

  if (extension_host && !extension_host->has_loaded_once()) {
    // Wait for host to finish loading.
    ExtensionHostTestHelper host_helper(browser_context_, extension_->id());
    host_helper.RestrictToHost(extension_host);
    host_helper.WaitForHostCompletedFirstLoad();
  } else if (!extension_host) {
    ExtensionHostTestHelper host_helper(browser_context_, extension_->id());
    host_helper.RestrictToType(mojom::ViewType::kExtensionBackgroundPage);
    extension_host = host_helper.WaitForHostCompletedFirstLoad();
  }

  ASSERT_TRUE(extension_host);
  ASSERT_TRUE(extension_host->has_loaded_once());
  ASSERT_EQ(extension_host,
            process_manager->GetBackgroundHostForExtension(extension_->id()));
}

void ExtensionBackgroundPageWaiter::WaitForBackgroundClosed() {
  ProcessManager* process_manager = ProcessManager::Get(browser_context_);
  ExtensionHost* extension_host =
      process_manager->GetBackgroundHostForExtension(extension_->id());
  if (!extension_host)
    return;  // Background page already closed.

  ExtensionHostTestHelper host_helper(browser_context_, extension_->id());
  host_helper.RestrictToHost(extension_host);
  host_helper.WaitForHostDestroyed();

  ASSERT_FALSE(
      process_manager->GetBackgroundHostForExtension(extension_->id()));
}

}  // namespace extensions
