/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DevToolsFrontendImpl_h
#define DevToolsFrontendImpl_h

#include "controller/ControllerExport.h"
#include "core/inspector/InspectorFrontendClient.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/text/WTFString.h"
#include "public/web/devtools_frontend.mojom-blink.h"

namespace blink {

class DevToolsHost;
class LocalFrame;

// This class lives as long as a frame (being a supplement), or until
// it's host (mojom.DevToolsFrontendHost) is destroyed.
class CONTROLLER_EXPORT DevToolsFrontendImpl final
    : public GarbageCollectedFinalized<DevToolsFrontendImpl>,
      public Supplement<LocalFrame>,
      public mojom::blink::DevToolsFrontend,
      public InspectorFrontendClient {
  WTF_MAKE_NONCOPYABLE(DevToolsFrontendImpl);
  USING_GARBAGE_COLLECTED_MIXIN(DevToolsFrontendImpl);

 public:
  static void BindMojoRequest(LocalFrame*,
                              mojom::blink::DevToolsFrontendAssociatedRequest);
  static DevToolsFrontendImpl* From(LocalFrame*);

  ~DevToolsFrontendImpl() override;
  void DidClearWindowObject();
  void Trace(blink::Visitor*);

 private:
  DevToolsFrontendImpl(LocalFrame&,
                       mojom::blink::DevToolsFrontendAssociatedRequest);
  static const char* SupplementName();
  void DestroyOnHostGone();

  // mojom::blink::DevToolsFrontend implementation.
  void SetupDevToolsFrontend(
      const String& api_script,
      mojom::blink::DevToolsFrontendHostAssociatedPtrInfo) override;
  void SetupDevToolsExtensionAPI(const String& extension_api) override;

  // InspectorFrontendClient implementation.
  void SendMessageToEmbedder(const String&) override;
  bool IsUnderTest() override;
  void ShowContextMenu(LocalFrame*,
                       float x,
                       float y,
                       ContextMenuProvider*) override;

  Member<DevToolsHost> devtools_host_;
  String api_script_;
  mojom::blink::DevToolsFrontendHostAssociatedPtr host_;
  mojo::AssociatedBinding<mojom::blink::DevToolsFrontend> binding_;
};

}  // namespace blink

#endif
