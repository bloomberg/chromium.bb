/*
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

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

struct WorkRequest {
  explicit WorkRequest(const std::string &message)
      : msg(message),
        next(reinterpret_cast<WorkRequest *>(NULL)) {}
  std::string msg;  // copied from HandleMessage
  WorkRequest *next;
 private:
  DISALLOW_COPY_AND_ASSIGN(WorkRequest);
};

// A Worker object is associated with a single worker thread and a
// plugin instance (which may be associated with multiple Worker
// objects/threads).  It is created by a plugin instance (on the event
// handler thread) with a refcount of 2, with the expectation that one
// reference will be immediately handed off to its associated worker
// thread.  When the plugin instance is about to be destroyed in the
// event handler thread, the event handler should invoke the
// ShouldExit member function, which automatically decrements the
// reference associated with the event handler thread (i.e., the event
// handler thread should no longer use the Worker*).
class Worker {
 public:
  explicit Worker(MyInstance *instance);

  // RunToCompletion should be invoked in the worker thread.  It
  // returns when the plugin instance went away, and will
  // automatically unref the Worker object, so the worker thread
  // should no longer use the Worker object pointer after invoking
  // RunToCompletion().
  void RunToCompletion();

  WorkRequest *Dequeue();
  void Enqueue(WorkRequest *req);

  void Initialize(nacl::StringBuffer *sb);
  void NameServiceDump(nacl::StringBuffer *sb);
  void ManifestListTest(nacl::StringBuffer *sb);
  void ManifestOpenTest(nacl::StringBuffer *sb);

  // Called on the event thread as part of the instance shutdown.
  // Automatically unreferences the Worker object, so the event thread
  // should stop using the Worker pointer after invoking ShouldExit.
  void ShouldExit();

  void Unref();  // used only for error cleanup, e.g., when the worker
                 // thread did not launch.

  bool InitializeChannel(nacl::StringBuffer *sb);

 protected:
  // Event thread operation(s):
  //
  // In order for a test method to send reply messages, it should use
  // this PostStringMessage method, since (currently) the PostMessage
  // interface is event thread-only and not thread-safe.  Returns true
  // if successful and the thread should continue to do work, false
  // otherwise (anchor has been abandoned).
  bool PostStringMessage(const std::string &msg);
  // ... more Event thread operations here.

 private:
  MyInstance *instance_;  // cannot use directly from test worker thread!
  nacl::WeakRefAnchor *anchor_;
  // must copy out and Ref in ctor, since instance_ might go bad at any time.

  NaClMutex mu_;
  NaClCondVar cv_;  // queue not empty or should_exit_

  int ref_count_;
  WorkRequest *queue_head_;
  WorkRequest **queue_insert_;

  bool should_exit_;

  ~Worker();

  WorkRequest *Dequeue_mu();
  void Enqueue_mu(WorkRequest *req);

  struct DispatchTable {
    char const *op_name;
    void (Worker::*mfunc)(nacl::StringBuffer *sb);
  };

  bool            ns_channel_initialized_;
  NaClSrpcChannel ns_channel_;

  static DispatchTable const kDispatch[];  // null terminated

  DISALLOW_COPY_AND_ASSIGN(Worker);
};

// This object represents one time the page says <embed>.
class MyInstance : public nacl_ppapi::NaClPpapiPluginInstance {
 public:
  explicit MyInstance(PP_Instance instance);
  virtual ~MyInstance();
  virtual void HandleMessage(const pp::Var& message_data);

  Worker *worker() { return worker_; }

  // used with plugin::WeakRefCompletionCallback
  void PostStringMessage_EventThread(PostStringMessageWrapper *msg_wrapper,
                                     int32_t err);
 private:
  Worker *worker_;

  DISALLOW_COPY_AND_ASSIGN(MyInstance);
};

// ---------------------------------------------------------------------------

// success/fail
bool EnumerateNames(NaClSrpcChannel *nschan, nacl::StringBuffer *sb) {
  char      *buffer;
  uint32_t  nbytes = 4;
  uint32_t  in_out_nbytes;
  char      *new_buffer;

  buffer = reinterpret_cast<char *>(malloc(nbytes));
  if (NULL == buffer) {
    sb->Printf("EnumerateNames: initial malloc failed\n");
    return false;
  }

  for (;;) {
    in_out_nbytes = nbytes;
    if (NACL_SRPC_RESULT_OK != NaClSrpcInvokeBySignature(nschan,
                                                         NACL_NAME_SERVICE_LIST,
                                                         &in_out_nbytes,
                                                         buffer)) {
      sb->Printf("NaClSrpcInvokeBySignature failed\n");
      return false;
    }
    sb->Printf("EnumerateNames: in_out_nbytes %d\n", in_out_nbytes);
    if (in_out_nbytes < nbytes) {
      break;
    }
    nbytes *= 2;
    new_buffer = reinterpret_cast<char *>(realloc(buffer, nbytes));
    if (NULL == new_buffer) {
      sb->Printf("EnumerateNames: out of memory during realloc\n");
      free(buffer);
      return false;
    }
    buffer = new_buffer;
    new_buffer = NULL;
  }
  nbytes = in_out_nbytes;
  sb->Printf("nbytes = %u\n", (size_t) nbytes);
  if (nbytes == sizeof buffer) {
    sb->Printf("Insufficent space for namespace enumeration\n");
    return false;
  }
  size_t name_len;
  for (char *p = buffer;
       static_cast<size_t>(p - buffer) < nbytes;
       p += name_len) {
    name_len = strlen(p) + 1;
    sb->Printf("%s\n", p);
  }
  free(buffer);
  return true;
}

// ---------------------------------------------------------------------------

PostStringMessageWrapper::~PostStringMessageWrapper() {}

// ---------------------------------------------------------------------------

MyInstance::MyInstance(PP_Instance instance)
    : nacl_ppapi::NaClPpapiPluginInstance(instance),
      worker_(new Worker(this)) {
}

MyInstance::~MyInstance() {
  worker_->ShouldExit();
}

void MyInstance::PostStringMessage_EventThread(
    PostStringMessageWrapper *msg_wrapper,
    int32_t err) {
  PostMessage(msg_wrapper->msg());
  msg_wrapper->SetResult(nacl_ppapi::g_void_result);
}

// ---------------------------------------------------------------------------

Worker::Worker(MyInstance *instance)
    : instance_(instance),
      anchor_(instance->anchor()->Ref()),
      ref_count_(2),  // one for the master and one for the dame...
      queue_head_(NULL),
      queue_insert_(&queue_head_),
      should_exit_(false),
      ns_channel_initialized_(false) {
  NaClXMutexCtor(&mu_);
  NaClXCondVarCtor(&cv_);
}

void Worker::Unref() {
  bool do_delete;
  do {
    nacl::MutexLocker take(&mu_);
    do_delete = (--ref_count_ == 0);
  } while (0);
  // dropped lock before invoking dtor
  if (do_delete) {
    delete this;
  }
}

Worker::~Worker() {
  anchor_->Unref();

  WorkRequest *req;
  while ((req = Dequeue_mu()) != NULL) {
    delete req;
  }

  NaClMutexDtor(&mu_);
  NaClCondVarDtor(&cv_);
}

void Worker::ShouldExit() {
  do {
    nacl::MutexLocker take(&mu_);
    should_exit_ = true;
    NaClXCondVarBroadcast(&cv_);
  } while (0);
  Unref();
}

WorkRequest *Worker::Dequeue_mu() {
  WorkRequest *head = queue_head_;

  if (head != NULL) {
    queue_head_ = head->next;
    if (queue_head_ == NULL) {
      queue_insert_ = &queue_head_;
    }
  }
  return head;
}

void Worker::Enqueue_mu(WorkRequest *req) {
  req->next = NULL;
  *queue_insert_ = req;
  queue_insert_ = &req->next;
}

WorkRequest *Worker::Dequeue() {
  nacl::MutexLocker take(&mu_);
  return Dequeue_mu();
}

void Worker::Enqueue(WorkRequest *req) {
  nacl::MutexLocker take(&mu_);
  Enqueue_mu(req);
  NaClXCondVarBroadcast(&cv_);
}

Worker::DispatchTable const Worker::kDispatch[] = {
  { "init", &Worker::Initialize },
  { "name_dump", &Worker::NameServiceDump },
  { "manifest_list", &Worker::ManifestListTest },
  { "manifest_open", &Worker::ManifestOpenTest },
  { reinterpret_cast<char const *>(NULL), NULL }
};

bool Worker::PostStringMessage(const std::string &msg) {
  nacl_ppapi::EventThreadWorkState<nacl_ppapi::VoidResult> state;
  plugin::WeakRefCallOnMainThread(anchor_,
                                  0  /* mS */,
                                  instance_,
                                  &MyInstance::PostStringMessage_EventThread,
                                  new PostStringMessageWrapper(&state, msg));
  if (NULL == state.WaitForCompletion()) {
    // anchor_ has been abandoned, so the plugin instance went away.
    // we should drop our ref to the anchor, then shut down the worker
    // thread.
    nacl::MutexLocker take(&mu_);
    should_exit_ = true;
    // There's no need to condvar broadcast, since it is the worker
    // thread that will look at the work queue and the should_exit_ to
    // act on this.  Unfortunately every worker thread must test the
    // return value of PostStringMessage to determine if it should do
    // early exit (if the worker needs to do multiple event-thread
    // operations).
    return false;
  }
  return true;
}

void Worker::RunToCompletion() {
  for (;;) {
    WorkRequest *req;
    do {
      nacl::MutexLocker take(&mu_);
      for (;;) {
        if (should_exit_) {
          // drop the lock and drop the reference count to this
          goto break_x3;
        }
        fprintf(stderr, "RunToCompletion: Dequeuing...\n");
        if ((req = Dequeue_mu()) != NULL) {
          fprintf(stderr, "RunToCompletion: found work %p\n",
                  reinterpret_cast<void *>(req));
          break;
        }
        fprintf(stderr, "RunToCompletion: waiting\n");
        NaClXCondVarWait(&cv_, &mu_);
        fprintf(stderr, "RunToCompletion: woke up\n");
      }
    } while (0);

    // Do the work, without holding the lock.  The work function
    // should reacquire mu_ as needed.

    nacl::StringBuffer sb;

    // scan dispatch table for op_name
    fprintf(stderr, "RunToCompletion: scanning for %s\n", req->msg.c_str());
    for (size_t ix = 0; kDispatch[ix].op_name != NULL; ++ix) {
      fprintf(stderr,
              "RunToCompletion: comparing against %s\n", kDispatch[ix].op_name);
      if (req->msg == kDispatch[ix].op_name) {
        if (InitializeChannel(&sb)) {
          fprintf(stderr, "RunToCompletion:  invoking table entry %u\n", ix);
          (this->*(kDispatch[ix].mfunc))(&sb);
        }
        break;
      }
    }
    // always post a reply, even if it is the empty string
    fprintf(stderr,
            "RunToCompletion: posting reply %s\n", sb.ToString().c_str());
    if (!PostStringMessage(sb.ToString())) {
      break;
    }
  }
 break_x3:
  fprintf(stderr, "RunToCompletion: exiting\n");
  Unref();
}

bool Worker::InitializeChannel(nacl::StringBuffer *sb) {
  if (ns_channel_initialized_) {
    return true;
  }
  int ns = -1;
  nacl_nameservice(&ns);
  printf("ns = %d\n", ns);
  assert(-1 != ns);
  int connected_socket = imc_connect(ns);
  assert(-1 != connected_socket);
  if (!NaClSrpcClientCtor(&ns_channel_, connected_socket)) {
    sb->Printf("Srpc client channel ctor failed\n");
    close(ns);
    return false;
  }
  sb->Printf("NaClSrpcClientCtor succeeded\n");
  close(ns);
  ns_channel_initialized_ = true;
  return true;
}

void Worker::Initialize(nacl::StringBuffer *sb) {
  // we just want the log output from the InitializeChannel
  return;
}

// return name service output in sb
void Worker::NameServiceDump(nacl::StringBuffer *sb) {
  (void) EnumerateNames(&ns_channel_, sb);
}

void Worker::ManifestListTest(nacl::StringBuffer *sb) {
  int status;
  int manifest;
  // name service lookup for the manifest service descriptor
  if (NACL_SRPC_RESULT_OK !=
      NaClSrpcInvokeBySignature(&ns_channel_, NACL_NAME_SERVICE_LOOKUP,
                                "ManifestNameService", O_RDWR,
                                &status, &manifest) ||
      NACL_NAME_SERVICE_SUCCESS != status) {
    sb->Printf("nameservice lookup failed, status %d\n", status);
  }
  sb->Printf("Got manifest descriptor %d\n", manifest);
  if (-1 == manifest) {
    return;
  }

  // connect to manifest name server
  int manifest_conn = imc_connect(manifest);
  close(manifest);
  sb->Printf("got manifest connection %d\n", manifest_conn);
  if (-1 == manifest_conn) {
    sb->Printf("could not connect\n");
    return;
  }

  // build the SRPC connection (do service discovery)
  struct NaClSrpcChannel manifest_channel;
  if (!NaClSrpcClientCtor(&manifest_channel, manifest_conn)) {
    sb->Printf("could not build srpc client\n");
    return;
  }
  sb->Printf("ManifestListTest: basic connectivity ok\n");

  // list manifest service contents
  char      buffer[1024];
  uint32_t  nbytes = sizeof buffer;

  if (NACL_SRPC_RESULT_OK !=
      NaClSrpcInvokeBySignature(&manifest_channel, NACL_NAME_SERVICE_LIST,
                                &nbytes, buffer)) {
    sb->Printf("manifest list RPC failed\n");
    NaClSrpcDtor(&manifest_channel);
    return;
  }

  sb->DiscardOutput();
  sb->Printf("Manifest Contents:\n");
  size_t name_len;
  // Should we explicitly sort the names?  This would ensure that the
  // test output is easy to compare with expected results.  Currently,
  // the manifest uses a set to hold the names, so the results will be
  // sorted anyway, but this is not a guarantee of the API.
  for (char *p = buffer;
       static_cast<size_t>(p - buffer) < nbytes;
       p += name_len + 1) {
    name_len = strlen(p);
    sb->Printf("%.*s\n", (int) name_len, p);
  }
  NaClSrpcDtor(&manifest_channel);
  return;
}

void Worker::ManifestOpenTest(nacl::StringBuffer *sb) {
  int                     status = -1;
  int                     manifest;
  struct NaClSrpcChannel  manifest_channel;

  // name service lookup for the manifest service descriptor
  if (NACL_SRPC_RESULT_OK !=
      NaClSrpcInvokeBySignature(&ns_channel_, NACL_NAME_SERVICE_LOOKUP,
                                "ManifestNameService", O_RDWR,
                                &status, &manifest) ||
      NACL_NAME_SERVICE_SUCCESS != status) {
    sb->Printf("nameservice lookup failed, status %d\n", status);
    return;
  }
  sb->Printf("Got manifest descriptor %d\n", manifest);
  if (-1 == manifest) {
    return;
  }

  // connect to manifest name server
  int manifest_conn = imc_connect(manifest);
  close(manifest);
  sb->Printf("got manifest connection %d\n", manifest_conn);
  if (-1 == manifest_conn) {
    sb->Printf("could not connect\n");
    return;
  }

  // build the SRPC connection (do service discovery)
  if (!NaClSrpcClientCtor(&manifest_channel, manifest_conn)) {
    sb->Printf("could not build srpc client\n");
    return;
  }

  int desc;

  sb->Printf("Invoking name service lookup\n");
  if (NACL_SRPC_RESULT_OK !=
      NaClSrpcInvokeBySignature(&manifest_channel,
                                NACL_NAME_SERVICE_LOOKUP,
                                "files/test_file", O_RDONLY,
                                &status, &desc)) {
    sb->Printf("manifest lookup RPC failed\n");
    NaClSrpcDtor(&manifest_channel);
    return;
  }

  sb->DiscardOutput();
  sb->Printf("File Contents:\n");

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
    sb->Printf("%s", buffer);
  }
  fclose(iob);  // closed desc
  NaClSrpcDtor(&manifest_channel);
  return;
}

// HandleMessage gets invoked when postMessage is called on the DOM
// element associated with this plugin instance.  In this case, if we
// are given a string, we'll post a message back to JavaScript with a
// reply -- essentially treating this as a string-based RPC.
void MyInstance::HandleMessage(const pp::Var& message) {
  if (message.is_string()) {
    fprintf(stderr,
            "HandleMessage: enqueuing %s\n", message.AsString().c_str());
    fflush(NULL);
    worker_->Enqueue(new WorkRequest(message.AsString()));
  } else {
    fprintf(stderr, "HandleMessage: message is not a string\n");
    fflush(NULL);
  }
}

void *worker_thread_start(void *arg) {
  Worker *worker = reinterpret_cast<Worker *>(arg);

  fprintf(stderr, "Sleeping...\n"); fflush(stderr);
  sleep(1);
  fprintf(stderr, "worker_thread_start: worker %p\n",
          reinterpret_cast<void *>(worker));
  fflush(NULL);
  worker->RunToCompletion();
  worker = NULL;  // RunToCompletion automatically Unrefs
  return reinterpret_cast<void *>(NULL);
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
  // spawn worker thread associated with this instance
  pthread_t thread;

  fprintf(stderr, "CreateInstance invoked\n"); fflush(NULL);
  if (0 != pthread_create(&thread,
                          reinterpret_cast<pthread_attr_t *>(NULL),
                          worker_thread_start,
                          reinterpret_cast<void *>(instance->worker()))) {
    // Remove the reference the ownership of which should have been
    // passed to the worker thread.
    instance->worker()->Unref();
    delete instance;
    instance = NULL;
    fprintf(stderr, "pthread_create failed\n"); fflush(NULL);
  } else {
    fprintf(stderr, "CreateInstance: Worker thread started\n");
    fprintf(stderr, "CreateInstance: worker thread object %p\n",
            reinterpret_cast<void *>(instance->worker()));
    (void) pthread_detach(thread);
  }
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
