/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef NATIVE_CLIENT_SRC_SHARED_SRPC_NACL_SRPC_H_
#define NATIVE_CLIENT_SRC_SHARED_SRPC_NACL_SRPC_H_

/**
 * @file
 * Defines the API in the
 * <a href="group___s_r_p_c.html">Simple RPC (SRPC) library</a>
 *
 * @addtogroup SRPC
 * @{
 */

#include <stdarg.h>
#include <stdio.h>

/*
 * TODO(sehr) break this file into separate files for sdk and service_runtime
 * inclusion.
 */
#ifdef __native_client__
#  include <stdint.h>
#  include <sys/types.h>
#  include <sys/nacl_imc_api.h>

#ifndef DOXYGEN_SHOULD_SKIP_THIS
/*
 * Avoid emacs' penchant for auto-indenting extern "C" blocks.
 */
#  ifdef __cplusplus
#    define EXTERN_C_BEGIN extern "C" {
#    define EXTERN_C_END   }
#  else
#    define EXTERN_C_BEGIN
#    define EXTERN_C_END
#  endif  /* __cplusplus */
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

/**
 * Contains a file descriptor for use as an IMC channel.
 */
typedef int NaClSrpcImcDescType;
#else
#  include "native_client/src/include/portability.h"
#  include "native_client/src/include/nacl_base.h"

#  include "native_client/src/shared/imc/nacl_imc_c.h"
#  include "native_client/src/shared/platform/nacl_log.h"
#  include "native_client/src/trusted/desc/nrd_all_modules.h"
#  include "native_client/src/trusted/desc/nrd_xfer.h"
#  include "native_client/src/trusted/desc/nrd_xfer_effector.h"
/*
 * In trusted code we use a NaClDesc to describe the IMC channel.
 * It is this difference between trusted and untrusted code that motivated
 * creating a type name.
 */
typedef struct NaClDesc* NaClSrpcImcDescType;
#endif

EXTERN_C_BEGIN

/**
 * Result codes to be returned from Simple RPC services.
 */
typedef enum NaClSrpcResultCodes {
  /** Invocation was successful. */
  NACL_SRPC_RESULT_OK = 256,
  /** Invocation was successful, exit the server dispatch loop.
   *  When returned from an RPC method, this causes the server to return
   *  NACL_SRPC_RESULT_OK and then exit the receive-dispatch-reply loop.
   */
  NACL_SRPC_RESULT_BREAK,
  /** Some or all of a message was not received. */
  NACL_SRPC_RESULT_MESSAGE_TRUNCATED,
  /** The SRPC runtime system ran out of memory. */
  NACL_SRPC_RESULT_NO_MEMORY,
  /** The SRPC message layer detected a protocol version difference. */
  NACL_SRPC_RESULT_PROTOCOL_MISMATCH,
  /** The RPC number was not valid for any method. */
  NACL_SRPC_RESULT_BAD_RPC_NUMBER,
  /** An unknown type was passed to or returned from an RPC. */
  NACL_SRPC_RESULT_BAD_ARG_TYPE,
  /** Too few arguments were passed to or returned from an RPC. */
  NACL_SRPC_RESULT_TOO_FEW_ARGS,
  /** Too many arguments were passed to or returned from an RPC. */
  NACL_SRPC_RESULT_TOO_MANY_ARGS,
  /** One or more input argument types did not match the expected types. */
  NACL_SRPC_RESULT_IN_ARG_TYPE_MISMATCH,
  /** One or more output argument types did not match the expected types. */
  NACL_SRPC_RESULT_OUT_ARG_TYPE_MISMATCH,
  /** An unknown SRPC internal error occurred. */
  NACL_SRPC_RESULT_INTERNAL,
  /** The application signalled a generic error. */
  NACL_SRPC_RESULT_APP_ERROR
} NaClSrpcError;

/**
 *  Returns a descriptive string for the specified NaClSrpcError value.
 *  @param error_val A NaClSrpcError to be translated to a string.
 *  @return If error_val is valid, returns a string.  Otherwise returns the
 *  string "Unrecognized NaClSrpcError value"
 */
extern char* NaClSrpcErrorString(NaClSrpcError error_val);

/**
 * Type tag values for NaClSrpcArg unions.
 */
enum NaClSrpcArgType {
  NACL_SRPC_ARG_TYPE_INVALID = 'X',  /**< invalid type */
  NACL_SRPC_ARG_TYPE_BOOL = 'b',  /**< scalar bool */
  NACL_SRPC_ARG_TYPE_CHAR_ARRAY = 'C',  /**< array of char */
  NACL_SRPC_ARG_TYPE_DOUBLE = 'd',  /**< scalar double */
  NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY = 'D',  /**< array of double */
  NACL_SRPC_ARG_TYPE_HANDLE = 'h',  /**< a descriptor (handle) */
  NACL_SRPC_ARG_TYPE_INT = 'i',  /**< scalar int */
  NACL_SRPC_ARG_TYPE_INT_ARRAY = 'I',  /**< array of int */
  NACL_SRPC_ARG_TYPE_STRING = 's',  /**< NUL-terminated string */
  NACL_SRPC_ARG_TYPE_OBJECT = 'o',  /**< scriptable object */
  NACL_SRPC_ARG_TYPE_VARIANT_ARRAY = 'A'  /**< array of NaClSrpcArg structs */
};

/**
 * Argument data type for passing arrays of NaClSrpcArg.
 */
struct NaClSrpcVariantArray {
  /** The number of elements in the array */
  uint32_t            count;
  /** A chunk of memory containing <code>count</code> NaClSrpcArg structures */
  struct NaClSrpcArg  *varr;
};

/**
 * Argument data type for passing arrays of char.
 * @see NaClSrpcArg
 */
struct NaClSrpcCharArray {
  /** The number of characters in the array */
  uint32_t  count;
  /** A chunk of memory containing <code>count</code> characters */
  char      *carr;
};

/**
 * Argument data type for passing arrays of int.
 * @see NaClSrpcArg
 */
struct NaClSrpcIntArray {
  /** The number of integers in the array */
  uint32_t  count;
  /** A chunk of memory containing <code>count</code> integers */
  int       *iarr;
};

/**
 * Argument data type for passing arrays of double.
 * @see NaClSrpcArg
 */
struct NaClSrpcDoubleArray {
  /** The number of doubles in the array */
  uint32_t  count;
  /** A chunk of memory containing <code>count</code> doubles */
  double    *darr;
};

#ifdef __cplusplus
namespace nacl_srpc {
class ScriptableHandleBase;
}
#endif

/**
 *  Used to convey parameters to and from RPC invocations.  It is a variant
 *  type, with the <code>tag</code> telling whether to treat the parameter as
 *  a bool or an array of characters, etc.
 */
struct NaClSrpcArg {
  /**
   * Determines which type this argument contains.  Its value determines
   * which element of union <code>u</code> may be validly referred to.
   */
  enum NaClSrpcArgType         tag;
  /**
   * Padding (unused) to ensure 8-byte alignment of the union u.
   */
  uint32_t                     reserved_pad;
  /*
   * For efficiency, doubles should be 8-byte aligned so that
   * loading them would require a single bus cycle, and arrays of
   * NaClSrpcArgs will have to be 8-byte aligned as well, so it's
   * either a 4 byte padding between the tag and the union or
   * (implicitly) at the end.  gcc does not (by default) enforce
   * 8-byte alignment but visual studio does, so we explicitly pad
   * so that both compilers will use the same memory layout, even if
   * the gcc -malign-double flag were omitted.
   */
  /**
   * A union containing the value of the argument.  The element containing
   * the valid data is determined by <code>tag</code>.
   */
  union {
    /** A boolean value */
    int                         bval;
    /** An integer value */
    int                         ival;
    /** A double-precision floating point value */
    double                      dval;
    /** An array of character values */
    struct NaClSrpcCharArray    caval;
    /** An array of integer values */
    struct NaClSrpcIntArray     iaval;
    /** An array of double-precision floating point values */
    struct NaClSrpcDoubleArray  daval;
    /** A zero-terminated string value */
    char                        *sval;
    /** A handle used to pass descriptors */
    NaClSrpcImcDescType         hval;
    /** An object value that can be exported to the browser as is */
    /* NOTE(gregoryd) - this usually points to an object scriptable by
     * the browser, i.e. NPObject or IDispatch. Declaring it as void* to avoid
     * including browser specific stuff here.
     */
    void                        *oval;
    /** An array of variant type values. @see NaClSrpcArg */
    struct NaClSrpcVariantArray vaval;
  } u;
};
#ifndef __cplusplus
/**
 * A typedef for struct NaClSrpcArg for use in C.
 */
typedef struct NaClSrpcArg NaClSrpcArg;
#endif

/**
 * Remote procedure call state structure.
 */
struct NaClSrpcRpc {
  uint32_t         protocol_version;
  uint64_t         request_id;
  uint8_t          is_request;
  uint32_t         rpc_number;
  NaClSrpcError    app_error;
};
#ifndef __cplusplus
/**
 *  A typedef for struct NaClSrpcRpc for use in C.
 */
typedef struct NaClSrpcRpc  NaClSrpcRpc;
#endif  /* __cplusplus */

/* TODO(gregoryd) - duplicate string? */
/**
 * A utility macro to set a string-typed argument.
 */
#define STRINGZ_TO_SRPCARG(val, arg) do { \
    (arg).tag = NACL_SRPC_ARG_TYPE_STRING; \
    (arg).u.sval = (val); } while (0)

/**
 * The maximum number of arguments per SRPC routine.
 */
#define NACL_SRPC_MAX_ARGS                     128

/**
 * The maximum number of characters returnable from service discovery.
 */
#define NACL_SRPC_MAX_SERVICE_DISCOVERY_CHARS  4000

/** A pointer to an SRPC channel data structure. */
typedef struct NaClSrpcChannel *NaClSrpcChannelPtr;

/**
 * Methods used to implement SRPC services have this type signature.
 */
typedef NaClSrpcError (*NaClSrpcMethod)(NaClSrpcChannelPtr channel,
                                        NaClSrpcArg* args[],
                                        NaClSrpcArg* rets[]);

/**
 * Binaries that implement SRPC methods (servers) describe their services
 * using these structures.  These are converted into NaClSrpcDescs by the
 * server constructor.  They are also used when service discovery is done.
 */
struct NaClSrpcHandlerDesc {
  /**
   * a string containing "name:input_types:output_types"
   */
  char const *entry_fmt;
  /**
   * function pointer used to process calls to the named method
   */
  NaClSrpcMethod handler;
};
#ifndef __cplusplus
/**
 * A typedef for struct NaClSrpcHandlerDesc for use in C.
 */
typedef struct NaClSrpcHandlerDesc NaClSrpcHandlerDesc;
#endif

/**
 * Used by clients to retain method name and type information.
 */
struct NaClSrpcDesc {
  char const  *rpc_name;  /**< string containing the method name*/
  char const  *in_args;  /**< string containing the method input types */
  char const  *out_args;  /**< string containing the method output types */
  /**
   * function pointer used to process calls to the named method
   */
  NaClSrpcMethod handler;
};
#ifndef __cplusplus
/**
 * A typedef for struct NaClSrpcDesc for use in C.
 */
typedef struct NaClSrpcDesc NaClSrpcDesc;
#endif

/**
 * The structure used to provide a buffering layer over the IMC.
 */
struct NaClSrpcImcBuffer {
  struct NaClImcMsgIoVec    iovec[1];  /**< IMC message descriptor */
  /**
   * index into <code>bytes</code> of the next descriptor to be read or written
   */
  uint32_t                  next_desc;
  /**
   * index into <code>bytes</code> of the next data byte to be read or written
   */
  size_t                    next_byte;
  /**
   * index into <code>bytes</code> of the last data byte to be read or written
   */
  size_t                    last_byte;
#ifdef __native_client__
  struct NaClImcMsgHdr      header;  /**< IMC message header */
  /**
   * array of descriptors to be sent or received
   */
  NaClSrpcImcDescType       descs[IMC_USER_DESC_MAX];
  /**
   * character array containing the data to be sent or received
   */
  unsigned char             bytes[IMC_USER_BYTES_MAX];
#else
  struct NaClImcTypedMsgHdr header;
  NaClSrpcImcDescType       descs[NACL_ABI_IMC_USER_DESC_MAX];
  unsigned char             bytes[NACL_ABI_IMC_USER_BYTES_MAX];
#endif
};
#ifndef __cplusplus
/**
 *  A typedef for struct NaClSrpcImcBuffer for use in C.
 */
typedef struct NaClSrpcImcBuffer NaClSrpcImcBuffer;
#endif

/**
 * The encapsulation of all the data necessary for an RPC connection,
 * either client or server.
 */
struct NaClSrpcChannel {
  /** A handle to the IMC channel used to send and receive RPCs */
  NaClSrpcImcDescType         imc_handle;
#ifndef __native_client__
  /**
   * An interface class used to enable passing of descriptors in trusted
   * code, such as the browser plugin
   */
  struct NaClNrdXferEffector  eff;
#endif
  /** The id of the next rpc request message sent over this channel */
  uint64_t                    next_outgoing_request_id;
  /** A structure used to buffer data to be sent over this channel */
  NaClSrpcImcBuffer           send_buf;
  /** A structure used to buffer data received over this channel */
  NaClSrpcImcBuffer           receive_buf;
  /**
   * A pointer to an array of RPC service descriptors used to type check and
   * dispatch RPC requests.
   */
  NaClSrpcDesc*               rpc_descr;
  /** The number of elements in the <code>rpc_descr</code> array. */
  uint32_t                    rpc_count;
  /**
   * A pointer to channel-specific data.  This allows RPC method
   * implementations to be used across multiple services while still
   * maintaining reentrancy
   */
  void                        *server_instance_data;
  /**
   * A zero terminated string containing name:ins:outs triples used to respond
   * to service_discovery RPCs.
   */
  const char*                 service_string;
  /** The length of <code>service_string</code> in bytes */
  size_t                      service_string_length;
  /**
   * A boolean value indicating execution timing is enabled on this channel
   */
  int                         timing_enabled;
  /**
   * The number of microseconds spent sending on this channel while
   * timing_enabled is true.  Includes time reported in
   * <code>imc_write_usec</code>
   */
  double                      send_usec;
  /**
   * The number of microseconds spent receiving on this channel while
   * timing_enabled is true.  Includes time reported in
   * <code>imc_read_usec</code>
   */
  double                      receive_usec;
  /**
   * The number of microseconds spent in IMC receives on this channel while
   * timing_enabled is true
   */
  double                      imc_read_usec;
  /**
   * The number of microseconds spent in IMC sends on this channel while
   * timing_enabled is true
   */
  double                      imc_write_usec;
};
#ifndef __cplusplus
/**
 *  A typedef for struct NaClSrpcChannel for use in C.
 */
typedef struct NaClSrpcChannel NaClSrpcChannel;
#endif

/**
 *  Constructs an SRPC client object communicating over an IMC descriptor.
 *  Clients issue RPC requests and receive responses.
 *  @param channel The channel descriptor to be constructed.
 *  @param imc_desc The NaClSrpcImcDescType describing the IMC channel to
 *  communicate over.
 *  @return On success, 1; on failure, 0.
 */
int NaClSrpcClientCtor(NaClSrpcChannel *channel, NaClSrpcImcDescType imc_desc);
/**
 *  Constructs an SRPC server object communicating over an IMC descriptor.
 *  Servers wait for RPC requests, dispatch them to implementations, and return
 *  responses.
 *  @param channel The channel descriptor to be constructed.
 *  @param imc_desc The NaClSrpcImcDescType describing the IMC channel to
 *  communicate over.
 *  @param handlers An array of NaClSrpcHandlerDesc structures describing the
 *  set of services handled by this server.
 *  @param instance_data A value to be stored on the channel descriptor
 *  for conveying data specific to this particular server instance.
 *  @return On success, 1; on failure, 0.
 */
int NaClSrpcServerCtor(NaClSrpcChannel           *channel,
                       NaClSrpcImcDescType       imc_desc,
                       const NaClSrpcHandlerDesc *handlers,
                       void*                     instance_data);
/**
 *  A utility function that creates a NaClSrpcImcDescType from the
 *  platform-defined NaClHandle type.
 *  @param handle The handle to be converted.
 *  @return On success, a valid NaClSrpcImcDescType.  On failure, -1.
 */
NaClSrpcImcDescType NaClSrpcImcDescTypeFromHandle(NaClHandle handle);

/**
 *  Destroys the specified client or server channel.
 *  @param channel The channel to be destroyed.
 */
void NaClSrpcDtor(NaClSrpcChannel *channel);

/**
 *  Runs an SRPC receive-dispatch-respond loop on the specified socket
 *  descriptor.
 *  @param socket_desc A NaClHandle that RPCs will communicate over.
 *  @param methods An array of NaClSrpcHandlerDesc structures describing the
 *  set of services handled by this server.
 *  @param instance_data A value to be stored on the channel descriptor
 *  for conveying data specific to this particular server instance.
 *  @return On success, 1; on failure, 0.
 */
int NaClSrpcServerLoop(NaClHandle                       socket_desc,
                       const struct NaClSrpcHandlerDesc methods[],
                       void                             *instance_data);

/**
 *  Runs an SRPC receive-dispatch-respond loop on the specified
 *  NaClSrpcImcDescType object.
 *  @param imc_socket_desc A NaClSrpcImcDescType object that RPCs will
 *  communicate over.
 *  @param methods An array of NaClSrpcHandlerDesc structures
 *  describing the set of services handled by this server.
 *  @param instance_data A value to be stored on the channel
 *  descriptor for conveying data specific to this particular server
 *  instance.
 *  @return On success, 1; on failure, 0.
 */
int NaClSrpcServerLoopImcDesc(NaClSrpcImcDescType              imc_socket_desc,
                              const struct NaClSrpcHandlerDesc methods[],
                              void                             *instance_data);

/* NOTE: we may consider moving this and other untrusted only APIs
         into a separate header file */
#ifdef __native_client__
/**
 *  Runs an SRPC receive-dispatch-respond loop on the "default" descriptor.
 *  NaCl modules are started with a default descriptor (obtained by an accept
 *  call on the bound socket created by the service runtime).  The methods
 *  exported are those using the NACL_SRPC_METHOD macro in the source of the
 *  module, plus the implicitly defined methods.
 *  @param instance_data A value to be stored on the channel descriptor
 *  for conveying data specific to this particular server instance.
 *  @return On success, 1; on failure, 0.
 */
int NaClSrpcDefaultServerLoop(void *instance_data);
#endif  /* __native_client__ */

/**
 *  Prints the methods available from the specified channel to stdout.  For
 *  services, the method list is the table specified when
 *  NaClSrpcServerCtor() was invoked.  For clients, it is set by the initial
 *  call to the implicit service_discovery method.
 *  @param channel A channel descriptor.
 */
void NaClSrpcDumpInterfaceDesc(const NaClSrpcChannel *channel);
/**
 *  Obtains the index of the specified RPC name.
 *  @param channel The channel descriptor to be searched.
 *  @param name The exported name of the method.
 *  @return A non-negative index if the name was found in the channel's set of
 *  methods.  If the name was not found, it returns -1.
 */
int NaClSrpcGetRpcNum(const NaClSrpcChannel *channel, char const *name);

#ifdef __native_client__
#ifndef DOXYGEN_SHOULD_SKIP_THIS
/*
 * Not documented: in Native Client code the default RPC descriptors are
 * recorded in a special section, ".nacl_rpc_methods".  The use of a special
 * section allows declaring RPC handlers in multiple files by having the linker
 * concatenate the respective chunks of this section.  The default RPC
 * processing loop refers to this section to build its tables.
 * The handler array begins at __kNaclSrpcHandlers.
 */
extern const struct NaClSrpcHandlerDesc
  __attribute__((section(".nacl_rpc_methods")))
  __attribute__((aligned(8)))
  __kNaClSrpcHandlers[];
/*
 * Not documented: in NativeClient code the default RPC descriptors are
 * recorded in a special section, ".nacl_rpc_methods".
 * The handler array ends at __kNaclSrpcHandlerEnd.
 */
extern const struct NaClSrpcHandlerDesc
  __attribute__((section(".nacl_rpc_methods")))
  __attribute__((aligned(8)))
  __kNaClSrpcHandlerEnd;
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

/**
 * Exports a method to the default RPC descriptor array.
 * @param entry_format a string with the format "x:y:z", where x is the method
 * name, * y is the parameter type list, and z is the return value type list.
 * @param method_handler A function pointer for the implementation of the
 * method.
 */
#define NACL_SRPC_METHOD(entry_format, method_handler) \
struct NaClSrpcHandlerDesc \
  __attribute__((section(".nacl_rpc_methods"))) \
  __attribute__((aligned(8))) \
  NaClSrpcMethod##method_handler = \
  { entry_format, method_handler }

#endif /* defined(NACL_LINUX) ... */

/**
 *  @clientSrpc Invokes a specified RPC on the given channel.  Parameters
 *  are passed by stdarg conventions and determined from the types specified
 *  in the method tables.
 *  @param channel The channel descriptor to use to invoke the RPC.
 *  @param rpc_name The name of the RPC to be invoked.
 *  @return A NaClSrpcResultCodes indicating success (NACL_SRPC_RESULT_OK)
 *  or failure.
 *  @see NaClSrpcResultCodes
 */
extern NaClSrpcError NaClSrpcInvokeByName(NaClSrpcChannel  *channel,
                                          const char       *rpc_name,
                                          ...);
/**
 *  @clientSrpc  Invokes a specified RPC on the given channel.  Parameters
 *  are passed by stdarg conventions and determined from the types specified
 *  in the method tables stored in channel.
 *  @param channel The channel descriptor to use to invoke the RPC.
 *  @param rpc_num The index of the RPC to be invoked.
 *  @return A NaClSrpcResultCodes indicating success (NACL_SRPC_RESULT_OK)
 *  or failure.
 *  @see NaClSrpcResultCodes
 */
extern NaClSrpcError NaClSrpcInvoke(NaClSrpcChannel  *channel,
                                    uint32_t         rpc_num,
                                    ...);
/**
 *  @clientSrpc Invokes a specified RPC on the given channel.  Parameters are
 *  passed in arrays and are type-checked against the types specified in the
 *  method tables stored in channel.
 *  @param channel The channel descriptor to use to invoke the RPC.
 *  @param rpc_num The index of the RPC to be invoked.
 *  @param args The array of parameter pointers to arguments to be passed in.
 *  @param rets The array of parameter pointers to arguments to be returned.
 *  @return A NaClSrpcResultCodes indicating success (NACL_SRPC_RESULT_OK)
 *  or failure.
 *  @see NaClSrpcResultCodes
 */
extern NaClSrpcError NaClSrpcInvokeV(NaClSrpcChannel *channel,
                                     uint32_t        rpc_num,
                                     NaClSrpcArg     *args[],
                                     NaClSrpcArg     *rets[]);
/**
 *  @clientSrpc Invokes a specified RPC on the given channel.  Parameters are
 *  passed in and returned through pointers to stdargs.  They are type-checked
 *  against the types specified in the method tables stored in channel.
 *  @param channel The channel descriptor to use to invoke the RPC.
 *  @param rpc_num The index of the RPC to be invoked.
 *  @param in_va The pointer to stdargs list of arguments to be passed in.
 *  @param out_va The pointer to stdargs list of arguments to be returned.
 *  @return A NaClSrpcResultCodes indicating success (NACL_SRPC_RESULT_OK)
 *  or failure.
 *  @see NaClSrpcResultCodes
 */
extern NaClSrpcError NaClSrpcInvokeVaList(NaClSrpcChannel *channel,
                                        uint32_t          rpc_num,
                                        va_list           in_va,
                                        va_list           out_va);

/**
 *  @serverSrpc  Receives an RPC request, invokes the corresponding method
 *  from the table in the channel, and returns the result.
 *  @param channel The channel descriptor to use to invoke the RPC.
 *  @return A NaClSrpcResultCodes indicating success (NACL_SRPC_RESULT_OK)
 *  or failure.
 *  @see NaClSrpcResultCodes
 */
NaClSrpcError NaClSrpcReceiveAndDispatch(NaClSrpcChannel *channel);

/**
 *  @eitherSrpc  Enables or disables timing of the specified channel.
 *  @param channel The channel descriptor whose timing to consult.
 *  @param enable_timing If zero, disable timing.  Otherwise, enable timing.
 *  @see NaClSrpcGetTimes()
 */
void NaClSrpcToggleChannelTiming(NaClSrpcChannel* channel, int enable_timing);
/**
 *  @eitherSrpc Gathers the microsecond-granularity timing results retained
 *  for the specified channel.  If timing is not enabled, this function sets
 *  all _time parameters to 0.0.
 *  @param channel A channel descriptor.
 *  @param send_time The time spent in sending via this channel.
 *  @param receive_time The time spent in receiving via this channel.
 *  @param imc_read_time The time spent waiting for IMC layer reads via
 *  this channel.
 *  @param imc_write_time The time spent waiting for IMC layer writes via
 *  this channel.
 *  @see NaclSrpcToggleChannelTiming()
 */
void NaClSrpcGetTimes(NaClSrpcChannel *channel,
                      double *send_time,
                      double *receive_time,
                      double *imc_read_time,
                      double *imc_write_time);

/**
 *  @serverSrpc  Initializes the SRPC system, setting up the command channel
 *  and preparing the graphics subsystem for embedding in the browser.
 */
void srpc_init();

/**
 * The current protocol (version) number used to send and receive RPCs.
 */
static const uint32_t kNaClSrpcProtocolVersion = 0xc0da0002;

/**
 * RPC number for "implicit" timing method.
 */
#define NACL_SRPC_GET_TIMES_METHOD              0xfffffffe
/**
 * RPC number for "implicit" timing control method.
 */
#define NACL_SRPC_TOGGLE_CHANNEL_TIMING_METHOD  0xfffffffd


/**
 * Receive an RPC request from a channel.
 */
extern int NaClSrpcRequestRead(NaClSrpcChannel* channel,
                               NaClSrpcRpc* rpc,
                               NaClSrpcArg* args[],
                               NaClSrpcArg* rets[]);

/**
 * Send an RPC request on the given channel.
 */
extern NaClSrpcError NaClSrpcRequestWrite(NaClSrpcChannel* channel,
                                          NaClSrpcRpc* rpc,
                                          NaClSrpcArg* args[],
                                          NaClSrpcArg* rets[]);

/**
 * Receive an RPC response from a channel.
 */
extern int NaClSrpcResponseRead(NaClSrpcChannel* channel,
                                NaClSrpcRpc* rpc,
                                NaClSrpcArg* rets[]);

/**
 * Send an RPC response on the given channel.
 */
extern NaClSrpcError NaClSrpcResponseWrite(NaClSrpcChannel* channel,
                                           NaClSrpcRpc* rpc,
                                           NaClSrpcArg* rets[]);
EXTERN_C_END

/**
 * @}
 * End of System Calls group
 */

#endif  /* NATIVE_CLIENT_SRC_SHARED_SRPC_NACL_SRPC_H_ */
