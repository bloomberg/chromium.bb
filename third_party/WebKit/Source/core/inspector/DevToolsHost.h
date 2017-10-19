/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DevToolsHost_h
#define DevToolsHost_h

#include "core/CoreExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class ContextMenuItem;
class FrontendMenuProvider;
class InspectorFrontendClient;
class LocalFrame;

class CORE_EXPORT DevToolsHost final
    : public GarbageCollectedFinalized<DevToolsHost>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DevToolsHost* Create(InspectorFrontendClient* client,
                              LocalFrame* frontend_frame) {
    return new DevToolsHost(client, frontend_frame);
  }

  ~DevToolsHost();
  void Trace(blink::Visitor*);
  void DisconnectClient();

  float zoomFactor();

  float ConvertLengthForEmbedder(float length);

  void copyText(const String& text);

  void ShowContextMenu(LocalFrame* target_frame,
                       float x,
                       float y,
                       const Vector<ContextMenuItem>& items);
  void sendMessageToEmbedder(const String& message);

  String getSelectionBackgroundColor();
  String getSelectionForegroundColor();

  bool isUnderTest();
  bool isHostedMode();

  LocalFrame* FrontendFrame() { return frontend_frame_; }

  void ClearMenuProvider() { menu_provider_ = nullptr; }

 private:
  friend class FrontendMenuProvider;

  DevToolsHost(InspectorFrontendClient*, LocalFrame* frontend_frame);
  void EvaluateScript(const String&);

  InspectorFrontendClient* client_;
  Member<LocalFrame> frontend_frame_;
  Member<FrontendMenuProvider> menu_provider_;
};

}  // namespace blink

#endif  // DevToolsHost_h
