// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_message_handler.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sharing/proto/shared_clipboard_message.pb.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "components/sync_device_info/device_info.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

SharedClipboardMessageHandler::SharedClipboardMessageHandler(
    SharingService* sharing_service)
    : sharing_service_(sharing_service) {}

SharedClipboardMessageHandler::~SharedClipboardMessageHandler() = default;

void SharedClipboardMessageHandler::OnMessage(
    const chrome_browser_sharing::SharingMessage& message) {
  DCHECK(message.has_shared_clipboard_message());

  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
      .WriteText(base::UTF8ToUTF16(message.shared_clipboard_message().text()));

  std::unique_ptr<syncer::DeviceInfo> device =
      sharing_service_->GetDeviceByGuid(message.sender_guid());
  if (device)
    ShowNotification(std::move(device));
}
