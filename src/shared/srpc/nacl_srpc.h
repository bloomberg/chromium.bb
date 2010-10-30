/*
 * Copyright (c) 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
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
#define kNaClSrpcInvalidImcDesc -1
#else
#  include "native_client/src/include/portability.h"
#  include "native_client/src/include/nacl_base.h"
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
#define kNaClSrpcInvalidImcDesc NULL
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
extern const char* NaClSrpcErrorString(NaClSrpcError error_val);

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
  NACL_SRPC_ARG_TYPE_INT = 'i',  /**< scalar int32_t */
  NACL_SRPC_ARG_TYPE_INT_ARRAY = 'I',  /**< array of int32_t */
  NACL_SRPC_ARG_TYPE_LONG = 'l',  /**< scalar int64_t */
  NACL_SRPC_ARG_TYPE_LONG_ARRAY = 'L',  /**< array of int64_t */
  NACL_SRPC_ARG_TYPE_STRING = 's',  /**< NUL-terminated string */
  NACL_SRPC_ARG_TYPE_OBJECT = 'o',  /**< scriptable object */
  NACL_SRPC_ARG_TYPE_VARIANT_ARRAY = 'A'  /**< array of NaClSrpcArg structs */
};

/**
 * Argument data type for passing arrays of NaClSrpcArg.
 */
struct NaClSrpcVariantArray {
  /** The number of elements in the array */
  nacl_abi_size_t     count;
  /** A chunk of memory containing <code>count</code> NaClSrpcArg structures */
  struct NaClSrpcArg  *varr;
};

/**
 * Argument data type for passing arrays of char.
 * @see NaClSrpcArg
 */
struct NaClSrpcCharArray {
  /** The number of characters in the array */
  nacl_abi_size_t  count;
  /** A chunk of memory containing <code>count</code> characters */
  char             *carr;
};

/**
 * Argument data type for passing arrays of double.
 * @see NaClSrpcArg
 */
struct NaClSrpcDoubleArray {
  /** The number of doubles in the array */
  nacl_abi_size_t   count;
  /** A chunk of memory containing <code>count</code> doubles */
  double            *darr;
};

/**
 * Argument data type for passing arrays of int32_t.
 * @see NaClSrpcArg
 */
struct NaClSrpcIntArray {
  /** The number of integers in the array */
  nacl_abi_size_t  count;
  /** A chunk of memory containing <code>count</code> int32_t */
  int32_t          *iarr;
};

/**
 * Argument data type for passing arrays of int64_t.
 * @see NaClSrpcArg
 */
struct NaClSrpcLongArray {
  /** The number of doubles in the array */
  nacl_abi_size_t   count;
  /** A chunk of memory containing <code>count</code> int64_t */
  int64_t           *larr;
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
    int32_t                     ival;
    /** A int64_t value */
    int64_t                     lval;
    /** A double-precision floating point value */
    double                      dval;
    /** An array of character values */
    struct NaClSrpcCharArray    caval;
    /** An array of double-precision floating point values */
    struct NaClSrpcDoubleArray  daval;
    /** An array of int32_t values */
    struct NaClSrpcIntArray     iaval;
    /** An array of int64_t values */
    struct NaClSrpcLongArray    laval;
    /** A zero-terminated string value */
    char                        *sval;
    /** A handle used to pass descriptors */
    NaClSrpcImcDescType         hval;
    /** An object value that can be exported to the browser as is */
    /** This field is only used with predeclared methods, never for
     *  communicating with nexes. TODO(polina): where can one assert for this?
     *  This usually points to an object scriptable by
     *  the browser, i.e. NPAPI's NPObject or PPAPI's ScriptableObject.
     *  Declaring it as void* to avoid including browser specific stuff here.
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
  uint32_t                  protocol_version;
  uint64_t                  request_id;
  uint8_t                   is_request;
  uint32_t                  rpc_number;
  NaClSrpcError             app_error;
  /* State maintained for transmission/reception, but not serialized */
  const char*               ret_types;
  NaClSrpcArg**             rets;
  struct NaClSrpcImcBuffer* buffer;
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
 * using these structures.
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
  nacl_abi_size_t           next_byte;
  /**
   * index into <code>bytes</code> of the last data byte to be read or written
   */
  nacl_abi_size_t           last_byte;
#ifdef __native_client__
  struct NaClImcMsgHdr      header;  /**< IMC message header */
  /**
   * array of descriptors to be sent or received
   */
  NaClSrpcImcDescType       descs[IMC_USER_DESC_MAX];
  /**
   * character array containing the data to be sent or received
   * This element must be an array to ensure that proper range checking
   * on reads and writes can be done using sizeof(bytes).  See the note
   * in imc_buffer.c.
   * TODO(sehr,bsy): use a preprocessor macro to assert that this element
   * is an array rather than a pointer.
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
 * A private structure type used to describe methods within NaClSrpcService.
 */
struct NaClSrpcMethodDesc;

/**
 * A description of the services available on a channel.
 */
struct NaClSrpcService {
  /**
   * A pointer to an array of RPC service descriptors used to type check and
   * dispatch RPC requests.
   */
  struct NaClSrpcMethodDesc*  rpc_descr;
  /** The number of elements in the <code>rpc_descr</code> array. */
  uint32_t                    rpc_count;
  /**
   * A zero terminated string containing name:ins:outs triples used to respond
   * to service_discovery RPCs.
   */
  const char*                 service_string;
  /** The length of <code>service_string</code> in bytes */
  nacl_abi_size_t             service_string_length;
};
#ifndef __cplusplus
/**
 *  A typedef for struct NaClSrpcService for use in C.
 */
typedef struct NaClSrpcService NaClSrpcService;
#endif

/**
 *  Constructs an SRPC service object from an array of handlers.
 *  @param service The service to be constructed.
 *  @param imc_desc handler_desc The handlers for each of the methods.
 *  @return On success, 1; on failure, 0.
 */
int NaClSrpcServiceHandlerCtor(NaClSrpcService* service,
                               const NaClSrpcHandlerDesc* handler_desc);

/**
 *  Constructs an SRPC service object from a service discovery string.
 *  @param service The service to be constructed.
 *  @param imc_desc handler_desc The string returned from a service discovery
 *  call.
 *  @return On success, 1; on failure, 0.
 */
int NaClSrpcServiceStringCtor(NaClSrpcService* service,
                              const char* service_discovery_string);

/**
 *  Destroys an SRPC service object.
 *  @param service The service to be destroyed.
 */
void NaClSrpcServiceDtor(NaClSrpcService* service);

/**
 *  Prints the methods available from the specified service to stdout.
 *  @param service A service.
 */
void NaClSrpcServicePrint(const NaClSrpcService *service);

/**
 *  Obtains the count of methods exported by the service.
 *  @param service The service to be examined.
 *  @return The number of methods exported by the service.
 */
uint32_t NaClSrpcServiceMethodCount(const NaClSrpcService *service);


#define kNaClSrpcInvalidMethodIndex ((uint32_t) ~0)
/**
 *  Obtains the index of the specified RPC name.
 *  @param service The service to be searched.
 *  @param signature The exported signature of the method.
 *  @return A non-negative index if the name was found in the channel's set of
 *  methods.  If the name was not found, it returns kNaClSrpcInvalidMethodIndex.
 */
uint32_t NaClSrpcServiceMethodIndex(const NaClSrpcService *service,
                                    char const *signature);

/**
 *  Obtains the name, input types, and output types of the specified RPC
 *  number.
 *  @param service The service to be searched.
 *  @param rpc_number The number of the rpc to be looked up.
 *  @param name The exported name of the method.
 *  @param input_types The types of the inputs of the method.
 *  @param output_types The types of the outputs of the method.
 *  @return On success, 1; on failure, 0.
 */
extern int NaClSrpcServiceMethodNameAndTypes(const NaClSrpcService* service,
                                             uint32_t rpc_number,
                                             const char** name,
                                             const char** input_types,
                                             const char** output_types);

/**
 *  Obtains the function pointer used to handle invocations of a given
 *  method number.
 *  @param service The service to be searched.
 *  @param rpc_number The number of the rpc to be looked up.
 *  @return On success, a pointer to the handler; on failure, NULL.
 */
extern NaClSrpcMethod NaClSrpcServiceMethod(const NaClSrpcService* service,
                                            uint32_t rpc_number);

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
   * The services implemented by this server.
   */
  NaClSrpcService             *server;
  /**
   * The services available to this client.
   */
  NaClSrpcService             *client;
  /**
   * A pointer to channel-specific data.  This allows RPC method
   * implementations to be used across multiple services while still
   * maintaining reentrancy
   */
  void                        *server_instance_data;
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
 *  @param service A NaClSrpcService structure describing the service
 *  handled by this server.
 *  @param instance_data A value to be stored on the channel descriptor
 *  for conveying data specific to this particular server instance.
 *  @return On success, 1; on failure, 0.
 */
int NaClSrpcServerCtor(NaClSrpcChannel           *channel,
                       NaClSrpcImcDescType       imc_desc,
                       NaClSrpcService           *service,
                       void*                     instance_data);

/**
 *  Destroys the specified client or server channel.
 *  @param channel The channel to be destroyed.
 */
void NaClSrpcDtor(NaClSrpcChannel *channel);

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
int NaClSrpcServerLoop(NaClSrpcImcDescType              imc_socket_desc,
                       const struct NaClSrpcHandlerDesc methods[],
                       void                             *instance_data);

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
#define NACL_SRPC_METHOD_SECTION_ATTRIBUTE \
  __attribute__((section(".nacl_rpc_methods"))) \
  __attribute__((aligned(8)))

extern const struct NaClSrpcHandlerDesc
  NACL_SRPC_METHOD_SECTION_ATTRIBUTE
  __kNaClSrpcHandlers[];
/*
 * Not documented: in NativeClient code the default RPC descriptors are
 * recorded in a special section, ".nacl_rpc_methods".
 * The handler array ends at __kNaclSrpcHandlerEnd.
 */
extern const struct NaClSrpcHandlerDesc
  NACL_SRPC_METHOD_SECTION_ATTRIBUTE
  __kNaClSrpcHandlerEnd;
#endif /* DOXYGEN_SHOULD_SKIP_THIS */
#else
#define NACL_SRPC_METHOD_SECTION_ATTRIBUTE
#endif /* defined(__native_client__) ... */

/**
 * Exports an array of method descriptors to the default RPC descriptor array.
 * @param name is the name of the array containing the method descriptors.
 */
#define NACL_SRPC_METHOD_ARRAY(name) \
    struct NaClSrpcHandlerDesc NACL_SRPC_METHOD_SECTION_ATTRIBUTE name[]

/**
 * Exports a method to the default RPC descriptor array.
 * @param entry_format a string with the format "x:y:z", where x is the method
 * name, * y is the parameter type list, and z is the return value type list.
 * @param method_handler A function pointer for the implementation of the
 * method.
 */
/* TODO(sehr): these names can clash. */
#define NACL_SRPC_METHOD(entry_format, method_handler) \
  NACL_SRPC_METHOD_ARRAY(NaClSrpcMethod##method_handler) = \
      { { entry_format, method_handler } }

/**
 *  Initializes the SRPC module.
 *  @return Returns one on success, zero otherwise.
 */
int NaClSrpcModuleInit();

/**
 *  Shuts down the SRPC module.
 */
void NaClSrpcModuleFini();

/**
 *  @serverSrpc Waits for a connection from a client.  When a client
 *  connects, the server starts responding to RPC requests.
 *  @param methods The array of methods exported.
 *  @return Returns one on success, zero otherwise.
 */
int NaClSrpcAcceptClientConnection(const struct NaClSrpcHandlerDesc *methods);

/**
 *  @serverSrpc Waits for a connection from a client.  When a client
 *  connects, spawns a new thread with a server responding to RPC requests.
 *  @param methods The array of methods exported.
 *  @return Returns one on success, zero otherwise.
 */
int NaClSrpcAcceptClientOnThread(const struct NaClSrpcHandlerDesc *methods);

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
extern NaClSrpcError NaClSrpcInvokeBySignature(NaClSrpcChannel  *channel,
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
 * The current protocol (version) number used to send and receive RPCs.
 */
static const uint32_t kNaClSrpcProtocolVersion = 0xc0da0002;

/**
 * Deserialize a message header from a buffer.
 */
extern int NaClSrpcRpcGet(NaClSrpcImcBuffer* buffer,
                          NaClSrpcRpc* rpc);


/**
 * Send an RPC request on the given channel.
 */
extern int NaClSrpcRequestWrite(NaClSrpcChannel* channel,
                                NaClSrpcRpc* rpc,
                                NaClSrpcArg* args[],
                                NaClSrpcArg* rets[]);

/**
 * Wait for a sent RPC to receive a response.
 */
extern void NaClSrpcRpcWait(NaClSrpcChannel* channel,
                            NaClSrpcRpc* rpc);

/**
 * A utility type used by NaClSrpcCommandLoop.
 */
typedef NaClSrpcError (*NaClSrpcInterpreter)(NaClSrpcService* service,
                                             NaClSrpcChannel* channel,
                                             uint32_t rpc_number,
                                             NaClSrpcArg** ins,
                                             NaClSrpcArg** outs);

/**
 * The interpreter loop used by sel_universal and non-embedded applications.
 * It runs a loop that reads from stdin, and invokes the specified command
 * interpreter.  As the interpreter may need to know either the service or
 * channel on which it needs to run, these are also specified.  To provide
 * access to at least one valid transferrable descriptor, we pass the
 * default socket address descriptor for the NaCl module.
 */
extern void NaClSrpcCommandLoop(NaClSrpcService* service,
                                NaClSrpcChannel* channel,
                                NaClSrpcInterpreter interpreter,
                                NaClSrpcImcDescType default_socket_address);

/**
 *  @serverSrpc  Returns whether the srpc server is being run "standalone";
 *  that is, not as a subprocess of sel_universal, the browser plugin, etc.
 */
int NaClSrpcIsStandalone();

/**
 * Runs a text-based interpreter given a list of SRPC methods.  This
 * calls NaClSrpcCommandLoop().
 * @return Returns zero on success, non-zero otherwise.
 */
int NaClSrpcCommandLoopMain(const struct NaClSrpcHandlerDesc *methods);

EXTERN_C_END

/**
 * @}
 * End of System Calls group
 */

#endif  /* NATIVE_CLIENT_SRC_SHARED_SRPC_NACL_SRPC_H_ */
