// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/input_engine.h"

#include "base/json/json_reader.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chromeos/services/ime/public/cpp/rulebased/engine.h"

namespace chromeos {
namespace ime {

namespace {

std::string GetIdFromImeSpec(const std::string& ime_spec) {
  static const std::string kPrefix("m17n:");
  return base::StartsWith(ime_spec, kPrefix, base::CompareCase::SENSITIVE)
             ? ime_spec.substr(kPrefix.length())
             : std::string();
}

}  // namespace

InputEngineContext::InputEngineContext(const std::string& ime) : ime_spec(ime) {
  // The |ime_spec|'s format for rule based imes is: "m17n:<id>".
  std::string id = GetIdFromImeSpec(ime_spec);
  if (rulebased::Engine::IsImeSupported(id)) {
    engine = std::make_unique<rulebased::Engine>();
    engine->Activate(id);
  }
}

InputEngineContext::~InputEngineContext() {}

InputEngine::InputEngine() {}

InputEngine::~InputEngine() {}

bool InputEngine::BindRequest(
    const std::string& ime_spec,
    mojo::PendingReceiver<mojom::InputChannel> receiver,
    mojo::PendingRemote<mojom::InputChannel> remote,
    const std::vector<uint8_t>& extra) {
  if (!IsImeSupported(ime_spec))
    return false;

  channel_receivers_.Add(this, std::move(receiver),
                         std::make_unique<InputEngineContext>(ime_spec));

  return true;
  // TODO(https://crbug.com/837156): Registry connection error handler.
}

bool InputEngine::IsImeSupported(const std::string& ime_spec) {
  return rulebased::Engine::IsImeSupported(GetIdFromImeSpec(ime_spec));
}

void InputEngine::ProcessText(const std::string& message,
                              ProcessTextCallback callback) {
  auto& context = channel_receivers_.current_context();
  std::string result = Process(message, context.get());
  std::move(callback).Run(result);
}

void InputEngine::ProcessMessage(const std::vector<uint8_t>& message,
                                 ProcessMessageCallback callback) {
  NOTIMPLEMENTED();  // Protobuf message is not used in the rulebased engine.
}

std::string InputEngine::Process(const std::string& message,
                                 const InputEngineContext* context) {
  std::string ime_spec = context->ime_spec;
  auto& engine = context->engine;
  if (!engine)
    return std::string();

  const char kFalseResponse[] = "{\"result\":false}";

  // The request message is in JSON format as:
  // {
  //   'method': <string>,  // 'reset' or 'keyEvent'
  //   'type': <string>,    // 'keydown' or 'keyup'
  //   'code': <string>,    // e.g. 'KeyA', 'Backspace', etc.
  //   'shift': <boolean>,
  //   'altgr': <boolean>,
  //   'caps': <boolean>,
  //   'ctrl': <boolean>,
  //   'alt': <boolean>
  // }
  // TODO(shuchen): make parser/writer util class for the JSON-based protocol.
  int error_code;
  std::string error_string;
  std::unique_ptr<base::Value> message_value =
      base::JSONReader::ReadAndReturnErrorDeprecated(
          message, base::JSON_PARSE_RFC, &error_code, &error_string);
  if (!message_value) {
    LOG(ERROR) << "Read message error: " << error_code << "; " << error_string;
    return kFalseResponse;
  }
  base::Value* method = message_value->FindKey("method");
  if (!method)
    return kFalseResponse;

  const std::string& method_str = method->GetString();
  if (method_str == "countKey") {
    return std::to_string(engine->process_key_count());
  }

  if (method_str == "reset") {
    engine->Reset();
    return "{\"result\":true}";
  }

  if (method_str == "keyEvent") {
    base::Value* type = message_value->FindKey("type");
    if (!type || type->GetString() != "keydown")
      return kFalseResponse;
  }

  base::Value* code = message_value->FindKey("code");
  base::Value* shift = message_value->FindKey("shift");
  base::Value* altgr = message_value->FindKey("altgr");
  base::Value* caps = message_value->FindKey("caps");
  if (!code || !shift || !altgr || !caps)
    return kFalseResponse;

  uint8_t modifiers = 0;
  if (shift->GetBool())
    modifiers |= rulebased::MODIFIER_SHIFT;
  if (altgr->GetBool())
    modifiers |= rulebased::MODIFIER_ALTGR;
  if (caps->GetBool())
    modifiers |= rulebased::MODIFIER_CAPSLOCK;

  rulebased::ProcessKeyResult res =
      engine->ProcessKey(code->GetString(), modifiers);

  // The response message is in JSON format as:
  // {
  //   'result': <boolean>,
  //   'operations': [
  //     {
  //       'method': 'commitText|setComposition',
  //       'arguments': <string>
  //     }
  //   ]
  // }
  std::string response_str = "{\"result\":";
  response_str += (res.key_handled ? "true" : "false");
  std::vector<std::string> ops;
  if (!res.commit_text.empty()) {
    std::string commit_text;
    base::EscapeJSONString(res.commit_text, false, &commit_text);
    ops.push_back("{\"method\":\"commitText\",\"arguments\":[\"" + commit_text +
                  "\"]}");
  }
  // Need to add the setComposition operation to the result when the key is
  // handled and commit_text and composition_text are both empty.
  // That is the case of using Backspace to delete the last character in
  // composition.
  if (!res.composition_text.empty() ||
      (res.key_handled && res.commit_text.empty())) {
    std::string composition_text;
    base::EscapeJSONString(res.composition_text, false, &composition_text);
    ops.push_back("{\"method\":\"setComposition\",\"arguments\":[\"" +
                  composition_text + "\"]}");
  }
  if (ops.empty()) {
    response_str += "}";
  } else {
    response_str += ",\"operations\":[";
    for (size_t i = 0; i < ops.size(); ++i) {
      if (i > 0)
        response_str += ",";
      response_str += ops[i];
    }
    response_str += "]}";
  }

  return response_str;
}

}  // namespace ime
}  // namespace chromeos
