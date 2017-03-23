// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothRemoteGATTCharacteristic_h
#define BluetoothRemoteGATTCharacteristic_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/DOMArrayPiece.h"
#include "core/dom/DOMDataView.h"
#include "modules/EventTargetModules.h"
#include "modules/bluetooth/BluetoothRemoteGATTService.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/bluetooth/web_bluetooth.mojom-blink.h"
#include "wtf/text/WTFString.h"

namespace blink {

class BluetoothCharacteristicProperties;
class BluetoothDevice;
class ExecutionContext;
class ScriptPromise;
class ScriptState;

// BluetoothRemoteGATTCharacteristic represents a GATT Characteristic, which is
// a basic data element that provides further information about a peripheral's
// service.
//
// Callbacks providing WebBluetoothRemoteGATTCharacteristicInit objects are
// handled by CallbackPromiseAdapter templatized with this class. See this
// class's "Interface required by CallbackPromiseAdapter" section and the
// CallbackPromiseAdapter class comments.
class BluetoothRemoteGATTCharacteristic final
    : public EventTargetWithInlineData,
      public ContextLifecycleObserver,
      public mojom::blink::WebBluetoothCharacteristicClient {
  USING_PRE_FINALIZER(BluetoothRemoteGATTCharacteristic, Dispose);
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(BluetoothRemoteGATTCharacteristic);

 public:
  explicit BluetoothRemoteGATTCharacteristic(
      ExecutionContext*,
      mojom::blink::WebBluetoothRemoteGATTCharacteristicPtr,
      BluetoothRemoteGATTService*,
      BluetoothDevice*);

  static BluetoothRemoteGATTCharacteristic* Create(
      ExecutionContext*,
      mojom::blink::WebBluetoothRemoteGATTCharacteristicPtr,
      BluetoothRemoteGATTService*,
      BluetoothDevice*);

  // Save value.
  void SetValue(DOMDataView*);

  // mojom::blink::WebBluetoothCharacteristicClient:
  void RemoteCharacteristicValueChanged(
      const WTF::Vector<uint8_t>& value) override;

  // ContextLifecycleObserver interface.
  void contextDestroyed(ExecutionContext*) override;

  // USING_PRE_FINALIZER interface.
  // Called before the object gets garbage collected.
  void Dispose();

  // EventTarget methods:
  const AtomicString& interfaceName() const override;
  ExecutionContext* getExecutionContext() const;

  // Interface required by garbage collection.
  DECLARE_VIRTUAL_TRACE();

  // IDL exposed interface:
  BluetoothRemoteGATTService* service() { return m_service; }
  String uuid() { return m_characteristic->uuid; }
  BluetoothCharacteristicProperties* properties() { return m_properties; }
  DOMDataView* value() const { return m_value; }
  ScriptPromise getDescriptor(ScriptState*,
                              const StringOrUnsignedLong& descriptor,
                              ExceptionState&);
  ScriptPromise getDescriptors(ScriptState*, ExceptionState&);
  ScriptPromise getDescriptors(ScriptState*,
                               const StringOrUnsignedLong& descriptor,
                               ExceptionState&);
  ScriptPromise readValue(ScriptState*);
  ScriptPromise writeValue(ScriptState*, const DOMArrayPiece&);
  ScriptPromise startNotifications(ScriptState*);
  ScriptPromise stopNotifications(ScriptState*);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(characteristicvaluechanged);

 protected:
  // EventTarget overrides.
  void addedEventListener(const AtomicString& eventType,
                          RegisteredEventListener&) override;

 private:
  friend class BluetoothRemoteGATTDescriptor;

  BluetoothRemoteGATTServer* GetGatt() { return m_service->device()->gatt(); }

  void ReadValueCallback(ScriptPromiseResolver*,
                         mojom::blink::WebBluetoothResult,
                         const Optional<Vector<uint8_t>>& value);
  void WriteValueCallback(ScriptPromiseResolver*,
                          const Vector<uint8_t>& value,
                          mojom::blink::WebBluetoothResult);
  void NotificationsCallback(ScriptPromiseResolver*,
                             mojom::blink::WebBluetoothResult);

  ScriptPromise GetDescriptorsImpl(ScriptState*,
                                   mojom::blink::WebBluetoothGATTQueryQuantity,
                                   const String& descriptorUUID = String());

  void GetDescriptorsCallback(
      const String& requestedDescriptorUUID,
      const String& characteristicInstanceId,
      mojom::blink::WebBluetoothGATTQueryQuantity,
      ScriptPromiseResolver*,
      mojom::blink::WebBluetoothResult,
      Optional<Vector<mojom::blink::WebBluetoothRemoteGATTDescriptorPtr>>
          descriptors);

  DOMException* CreateInvalidCharacteristicError();

  mojom::blink::WebBluetoothRemoteGATTCharacteristicPtr m_characteristic;
  Member<BluetoothRemoteGATTService> m_service;
  Member<BluetoothCharacteristicProperties> m_properties;
  Member<DOMDataView> m_value;
  Member<BluetoothDevice> m_device;
  mojo::AssociatedBindingSet<mojom::blink::WebBluetoothCharacteristicClient>
      m_clientBindings;
};

}  // namespace blink

#endif  // BluetoothRemoteGATTCharacteristic_h
