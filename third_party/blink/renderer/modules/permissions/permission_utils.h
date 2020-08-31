// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PERMISSIONS_PERMISSION_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PERMISSIONS_PERMISSION_UTILS_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ExecutionContext;
class ExceptionState;
class ScriptState;
class ScriptValue;

void ConnectToPermissionService(
    ExecutionContext*,
    mojo::PendingReceiver<mojom::blink::PermissionService>);

String PermissionStatusToString(mojom::blink::PermissionStatus);

mojom::blink::PermissionDescriptorPtr CreatePermissionDescriptor(
    mojom::blink::PermissionName);

mojom::blink::PermissionDescriptorPtr CreateMidiPermissionDescriptor(
    bool sysex);

mojom::blink::PermissionDescriptorPtr CreateClipboardPermissionDescriptor(
    mojom::blink::PermissionName,
    bool allow_without_gesture,
    bool allow_without_sanitization);

// Parses the raw permission dictionary and returns the Mojo
// PermissionDescriptor if parsing was successful. If an exception occurs, it
// will be stored in |exceptionState| and nullptr will be returned.
//
// Websites will be able to run code when `name()` is called, changing the
// current context. The caller should make sure that no assumption is made
// after this has been called.
MODULES_EXPORT mojom::blink::PermissionDescriptorPtr ParsePermissionDescriptor(
    ScriptState*,
    const ScriptValue& raw_permission,
    ExceptionState&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PERMISSIONS_PERMISSION_UTILS_H_
