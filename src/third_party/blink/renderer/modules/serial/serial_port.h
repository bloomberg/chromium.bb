// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_SERIAL_PORT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_SERIAL_PORT_H_

#include "services/device/public/mojom/serial.mojom-blink.h"
#include "third_party/blink/public/mojom/serial/serial.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace base {
class UnguessableToken;
}

namespace blink {

class ReadableStream;
class ScriptState;
class Serial;
class SerialOptions;
class WritableStream;

class SerialPort final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit SerialPort(Serial* parent, mojom::blink::SerialPortInfoPtr info);
  ~SerialPort() override;

  // Web-exposed functions
  ReadableStream* in() const { return nullptr; }
  WritableStream* out() const { return nullptr; }

  ScriptPromise open(ScriptState*, const SerialOptions* options);
  ScriptPromise close(ScriptState*);

  const base::UnguessableToken& token() const { return info_->token; }

  void ContextDestroyed();
  void Trace(Visitor*) override;

 private:
  mojom::blink::SerialPortInfoPtr info_;
  device::mojom::blink::SerialPortPtr port_;

  Member<Serial> parent_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_SERIAL_PORT_H_
