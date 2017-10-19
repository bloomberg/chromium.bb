/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "core/html/forms/ColorChooserUIController.h"

#include "core/frame/WebLocalFrameImpl.h"
#include "core/html/forms/ColorChooserClient.h"
#include "platform/graphics/Color.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/WebColor.h"
#include "public/web/WebColorChooser.h"
#include "public/web/WebColorSuggestion.h"
#include "public/web/WebFrameClient.h"

namespace blink {

ColorChooserUIController::ColorChooserUIController(LocalFrame* frame,
                                                   ColorChooserClient* client)
    : client_(client), frame_(frame) {}

ColorChooserUIController::~ColorChooserUIController() {
  // The client cannot be accessed when finalizing.
  client_ = nullptr;
  EndChooser();
}

void ColorChooserUIController::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_);
  visitor->Trace(client_);
  ColorChooser::Trace(visitor);
}

void ColorChooserUIController::OpenUI() {
  OpenColorChooser();
}

void ColorChooserUIController::SetSelectedColor(const Color& color) {
  if (chooser_)
    chooser_->SetSelectedColor(static_cast<WebColor>(color.Rgb()));
}

void ColorChooserUIController::EndChooser() {
  if (chooser_)
    chooser_->EndChooser();
}

AXObject* ColorChooserUIController::RootAXObject() {
  return nullptr;
}

void ColorChooserUIController::DidChooseColor(const WebColor& color) {
  DCHECK(client_);
  client_->DidChooseColor(Color(static_cast<RGBA32>(color)));
}

void ColorChooserUIController::DidEndChooser() {
  chooser_ = nullptr;
  if (client_)
    client_->DidEndChooser();
}

void ColorChooserUIController::OpenColorChooser() {
  DCHECK(!chooser_);
  WebLocalFrameImpl* frame = WebLocalFrameImpl::FromFrame(frame_);
  WebFrameClient* web_frame_client = frame->Client();
  if (!web_frame_client)
    return;
  chooser_ = WTF::WrapUnique(web_frame_client->CreateColorChooser(
      this, static_cast<WebColor>(client_->CurrentColor().Rgb()),
      client_->Suggestions()));
}

}  // namespace blink
