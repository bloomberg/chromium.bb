// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_TEST_EXTENSION_BACKGROUND_PAGE_WAITER_H_
#define EXTENSIONS_TEST_EXTENSION_BACKGROUND_PAGE_WAITER_H_

#include "extensions/common/extension.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// A class to wait for an extension's background to reach a certain state.
// NOTE: See also ExtensionHostTestHelper.
// * Use this class when you need to wait for a _state_ - e.g., for the
//   extension's background context to be initialized - and you don't
//   necessarily care when it happened. The state may already be active.
// * Use ExtensionHostTestHelper when you need to wait for an expected
//   _event_ - such as host destruction.
// See also
// https://chromium.googlesource.com/chromium/src/+/main/docs/patterns/synchronous-runloop.md#events-vs-states
// Note: This does not (yet) accommodate ServiceWorker-based extensions.
// TODO(devlin): Combine this and BackgroundPageWatcher.
class ExtensionBackgroundPageWaiter {
 public:
  ExtensionBackgroundPageWaiter(content::BrowserContext* browser_context,
                                const Extension& extension);
  ExtensionBackgroundPageWaiter(const ExtensionBackgroundPageWaiter& other) =
      delete;
  ExtensionBackgroundPageWaiter& operator=(
      const ExtensionBackgroundPageWaiter& other) = delete;
  ~ExtensionBackgroundPageWaiter();

  // Returns true if this class can wait for the specified `extension`. If
  // false, populates `reason` with why it can't (e.g., the extension has
  // no background context).
  static bool CanWaitFor(const Extension& extension, std::string& reason);

  // Waits for the extension's background context to have initialized at some
  // point. This doesn't require the background context to be currently open,
  // in the case of lazy background contexts.
  // NOTE: The only way to determine if a lazy background context was previously
  // initialized is by checking for registered events. If a background context
  // was fully initialized and shut down without registering any event
  // listeners, this will spin indefinitely.
  void WaitForBackgroundInitialized();

  // Waits for the extension background context to currently be open and active.
  void WaitForBackgroundOpen();

  // Waits for the extension background context to be closed.
  void WaitForBackgroundClosed();

 private:
  content::BrowserContext* const browser_context_;
  scoped_refptr<const Extension> extension_;
};

}  // namespace extensions

#endif  // EXTENSIONS_TEST_EXTENSION_BACKGROUND_PAGE_WAITER_H_
