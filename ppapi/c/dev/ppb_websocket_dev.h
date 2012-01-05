/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_websocket_dev.idl modified Mon Dec 19 19:44:12 2011. */

#ifndef PPAPI_C_DEV_PPB_WEBSOCKET_DEV_H_
#define PPAPI_C_DEV_PPB_WEBSOCKET_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_WEBSOCKET_DEV_INTERFACE_0_1 "PPB_WebSocket(Dev);0.1"
#define PPB_WEBSOCKET_DEV_INTERFACE PPB_WEBSOCKET_DEV_INTERFACE_0_1

/**
 * @file
 * This file defines the <code>PPB_WebSocket_Dev</code> interface.
 */


/**
 * @addtogroup Enums
 * @{
 */
/**
 * This enumeration contains the types representing the WebSocket ready state
 * and these states are based on the JavaScript WebSocket API specification.
 * GetReadyState() returns one of these states.
 */
typedef enum {
  /**
   * Ready state is queried on an invalid resource.
   */
  PP_WEBSOCKETREADYSTATE_INVALID_DEV = -1,
  /**
   * Ready state that the connection has not yet been established.
   */
  PP_WEBSOCKETREADYSTATE_CONNECTING_DEV = 0,
  /**
   * Ready state that the WebSocket connection is established and communication
   * is possible.
   */
  PP_WEBSOCKETREADYSTATE_OPEN_DEV = 1,
  /**
   * Ready state that the connection is going through the closing handshake.
   */
  PP_WEBSOCKETREADYSTATE_CLOSING_DEV = 2,
  /**
   * Ready state that the connection has been closed or could not be opened.
   */
  PP_WEBSOCKETREADYSTATE_CLOSED_DEV = 3
} PP_WebSocketReadyState_Dev;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_WebSocketReadyState_Dev, 4);

/**
 * This enumeration contains the types representing the WebSocket message type
 * and these types are based on the JavaScript WebSocket API specification.
 * ReceiveMessage() and SendMessage() use them as a parameter to represent
 * handling message types.
 */
typedef enum {
  /**
   * Message type that represents a text message type.
   */
  PP_WEBSOCKET_MESSAGE_TYPE_TEXT_DEV = 0,
  /**
   * Message type that represents a binary message type.
   */
  PP_WEBSOCKET_MESSAGE_TYPE_BINARY_DEV = 1
} PP_WebSocketMessageType_Dev;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_WebSocketMessageType_Dev, 4);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_WebSocket_Dev_0_1 {
  /**
   * Create() creates a WebSocket instance.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying the instance
   * with the WebSocket.
   *
   * @return A <code>PP_Resource</code> corresponding to a WebSocket if
   * successful.
   */
  PP_Resource (*Create)(PP_Instance instance);
  /**
   * IsWebSocket() determines if the provided <code>resource</code> is a
   * WebSocket instance.
   *
   * @param[in] resource A <code>PP_Resource</code> corresponding to a
   * WebSocket.
   *
   * @return Returns <code>PP_TRUE</code> if <code>resource</code> is a
   * <code>PPB_WebSocket_Dev</code>, <code>PP_FALSE</code> if the
   * <code>resource</code> is invalid or some type other than
   * <code>PPB_WebSocket_Dev</code>.
   */
  PP_Bool (*IsWebSocket)(PP_Resource resource);
  /**
   * Connect() connects to the specified WebSocket server. Caller can call this
   * method at most once for a <code>web_socket</code>.
   *
   * @param[in] web_socket A <code>PP_Resource</code> corresponding to a
   * WebSocket.
   *
   * @param[in] url A <code>PP_Var</code> representing a WebSocket server URL.
   * The <code>PP_VarType</code> must be <code>PP_VARTYPE_STRING</code>.
   *
   * @param[in] protocols A pointer to an array of <code>PP_Var</code>
   * specifying sub-protocols. Each <code>PP_Var</code> represents one
   * sub-protocol and its <code>PP_VarType</code> must be
   * <code>PP_VARTYPE_STRING</code>. This argument can be null only if
   * <code>protocol_count</code> is 0.
   *
   * @param[in] protocol_count The number of sub-protocols in
   * <code>protocols</code>.
   *
   * @param[in] callback A <code>PP_CompletionCallback</code> which is called
   * when a connection is established or an error occurs in establishing
   * connection.
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   * Returns <code>PP_ERROR_BADARGUMENT</code> if specified <code>url</code>,
   * or <code>protocols</code> contains invalid string as
   * <code>The WebSocket API specification</code> defines. It corresponds to
   * SyntaxError of the specification.
   * Returns <code>PP_ERROR_NOACCESS</code> if the protocol specified in the
   * <code>url</code> is not a secure protocol, but the origin of the caller
   * has a secure scheme. Also returns it if the port specified in the
   * <code>url</code> is a port to which the user agent is configured to block
   * access because the port is a well-known port like SMTP. It corresponds to
   * SecurityError of the specification.
   * Returns <code>PP_ERROR_INPROGRESS</code> if the call is not the first
   * time.
   */
  int32_t (*Connect)(PP_Resource web_socket,
                     struct PP_Var url,
                     const struct PP_Var protocols[],
                     uint32_t protocol_count,
                     struct PP_CompletionCallback callback);
  /**
   * Close() closes the specified WebSocket connection by specifying
   * <code>code</code> and <code>reason</code>.
   *
   * @param[in] web_socket A <code>PP_Resource</code> corresponding to a
   * WebSocket.
   *
   * @param[in] code The WebSocket close code. Ignored if it is 0.
   *
   * @param[in] reason A <code>PP_Var</code> which represents the WebSocket
   * close reason. Ignored if it is <code>PP_VARTYPE_UNDEFINED</code>.
   * Otherwise, its <code>PP_VarType</code> must be
   * <code>PP_VARTYPE_STRING</code>.
   *
   * @param[in] callback A <code>PP_CompletionCallback</code> which is called
   * when the connection is closed or an error occurs in closing the
   * connection.
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   * Returns <code>PP_ERROR_BADARGUMENT</code> if <code>reason</code> contains
   * an invalid character as a UTF-8 string, or longer than 123 bytes. It
   * corresponds to JavaScript SyntaxError of the specification.
   * Returns <code>PP_ERROR_NOACCESS</code> if the code is not an integer
   * equal to 1000 or in the range 3000 to 4999. It corresponds to
   * InvalidAccessError of the specification. Returns
   * <code>PP_ERROR_INPROGRESS</code> if the call is not the first time.
   */
  int32_t (*Close)(PP_Resource web_socket,
                   uint16_t code,
                   struct PP_Var reason,
                   struct PP_CompletionCallback callback);
  /**
   * ReceiveMessage() receives a message from the WebSocket server.
   * This interface only returns a single message. That is, this interface must
   * be called at least N times to receive N messages, no matter how small each
   * message is.
   *
   * @param[in] web_socket A <code>PP_Resource</code> corresponding to a
   * WebSocket.
   *
   * @param[out] message The received message is copied to provided
   * <code>message</code>. The <code>message</code> must remain valid until
   * the ReceiveMessage operation completes.
   *
   * @param[in] callback A <code>PP_CompletionCallback</code> which is called
   * when the receiving message is completed. It is ignored if ReceiveMessage
   * completes synchronously and returns <code>PP_OK</code>.
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   * If an error is detected or connection is closed, returns
   * <code>PP_ERROR_FAILED</code> after all buffered messages are received.
   * Until buffered message become empty, continues to returns
   * <code>PP_OK</code> as if connection is still established without errors.
   */
  int32_t (*ReceiveMessage)(PP_Resource web_socket,
                            struct PP_Var* message,
                            struct PP_CompletionCallback callback);
  /**
   * SendMessage() sends a message to the WebSocket server.
   *
   * @param[in] web_socket A <code>PP_Resource</code> corresponding to a
   * WebSocket.
   *
   * @param[in] message A message to send. The message is copied to internal
   * buffer. So caller can free <code>message</code> safely after returning
   * from the function.
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   * Returns <code>PP_ERROR_FAILED</code> if the ReadyState is
   * <code>PP_WEBSOCKETREADYSTATE_CONNECTING_DEV</code>. It corresponds
   * JavaScript InvalidStateError of the specification.
   * Returns <code>PP_ERROR_BADARGUMENT</code> if provided <code>message</code>
   * of string type contains an invalid character as a UTF-8 string. It
   * corresponds to JavaScript SyntaxError of the specification.
   * Otherwise, returns <code>PP_OK</code>, but it doesn't necessarily mean
   * that the server received the message.
   */
  int32_t (*SendMessage)(PP_Resource web_socket, struct PP_Var message);
  /**
   * GetBufferedAmount() returns the number of bytes of text and binary
   * messages that have been queued for the WebSocket connection to send but
   * have not been transmitted to the network yet.
   *
   * @param[in] web_socket A <code>PP_Resource</code> corresponding to a
   * WebSocket.
   *
   * @return Returns the number of bytes.
   */
  uint64_t (*GetBufferedAmount)(PP_Resource web_socket);
  /**
   * GetCloseCode() returns the connection close code for the WebSocket
   * connection.
   *
   * @param[in] web_socket A <code>PP_Resource</code> corresponding to a
   * WebSocket.
   *
   * @return Returns 0 if called before the close code is set.
   */
  uint16_t (*GetCloseCode)(PP_Resource web_socket);
  /**
   * GetCloseReason() returns the connection close reason for the WebSocket
   * connection.
   *
   * @param[in] web_socket A <code>PP_Resource</code> corresponding to a
   * WebSocket.
   *
   * @return Returns a <code>PP_VARTYPE_STRING</code> var. If called before the
   * close reason is set, it contains an empty string. Returns a
   * <code>PP_VARTYPE_UNDEFINED</code> if called on an invalid resource.
   */
  struct PP_Var (*GetCloseReason)(PP_Resource web_socket);
  /**
   * GetCloseWasClean() returns if the connection was closed cleanly for the
   * specified WebSocket connection.
   *
   * @param[in] web_socket A <code>PP_Resource</code> corresponding to a
   * WebSocket.
   *
   * @return Returns <code>PP_FALSE</code> if called before the connection is
   * closed, or called on an invalid resource. Otherwise, returns
   * <code>PP_TRUE</code> if the connection was closed cleanly, or returns
   * <code>PP_FALSE</code> if the connection was closed for abnormal reasons.
   */
  PP_Bool (*GetCloseWasClean)(PP_Resource web_socket);
  /**
   * GetExtensions() returns the extensions selected by the server for the
   * specified WebSocket connection.
   *
   * @param[in] web_socket A <code>PP_Resource</code> corresponding to a
   * WebSocket.
   *
   * @return Returns a <code>PP_VARTYPE_STRING</code> var. If called before the
   * connection is established, its data is an empty string. Returns a
   * <code>PP_VARTYPE_UNDEFINED</code> if called on an invalid resource.
   * Currently its data for valid resources are always an empty string.
   */
  struct PP_Var (*GetExtensions)(PP_Resource web_socket);
  /**
   * GetProtocol() returns the sub-protocol chosen by the server for the
   * specified WebSocket connection.
   *
   * @param[in] web_socket A <code>PP_Resource</code> corresponding to a
   * WebSocket.
   *
   * @return Returns a <code>PP_VARTYPE_STRING</code> var. If called before the
   * connection is established, it contains the empty string. Returns a
   * <code>PP_VARTYPE_UNDEFINED</code> if called on an invalid resource.
   */
  struct PP_Var (*GetProtocol)(PP_Resource web_socket);
  /**
   * GetReadyState() returns the ready state of the specified WebSocket
   * connection.
   *
   * @param[in] web_socket A <code>PP_Resource</code> corresponding to a
   * WebSocket.
   *
   * @return Returns <code>PP_WEBSOCKETREADYSTATE_INVALID_DEV</code> if called
   * before connect() is called, or called on an invalid resource.
   */
  PP_WebSocketReadyState_Dev (*GetReadyState)(PP_Resource web_socket);
  /**
   * GetURL() returns the URL associated with specified WebSocket connection.
   *
   * @param[in] web_socket A <code>PP_Resource</code> corresponding to a
   * WebSocket.
   *
   * @return Returns a <code>PP_VARTYPE_STRING</code> var. If called before the
   * connection is established, it contains the empty string. Return a
   * <code>PP_VARTYPE_UNDEFINED</code> if called on an invalid resource.
   */
  struct PP_Var (*GetURL)(PP_Resource web_socket);
};

typedef struct PPB_WebSocket_Dev_0_1 PPB_WebSocket_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_WEBSOCKET_DEV_H_ */

