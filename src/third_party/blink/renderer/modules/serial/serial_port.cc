// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/serial/serial_port.h"

#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/modules/serial/serial.h"

namespace blink {

SerialPort::SerialPort(Serial* parent, mojom::blink::SerialPortInfoPtr info)
    : info_(std::move(info)), parent_(parent) {}

SerialPort::~SerialPort() = default;

ScriptPromise SerialPort::open(ScriptState* script_state,
                               const SerialOptions* options) {
  if (port_)
    return ScriptPromise::CastUndefined(script_state);

  parent_->GetPort(info_->token, mojo::MakeRequest(&port_));
  // TODO(https://crbug.com/884928): Call port_->Open() and initialize the
  // ReadableStream and WritableStream.
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise SerialPort::close(ScriptState* script_state) {
  ContextDestroyed();
  return ScriptPromise::CastUndefined(script_state);
}

void SerialPort::ContextDestroyed() {
  // Release connection-related resources as quickly as possible.
  port_.reset();
}

void SerialPort::Trace(Visitor* visitor) {
  visitor->Trace(parent_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
