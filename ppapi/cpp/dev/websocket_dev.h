// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_WEBSOCKET_DEV_H_
#define PPAPI_CPP_DEV_WEBSOCKET_DEV_H_

#include "ppapi/c/dev/ppb_websocket_dev.h"
#include "ppapi/cpp/resource.h"

/// @file
/// This file defines the WebSocket_Dev interface.

namespace pp {

class CompletionCallback;
class Instance;
class Var;

/// The <code>WebSocket_Dev</code> class
class WebSocket_Dev : public Resource {
 public:
  /// Constructs a WebSocket_Dev object.
  WebSocket_Dev(Instance* instance);

  /// Destructs a WebSocket_Dev object.
  virtual ~WebSocket_Dev();

  /// Connect() connects to the specified WebSocket server. Caller can call
  /// this method at most once.
  ///
  /// @param[in] url A <code>Var</code> of string type representing a WebSocket
  /// server URL.
  /// @param[in] protocols A pointer to an array of string type
  /// <code>Var</code> specifying sub-protocols. Each <code>Var</code>
  /// represents one sub-protocol. This argument can be null only if
  /// <code>protocol_count</code> is 0.
  /// @param[in] protocol_count The number of sub-protocols in
  /// <code>protocols</code>.
  /// @param[in] callback A <code>CompletionCallback</code> which is called
  /// when a connection is established or an error occurs in establishing
  /// connection.
  ///
  /// @return An int32_t containing an error code from
  /// <code>pp_errors.h</code>.
  /// Returns <code>PP_ERROR_BADARGUMENT</code> if specified <code>url</code>,
  /// or <code>protocols</code> contains invalid string as
  /// <code>The WebSocket API specification</code> defines. It corresponds to
  /// SyntaxError of the specification.
  /// Returns <code>PP_ERROR_NOACCESS</code> if the protocol specified in the
  /// <code>url</code> is not a secure protocol, but the origin of the caller
  /// has a secure scheme. Also returns it if the port specified in the
  /// <code>url</code> is a port to which the user agent is configured to block
  /// access because the port is a well-known port like SMTP. It corresponds to
  /// SecurityError of the specification.
  /// Returns <code>PP_ERROR_INPROGRESS</code> if the call is not the first
  /// time.
  int32_t Connect(const Var& url, const Var protocols[],
                  uint32_t protocol_count, const CompletionCallback& callback);

  /// Close() closes the specified WebSocket connection by specifying
  /// <code>code</code> and <code>reason</code>.
  ///
  /// @param[in] code The WebSocket close code. Ignored if it is 0.
  /// @param[in] reason A <code>Var</code> of string type which represents the
  /// WebSocket close reason. Ignored if it is undefined type.
  /// @param[in] callback A <code>CompletionCallback</code> which is called
  /// when the connection is closed or an error occurs in closing the
  /// connection.
  ///
  /// @return An int32_t containing an error code from
  /// <code>pp_errors.h</code>.
  /// Returns <code>PP_ERROR_BADARGUMENT</code> if <code>reason</code> contains
  /// an invalid character as a UTF-8 string, or longer than 123 bytes. It
  /// corresponds to JavaScript SyntaxError of the specification.
  /// Returns <code>PP_ERROR_NOACCESS</code> if the code is not an integer
  /// equal to 1000 or in the range 3000 to 4999. It corresponds to
  /// InvalidAccessError of the specification. Returns
  /// <code>PP_ERROR_INPROGRESS</code> if the call is not the first time.
  int32_t Close(uint16_t code, const Var& reason,
                const CompletionCallback& callback);

  /// ReceiveMessage() receives a message from the WebSocket server.
  /// This interface only returns a single message. That is, this interface
  /// must be called at least N times to receive N messages, no matter how
  /// small each message is.
  ///
  /// @param[out] message The received message is copied to provided
  /// <code>message</code>. The <code>message</code> must remain valid until
  /// the ReceiveMessage operation completes.
  /// @param[in] callback A <code>CompletionCallback</code> which is called
  /// when the receiving message is completed. It is ignored if ReceiveMessage
  /// completes synchronously and returns <code>PP_OK</code>.
  ///
  /// @return An int32_t containing an error code from
  /// <code>pp_errors.h</code>.
  /// If an error is detected or connection is closed, returns
  /// <code>PP_ERROR_FAILED</code> after all buffered messages are received.
  /// Until buffered message become empty, continues to returns
  /// <code>PP_OK</code> as if connection is still established without errors.
  int32_t ReceiveMessage(Var* message,
                         const CompletionCallback& callback);

  /// Send() sends a message to the WebSocket server.
  ///
  /// @param[in] data A message to send. The message is copied to internal
  /// buffer. So caller can free <code>data</code> safely after returning
  /// from the function.
  ///
  /// @return An int32_t containing an error code from
  /// <code>pp_errors.h</code>.
  /// Returns <code>PP_ERROR_FAILED</code> if the ReadyState is
  /// <code>PP_WEBSOCKETREADYSTATE_CONNECTING_DEV</code>. It corresponds
  /// JavaScript InvalidStateError of the specification.
  /// Returns <code>PP_ERROR_BADARGUMENT</code> if provided
  /// <code>message</code> of string type contains an invalid character as a
  /// UTF-8 string. It corresponds to JavaScript SyntaxError of the
  /// specification.
  /// Otherwise, returns <code>PP_OK</code>, but it doesn't necessarily mean
  /// that the server received the message.
  int32_t SendMessage(const Var& message);

  /// GetBufferedAmount() returns the number of bytes of text and binary
  /// messages that have been queued for the WebSocket connection to send but
  /// have not been transmitted to the network yet.
  ///
  /// @return Returns the number of bytes.
  uint64_t GetBufferedAmount();

  /// GetCloseCode() returns the connection close code for the WebSocket
  /// connection.
  ///
  /// @return Returns 0 if called before the close code is set.
  uint16_t GetCloseCode();

  /// GetCloseReason() returns the connection close reason for the WebSocket
  /// connection.
  ///
  /// @return Returns a <code>Var</code> of string type. If called before the
  /// close reason is set, it contains an empty string.
  Var GetCloseReason();

  /// GetCloseWasClean() returns if the connection was closed cleanly for the
  /// specified WebSocket connection.
  ///
  /// @return Returns <code>false</code> if called before the connection is
  /// closed, or called on an invalid resource. Otherwise, returns
  /// <code>true</code> if the connection was closed cleanly, or returns
  /// <code>false</code> if the connection was closed for abnormal reasons.
  bool GetCloseWasClean();

  /// GetExtensions() returns the extensions selected by the server for the
  /// specified WebSocket connection.
  ///
  /// @return Returns a <code>Var</code> of string type. If called before the
  /// connection is established, its data is an empty string.
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

  /// SetBinaryType() specifies the binary object type for receiving binary
  /// frames representation. Receiving text frames are always mapped to
  /// <PP_VARTYPE_STRING</code> var regardless of this attribute.
  /// This function should be called before Connect() to ensure receiving all
  /// incoming binary frames as the specified binary object type.
  /// Default type is <code>PP_WEBSOCKETBINARYTYPE_BLOB_DEV</code>.
  ///
  /// Currently, Blob bindings is not supported in Pepper, so receiving binary
  /// type is always ArrayBuffer. To ensure backward compatibility, you must
  /// call this function with
  /// <code>PP_WEBSOCKETBINARYTYPE_ARRAYBUFFER_DEV</code> to use binary frames.
  ///
  /// @param[in] binary_type Binary object type for receiving binary frames
  /// representation.
  ///
  /// @return Returns <code>false</code> if the specified type is not
  /// supported. Otherwise, returns <code>true</code>.
  ///
  bool SetBinaryType(PP_WebSocketBinaryType_Dev binary_type);

  /// GetBinaryType() returns the currently specified binary object type for
  /// receiving binary frames.
  ///
  /// @return Returns <code>PP_WebSocketBinaryType_Dev</code> represents the
  /// current binary object type.
  PP_WebSocketBinaryType_Dev GetBinaryType();
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_WEBSOCKET_DEV_H_
