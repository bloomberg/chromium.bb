// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_HELPER_DEV_WEBSOCKET_API_DEV_H_
#define PPAPI_CPP_HELPER_DEV_WEBSOCKET_API_DEV_H_

#include "ppapi/c/dev/ppb_websocket_dev.h"

/// @file
/// This file defines the helper::WebSocketAPI_Dev interface.

namespace pp {

class CompletionCallback;
class Instance;
class Var;

namespace helper {

/// The <code>WebSocketAPI_Dev</code> class
class WebSocketAPI_Dev {
 public:
  /// Constructs a WebSocketAPI_Dev object.
  WebSocketAPI_Dev(Instance* instance);

  /// Destructs a WebSocketAPI_Dev object.
  virtual ~WebSocketAPI_Dev();

  /// Connect() connects to the specified WebSocket server. Caller can call
  /// this method at most once.
  ///
  /// @param[in] url A <code>Var</code> of string type representing a WebSocket
  /// server URL.
  /// @param[in] protocols A pointer to an array of string type
  /// <code>Var</code> specifying sub-protocols. Each <code>Var</code>
  /// represents one sub-protocol and its <code>PP_VarType</code> must be
  /// <code>PP_VARTYPE_STRING</code>. This argument can be null only if
  /// <code>protocol_count</code> is 0.
  /// @param[in] protocol_count The number of sub-protocols in
  /// <code>protocols</code>.
  ///
  /// @return An int32_t containing an error code from
  /// <code>pp_errors.h</code>.
  /// See also <code>pp::WebSocket_Dev::Connect</code>.
  int32_t Connect(const Var& url, const Var protocols[],
                  uint32_t protocol_count);

  /// Close() closes the specified WebSocket connection by specifying
  /// <code>code</code> and <code>reason</code>.
  ///
  /// @param[in] code The WebSocket close code. Ignored if it is 0.
  /// @param[in] reason A <code>Var</code> of string type which represents the
  /// WebSocket close reason. Ignored if it is undefined type.
  ///
  /// @return An int32_t containing an error code from
  /// <code>pp_errors.h</code>.
  /// See also <code>pp::WebSocket_Dev::Close</code>.
  int32_t Close(uint16_t code, const Var& reason);

  /// Send() sends a message to the WebSocket server.
  ///
  /// @param[in] data A message to send. The message is copied to internal
  /// buffer. So caller can free <code>data</code> safely after returning
  /// from the function.
  ///
  /// @return An int32_t containing an error code from
  /// <code>pp_errors.h</code>.
  /// See also <code>pp::WebSocket_Dev::SendMessage</code>.
  int32_t Send(const Var& data);

  /// GetBufferedAmount() returns the number of bytes of text and binary
  /// messages that have been queued for the WebSocket connection to send but
  /// have not been transmitted to the network yet.
  ///
  /// @return Returns the number of bytes.
  uint64_t GetBufferedAmount();

  /// GetExtensions() returns the extensions selected by the server for the
  /// specified WebSocket connection.
  ///
  /// @return Returns a <code>Var</code> of string type. If called before the
  /// connection is established, its data is empty string.
  /// Currently its data is always an empty string.
  Var GetExtensions();

  /// GetProtocol() returns the sub-protocol chosen by the server for the
  /// specified WebSocket connection.
  ///
  /// @return Returns a <code>Var</code> of string type. If called before the
  /// connection is established, it contains the empty string.
  Var GetProtocol();

  /// GetReadyState() returns the ready state of the specified WebSocket
  /// connection.
  ///
  /// @return Returns <code>PP_WEBSOCKETREADYSTATE_INVALID_DEV</code> if called
  /// before connect() is called.
  PP_WebSocketReadyState_Dev GetReadyState();

  /// GetURL() returns the URL associated with specified WebSocket connection.
  ///
  /// @return Returns a <code>Var</code> of string type. If called before the
  /// connection is established, it contains the empty string.
  Var GetURL();

  /// OnOpen() is invoked when the connection is established by Connect().
  virtual void OnOpen() = 0;

  /// OnMessage() is invoked when a message is received.
  virtual void OnMessage(const Var& message) = 0;

  /// OnError() is invoked if the user agent was required to fail the WebSocket
  /// connection or the WebSocket connection is closed with prejudice.
  /// OnClose() always follows OnError().
  virtual void OnError() = 0;

  /// OnClose() is invoked when the connection is closed by errors or Close().
  virtual void OnClose(bool wasClean, uint16_t code, const Var& reason) = 0;

 private:
  class Implement;
  Implement* impl_;
};

}  // namespace helper

}  // namespace pp

#endif  // PPAPI_CPP_HELPER_DEV_WEBSOCKET_API_DEV_H_
