/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Native WebWorker support.
 * Please note that this header file is used to interface between the
 * native_client build environment and other clients that may not have
 * access to native client header files.  Hence there should not be any
 * includes into this header file from the native client tree.
 */

#ifndef NATIVE_CLIENT_SERVICE_RUNTIME_WEB_WORKER_STUB_H__
#define NATIVE_CLIENT_SERVICE_RUNTIME_WEB_WORKER_STUB_H__ 1
/*
 * As we are not including the native client header files, several types
 * from them need to be named outside of structure/method declarations to
 * avoid compiler warnings/errors.
 */
struct NaClApp;
struct NaClDesc;
struct NaClSrpcChannel;

#ifdef __cplusplus
class WebKit::WebWorkerClient;
typedef WebKit::WebWorkerClient *WWClientPointer;
#else
typedef void *WWClientPointer;
#endif  /* __cplusplus */

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/*
 * The thunk type used by upcall (postMessage from worker) handling.
 */
typedef void (*NativeWorkerPostMessageFunc)(const char *str,
                                            WWClientPointer client);

/*
 * Start a native webworker from the module in buffer.
 */
int NaClStartNativeWebWorker(char *buffer,
                             size_t buf_len,
                             struct NaClApp **nap,
                             struct NaClSrpcChannel **untrusted_channel);

/*
 * Performs a postMessage to the worker.
 */
int NaClPostMessageToNativeWebWorker(char *buffer,
                                     size_t buf_len,
                                     struct NaClApp **nap,
                                     struct NaClSrpcChannel **untrusted_ch);

/*
 * Causes a native web worker to be shut down.
 */
int NaClTerminateNativeWebWorker(struct NaClApp **nap,
                                 struct NaClSrpcChannel **untrusted_channel);

/*
 * Creates a channel that can be used to receive upcalls (postMessage from
 * a worker).
 */
int NaClCreateUpcallChannel(struct NaClDesc* desc[2]);

/*
 * Part of the initialization of a worker.  Sends the descriptor to the
 * worker library to indicate where to send postMessage RPCs.
 */
int NaClSrpcSendUpcallDesc(struct NaClSrpcChannel *channel,
                           struct NaClDesc *nacl_desc);

/*
 * Runs an SRPC server loop on the specified channel.  The post_message_func
 * is invoked whenever the "postMessage" RPC is received.
 */
int NaClSrpcListenerLoop(struct NaClDesc *chrome_desc,
                         NativeWorkerPostMessageFunc func,
                         WWClientPointer client);

/*
 * Destroys the upcall channel.
 */
void NaClDestroyUpcallChannel(struct NaClDesc* desc[2]);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif  /* NATIVE_CLIENT_SERVICE_RUNTIME_WEB_WORKER_STUB_H__ */
