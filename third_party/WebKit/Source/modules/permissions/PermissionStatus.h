// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PermissionStatus_h
#define PermissionStatus_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "core/dom/SuspendableObject.h"
#include "core/events/EventTarget.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/permissions/permission.mojom-blink.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExecutionContext;
class ScriptPromiseResolver;

// Expose the status of a given WebPermissionType for the current
// ExecutionContext.
class PermissionStatus final : public EventTargetWithInlineData,
                               public ActiveScriptWrappable,
                               public SuspendableObject {
  USING_GARBAGE_COLLECTED_MIXIN(PermissionStatus);
  DEFINE_WRAPPERTYPEINFO();

  using MojoPermissionDescriptor = mojom::blink::PermissionDescriptorPtr;
  using MojoPermissionStatus = mojom::blink::PermissionStatus;

 public:
  static PermissionStatus* take(ScriptPromiseResolver*,
                                MojoPermissionStatus,
                                MojoPermissionDescriptor);

  static PermissionStatus* createAndListen(ExecutionContext*,
                                           MojoPermissionStatus,
                                           MojoPermissionDescriptor);
  ~PermissionStatus() override;

  // EventTarget implementation.
  const AtomicString& interfaceName() const override;
  ExecutionContext* getExecutionContext() const override;

  // ScriptWrappable implementation.
  bool hasPendingActivity() const final;

  // SuspendableObject implementation.
  void suspend() override;
  void resume() override;
  void contextDestroyed() override;

  String state() const;
  void permissionChanged(MojoPermissionStatus);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(change);

  DECLARE_VIRTUAL_TRACE();

 private:
  PermissionStatus(ExecutionContext*,
                   MojoPermissionStatus,
                   MojoPermissionDescriptor);

  void startListening();
  void stopListening();

  MojoPermissionStatus m_status;
  MojoPermissionDescriptor m_descriptor;
  mojom::blink::PermissionServicePtr m_service;
};

}  // namespace blink

#endif  // PermissionStatus_h
