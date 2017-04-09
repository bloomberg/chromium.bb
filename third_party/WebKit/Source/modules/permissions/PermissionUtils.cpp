// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/permissions/PermissionUtils.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"

namespace blink {

using mojom::blink::PermissionDescriptor;
using mojom::blink::PermissionDescriptorPtr;
using mojom::blink::PermissionName;

bool ConnectToPermissionService(
    ExecutionContext* execution_context,
    mojom::blink::PermissionServiceRequest request) {
  InterfaceProvider* interface_provider = nullptr;
  if (execution_context->IsDocument()) {
    Document* document = ToDocument(execution_context);
    if (document->GetFrame())
      interface_provider = document->GetFrame()->GetInterfaceProvider();
  } else {
    interface_provider = Platform::Current()->GetInterfaceProvider();
  }

  if (interface_provider)
    interface_provider->GetInterface(std::move(request));
  return interface_provider;
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
