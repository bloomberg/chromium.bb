/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

//
// Post-message based test for simple rpc based access to name services.
//

#include <string>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/fcntl.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/platform/nacl_sync_raii.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

// TODO(bsy): move weak_ref module to the shared directory
#include "native_client/src/trusted/weak_ref/weak_ref.h"
#include "native_client/src/trusted/weak_ref/call_on_main_thread.h"

#include "native_client/src/untrusted/nacl_ppapi_util/nacl_ppapi_util.h"
#include "native_client/src/untrusted/nacl_ppapi_util/string_buffer.h"

#include <sys/nacl_syscalls.h>
#include <sys/nacl_name_service.h>

#include "native_client/src/shared/ppapi_proxy/ppruntime.h"

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

std::string *manifest_contents = NULL;

void TestManifestContents() {
  nacl::StringBuffer      sb;
  int                     status = -1;
  int                     manifest;
  struct NaClSrpcChannel  manifest_channel;
  struct NaClSrpcChannel  ns_channel;

  int ns = -1;
  nacl_nameservice(&ns);
  printf("ns = %d\n", ns);
  assert(-1 != ns);
  int connected_socket = imc_connect(ns);
  assert(-1 != connected_socket);
  if (!NaClSrpcClientCtor(&ns_channel, connected_socket)) {
    close(ns);
    sb.Printf("Srpc client channel ctor failed\n");
    manifest_contents = new std::string(sb.ToString());
    return;
  }
  sb.Printf("NaClSrpcClientCtor succeeded\n");
  close(ns);
  // name service lookup for the manifest service descriptor
  if (NACL_SRPC_RESULT_OK !=
      NaClSrpcInvokeBySignature(&ns_channel, NACL_NAME_SERVICE_LOOKUP,
                                "ManifestNameService", O_RDWR,
                                &status, &manifest) ||
      NACL_NAME_SERVICE_SUCCESS != status) {
    sb.Printf("nameservice lookup failed, status %d\n", status);
    manifest_contents = new std::string(sb.ToString());
    return;
  }
  sb.Printf("Got manifest descriptor %d\n", manifest);
  if (-1 == manifest) {
    manifest_contents = new std::string(sb.ToString());
    return;
  }

  // connect to manifest name server
  int manifest_conn = imc_connect(manifest);
  close(manifest);
  sb.Printf("got manifest connection %d\n", manifest_conn);
  if (-1 == manifest_conn) {
    sb.Printf("could not connect\n");
    manifest_contents = new std::string(sb.ToString());
    return;
  }

  // build the SRPC connection (do service discovery)
  if (!NaClSrpcClientCtor(&manifest_channel, manifest_conn)) {
    sb.Printf("could not build srpc client\n");
    manifest_contents = new std::string(sb.ToString());
    return;
  }

  int desc;

  sb.Printf("Invoking name service lookup\n");
  if (NACL_SRPC_RESULT_OK !=
      NaClSrpcInvokeBySignature(&manifest_channel,
                                NACL_NAME_SERVICE_LOOKUP,
                                "files/test_file", O_RDONLY,
                                &status, &desc)) {
    sb.Printf("manifest lookup RPC failed\n");
    NaClSrpcDtor(&manifest_channel);
    manifest_contents = new std::string(sb.ToString());
    return;
  }

  sb.DiscardOutput();
  sb.Printf("File Contents:\n");

  FILE *iob = fdopen(desc, "r");
  char buffer[4096];
  while (fgets(buffer, sizeof buffer, iob) != NULL) {
    // NB: fgets does not discard the newline nor any carriage return
    // character before that.
    //
    // Note that CR LF is the default end-of-line style for Windows.
    // Furthermore, when the test_file (input data, which happens to
    // be the nmf file) is initially created in a change list, the
    // patch is sent to our try bots as text.  This means that when
    // the file arrives, it has CR LF endings instead of the original
    // LF line endings.  Since the expected or golden data is
    // (manually) encoded in the HTML file's JavaScript, there will be
    // a mismatch.  After submission, the svn property svn:eol-style
    // will be set to LF, so a clean check out should have LF and not
    // CR LF endings, and the tests will pass without CR removal.
    // However -- and there's always a however in long discourses --
    // if the nmf file is edited, say, because the test is being
    // modified, and the modification is being done on a Windows
    // machine, then it is likely that the editor used by the
    // programmer will convert the file to CR LF endings.  Which,
    // unfortunatly, implies that the test will mysteriously fail
    // again.
    //
    // To defend against such nonsense, we weaken the test slighty,
    // and just strip the CR if it is present.
    int len = strlen(buffer);
    if (len >= 2 && buffer[len-1] == '\n' && buffer[len-2] == '\r') {
      buffer[len-2] = '\n';
      buffer[len-1] = '\0';
    }
    sb.Printf("%s", buffer);
  }
  fclose(iob);  // closed desc

  sb.Printf("\n");
  sb.Printf("Opening non-existent file:\n");
  if (NACL_SRPC_RESULT_OK !=
      NaClSrpcInvokeBySignature(&manifest_channel,
                                NACL_NAME_SERVICE_LOOKUP,
                                "foobar/baz", O_RDONLY,
                                &status, &desc)) {
    sb.Printf("bogus manifest lookup RPC failed\n");
    NaClSrpcDtor(&manifest_channel);
    manifest_contents = new std::string(sb.ToString());
    return;
  }
  sb.Printf("Got descriptor %d, status %d\n", desc, status);
  if (-1 != desc) {
    (void) close(desc);
  }

  NaClSrpcDtor(&manifest_channel);
  manifest_contents = new std::string(sb.ToString());
}

class PostStringMessageWrapper
    : public nacl_ppapi::EventThreadWorkStateWrapper<nacl_ppapi::VoidResult> {
 public:
  PostStringMessageWrapper(nacl_ppapi::EventThreadWorkState<
                             nacl_ppapi::VoidResult>
                           *state,
                           const std::string &msg)
      : nacl_ppapi::EventThreadWorkStateWrapper<nacl_ppapi::VoidResult>(
          state),
        msg_(msg) {}
  ~PostStringMessageWrapper();
  const std::string &msg() const { return msg_; }
 private:
  std::string msg_;

  DISALLOW_COPY_AND_ASSIGN(PostStringMessageWrapper);
};

// ---------------------------------------------------------------------------

class MyInstance;

// This object represents one time the page says <embed>.
class MyInstance : public nacl_ppapi::NaClPpapiPluginInstance {
 public:
  explicit MyInstance(PP_Instance instance);
  virtual ~MyInstance();
  virtual void HandleMessage(const pp::Var& message_data);

 private:
  DISALLOW_COPY_AND_ASSIGN(MyInstance);
};

// ---------------------------------------------------------------------------

MyInstance::MyInstance(PP_Instance instance)
    : nacl_ppapi::NaClPpapiPluginInstance(instance) {
}

MyInstance::~MyInstance() {}

// ---------------------------------------------------------------------------

// HandleMessage gets invoked when postMessage is called on the DOM
// element associated with this plugin instance.  In this case, if we
// are given a string, we'll post a message back to JavaScript with a
// reply -- essentially treating this as a string-based RPC.
void MyInstance::HandleMessage(const pp::Var& message) {
  if (message.is_string()) {
    if (message.AsString() == "manifest_data") {
      PostMessage(*manifest_contents);
    } else {
      fprintf(stderr, "HandleMessage: Unrecognized request \"%s\".\n",
              message.AsString().c_str());
    }
  } else {
    fprintf(stderr, "HandleMessage: message is not a string\n");
    fflush(NULL);
  }
}

// This object is the global object representing this plugin library as long
// as it is loaded.
class MyModule : public pp::Module {
 public:
  MyModule() : pp::Module() {}
  virtual ~MyModule() {}

  // Override CreateInstance to create your customized Instance object.
  virtual pp::Instance *CreateInstance(PP_Instance instance);

  DISALLOW_COPY_AND_ASSIGN(MyModule);
};

pp::Instance *MyModule::CreateInstance(PP_Instance pp_instance) {
  MyInstance *instance = new MyInstance(pp_instance);
  fprintf(stderr, "CreateInstance: returning instance %p\n",
          reinterpret_cast<void *>(instance));

  return instance;
}

namespace pp {

// Factory function for your specialization of the Module object.
Module* CreateModule() {
  fprintf(stderr, "CreateModule invoked\n"); fflush(NULL);
  return new MyModule();
}

}  // namespace pp

int main() {
  NaClSrpcModuleInit();
  TestManifestContents();
  return PpapiPluginMain();
}
