// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_websocket_impl.h"

#include <set>
#include <string>

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppb_var_array_buffer.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebArrayBuffer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSocket.h"
#include "webkit/plugins/ppapi/host_array_buffer_var.h"
#include "webkit/plugins/ppapi/host_globals.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/resource_helper.h"

using ppapi::ArrayBufferVar;
using ppapi::PpapiGlobals;
using ppapi::StringVar;
using ppapi::thunk::PPB_WebSocket_API;
using ppapi::TrackedCallback;
using ppapi::Var;
using ppapi::VarTracker;
using WebKit::WebArrayBuffer;
using WebKit::WebDocument;
using WebKit::WebString;
using WebKit::WebSocket;
using WebKit::WebSocketClient;
using WebKit::WebURL;

const uint32_t kMaxReasonSizeInBytes = 123;
const size_t kHybiBaseFramingOverhead = 2;
const size_t kHybiMaskingKeyLength = 4;
const size_t kMinimumPayloadSizeWithTwoByteExtendedPayloadLength = 126;
const size_t kMinimumPayloadSizeWithEightByteExtendedPayloadLength = 0x10000;

namespace {

uint64_t SaturateAdd(uint64_t a, uint64_t b) {
  if (kuint64max - a < b)
    return kuint64max;
  return a + b;
}

uint64_t GetFrameSize(uint64_t payload_size) {
  uint64_t overhead = kHybiBaseFramingOverhead + kHybiMaskingKeyLength;
  if (payload_size > kMinimumPayloadSizeWithEightByteExtendedPayloadLength)
    overhead += 8;
  else if (payload_size > kMinimumPayloadSizeWithTwoByteExtendedPayloadLength)
    overhead += 2;
  return SaturateAdd(payload_size, overhead);
}

bool InValidStateToReceive(PP_WebSocketReadyState state) {
  return state == PP_WEBSOCKETREADYSTATE_OPEN ||
      state == PP_WEBSOCKETREADYSTATE_CLOSING;
}

}  // namespace

namespace webkit {
namespace ppapi {

PPB_WebSocket_Impl::PPB_WebSocket_Impl(PP_Instance instance)
    : Resource(::ppapi::OBJECT_IS_IMPL, instance),
      state_(PP_WEBSOCKETREADYSTATE_INVALID),
      error_was_received_(false),
      receive_callback_var_(NULL),
      wait_for_receive_(false),
      close_code_(0),
      close_was_clean_(PP_FALSE),
      empty_string_(new StringVar("", 0)),
      buffered_amount_(0),
      buffered_amount_after_close_(0) {
}

PPB_WebSocket_Impl::~PPB_WebSocket_Impl() {
  if (websocket_.get())
    websocket_->disconnect();
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
  if (state_ != PP_WEBSOCKETREADYSTATE_INVALID)
    return PP_ERROR_INPROGRESS;
  state_ = PP_WEBSOCKETREADYSTATE_CLOSED;

  // Validate url and convert it to WebURL.
  scoped_refptr<StringVar> url_string = StringVar::FromPPVar(url);
  if (!url_string)
    return PP_ERROR_BADARGUMENT;
  GURL gurl(url_string->value());
  url_ = new StringVar(gurl.spec());
  if (!gurl.is_valid())
    return PP_ERROR_BADARGUMENT;
  if (!gurl.SchemeIs("ws") && !gurl.SchemeIs("wss"))
    return PP_ERROR_BADARGUMENT;
  if (gurl.has_ref())
    return PP_ERROR_BADARGUMENT;
  if (!net::IsPortAllowedByDefault(gurl.IntPort()))
    return PP_ERROR_BADARGUMENT;
  WebURL web_url(gurl);

  // Validate protocols and convert it to WebString.
  std::string protocol_string;
  std::set<std::string> protocol_set;
  for (uint32_t i = 0; i < protocol_count; i++) {
    // TODO(toyoshim): Similar function exist in WebKit::WebSocket.
    // We must rearrange them into WebKit::WebChannel and share its protocol
    // related implementation via WebKit API.
    scoped_refptr<StringVar> protocol(StringVar::FromPPVar(protocols[i]));

    // Check invalid and empty entries.
    if (!protocol || !protocol->value().length())
      return PP_ERROR_BADARGUMENT;

    // Check duplicated protocol entries.
    if (protocol_set.find(protocol->value()) != protocol_set.end())
      return PP_ERROR_BADARGUMENT;
    protocol_set.insert(protocol->value());

    // Check containing characters.
    for (std::string::const_iterator it = protocol->value().begin();
        it != protocol->value().end();
        ++it) {
      uint8_t character = *it;
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
    // Join protocols with the comma separator.
    if (i != 0)
      protocol_string.append(",");
    protocol_string.append(protocol->value());
  }
  WebString web_protocols = WebString::fromUTF8(protocol_string);

  // Validate |callback| (Doesn't support blocking callback)
  if (!callback.func)
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  // Create WebKit::WebSocket object and connect.
  WebDocument document = plugin_instance->container()->element().document();
  websocket_.reset(WebSocket::create(document, this));
  DCHECK(websocket_.get());
  if (!websocket_.get())
    return PP_ERROR_NOTSUPPORTED;

  // Set receiving binary object type.
  websocket_->setBinaryType(WebSocket::BinaryTypeArrayBuffer);

  websocket_->connect(web_url, web_protocols);
  state_ = PP_WEBSOCKETREADYSTATE_CONNECTING;

  // Install callback.
  connect_callback_ = new TrackedCallback(this, callback);

  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_WebSocket_Impl::Close(uint16_t code,
                                  PP_Var reason,
                                  PP_CompletionCallback callback) {
  // Check mandarory interfaces.
  if (!websocket_.get())
    return PP_ERROR_FAILED;

  // Validate |code|. Need to cast |CloseEventCodeNotSpecified| which is -1 to
  // uint16_t for the comparison to work.
  if (code != static_cast<uint16_t>(WebSocket::CloseEventCodeNotSpecified)) {
    if (!(code == WebSocket::CloseEventCodeNormalClosure ||
        (WebSocket::CloseEventCodeMinimumUserDefined <= code &&
        code <= WebSocket::CloseEventCodeMaximumUserDefined)))
    // RFC 6455 limits applications to use reserved connection close code in
    // section 7.4.2.. The WebSocket API (http://www.w3.org/TR/websockets/)
    // defines this out of range error as InvalidAccessError in JavaScript.
    return PP_ERROR_NOACCESS;
  }

  scoped_refptr<StringVar> reason_string;
  WebString web_reason;
  // |reason| must be ignored if it is PP_VARTYPE_UNDEFINED.
  if (reason.type != PP_VARTYPE_UNDEFINED) {
    // Validate |reason|.
    reason_string = StringVar::FromPPVar(reason);
    if (!reason_string ||
        reason_string->value().size() > kMaxReasonSizeInBytes)
      return PP_ERROR_BADARGUMENT;
    web_reason = WebString::fromUTF8(reason_string->value());
  }

  // Check state.
  if (state_ == PP_WEBSOCKETREADYSTATE_CLOSING)
    return PP_ERROR_INPROGRESS;
  if (state_ == PP_WEBSOCKETREADYSTATE_CLOSED)
    return PP_OK;

  // Validate |callback| (Doesn't support blocking callback)
  if (!callback.func)
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  // Install |callback|.
  close_callback_ = new TrackedCallback(this, callback);

  // Abort ongoing connect.
  if (state_ == PP_WEBSOCKETREADYSTATE_CONNECTING) {
    state_ = PP_WEBSOCKETREADYSTATE_CLOSING;
    // Need to do a "Post" to avoid reentering the plugin.
    connect_callback_->PostAbort();
    connect_callback_ = NULL;
    websocket_->fail(
        "WebSocket was closed before the connection was established.");
    return PP_OK_COMPLETIONPENDING;
  }

  // Abort ongoing receive.
  if (wait_for_receive_) {
    wait_for_receive_ = false;
    receive_callback_var_ = NULL;

    // Need to do a "Post" to avoid reentering the plugin.
    receive_callback_->PostAbort();
    receive_callback_ = NULL;
  }

  // Close connection.
  state_ = PP_WEBSOCKETREADYSTATE_CLOSING;
  websocket_->close(code, web_reason);

  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_WebSocket_Impl::ReceiveMessage(PP_Var* message,
                                           PP_CompletionCallback callback) {
  // Check state.
  if (state_ == PP_WEBSOCKETREADYSTATE_INVALID ||
      state_ == PP_WEBSOCKETREADYSTATE_CONNECTING)
    return PP_ERROR_BADARGUMENT;

  // Just return received message if any received message is queued.
  if (!received_messages_.empty()) {
    receive_callback_var_ = message;
    return DoReceive();
  }

  // Check state again. In CLOSED state, no more messages will be received.
  if (state_ == PP_WEBSOCKETREADYSTATE_CLOSED)
    return PP_ERROR_BADARGUMENT;

  // Returns PP_ERROR_FAILED after an error is received and received messages
  // is exhausted.
  if (error_was_received_)
    return PP_ERROR_FAILED;

  // Validate |callback| (Doesn't support blocking callback)
  if (!callback.func)
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  // Or retain |message| as buffer to store and install |callback|.
  wait_for_receive_ = true;
  receive_callback_var_ = message;
  receive_callback_ = new TrackedCallback(this, callback);

  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_WebSocket_Impl::SendMessage(PP_Var message) {
  // Check mandatory interfaces.
  if (!websocket_.get())
    return PP_ERROR_FAILED;

  // Check state.
  if (state_ == PP_WEBSOCKETREADYSTATE_INVALID ||
      state_ == PP_WEBSOCKETREADYSTATE_CONNECTING)
    return PP_ERROR_BADARGUMENT;

  if (state_ == PP_WEBSOCKETREADYSTATE_CLOSING ||
      state_ == PP_WEBSOCKETREADYSTATE_CLOSED) {
    // Handle buffered_amount_after_close_.
    uint64_t payload_size = 0;
    if (message.type == PP_VARTYPE_STRING) {
      scoped_refptr<StringVar> message_string = StringVar::FromPPVar(message);
      if (message_string)
        payload_size += message_string->value().length();
    } else if (message.type == PP_VARTYPE_ARRAY_BUFFER) {
      scoped_refptr<ArrayBufferVar> message_array_buffer =
          ArrayBufferVar::FromPPVar(message);
      if (message_array_buffer)
        payload_size += message_array_buffer->ByteLength();
    } else {
      // TODO(toyoshim): Support Blob.
      return PP_ERROR_NOTSUPPORTED;
    }

    buffered_amount_after_close_ =
        SaturateAdd(buffered_amount_after_close_, GetFrameSize(payload_size));

    return PP_ERROR_FAILED;
  }

  // Send the message.
  if (message.type == PP_VARTYPE_STRING) {
    // Convert message to WebString.
    scoped_refptr<StringVar> message_string = StringVar::FromPPVar(message);
    if (!message_string)
      return PP_ERROR_BADARGUMENT;
    WebString web_message = WebString::fromUTF8(message_string->value());
    if (!websocket_->sendText(web_message))
      return PP_ERROR_BADARGUMENT;
  } else if (message.type == PP_VARTYPE_ARRAY_BUFFER) {
    // Convert message to WebArrayBuffer.
    scoped_refptr<HostArrayBufferVar> host_message =
        static_cast<HostArrayBufferVar*>(ArrayBufferVar::FromPPVar(message));
    if (!host_message)
      return PP_ERROR_BADARGUMENT;
    WebArrayBuffer& web_message = host_message->webkit_buffer();
    if (!websocket_->sendArrayBuffer(web_message))
      return PP_ERROR_BADARGUMENT;
  } else {
    // TODO(toyoshim): Support Blob.
    return PP_ERROR_NOTSUPPORTED;
  }

  return PP_OK;
}

uint64_t PPB_WebSocket_Impl::GetBufferedAmount() {
  return SaturateAdd(buffered_amount_, buffered_amount_after_close_);
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
  // Check mandatory interfaces.
  if (!websocket_.get())
    return empty_string_->GetPPVar();

  std::string extensions = websocket_->extensions().utf8();
  return StringVar::StringToPPVar(extensions);
}

PP_Var PPB_WebSocket_Impl::GetProtocol() {
  // Check mandatory interfaces.
  if (!websocket_.get())
    return empty_string_->GetPPVar();

  std::string protocol = websocket_->subprotocol().utf8();
  return StringVar::StringToPPVar(protocol);
}

PP_WebSocketReadyState PPB_WebSocket_Impl::GetReadyState() {
  return state_;
}

PP_Var PPB_WebSocket_Impl::GetURL() {
  if (!url_)
    return empty_string_->GetPPVar();
  return url_->GetPPVar();
}

void PPB_WebSocket_Impl::didConnect() {
  DCHECK_EQ(PP_WEBSOCKETREADYSTATE_CONNECTING, state_);
  state_ = PP_WEBSOCKETREADYSTATE_OPEN;
  TrackedCallback::ClearAndRun(&connect_callback_, PP_OK);
}

void PPB_WebSocket_Impl::didReceiveMessage(const WebString& message) {
  // Dispose packets after receiving an error or in invalid state.
  if (error_was_received_ || !InValidStateToReceive(state_))
    return;

  // Append received data to queue.
  std::string string = message.utf8();
  received_messages_.push(scoped_refptr<Var>(new StringVar(string)));

  if (!wait_for_receive_)
    return;

  TrackedCallback::ClearAndRun(&receive_callback_, DoReceive());
}

void PPB_WebSocket_Impl::didReceiveArrayBuffer(
    const WebArrayBuffer& binaryData) {
  // Dispose packets after receiving an error or in invalid state.
  if (error_was_received_ || !InValidStateToReceive(state_))
    return;

  // Append received data to queue.
  received_messages_.push(
      scoped_refptr<Var>(new HostArrayBufferVar(binaryData)));

  if (!wait_for_receive_)
    return;

  TrackedCallback::ClearAndRun(&receive_callback_, DoReceive());
}

void PPB_WebSocket_Impl::didReceiveMessageError() {
  // Ignore error notification in invalid state.
  if (!InValidStateToReceive(state_))
    return;

  // Records the error, then stops receiving any frames after this error.
  // The error will be notified after all queued messages are read via
  // ReceiveMessage().
  error_was_received_ = true;
  if (!wait_for_receive_)
    return;

  // But, if no messages are queued and ReceiveMessage() is now on going.
  // We must invoke the callback with error code here.
  wait_for_receive_ = false;
  receive_callback_var_ = NULL;
  TrackedCallback::ClearAndRun(&receive_callback_, PP_ERROR_FAILED);
}

void PPB_WebSocket_Impl::didUpdateBufferedAmount(
    unsigned long buffered_amount) {
  if (state_ == PP_WEBSOCKETREADYSTATE_CLOSED)
    return;
  buffered_amount_ = buffered_amount;
}

void PPB_WebSocket_Impl::didStartClosingHandshake() {
  state_ = PP_WEBSOCKETREADYSTATE_CLOSING;
}

void PPB_WebSocket_Impl::didClose(unsigned long unhandled_buffered_amount,
                                  ClosingHandshakeCompletionStatus status,
                                  unsigned short code,
                                  const WebString& reason) {
  // Store code and reason.
  close_code_ = code;
  close_reason_ = new StringVar(reason.utf8());

  // Set close_was_clean_.
  bool was_clean =
      state_ == PP_WEBSOCKETREADYSTATE_CLOSING &&
      !unhandled_buffered_amount &&
      status == WebSocketClient::ClosingHandshakeComplete;
  close_was_clean_ = was_clean ? PP_TRUE : PP_FALSE;

  // Update buffered_amount_.
  buffered_amount_ = unhandled_buffered_amount;

  // Handle state transition and invoking callback.
  DCHECK_NE(PP_WEBSOCKETREADYSTATE_CLOSED, state_);
  PP_WebSocketReadyState state = state_;
  state_ = PP_WEBSOCKETREADYSTATE_CLOSED;

  if (state == PP_WEBSOCKETREADYSTATE_CONNECTING)
    TrackedCallback::ClearAndRun(&connect_callback_, PP_ERROR_FAILED);

  if (wait_for_receive_) {
    wait_for_receive_ = false;
    receive_callback_var_ = NULL;
    TrackedCallback::ClearAndRun(&receive_callback_, PP_ERROR_FAILED);
  }

  if ((state == PP_WEBSOCKETREADYSTATE_CLOSING) && close_callback_.get())
    TrackedCallback::ClearAndRun(&close_callback_, PP_OK);

  // Disconnect.
  if (websocket_.get())
    websocket_->disconnect();
}

int32_t PPB_WebSocket_Impl::DoReceive() {
  if (!receive_callback_var_)
    return PP_OK;

  *receive_callback_var_ = received_messages_.front()->GetPPVar();
  received_messages_.pop();
  receive_callback_var_ = NULL;
  wait_for_receive_ = false;
  return PP_OK;
}

}  // namespace ppapi
}  // namespace webkit
