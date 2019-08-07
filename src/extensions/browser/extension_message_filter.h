// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_MESSAGE_FILTER_H_
#define EXTENSIONS_BROWSER_EXTENSION_MESSAGE_FILTER_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"

class GURL;
struct ExtensionMsg_ExternalConnectionInfo;
struct ExtensionMsg_TabTargetConnectionInfo;
struct ServiceWorkerIdentifier;

namespace content {
class BrowserContext;
}

namespace extensions {
class EventRouter;
struct Message;
struct PortContext;
struct PortId;

// This class filters out incoming extension-specific IPC messages from the
// renderer process. It is created and destroyed on the UI thread and handles
// messages there.
class ExtensionMessageFilter : public content::BrowserMessageFilter {
 public:
  ExtensionMessageFilter(int render_process_id,
                         content::BrowserContext* context);

  int render_process_id() { return render_process_id_; }

  static void EnsureShutdownNotifierFactoryBuilt();

 private:
  friend class base::DeleteHelper<ExtensionMessageFilter>;
  friend class content::BrowserThread;

  ~ExtensionMessageFilter() override;

  EventRouter* GetEventRouter();

  void ShutdownOnUIThread();

  // content::BrowserMessageFilter implementation:
  void OverrideThreadForMessage(const IPC::Message& message,
                                content::BrowserThread::ID* thread) override;
  void OnDestruct() const override;
  bool OnMessageReceived(const IPC::Message& message) override;

  // Message handlers on the UI thread.
  void OnExtensionAddListener(const std::string& extension_id,
                              const GURL& listener_url,
                              const std::string& event_name,
                              int64_t service_worker_version_id,
                              int worker_thread_id);
  void OnExtensionRemoveListener(const std::string& extension_id,
                                 const GURL& listener_url,
                                 const std::string& event_name,
                                 int64_t service_worker_version_id,
                                 int worker_thread_id);
  void OnExtensionAddLazyListener(const std::string& extension_id,
                                  const std::string& event_name);
  void OnExtensionAddLazyServiceWorkerListener(
      const std::string& extension_id,
      const std::string& event_name,
      const GURL& service_worker_scope);
  void OnExtensionRemoveLazyListener(const std::string& extension_id,
                                     const std::string& event_name);
  void OnExtensionRemoveLazyServiceWorkerListener(
      const std::string& extension_id,
      const std::string& event_name,
      const GURL& worker_scope_url);
  void OnExtensionAddFilteredListener(
      const std::string& extension_id,
      const std::string& event_name,
      base::Optional<ServiceWorkerIdentifier> sw_identifier,
      const base::DictionaryValue& filter,
      bool lazy);
  void OnExtensionRemoveFilteredListener(
      const std::string& extension_id,
      const std::string& event_name,
      base::Optional<ServiceWorkerIdentifier> sw_identifier,
      const base::DictionaryValue& filter,
      bool lazy);
  void OnExtensionShouldSuspendAck(const std::string& extension_id,
                                   int sequence_id);
  void OnExtensionSuspendAck(const std::string& extension_id);
  void OnExtensionTransferBlobsAck(const std::vector<std::string>& blob_uuids);
  void OnExtensionWakeEventPage(int request_id,
                                const std::string& extension_id);

  void OnOpenChannelToExtension(const PortContext& source_context,
                                const ExtensionMsg_ExternalConnectionInfo& info,
                                const std::string& channel_name,
                                const extensions::PortId& port_id);
  void OnOpenChannelToNativeApp(const PortContext& source_context,
                                const std::string& native_app_name,
                                const extensions::PortId& port_id);
  void OnOpenChannelToTab(const PortContext& source_context,
                          const ExtensionMsg_TabTargetConnectionInfo& info,
                          const std::string& extension_id,
                          const std::string& channel_name,
                          const extensions::PortId& port_id);
  void OnOpenMessagePort(const PortContext& port_context,
                         const extensions::PortId& port_id);
  void OnCloseMessagePort(const PortContext& context,
                          const extensions::PortId& port_id,
                          bool force_close);
  void OnPostMessage(const extensions::PortId& port_id,
                     const extensions::Message& message);

  // Responds to the ExtensionHostMsg_WakeEventPage message.
  void SendWakeEventPageResponse(int request_id, bool success);

  const int render_process_id_;

  std::unique_ptr<KeyedServiceShutdownNotifier::Subscription>
      shutdown_notifier_;

  // Only access from the UI thread.
  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMessageFilter);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_MESSAGE_FILTER_H_
