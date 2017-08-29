// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PermissionStatus_h
#define PermissionStatus_h

#include "core/dom/SuspendableObject.h"
#include "core/dom/events/EventTarget.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "platform/bindings/ActiveScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/modules/permissions/permission.mojom-blink.h"

namespace blink {

class ExecutionContext;
class ScriptPromiseResolver;

// Expose the status of a given permission type for the current
// ExecutionContext.
class PermissionStatus final : public EventTargetWithInlineData,
                               public ActiveScriptWrappable<PermissionStatus>,
                               public SuspendableObject,
                               public mojom::blink::PermissionObserver {
  USING_GARBAGE_COLLECTED_MIXIN(PermissionStatus);
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(PermissionStatus, Dispose);

  using MojoPermissionDescriptor = mojom::blink::PermissionDescriptorPtr;
  using MojoPermissionStatus = mojom::blink::PermissionStatus;

 public:
  static PermissionStatus* Take(ScriptPromiseResolver*,
                                MojoPermissionStatus,
                                MojoPermissionDescriptor);

  static PermissionStatus* CreateAndListen(ExecutionContext*,
                                           MojoPermissionStatus,
                                           MojoPermissionDescriptor);
  ~PermissionStatus() override;
  void Dispose();

  // EventTarget implementation.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // ScriptWrappable implementation.
  bool HasPendingActivity() const final;

  // SuspendableObject implementation.
  void Suspend() override;
  void Resume() override;
  void ContextDestroyed(ExecutionContext*) override;

  String state() const;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(change);

  DECLARE_VIRTUAL_TRACE();

 private:
  PermissionStatus(ExecutionContext*,
                   MojoPermissionStatus,
                   MojoPermissionDescriptor);

  void StartListening();
  void StopListening();

  void OnPermissionStatusChange(MojoPermissionStatus);

  MojoPermissionStatus status_;
  MojoPermissionDescriptor descriptor_;
  mojo::Binding<mojom::blink::PermissionObserver> binding_;
};

}  // namespace blink

#endif  // PermissionStatus_h
