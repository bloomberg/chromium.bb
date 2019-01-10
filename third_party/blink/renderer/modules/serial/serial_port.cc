// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/serial/serial_port.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"

namespace blink {

SerialPort::SerialPort(mojom::blink::SerialPortInfoPtr info)
    : info_(std::move(info)) {}

SerialPort::~SerialPort() = default;

ReadableStream* SerialPort::in() {
  return nullptr;
}

WritableStream* SerialPort::out() {
  return nullptr;
}

ScriptPromise SerialPort::open(ScriptState* script_state,
                               const SerialOptions* options) {
  return ScriptPromise::RejectWithDOMException(
      script_state, DOMException::Create(DOMExceptionCode::kNotSupportedError));
}

ScriptPromise SerialPort::close(ScriptState* script_state) {
  return ScriptPromise::RejectWithDOMException(
      script_state, DOMException::Create(DOMExceptionCode::kNotSupportedError));
}

const base::UnguessableToken& SerialPort::Token() const {
  return info_->token;
}

}  // namespace blink
