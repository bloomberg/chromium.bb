// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdlib.h>

#include <iostream>
#include <sstream>
#include <string>
#include <queue>

#include <nacl/nacl_check.h>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_io.h"

#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/url_response_info.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/var.h"

using std::string;
using std::ostringstream;

const int kDefaultChunkSize = 1024;


class MyInstance : public pp::Instance {
 private:
  string url_;
  bool stream_to_file_;
  uint32_t chunk_size_;
  bool debug_;
  bool pdebug_;

  void ParseArgs(uint32_t argc, const char* argn[], const char* argv[]) {
     for (uint32_t i = 0; i < argc; ++i) {
       const std::string tag = argn[i];
       if (tag == "chunk_size") chunk_size_ = strtol(argv[i], 0, 0);
       if (tag == "url") url_ = argv[i];
       if (tag == "to_file") stream_to_file_ = strtol(argv[i], 0, 0);
       if (tag == "debug") debug_ = strtol(argv[i], 0, 0);
       if (tag == "pdebug") pdebug_ = strtol(argv[i], 0, 0);
       // ignore other tags
     }
  }

 public:
  void Message(const string& s) {
    ostringstream stream;
    stream << pp_instance() << ": " << s;
    pp::Var message(stream.str());
    PostMessage(message);
  }


  void Debug(const string& s) {
    if (debug_) {
      std::cout << "DEBUG: " << s;
    }
    if (pdebug_) {
      Message("DEBUG: " + s);
    }
  }

  explicit MyInstance(PP_Instance instance)
    : pp::Instance(instance),
      url_("no_url_given.html"),
      stream_to_file_(true),
      chunk_size_(kDefaultChunkSize),
      debug_(false),
      pdebug_(false) {
  }

  virtual ~MyInstance() {}

  // Defined below. This function does the real work by delegating it to
  // either ReaderStreamAsFile or ReaderResponseBody
  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]);
};



class ReaderStreamAsFile {
 private:
  uint32_t current_offset_;
  uint32_t chunk_size_;
  char* buffer_;
  pp::URLResponseInfo* response_info_;
  pp::FileRef* file_ref_;
  pp::FileIO* file_io_;
  MyInstance* instance_;
  pp::URLLoader loader_;

  // forward to instance
  void Message(const string& s) {
    instance_->Message(s);
  }

  void Debug(const string& s) {
    instance_->Debug(s);
  }

  static void ReadCompleteCallback(void* thiz, int32_t result) {
    ReaderStreamAsFile* reader = static_cast<ReaderStreamAsFile*>(thiz);
    ostringstream stream;
    stream << "ReadCompleteCallback Bytes Read: " << result;
    reader->Debug(stream.str());
    if (result <= 0) {
      reader->Message("COMPLETE");
      return;
    }

    reader->current_offset_ += result;
    reader->ReadMore();
  }

  void ReadMore() {
    pp::CompletionCallback cc(ReadCompleteCallback, this);
    cc.set_flags(PP_COMPLETIONCALLBACK_FLAG_OPTIONAL);
    int32_t rv = file_io_->Read(current_offset_, buffer_, chunk_size_, cc);
    if (rv == PP_OK) {
      cc.Run(rv);
    } else if (rv != PP_OK_COMPLETIONPENDING) {
      Message("Error: ReadMore unexpected rv");
    }
  }

  static void OpenFileCompleteCallback(void* thiz, int32_t result) {
    ReaderStreamAsFile* reader = static_cast<ReaderStreamAsFile*>(thiz);

    if (result != PP_OK) {
      reader->Message("Error: FileOpenCompleteCallback unexpected result");
      return;
    }

    reader->ReadMore();
  }

  void OpenFile() {
    file_ref_ = new pp::FileRef(response_info_->GetBodyAsFileRef());
    CHECK(!file_ref_->is_null());

    file_io_ = new  pp::FileIO(instance_);
    CHECK(!file_io_->is_null());

    pp::CompletionCallback cc(OpenFileCompleteCallback, this);
    int32_t rv = file_io_->Open(*file_ref_, PP_FILEOPENFLAG_READ, cc);
    if (rv != PP_OK_COMPLETIONPENDING) {
      Message("Error: OpenFile unexpected rv");
    }
  }

  static void FinishCompleteCallback(void* thiz, int32_t result) {
    ReaderStreamAsFile* reader = static_cast<ReaderStreamAsFile*>(thiz);
    if (result != PP_OK) {
      reader->Message("Error: FinishCompleteCallback unexpected result");
      return;
    }

    reader->OpenFile();
  }

  void Finish() {
    pp::CompletionCallback cc(FinishCompleteCallback, this);
    cc.set_flags(PP_COMPLETIONCALLBACK_FLAG_OPTIONAL);
    int32_t rv = loader_.FinishStreamingToFile(cc);
    if (rv == PP_OK) {
      cc.Run(rv);
    } else if (rv != PP_OK_COMPLETIONPENDING) {
      Message("Error: Finish unexpected rv");
    }
  }

  // this callback handles both regular and "stream-to-file" mode
  // But control flow diverges afterwards
  static void OpenURLCompleteCallback(void* thiz, int32_t result) {
    ReaderStreamAsFile* reader = static_cast<ReaderStreamAsFile*>(thiz);

    reader->response_info_ =
      new pp::URLResponseInfo(reader->loader_.GetResponseInfo());
    CHECK(!reader->response_info_->is_null());
    int32_t status_code = reader->response_info_->GetStatusCode();
    if (status_code != 200) {
      reader->Message("Error: OpenURLCompleteCallback unexpected status code");
      return;
    }

    reader->Finish();
  }

  void OpenURL(const string& url) {
    pp::URLRequestInfo request(instance_);
    request.SetURL(url);
    request.SetStreamToFile(true);

    pp::CompletionCallback cc(OpenURLCompleteCallback, this);
    int32_t rv = loader_.Open(request, cc);
    if (rv != PP_OK_COMPLETIONPENDING) {
      Message("Error: OpenURL unexpected rv");
    }
  }

 public:
  ReaderStreamAsFile(MyInstance* instance,
                     uint32_t chunk_size,
                     const string& url) :
    current_offset_(0),
    chunk_size_(chunk_size),
    buffer_(new char[chunk_size]),
    response_info_(0),
    file_ref_(0),
    file_io_(0),
    instance_(instance),
    loader_(instance) {
      OpenURL(url);
    }
};


class ReaderResponseBody {
 private:
  uint32_t chunk_size_;
  char* buffer_;
  MyInstance* instance_;
  pp::URLLoader loader_;

  // forward to instance
  void Message(const string& s) {
    instance_->Message(s);
  }

  void Debug(const string& s) {
    instance_->Debug(s);
  }

  static void ReadCompleteCallback(void* thiz, int32_t result) {
    ReaderResponseBody* reader = static_cast<ReaderResponseBody*>(thiz);
    ostringstream stream;
    stream << "ReadCompleteCallback Bytes Read: " << result;
    reader->Debug(stream.str());
    if (result <= 0) {
      reader->Message("COMPLETE");
      return;
    }
    reader->ReadMore();
  }

  void ReadMore() {
    pp::CompletionCallback cc(ReadCompleteCallback, this);
    cc.set_flags(PP_COMPLETIONCALLBACK_FLAG_OPTIONAL);
    int rv = loader_.ReadResponseBody(buffer_, chunk_size_, cc);
    if (rv == PP_OK) {
      cc.Run(rv);
    } else if (rv != PP_OK_COMPLETIONPENDING) {
      Message("Error: ReadMore unexpected rv");
    }
  }

  static void LoadCompleteCallback(void* thiz, int32_t result) {
    ReaderResponseBody* reader = static_cast<ReaderResponseBody*>(thiz);
    ostringstream stream;
    stream << "LoadCompleteCallback: " << result;
    reader->Debug(stream.str());
    pp::URLResponseInfo response_info(reader->loader_.GetResponseInfo());
    CHECK(!response_info.is_null());
    int32_t status_code = response_info.GetStatusCode();
    if (status_code != 200) {
      reader->Message("Error: LoadCompleteCallback unexpected status code");
      return;
    }

    reader->ReadMore();
  }

 public:
  ReaderResponseBody(MyInstance* instance,
                     uint32_t chunk_size,
                     const string& url) :
    chunk_size_(chunk_size),
    buffer_(new char[chunk_size]),
    instance_(instance),
    loader_(instance) {
    pp::URLRequestInfo request(instance_);
    request.SetURL(url);
    request.SetStreamToFile(false);

    pp::CompletionCallback cc(LoadCompleteCallback, this);
    int32_t rv = loader_.Open(request, cc);
    if (rv != PP_OK_COMPLETIONPENDING) {
      Message("Error: ReaderResponseBody: unexpected rv");
    }
  }
};

// Defined here because of circular class visibility issues
bool MyInstance::Init(uint32_t argc, const char* argn[], const char* argv[]) {
  ParseArgs(argc, argn, argv);
  if (stream_to_file_) {
    new ReaderStreamAsFile(this, chunk_size_, url_);
  } else {
    new ReaderResponseBody(this, chunk_size_, url_);
  }
  return true;
}

// standard boilerplate code below
class MyModule : public pp::Module {
 public:
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new MyInstance(instance);
  }
};

namespace pp {
  Module* CreateModule() {
    return new MyModule();
  }
}
