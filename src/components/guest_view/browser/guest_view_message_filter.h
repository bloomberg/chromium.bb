// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_VIEW_BROWSER_GUEST_VIEW_MESSAGE_FILTER_H_
#define COMPONENTS_GUEST_VIEW_BROWSER_GUEST_VIEW_MESSAGE_FILTER_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"

namespace base {
class DictionaryValue;
}

namespace content {
class BrowserContext;
}

namespace guest_view {
class GuestViewManager;

// This class filters out incoming GuestView-specific IPC messages from the
// renderer process. It is created on the UI thread. Messages may be handled on
// the IO thread or the UI thread.
class GuestViewMessageFilter : public content::BrowserMessageFilter {
 public:
  GuestViewMessageFilter(const GuestViewMessageFilter&) = delete;
  GuestViewMessageFilter& operator=(const GuestViewMessageFilter&) = delete;

  static void EnsureShutdownNotifierFactoryBuilt();

 protected:
  GuestViewMessageFilter(const uint32_t* message_classes_to_filter,
                         size_t num_message_classes_to_filter,
                         int render_process_id,
                         content::BrowserContext* context);

  ~GuestViewMessageFilter() override;

  // Returns the GuestViewManager for |browser_context_| if one already exists,
  // or creates and returns one for |browser_context_| otherwise.
  virtual GuestViewManager* GetOrCreateGuestViewManager();

  // Returns the GuestViewManager for |browser_context_| if it exists.
  // Callers consider the renderer to be misbehaving if we don't have a
  // GuestViewManager at this point, in which case we kill the renderer and
  // return nullptr.
  GuestViewManager* GetGuestViewManagerOrKill();

  // content::BrowserMessageFilter implementation.
  void OverrideThreadForMessage(const IPC::Message& message,
                                content::BrowserThread::ID* thread) override;
  void OnDestruct() const override;
  bool OnMessageReceived(const IPC::Message& message) override;

  const int render_process_id_;

  // Should only be accessed on the UI thread.
  // May become null if this filter outlives the BrowserContext.
  raw_ptr<content::BrowserContext> browser_context_;

 private:
  friend class content::BrowserThread;
  friend class base::DeleteHelper<GuestViewMessageFilter>;

  // Message handlers on the UI thread.
  void OnAttachToEmbedderFrame(int embedder_local_render_frame_id,
                               int element_instance_id,
                               int guest_instance_id,
                               const base::DictionaryValue& params);
  void OnViewCreated(int view_instance_id, const std::string& view_type);
  void OnViewGarbageCollected(int view_instance_id);

  void OnBrowserContextShutdown();

  base::CallbackListSubscription browser_context_shutdown_subscription_;
};

}  // namespace guest_view

#endif  // COMPONENTS_GUEST_VIEW_BROWSER_GUEST_VIEW_MESSAGE_FILTER_H_
