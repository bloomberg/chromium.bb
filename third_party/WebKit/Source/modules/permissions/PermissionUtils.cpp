// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/permissions/PermissionUtils.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerThread.h"
#include "public/platform/InterfaceProvider.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace blink {

using mojom::blink::PermissionDescriptor;
using mojom::blink::PermissionDescriptorPtr;
using mojom::blink::PermissionName;

bool ConnectToPermissionService(
    ExecutionContext* execution_context,
    mojom::blink::PermissionServiceRequest request) {
  if (execution_context->IsDocument()) {
    LocalFrame* frame = ToDocument(execution_context)->GetFrame();
    if (frame) {
      frame->Client()->GetInterfaceProvider()->GetInterface(std::move(request));
      return true;
    }
  } else if (execution_context->IsWorkerGlobalScope()) {
    WorkerThread* thread = ToWorkerGlobalScope(execution_context)->GetThread();
    if (thread) {
      thread->GetInterfaceProvider()->GetInterface(std::move(request));
      return true;
    }
  }

  return false;
}

PermissionDescriptorPtr CreatePermissionDescriptor(PermissionName name) {
  auto descriptor = PermissionDescriptor::New();
  descriptor->name = name;
  return descriptor;
}

PermissionDescriptorPtr CreateMidiPermissionDescriptor(bool sysex) {
  auto descriptor =
      CreatePermissionDescriptor(mojom::blink::PermissionName::MIDI);
  auto midi_extension = mojom::blink::MidiPermissionDescriptor::New();
  midi_extension->sysex = sysex;
  descriptor->extension = mojom::blink::PermissionDescriptorExtension::New();
  descriptor->extension->set_midi(std::move(midi_extension));
  return descriptor;
}

}  // namespace blink
