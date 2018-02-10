/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/speech/SpeechRecognitionController.h"

#include <memory>

namespace blink {

const char SpeechRecognitionController::kSupplementName[] =
    "SpeechRecognitionController";

SpeechRecognitionController::SpeechRecognitionController(
    std::unique_ptr<SpeechRecognitionClient> client)
    : client_(std::move(client)) {}

SpeechRecognitionController::~SpeechRecognitionController() {
  // FIXME: Call m_client->pageDestroyed(); once we have implemented a client.
}

SpeechRecognitionController* SpeechRecognitionController::Create(
    std::unique_ptr<SpeechRecognitionClient> client) {
  return new SpeechRecognitionController(std::move(client));
}

void ProvideSpeechRecognitionTo(
    Page& page,
    std::unique_ptr<SpeechRecognitionClient> client) {
  SpeechRecognitionController::ProvideTo(
      page, SpeechRecognitionController::Create(std::move(client)));
}

}  // namespace blink
