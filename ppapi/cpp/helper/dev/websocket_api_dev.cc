// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/helper/dev/websocket_api_dev.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/websocket_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/var.h"

namespace pp {

namespace helper {

class WebSocketAPI_Dev::Implement : public WebSocket_Dev {
 public:
  Implement(Instance* instance, WebSocketAPI_Dev* api)
      : WebSocket_Dev(instance),
        api_(api),
        callback_factory_(PP_ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  }

  ~Implement() {}

  int32_t Connect(const Var& url, const Var protocols[],
                  uint32_t protocol_count) {
    return WebSocket_Dev::Connect(url, protocols, protocol_count,
        callback_factory_.NewOptionalCallback(&Implement::DidConnect));
  }

  int32_t Close(uint16_t code, const Var& reason) {
    return WebSocket_Dev::Close(code, reason,
        callback_factory_.NewOptionalCallback(&Implement::DidClose));
  }

  void Receive() {
    WebSocket_Dev::ReceiveMessage(&receive_message_var_,
        callback_factory_.NewCallback(&Implement::DidReceive));
  }

  void DidConnect(int32_t result) {
    if (result == PP_OK) {
      api_->OnOpen();
      Receive();
    } else if (result != PP_ERROR_ABORTED) {
      DidClose(result);
    }
  }

  void DidReceive(int32_t result) {
    if (result == PP_OK) {
      api_->OnMessage(receive_message_var_);
      Receive();
    } else if (result != PP_ERROR_ABORTED) {
      DidClose(result);
    }
  }

  void DidClose(int32_t result) {
    bool was_clean = GetCloseWasClean() && result == PP_OK;
    if (!was_clean)
      api_->OnError();
    api_->OnClose(was_clean, GetCloseCode(), GetCloseReason());
  }

 private:
  WebSocketAPI_Dev* api_;
  CompletionCallbackFactory<Implement> callback_factory_;
  Var receive_message_var_;
};

WebSocketAPI_Dev::WebSocketAPI_Dev(Instance* instance)
    : impl_(new Implement(instance, PP_ALLOW_THIS_IN_INITIALIZER_LIST(this))) {
}

WebSocketAPI_Dev::~WebSocketAPI_Dev() {
  delete impl_;
}

int32_t WebSocketAPI_Dev::Connect(const Var& url, const Var protocols[],
                                  uint32_t protocol_count) {
  return impl_->Connect(url, protocols, protocol_count);
}

int32_t WebSocketAPI_Dev::Close(uint16_t code, const Var& reason) {
  return impl_->Close(code, reason);
}

int32_t WebSocketAPI_Dev::Send(const Var& data) {
  return impl_->SendMessage(data);
}

uint64_t WebSocketAPI_Dev::GetBufferedAmount() {
  return impl_->GetBufferedAmount();
}

Var WebSocketAPI_Dev::GetExtensions() {
  return impl_->GetExtensions();
}

Var WebSocketAPI_Dev::GetProtocol() {
  return impl_->GetProtocol();
}

PP_WebSocketReadyState_Dev WebSocketAPI_Dev::GetReadyState() {
  return impl_->GetReadyState();
}

Var WebSocketAPI_Dev::GetURL() {
  return impl_->GetURL();
}

}  // namespace helper

}  // namespace pp
