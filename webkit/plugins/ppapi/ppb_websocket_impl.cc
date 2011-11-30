// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_websocket_impl.h"

#include <string>

#include "base/logging.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSocket.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "webkit/plugins/ppapi/host_globals.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/resource_helper.h"

using ppapi::PpapiGlobals;
using ppapi::StringVar;
using ppapi::thunk::PPB_WebSocket_API;
using ppapi::VarTracker;
using WebKit::WebData;
using WebKit::WebDocument;
using WebKit::WebString;
using WebKit::WebSocket;
using WebKit::WebSocketClient;
using WebKit::WebURL;

static const uint32_t kMaxReasonSizeInBytes = 123;

namespace webkit {
namespace ppapi {

PPB_WebSocket_Impl::PPB_WebSocket_Impl(PP_Instance instance)
    : Resource(instance),
      state_(PP_WEBSOCKETREADYSTATE_INVALID_DEV),
      receive_callback_var_(NULL),
      wait_for_receive_(false),
      close_code_(0),
      close_was_clean_(PP_FALSE) {
  empty_string_ = new StringVar(
      PpapiGlobals::Get()->GetModuleForInstance(instance), "", 0);
}

PPB_WebSocket_Impl::~PPB_WebSocket_Impl() {
  if (websocket_.get())
    websocket_->disconnect();

  // Clean up received and unread messages
  VarTracker* var_tracker = PpapiGlobals::Get()->GetVarTracker();
  while (!received_messages_.empty()) {
    PP_Var var = received_messages_.front();
    received_messages_.pop();
    var_tracker->ReleaseVar(var);
  }
}

// static
PP_Resource PPB_WebSocket_Impl::Create(PP_Instance instance) {
  scoped_refptr<PPB_WebSocket_Impl> ws(new PPB_WebSocket_Impl(instance));
  return ws->GetReference();
}

PPB_WebSocket_API* PPB_WebSocket_Impl::AsPPB_WebSocket_API() {
  return this;
}

int32_t PPB_WebSocket_Impl::Connect(PP_Var url,
                                    const PP_Var protocols[],
                                    uint32_t protocol_count,
                                    PP_CompletionCallback callback) {
  // Check mandatory interfaces.
  PluginInstance* plugin_instance = ResourceHelper::GetPluginInstance(this);
  DCHECK(plugin_instance);
  if (!plugin_instance)
    return PP_ERROR_FAILED;

  // Connect() can be called at most once.
  if (websocket_.get())
    return PP_ERROR_INPROGRESS;
  if (state_ != PP_WEBSOCKETREADYSTATE_INVALID_DEV)
    return PP_ERROR_INPROGRESS;
  state_ = PP_WEBSOCKETREADYSTATE_CLOSED_DEV;

  // Validate |callback| (Doesn't support blocking callback)
  if (!callback.func)
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  // Validate url and convert it to WebURL.
  scoped_refptr<StringVar> url_string = StringVar::FromPPVar(url);
  if (!url_string)
    return PP_ERROR_BADARGUMENT;
  GURL gurl(url_string->value());
  if (!gurl.is_valid())
    return PP_ERROR_BADARGUMENT;
  if (!gurl.SchemeIs("ws") && !gurl.SchemeIs("wss"))
    return PP_ERROR_BADARGUMENT;
  if (gurl.has_ref())
    return PP_ERROR_BADARGUMENT;
  // TODO(toyoshim): Must check if the port is allowed by default.
  // We could not just use net::IsPortAllowedByDefault() because it doesn't
  // be exported over the shared library.
  WebURL web_url(gurl);

  // Validate protocols and convert it to WebString.
  // TODO(toyoshim): Detect duplicated protocols as error.
  std::string protocol_string;
  for (uint32_t i = 0; i < protocol_count; i++) {
    // TODO(toyoshim): Similar function exist in WebKit::WebSocket.
    // We must rearrange them into WebKit::WebChannel and share its protocol
    // related implementation via WebKit API.
    scoped_refptr<StringVar> string_var;
    string_var = StringVar::FromPPVar(protocols[i]);
    if (!string_var || !string_var->value().length())
      return PP_ERROR_BADARGUMENT;
    for (std::string::const_iterator it = string_var->value().begin();
        it != string_var->value().end();
        ++it) {
      uint8_t character = static_cast<uint8_t>(*it);
      // WebSocket specification says "(Subprotocol string must consist of)
      // characters in the range U+0021 to U+007E not including separator
      // characters as defined in [RFC2616]."
      const uint8_t minimumProtocolCharacter = '!';  // U+0021.
      const uint8_t maximumProtocolCharacter = '~';  // U+007E.
      if (character < minimumProtocolCharacter ||
          character > maximumProtocolCharacter ||
          character == '"' || character == '(' || character == ')' ||
          character == ',' || character == '/' ||
          (character >= ':' && character <= '@') ||  // U+003A - U+0040
          (character >= '[' && character <= ']') ||  // U+005B - u+005D
          character == '{' || character == '}')
        return PP_ERROR_BADARGUMENT;
    }
    if (i != 0)
      protocol_string.append(",");
    protocol_string.append(string_var->value());
  }
  WebString web_protocols = WebString::fromUTF8(protocol_string);

  // Create WebKit::WebSocket object.
  WebDocument document = plugin_instance->container()->element().document();
  websocket_.reset(WebSocket::create(document, this));
  DCHECK(websocket_.get());
  if (!websocket_.get())
    return PP_ERROR_NOTSUPPORTED;

  websocket_->connect(web_url, web_protocols);
  state_ = PP_WEBSOCKETREADYSTATE_CONNECTING_DEV;

  // Install callback.
  connect_callback_ = callback;

  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_WebSocket_Impl::Close(uint16_t code,
                                  PP_Var reason,
                                  PP_CompletionCallback callback) {
  // Check mandarory interfaces.
  if (!websocket_.get())
    return PP_ERROR_FAILED;

  // Validate |callback| (Doesn't support blocking callback)
  if (!callback.func)
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  // Validate |code|.
  if (code != WebSocket::CloseEventCodeNotSpecified) {
    if (!(code == WebSocket::CloseEventCodeNormalClosure ||
        (WebSocket::CloseEventCodeMinimumUserDefined <= code &&
        code <= WebSocket::CloseEventCodeMaximumUserDefined)))
    return PP_ERROR_NOACCESS;
  }
  // Validate |reason|.
  // TODO(toyoshim): Returns PP_ERROR_BADARGUMENT if |reason| contains any
  // surrogates.
  scoped_refptr<StringVar> reason_string = StringVar::FromPPVar(reason);
  if (!reason_string || reason_string->value().size() > kMaxReasonSizeInBytes)
    return PP_ERROR_BADARGUMENT;

  // Check state.
  if (state_ == PP_WEBSOCKETREADYSTATE_CLOSING_DEV ||
      state_ == PP_WEBSOCKETREADYSTATE_CLOSED_DEV)
    return PP_ERROR_INPROGRESS;

  // Install |callback|.
  close_callback_ = callback;

  if (state_ == PP_WEBSOCKETREADYSTATE_CONNECTING_DEV) {
    state_ = PP_WEBSOCKETREADYSTATE_CLOSING_DEV;
    PP_RunAndClearCompletionCallback(&connect_callback_, PP_ERROR_ABORTED);
    websocket_->fail(
        "WebSocket was closed before the connection was established.");
    return PP_OK_COMPLETIONPENDING;
  }

  // TODO(toyoshim): Handle bufferedAmount here.

  state_ = PP_WEBSOCKETREADYSTATE_CLOSING_DEV;
  WebString web_reason = WebString::fromUTF8(reason_string->value());
  websocket_->close(code, web_reason);

  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_WebSocket_Impl::ReceiveMessage(PP_Var* message,
                                           PP_CompletionCallback callback) {
  // Check state.
  if (state_ == PP_WEBSOCKETREADYSTATE_INVALID_DEV ||
      state_ == PP_WEBSOCKETREADYSTATE_CONNECTING_DEV)
    return PP_ERROR_BADARGUMENT;

  // Just return received message if any received message is queued.
  if (!received_messages_.empty())
    return DoReceive();

  // Or retain |message| as buffer to store and install |callback|.
  wait_for_receive_ = true;
  receive_callback_var_ = message;
  receive_callback_ = callback;

  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_WebSocket_Impl::SendMessage(PP_Var message) {
  // Check mandatory interfaces.
  if (!websocket_.get())
    return PP_ERROR_FAILED;

  // Check state.
  if (state_ == PP_WEBSOCKETREADYSTATE_INVALID_DEV ||
      state_ == PP_WEBSOCKETREADYSTATE_CONNECTING_DEV)
    return PP_ERROR_BADARGUMENT;

  if (state_ == PP_WEBSOCKETREADYSTATE_CLOSING_DEV ||
      state_ == PP_WEBSOCKETREADYSTATE_CLOSED_DEV) {
    // TODO(toyoshim): Handle bufferedAmount here.
  }

  if (message.type != PP_VARTYPE_STRING) {
    // TODO(toyoshim): Support binary data.
    return PP_ERROR_NOTSUPPORTED;
  }

  // Convert message to WebString.
  scoped_refptr<StringVar> message_string = StringVar::FromPPVar(message);
  if (!message_string)
    return PP_ERROR_BADARGUMENT;
  WebString web_message = WebString::fromUTF8(message_string->value());
  if (!websocket_->sendText(web_message))
    return PP_ERROR_BADARGUMENT;

  return PP_OK;
}

uint64_t PPB_WebSocket_Impl::GetBufferedAmount() {
  // TODO(toyoshim): Implement.
  return 0;
}

uint16_t PPB_WebSocket_Impl::GetCloseCode() {
  return close_code_;
}

PP_Var PPB_WebSocket_Impl::GetCloseReason() {
  if (!close_reason_)
    return empty_string_->GetPPVar();
  return close_reason_->GetPPVar();
}

PP_Bool PPB_WebSocket_Impl::GetCloseWasClean() {
  return close_was_clean_;
}

PP_Var PPB_WebSocket_Impl::GetExtensions() {
  // TODO(toyoshim): For now, always returns empty string.
  if (!extensions_)
    return empty_string_->GetPPVar();
  return extensions_->GetPPVar();
}

PP_Var PPB_WebSocket_Impl::GetProtocol() {
  // TODO(toyoshim): Implement.
  if (!protocol_)
    return empty_string_->GetPPVar();
  return protocol_->GetPPVar();
}

PP_WebSocketReadyState_Dev PPB_WebSocket_Impl::GetReadyState() {
  return state_;
}

PP_Var PPB_WebSocket_Impl::GetURL() {
  // TODO(toyoshim): For now, always returns empty string.
  if (!url_)
    return empty_string_->GetPPVar();
  return url_->GetPPVar();
}

void PPB_WebSocket_Impl::didConnect() {
  DCHECK_EQ(PP_WEBSOCKETREADYSTATE_CONNECTING_DEV, state_);
  state_ = PP_WEBSOCKETREADYSTATE_OPEN_DEV;
  PP_RunAndClearCompletionCallback(&connect_callback_, PP_OK);
}

void PPB_WebSocket_Impl::didReceiveMessage(const WebString& message) {
  // Append received data to queue.
  std::string string = message.utf8();
  PP_Var var = StringVar::StringToPPVar(
      PpapiGlobals::Get()->GetModuleForInstance(pp_instance()), string);
  received_messages_.push(var);

  if (!wait_for_receive_)
    return;

  PP_RunAndClearCompletionCallback(&receive_callback_, DoReceive());
}

void PPB_WebSocket_Impl::didReceiveBinaryData(const WebData& binaryData) {
  DLOG(INFO) << "didReceiveBinaryData is not implemented yet.";
  // TODO(toyoshim): Support to receive binary data.
}

void PPB_WebSocket_Impl::didReceiveMessageError() {
  // TODO(toyoshim): Must implement.
  DLOG(INFO) << "didReceiveMessageError is not implemented yet.";
}

void PPB_WebSocket_Impl::didUpdateBufferedAmount(unsigned long bufferedAmount) {
  // TODO(toyoshim): Must implement.
  DLOG(INFO) << "didUpdateBufferedAmount is not implemented yet.";
}

void PPB_WebSocket_Impl::didStartClosingHandshake() {
  // TODO(toyoshim): Must implement.
  DLOG(INFO) << "didStartClosingHandshake is not implemented yet.";
}

void PPB_WebSocket_Impl::didClose(unsigned long bufferedAmount,
                                  ClosingHandshakeCompletionStatus status,
                                  unsigned short code,
                                  const WebString& reason) {
  // Store code and reason.
  close_code_ = code;
  std::string reason_string = reason.utf8();
  close_reason_ = new StringVar(
      PpapiGlobals::Get()->GetModuleForInstance(pp_instance()), reason_string);

  // TODO(toyoshim): Set close_was_clean_.

  // Handle state transition and invoking callback.
  DCHECK_NE(PP_WEBSOCKETREADYSTATE_CLOSED_DEV, state_);
  PP_WebSocketReadyState_Dev state = state_;
  state_ = PP_WEBSOCKETREADYSTATE_CLOSED_DEV;

  if (state == PP_WEBSOCKETREADYSTATE_CONNECTING_DEV)
    PP_RunAndClearCompletionCallback(&connect_callback_, PP_OK);

  if (state == PP_WEBSOCKETREADYSTATE_CLOSING_DEV)
    PP_RunAndClearCompletionCallback(&close_callback_, PP_OK);
}

int32_t PPB_WebSocket_Impl::DoReceive() {
  // TODO(toyoshim): Check state.

  if (!receive_callback_var_)
    return PP_OK;

  *receive_callback_var_ = received_messages_.front();
  received_messages_.pop();
  receive_callback_var_ = NULL;
  wait_for_receive_ = false;
  return PP_OK;
}

}  // namespace ppapi
}  // namespace webkit
