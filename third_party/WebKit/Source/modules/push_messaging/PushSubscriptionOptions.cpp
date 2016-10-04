// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/push_messaging/PushSubscriptionOptions.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/ExceptionCode.h"
#include "modules/push_messaging/PushSubscriptionOptionsInit.h"
#include "public/platform/WebString.h"
#include "public/platform/modules/push_messaging/WebPushSubscriptionOptions.h"
#include "wtf/Assertions.h"
#include "wtf/text/WTFString.h"

namespace blink {
namespace {

const int kMaxApplicationServerKeyLength = 255;

String bufferSourceToString(
    const ArrayBufferOrArrayBufferView& applicationServerKey,
    ExceptionState& exceptionState) {
  // Check the validity of the sender info. It must be a 65-byte uncompressed
  // key, which has the byte 0x04 as the first byte as a marker.
  unsigned char* input;
  int length;
  if (applicationServerKey.isArrayBuffer()) {
    input = static_cast<unsigned char*>(
        applicationServerKey.getAsArrayBuffer()->data());
    length = applicationServerKey.getAsArrayBuffer()->byteLength();
  } else if (applicationServerKey.isArrayBufferView()) {
    input = static_cast<unsigned char*>(
        applicationServerKey.getAsArrayBufferView()->buffer()->data());
    length =
        applicationServerKey.getAsArrayBufferView()->buffer()->byteLength();
  } else {
    NOTREACHED();
    return String();
  }

  // If the key is valid, just treat it as a string of bytes and pass it to
  // the push service.
  if (length <= kMaxApplicationServerKeyLength)
    return WebString::fromLatin1(input, length);

  exceptionState.throwDOMException(
      InvalidAccessError, "The provided applicationServerKey is not valid.");
  return String();
}

}  // namespace

// static
WebPushSubscriptionOptions PushSubscriptionOptions::toWeb(
    const PushSubscriptionOptionsInit& options,
    ExceptionState& exceptionState) {
  WebPushSubscriptionOptions webOptions;
  webOptions.userVisibleOnly = options.userVisibleOnly();
  if (options.hasApplicationServerKey())
    webOptions.applicationServerKey =
        bufferSourceToString(options.applicationServerKey(), exceptionState);
  return webOptions;
}

PushSubscriptionOptions::PushSubscriptionOptions(
    const WebPushSubscriptionOptions& options)
    : m_userVisibleOnly(options.userVisibleOnly),
      m_applicationServerKey(
          DOMArrayBuffer::create(options.applicationServerKey.latin1().data(),
                                 options.applicationServerKey.length())) {}

DEFINE_TRACE(PushSubscriptionOptions) {
  visitor->trace(m_applicationServerKey);
}

}  // namespace blink
