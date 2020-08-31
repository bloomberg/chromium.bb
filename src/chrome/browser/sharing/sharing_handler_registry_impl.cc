
// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_handler_registry_impl.h"

#include "build/build_config.h"
#include "chrome/browser/sharing/ack_message_handler.h"
#include "chrome/browser/sharing/ping_message_handler.h"
#include "chrome/browser/sharing/sharing_device_registration.h"
#include "chrome/browser/sharing/sharing_message_handler.h"
#include "chrome/browser/sharing/sharing_message_sender.h"

#if defined(OS_ANDROID)
#include "base/feature_list.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_message_handler_android.h"
#include "chrome/browser/sharing/click_to_call/feature.h"
#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_message_handler_android.h"
#include "chrome/browser/sharing/sms/sms_fetch_request_handler.h"
#else
#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_message_handler_desktop.h"
#include "chrome/browser/sharing/webrtc/sharing_service_host.h"
#include "chrome/browser/sharing/webrtc/webrtc_message_handler.h"
#endif  // defined(OS_ANDROID)

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
#include "chrome/browser/sharing/shared_clipboard/remote_copy_message_handler.h"
#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
        // defined(OS_CHROMEOS)

SharingHandlerRegistryImpl::SharingHandlerRegistryImpl(
    Profile* profile,
    SharingDeviceRegistration* sharing_device_registration,
    SharingMessageSender* message_sender,
    SharingDeviceSource* device_source,
    content::SmsFetcher* sms_fetcher,
    SharingServiceHost* sharing_service_host) {
  AddSharingHandler(std::make_unique<PingMessageHandler>(),
                    {chrome_browser_sharing::SharingMessage::kPingMessage});

  AddSharingHandler(std::make_unique<AckMessageHandler>(message_sender),
                    {chrome_browser_sharing::SharingMessage::kAckMessage});

#if defined(OS_ANDROID)
  // Note: IsClickToCallSupported() is not used as it requires JNI call.
  if (base::FeatureList::IsEnabled(kClickToCallReceiver)) {
    AddSharingHandler(
        std::make_unique<ClickToCallMessageHandler>(),
        {chrome_browser_sharing::SharingMessage::kClickToCallMessage});
  }

  if (sharing_device_registration->IsSmsFetcherSupported()) {
    AddSharingHandler(
        std::make_unique<SmsFetchRequestHandler>(sms_fetcher),
        {chrome_browser_sharing::SharingMessage::kSmsFetchRequest});
  }
#endif  // defined(OS_ANDROID)

  if (sharing_device_registration->IsSharedClipboardSupported()) {
    std::unique_ptr<SharingMessageHandler> shared_clipboard_message_handler;
#if defined(OS_ANDROID)
    shared_clipboard_message_handler =
        std::make_unique<SharedClipboardMessageHandlerAndroid>(device_source);
#else
    shared_clipboard_message_handler =
        std::make_unique<SharedClipboardMessageHandlerDesktop>(device_source,
                                                               profile);
#endif  // defined(OS_ANDROID)
    AddSharingHandler(
        std::move(shared_clipboard_message_handler),
        {chrome_browser_sharing::SharingMessage::kSharedClipboardMessage});
  }

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
  if (sharing_device_registration->IsRemoteCopySupported()) {
    AddSharingHandler(
        std::make_unique<RemoteCopyMessageHandler>(profile),
        {chrome_browser_sharing::SharingMessage::kRemoteCopyMessage});
  }
#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS)

#if !defined(OS_ANDROID)
  if (sharing_device_registration->IsPeerConnectionSupported()) {
    sharing_service_host->SetSharingHandlerRegistry(this);
    AddSharingHandler(
        std::make_unique<WebRtcMessageHandler>(sharing_service_host),
        {chrome_browser_sharing::SharingMessage::kPeerConnectionOfferMessage,
         chrome_browser_sharing::SharingMessage::
             kPeerConnectionIceCandidatesMessage});
  }
#endif  // !defined(OS_ANDROID)
}

SharingHandlerRegistryImpl::~SharingHandlerRegistryImpl() = default;

SharingMessageHandler* SharingHandlerRegistryImpl::GetSharingHandler(
    chrome_browser_sharing::SharingMessage::PayloadCase payload_case) {
  auto it = handler_map_.find(payload_case);
  if (it != handler_map_.end())
    return it->second;

  auto extra_it = extra_handler_map_.find(payload_case);
  if (extra_it != extra_handler_map_.end())
    return extra_it->second.get();

  return nullptr;
}

void SharingHandlerRegistryImpl::AddSharingHandler(
    std::unique_ptr<SharingMessageHandler> handler,
    std::set<chrome_browser_sharing::SharingMessage::PayloadCase>
        payload_cases) {
  DCHECK(handler) << "Received request to add null handler";

  for (const auto& payload_case : payload_cases) {
    DCHECK(payload_case !=
           chrome_browser_sharing::SharingMessage::PAYLOAD_NOT_SET)
        << "Incorrect payload type specified for handler";
    DCHECK(!handler_map_.count(payload_case)) << "Handler already exists";
    handler_map_[payload_case] = handler.get();
  }

  handlers_.push_back(std::move(handler));
}

void SharingHandlerRegistryImpl::RegisterSharingHandler(
    std::unique_ptr<SharingMessageHandler> handler,
    chrome_browser_sharing::SharingMessage::PayloadCase payload_case) {
  DCHECK(handler) << "Received request to add null handler";
  DCHECK(!GetSharingHandler(payload_case));
  DCHECK(payload_case !=
         chrome_browser_sharing::SharingMessage::PAYLOAD_NOT_SET)
      << "Incorrect payload type specified for handler";

  extra_handler_map_[payload_case] = std::move(handler);
}

void SharingHandlerRegistryImpl::UnregisterSharingHandler(
    chrome_browser_sharing::SharingMessage::PayloadCase payload_case) {
  extra_handler_map_.erase(payload_case);
}
