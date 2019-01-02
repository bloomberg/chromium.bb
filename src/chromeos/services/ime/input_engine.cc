// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/input_engine.h"

#include "base/logging.h"
#include "base/strings/string_util.h"

namespace chromeos {
namespace ime {

InputEngine::InputEngine() {}

InputEngine::~InputEngine() {}

bool InputEngine::BindRequest(const std::string& ime_spec,
                              mojom::InputChannelRequest request,
                              mojom::InputChannelPtr client,
                              const std::vector<uint8_t>& extra) {
  if (!IsImeSupported(ime_spec))
    return false;

  channel_bindings_.AddBinding(this, std::move(request), ime_spec);
  return true;
  // TODO(https://crbug.com/837156): Registry connection error handler.
}

bool InputEngine::IsImeSupported(const std::string& ime_spec) {
  // TODO(https://crbug.com/837156): Add all supported IME tpyes.
  return false;
}

void InputEngine::ProcessText(const std::string& message,
                              ProcessTextCallback callback) {
  auto& ime_spec = channel_bindings_.dispatch_context();
  std::string result = Process(message, ime_spec);
  std::move(callback).Run(result);
}

void InputEngine::ProcessMessage(const std::vector<uint8_t>& message,
                                 ProcessMessageCallback callback) {
  NOTIMPLEMENTED();  // Protobuf message is not used in the basic engine.
}

const std::string& InputEngine::Process(const std::string& message,
                                        const std::string& ime_spec) {
  // TODO(https://crbug.com/859432) Implement the m17n model to handle message
  // by ime_spec.
  return base::EmptyString();
}

}  // namespace ime
}  // namespace chromeos
