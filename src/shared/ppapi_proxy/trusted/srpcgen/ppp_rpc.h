// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
//
// Automatically generated code.  See srpcgen.py
//
// NaCl Simple Remote Procedure Call interface abstractions.

#ifndef GEN_PPAPI_PROXY_PPP_RPC_H_
#define GEN_PPAPI_PROXY_PPP_RPC_H_
#ifdef __native_client__
#include "native_client/src/shared/srpc/nacl_srpc.h"
#else
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#endif  // __native_client__

class ObjectStubRpcClient {
 public:
  static NaClSrpcError HasProperty(
      NaClSrpcChannel* channel,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      int32_t* success,
      nacl_abi_size_t* exception_bytes, char* exception
  );
  static NaClSrpcError HasMethod(
      NaClSrpcChannel* channel,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      int32_t* success,
      nacl_abi_size_t* exception_bytes, char* exception
  );
  static NaClSrpcError GetProperty(
      NaClSrpcChannel* channel,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      nacl_abi_size_t* value_bytes, char* value,
      nacl_abi_size_t* exception_bytes, char* exception
  );
  static NaClSrpcError GetAllPropertyNames(
      NaClSrpcChannel* channel,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      int32_t* property_count,
      nacl_abi_size_t* properties_bytes, char* properties,
      nacl_abi_size_t* exception_bytes, char* exception
  );
  static NaClSrpcError SetProperty(
      NaClSrpcChannel* channel,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      nacl_abi_size_t value_bytes, char* value,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      nacl_abi_size_t* exception_bytes, char* exception
  );
  static NaClSrpcError RemoveProperty(
      NaClSrpcChannel* channel,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      nacl_abi_size_t* exception_bytes, char* exception
  );
  static NaClSrpcError Call(
      NaClSrpcChannel* channel,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      int32_t argc,
      nacl_abi_size_t argv_bytes, char* argv,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      nacl_abi_size_t* ret_bytes, char* ret,
      nacl_abi_size_t* exception_bytes, char* exception
  );
  static NaClSrpcError Construct(
      NaClSrpcChannel* channel,
      nacl_abi_size_t capability_bytes, char* capability,
      int32_t argc,
      nacl_abi_size_t argv_bytes, char* argv,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      nacl_abi_size_t* ret_bytes, char* ret,
      nacl_abi_size_t* exception_bytes, char* exception
  );
  static NaClSrpcError Deallocate(
      NaClSrpcChannel* channel,
      nacl_abi_size_t capability_bytes, char* capability
  );

 private:
  ObjectStubRpcClient();
  ObjectStubRpcClient(const ObjectStubRpcClient&);
  void operator=(const ObjectStubRpcClient);

};  // class ObjectStubRpcClient

class CompletionCallbackRpcClient {
 public:
  static NaClSrpcError InvokeCompletionCallback(
      NaClSrpcChannel* channel,
      int32_t callback_id,
      int32_t result
  );

 private:
  CompletionCallbackRpcClient();
  CompletionCallbackRpcClient(const CompletionCallbackRpcClient&);
  void operator=(const CompletionCallbackRpcClient);

};  // class CompletionCallbackRpcClient

class PppRpcClient {
 public:
  static NaClSrpcError PPP_InitializeModule(
      NaClSrpcChannel* channel,
      int32_t pid,
      int64_t module,
      NaClSrpcImcDescType upcall_channel_desc,
      char* service_description,
      int32_t* nacl_pid,
      int32_t* success
  );
  static NaClSrpcError PPP_ShutdownModule(
      NaClSrpcChannel* channel
  );
  static NaClSrpcError PPP_GetInterface(
      NaClSrpcChannel* channel,
      char* interface_name,
      int32_t* exports_interface_name
  );

 private:
  PppRpcClient();
  PppRpcClient(const PppRpcClient&);
  void operator=(const PppRpcClient);

};  // class PppRpcClient

class PppInstanceRpcClient {
 public:
  static NaClSrpcError PPP_Instance_DidCreate(
      NaClSrpcChannel* channel,
      int64_t instance,
      int32_t argc,
      nacl_abi_size_t argn_bytes, char* argn,
      nacl_abi_size_t argv_bytes, char* argv,
      int32_t* success
  );
  static NaClSrpcError PPP_Instance_DidDestroy(
      NaClSrpcChannel* channel,
      int64_t instance
  );
  static NaClSrpcError PPP_Instance_DidChangeView(
      NaClSrpcChannel* channel,
      int64_t instance,
      nacl_abi_size_t position_bytes, int32_t* position,
      nacl_abi_size_t clip_bytes, int32_t* clip
  );
  static NaClSrpcError PPP_Instance_DidChangeFocus(
      NaClSrpcChannel* channel,
      int64_t instance,
      bool has_focus
  );
  static NaClSrpcError PPP_Instance_HandleInputEvent(
      NaClSrpcChannel* channel,
      int64_t instance,
      nacl_abi_size_t event_data_bytes, char* event_data,
      int32_t* success
  );
  static NaClSrpcError PPP_Instance_HandleDocumentLoad(
      NaClSrpcChannel* channel,
      int64_t instance,
      int64_t url_loader,
      int32_t* success
  );
  static NaClSrpcError PPP_Instance_GetInstanceObject(
      NaClSrpcChannel* channel,
      int64_t instance,
      nacl_abi_size_t* capability_bytes, char* capability
  );

 private:
  PppInstanceRpcClient();
  PppInstanceRpcClient(const PppInstanceRpcClient&);
  void operator=(const PppInstanceRpcClient);

};  // class PppInstanceRpcClient



#endif  // GEN_PPAPI_PROXY_PPP_RPC_H_

