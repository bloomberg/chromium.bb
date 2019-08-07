// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_DEVICE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_DEVICE_H_

#include "services/device/public/mojom/hid.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/array_buffer_or_array_buffer_view.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class ExecutionContext;
class HIDCollectionInfo;

class HIDDevice : public EventTargetWithInlineData,
                  public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(HIDDevice);

 public:
  HIDDevice(device::mojom::blink::HidDeviceInfoPtr info,
            ExecutionContext* execution_context);
  ~HIDDevice() override;

  // EventTarget overrides.
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  // hid_device.idl
  DEFINE_ATTRIBUTE_EVENT_LISTENER(inputreport, kInputreport)

  bool opened() const;
  uint16_t vendorId() const;
  uint16_t productId() const;
  String productName() const;
  const HeapVector<Member<HIDCollectionInfo>>& collections() const;

  ScriptPromise open(ScriptState*);
  ScriptPromise close(ScriptState*);
  ScriptPromise sendReport(ScriptState*,
                           uint8_t report_id,
                           const ArrayBufferOrArrayBufferView& data);
  ScriptPromise sendFeatureReport(ScriptState*,
                                  uint8_t report_id,
                                  const ArrayBufferOrArrayBufferView& data);
  ScriptPromise receiveFeatureReport(ScriptState*, uint8_t report_id);

  void Trace(blink::Visitor*) override;

 protected:
  // EventTarget protected overrides.
  void AddedEventListener(const AtomicString& event_type,
                          RegisteredEventListener& listener) override;

 private:
  HeapVector<Member<HIDCollectionInfo>> collections_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_DEVICE_H_
