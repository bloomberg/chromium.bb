// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_WEBSOCKET_DEV_H_
#define PPAPI_CPP_DEV_WEBSOCKET_DEV_H_

#include "ppapi/c/dev/ppb_websocket_dev.h"

/// @file
/// This file defines the WebSocket_Dev interface.

namespace pp {

class Var;

/// The <code>WebSocket_Dev</code> class
/// A version that use virtual functions
class WebSocket_Dev : public Resource {
 public:
  /// Constructs a WebSocket_Dev object.
  WebSocket_Dev();

  /// Destructs a WebSocket_Dev object.
  virtual ~WebSocket_Dev();

  /// Connect() connects to the specified WebSocket server. Caller can call
  /// this method at most once.
  ///
  /// @param[in] url A <code>PP_Var</code> representing a WebSocket server URL.
  /// The <code>PP_VarType</code> must be <code>PP_VARTYPE_STRING</code>.
  /// @param[in] protocols A pointer to an array of <code>PP_Var</code>
  /// specifying sub-protocols. Each <code>PP_Var</code> represents one
  /// sub-protocol and its <code>PP_VarType</code> must be
  /// <code>PP_VARTYPE_STRING</code>. This argument can be null only if
  /// <code>protocol_count</code> is 0.
  /// @param[in] protocol_count The number of sub-protocols in
  /// <code>protocols</code>.
  ///
  /// @return In case of immediate failure, returns an error code as follows.
  /// Returns <code>PP_ERROR_BADARGUMENT</code> corresponding to JavaScript
  /// SyntaxError and <code>PP_ERROR_NOACCESS</code> corresponding to
  /// JavaScript SecurityError. Otherwise, returns
  /// <code>PP_OK_COMPLETIONPENDING</code> and later invokes
  /// <code>OnOpen()</code> on success or <code>OnClose()</code> on failure.
  int32_t Connect(const Var& url, const Var protocols[],
                  uint32_t protocol_count);

  /// Close() closes the specified WebSocket connection by specifying
  /// <code>code</code> and <code>reason</code>.
  ///
  /// @param[in] code The WebSocket close code. Ignored if it is 0.
  /// @param[in] reason A <code>PP_Var</code> which represents the WebSocket
  /// close reason. Ignored if it is <code>PP_VARTYPE_UNDEFINED</code>.
  /// Otherwise, its <code>PP_VarType</code> must be
  /// <code>PP_VARTYPE_STRING</code>.
  ///
  /// @return In case of immediate failure, returns an error code as follows.
  /// Returns <code>PP_ERROR_BADARGUMENT</code> corresponding to JavaScript
  /// SyntaxError and <code>PP_ERROR_NOACCESS</code> corresponding to
  /// JavaScript InvalidAccessError. Otherwise, returns
  /// <code>PP_OK_COMPLETIONPENDING</code> and invokes <code>OnClose</code>.
  int32_t Close(uint16_t code, const Var& reason);

  /// Send() sends a message to the WebSocket server.
  ///
  /// @param[in] data A message to send. The message is copied to internal
  /// buffer. So caller can free <code>data</code> safely after returning
  /// from the function.
  ///
  /// @return In case of immediate failure, returns an error code as follows.
  /// Returns <code>PP_ERROR_FAILED</code> corresponding to JavaScript
  /// InvalidStateError and <code>PP_ERROR_BADARGUMENT</code> corresponding to
  /// JavaScript SyntaxError. Otherwise, return <code>PP_OK</code>.
  /// <code>PP_OK</code> doesn't necessarily mean that the server received the
  /// message.
  int32_t Send(const Var& data);

  /// GetBufferedAmount() returns the number of bytes of text and binary
  /// messages that have been queued for the WebSocket connection to send but
  /// have not been transmitted to the network yet.
  ///
  /// Note: This interface might not be able to return exact bytes in the first
  /// release. Current WebSocket implementation can not estimate exact protocol
  /// frame overheads.
  ///
  /// @return Returns the number of bytes.
  uint64_t GetBufferedAmount();

  /// GetExtensions() returns the extensions selected by the server for the
  /// specified WebSocket connection.
  ///
  /// @return Returns a <code>PP_VARTYPE_STRING</code> var. If called before
  /// the connection is established, its data is empty string.
  /// Currently its data is always empty string.
  Var GetExtensions();

  /// GetProtocol() returns the sub-protocol chosen by the server for the
  /// specified WebSocket connection.
  ///
  /// @return Returns a <code>PP_VARTYPE_STRING</code> var. If called before
  /// the connection is established, its data is empty string.
  Var GetProtocol();

  /// GetReadyState() returns the ready state of the specified WebSocket
  /// connection.
  ///
  /// @return Returns <code>PP_WEBSOCKETREADYSTATE_CONNECTING</code> if called
  /// before the connection is established.
  PP_WebSocketReadyState_Dev GetReadyState();

  /// GetURL() returns the URL associated with specified WebSocket connection.
  ///
  /// @return Returns a <code>PP_VARTYPE_STRING</code> var. If called before
  /// the connection is established, its data is empty string.
  Var GetURL();

  /// OnOpen() is invoked when the connection is established by Connect().
  virtual void OnOpen() = 0;

  /// OnMessage() is invoked when a message is received.
  virtual void OnMessage(Var message) = 0;

  /// OnError() is invoked if the user agent was required to fail the WebSocket
  /// connection or the WebSocket connection is closed with prejudice.
  /// OnClose() always follows OnError().
  virtual void OnError() = 0;

  /// OnClose() is invoked when the connection is closed by errors or Close().
  virtual void OnClose(bool wasClean, uint16_t code, const Var& reason) = 0;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_WEBSOCKET_DEV_H_
