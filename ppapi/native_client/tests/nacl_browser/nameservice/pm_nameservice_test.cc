/*
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Post-message based test for simple rpc based access to name services.
 *
 * Converted from srpc_nameservice_test (deprecated), i.e., C -> C++,
 * srpc -> post message.
 */
#include <string>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/fcntl.h>
#include <string.h>
#include <unistd.h>

#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/untrusted/nacl_ppapi_util/nacl_ppapi_util.h"
#include "native_client/src/untrusted/nacl_ppapi_util/string_buffer.h"

#include <sys/nacl_syscalls.h>
#include <sys/nacl_name_service.h>

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

#define RNG_OUTPUT_BYTES  1024

#define BYTES_PER_LINE    32
#define BYTE_SPACING      4

bool            g_ns_channel_initialized = false;
NaClSrpcChannel g_ns_channel;

void dump_output(nacl::StringBuffer *sb, int d, size_t nbytes) {
  nacl::scoped_array<uint8_t> bytes;
  size_t                      got;
  int                         copied;

  bytes.reset(new uint8_t[nbytes]);
  if (NULL == bytes.get()) {
    perror("dump_output");
    fprintf(stderr, "No memory\n");
    return;
  }
  // Read the RNG output.
  for (got = 0; got < nbytes; got += copied) {
    copied = read(d, bytes.get() + got, nbytes - got);
    if (-1 == copied) {
      perror("dump_output:read");
      fprintf(stderr, "read failure\n");
      break;
    }
    printf("read(%d, ..., %u) -> %d\n", d, nbytes - got, copied);
  }
  // Hex dump it so we can eyeball it for randomness.  Ideally we
  // would have a chi-square test here to test randomness.
  for (size_t ix = 0; ix < got; ++ix) {
    if (0 == (ix & (BYTES_PER_LINE - 1))) {
      sb->Printf("\n%04x:", ix);
    } else if (0 == (ix & (BYTE_SPACING - 1))) {
      sb->Printf(" ");
    }
    sb->Printf("%02x", bytes[ix]);
  }
  sb->Printf("\n");
}

void EnumerateNames(NaClSrpcChannel *nschan, nacl::StringBuffer *sb) {
  char      buffer[1024];
  uint32_t  nbytes = sizeof buffer;

  if (NACL_SRPC_RESULT_OK != NaClSrpcInvokeBySignature(nschan,
                                                       NACL_NAME_SERVICE_LIST,
                                                       &nbytes, buffer)) {
    sb->Printf("NaClSrpcInvokeBySignature failed\n");
    return;
  }
  sb->Printf("nbytes = %u\n", (size_t) nbytes);
  if (nbytes == sizeof buffer) {
    sb->Printf("Insufficent space for namespace enumeration\n");
    return;
  }

  size_t name_len;
  for (char *p = buffer;
       static_cast<size_t>(p - buffer) < nbytes;
       p += name_len) {
    name_len = strlen(p) + 1;
    sb->Printf("%s\n", p);
  }
}

void Initialize(const pp::Var& message_data, nacl::StringBuffer* sb) {
  if (g_ns_channel_initialized) {
    return;
  }
  int ns = -1;
  nacl_nameservice(&ns);
  printf("ns = %d\n", ns);
  assert(-1 != ns);
  int connected_socket = imc_connect(ns);
  assert(-1 != connected_socket);
  if (!NaClSrpcClientCtor(&g_ns_channel, connected_socket)) {
    sb->Printf("Srpc client channel ctor failed\n");
    close(ns);
  }
  sb->Printf("NaClSrpcClientCtor succeeded\n");
  close(ns);
  g_ns_channel_initialized = 1;
}

//
// Return name service output
//
void NameServiceDump(const pp::Var& message_data, nacl::StringBuffer* sb) {
  Initialize(message_data, sb);
  EnumerateNames(&g_ns_channel, sb);
}

//
// Dump RNG output into a string.
//
void RngDump(const pp::Var& message_data, nacl::StringBuffer* sb) {
  NaClSrpcError rpc_result;
  int status;
  int rng;

  Initialize(message_data, sb);

  rpc_result = NaClSrpcInvokeBySignature(&g_ns_channel,
                                         NACL_NAME_SERVICE_LOOKUP,
                                         "SecureRandom", O_RDONLY,
                                         &status, &rng);
  assert(NACL_SRPC_RESULT_OK == rpc_result);
  printf("rpc status %d\n", status);
  assert(NACL_NAME_SERVICE_SUCCESS == status);
  printf("rng descriptor %d\n", rng);

  dump_output(sb, rng, RNG_OUTPUT_BYTES);
  close(rng);
}

void ManifestTest(const pp::Var& message_data, nacl::StringBuffer* sb) {
  int status = -1;
  int manifest;

  Initialize(message_data, sb);

  // Make the name service lookup for the manifest service descriptor.
  if (NACL_SRPC_RESULT_OK !=
      NaClSrpcInvokeBySignature(&g_ns_channel, NACL_NAME_SERVICE_LOOKUP,
                                "ManifestNameService", O_RDWR,
                                &status, &manifest) ||
      NACL_NAME_SERVICE_SUCCESS != status) {
    fprintf(stderr, "nameservice lookup failed, status %d\n", status);
    return;
  }
  sb->Printf("Got manifest descriptor %d\n", manifest);
  if (-1 == manifest) {
    return;
  }

  // Connect to manifest name server.
  int manifest_conn = imc_connect(manifest);
  close(manifest);
  sb->Printf("got manifest connection %d\n", manifest_conn);
  if (-1 == manifest_conn) {
    sb->Printf("could not connect\n");
    return;
  }
  sb->DiscardOutput();
  sb->Printf("ManifestTest: basic connectivity ok\n");

  close(manifest_conn);
}

struct PostMessageHandlerDesc {
  char const *request;
  void (*handler)(const pp::Var& message_data, nacl::StringBuffer* out);
};

// This object represents one time the page says <embed>.
class MyInstance : public pp::Instance {
 public:
  explicit MyInstance(PP_Instance instance) : pp::Instance(instance) {}
  virtual ~MyInstance() {}
  virtual void HandleMessage(const pp::Var& message_data);
};

// HandleMessage gets invoked when postMessage is called on the DOM
// element associated with this plugin instance.  In this case, if we
// are given a string, we'll post a message back to JavaScript with a
// reply string -- essentially treating this as a string-based RPC.
void MyInstance::HandleMessage(const pp::Var& message_data) {
  static struct PostMessageHandlerDesc kMsgHandlers[] = {
    { "init", Initialize },
    { "nameservice", NameServiceDump },
    { "rng", RngDump },
    { "manifest_test", ManifestTest },
    { reinterpret_cast<char const *>(NULL),
      reinterpret_cast<void (*)(const pp::Var&, nacl::StringBuffer*)>(NULL) }
  };
  nacl::StringBuffer sb;

  if (message_data.is_string()) {
    std::string op_name(message_data.AsString());
    std::string reply;
    size_t len;

    fprintf(stderr, "Searching for handler for request \"%s\".\n",
            op_name.c_str());

    for (size_t ix = 0; kMsgHandlers[ix].request != NULL; ++ix) {
      if (op_name == kMsgHandlers[ix].request) {
        fprintf(stderr, "found at index %u\n", ix);
        kMsgHandlers[ix].handler(message_data, &sb);
        break;
      }
    }

    reply = sb.ToString();
    len = strlen(reply.c_str());
    fprintf(stderr, "posting reply len %d\n", len);
    // fprintf(stderr, "posting reply \"%s\".\n", sb.ToString().c_str());
    fprintf(stderr, "posting reply \"");
    fflush(stderr);
    write(2, reply.c_str(), len);
    fprintf(stderr, "\".\n");
    fflush(stderr);

    PostMessage(pp::Var(sb.ToString()));
    fprintf(stderr, "returning\n");
    fflush(stderr);
  }
}

// This object is the global object representing this plugin library as long
// as it is loaded.
class MyModule : public pp::Module {
 public:
  MyModule() : pp::Module() {}
  virtual ~MyModule() {}

  // Override CreateInstance to create your customized Instance object.
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new MyInstance(instance);
  }
};

namespace pp {

// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new MyModule();
}

}  // namespace pp
