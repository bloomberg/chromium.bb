// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/push_messaging/PushSubscriptionOptions.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/typed_arrays/DOMArrayBuffer.h"
#include "modules/push_messaging/PushSubscriptionOptionsInit.h"
#include "platform/wtf/ASCIICType.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebString.h"
#include "public/platform/modules/push_messaging/WebPushSubscriptionOptions.h"

namespace blink {
namespace {

const int kMaxApplicationServerKeyLength = 255;

String BufferSourceToString(
    const ArrayBufferOrArrayBufferView& application_server_key,
    ExceptionState& exception_state) {
  unsigned char* input;
  int length;
  // Convert the input array into a string of bytes.
  if (application_server_key.isArrayBuffer()) {
    input = static_cast<unsigned char*>(
        application_server_key.getAsArrayBuffer()->Data());
    length = application_server_key.getAsArrayBuffer()->ByteLength();
  } else if (application_server_key.isArrayBufferView()) {
    input = static_cast<unsigned char*>(
        application_server_key.getAsArrayBufferView().View()->buffer()->Data());
    length = application_server_key.getAsArrayBufferView()
                 .View()
                 ->buffer()
                 ->ByteLength();
  } else {
    NOTREACHED();
    return String();
  }

  // Check the validity of the sender info. It must either be a 65-byte
  // uncompressed VAPID key, which has the byte 0x04 as the first byte or a
  // numeric sender ID.
  const bool is_vapid = length == 65 && *input == 0x04;
  const bool is_sender_id =
      length > 0 && length < kMaxApplicationServerKeyLength &&
      (std::find_if_not(input, input + length,
                        &WTF::IsASCIIDigit<unsigned char>) == input + length);

  if (is_vapid || is_sender_id)
    return WebString::FromLatin1(input, length);

  exception_state.ThrowDOMException(
      kInvalidAccessError, "The provided applicationServerKey is not valid.");
  return String();
}

}  // namespace

// static
WebPushSubscriptionOptions PushSubscriptionOptions::ToWeb(
    const PushSubscriptionOptionsInit& options,
    ExceptionState& exception_state) {
  WebPushSubscriptionOptions web_options;
  web_options.user_visible_only = options.userVisibleOnly();
  if (options.hasApplicationServerKey())
    web_options.application_server_key =
        BufferSourceToString(options.applicationServerKey(), exception_state);
  return web_options;
}

PushSubscriptionOptions::PushSubscriptionOptions(
    const WebPushSubscriptionOptions& options)
    : user_visible_only_(options.user_visible_only),
      application_server_key_(
          DOMArrayBuffer::Create(options.application_server_key.Latin1().data(),
                                 options.application_server_key.length())) {}

DEFINE_TRACE(PushSubscriptionOptions) {
  visitor->Trace(application_server_key_);
}

}  // namespace blink
