// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_FLIP_SERVER_MEM_CACHE_H_
#define NET_TOOLS_FLIP_SERVER_MEM_CACHE_H_

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "net/tools/flip_server/balsa_headers.h"
#include "net/tools/flip_server/balsa_visitor_interface.h"
#include "net/tools/flip_server/constants.h"

namespace net {

class StoreBodyAndHeadersVisitor: public BalsaVisitorInterface {
 public:
  void HandleError() { error_ = true; }

  // BalsaVisitorInterface:
  virtual void ProcessBodyInput(const char *input, size_t size) OVERRIDE {}
  virtual void ProcessBodyData(const char *input, size_t size) OVERRIDE;
  virtual void ProcessHeaderInput(const char *input, size_t size) OVERRIDE {}
  virtual void ProcessTrailerInput(const char *input, size_t size) OVERRIDE {}
  virtual void ProcessHeaders(const BalsaHeaders& headers) OVERRIDE {
    // nothing to do here-- we're assuming that the BalsaFrame has
    // been handed our headers.
  }
  virtual void ProcessRequestFirstLine(const char* line_input,
                                       size_t line_length,
                                       const char* method_input,
                                       size_t method_length,
                                       const char* request_uri_input,
                                       size_t request_uri_length,
                                       const char* version_input,
                                       size_t version_length) OVERRIDE {}
  virtual void ProcessResponseFirstLine(const char *line_input,
                                        size_t line_length,
                                        const char *version_input,
                                        size_t version_length,
                                        const char *status_input,
                                        size_t status_length,
                                        const char *reason_input,
                                        size_t reason_length) OVERRIDE {}
  virtual void ProcessChunkLength(size_t chunk_length) OVERRIDE {}
  virtual void ProcessChunkExtensions(const char *input,
                                      size_t size) OVERRIDE {}
  virtual void HeaderDone() OVERRIDE {}
  virtual void MessageDone() OVERRIDE {}
  virtual void HandleHeaderError(BalsaFrame* framer) OVERRIDE;
  virtual void HandleHeaderWarning(BalsaFrame* framer) OVERRIDE;
  virtual void HandleChunkingError(BalsaFrame* framer) OVERRIDE;
  virtual void HandleBodyError(BalsaFrame* framer) OVERRIDE;

  BalsaHeaders headers;
  std::string body;
  bool error_;
};

////////////////////////////////////////////////////////////////////////////////

struct FileData {
  FileData();
  FileData(BalsaHeaders* h, const std::string& b);
  ~FileData();
  void CopyFrom(const FileData& file_data);

  BalsaHeaders* headers;
  std::string filename;
  // priority, filename
  std::vector< std::pair<int, std::string> > related_files;
  std::string body;
};

////////////////////////////////////////////////////////////////////////////////

class MemCacheIter {
 public:
  MemCacheIter() :
      file_data(NULL),
      priority(0),
      transformed_header(false),
      body_bytes_consumed(0),
      stream_id(0),
      max_segment_size(kInitialDataSendersThreshold),
      bytes_sent(0) {}
  explicit MemCacheIter(FileData* fd) :
      file_data(fd),
      priority(0),
      transformed_header(false),
      body_bytes_consumed(0),
      stream_id(0),
      max_segment_size(kInitialDataSendersThreshold),
      bytes_sent(0) {}
  FileData* file_data;
  int priority;
  bool transformed_header;
  size_t body_bytes_consumed;
  uint32 stream_id;
  uint32 max_segment_size;
  size_t bytes_sent;
};

////////////////////////////////////////////////////////////////////////////////

class MemoryCache {
 public:
  typedef std::map<std::string, FileData> Files;

 public:
  MemoryCache();
  ~MemoryCache();

  void CloneFrom(const MemoryCache& mc);

  void AddFiles();

  void ReadToString(const char* filename, std::string* output);

  void ReadAndStoreFileContents(const char* filename);

  FileData* GetFileData(const std::string& filename);

  bool AssignFileData(const std::string& filename, MemCacheIter* mci);

  Files files_;
  std::string cwd_;
};

class NotifierInterface {
 public:
  virtual ~NotifierInterface() {}
  virtual void Notify() = 0;
};

}  // namespace net

#endif  // NET_TOOLS_FLIP_SERVER_MEM_CACHE_H_

