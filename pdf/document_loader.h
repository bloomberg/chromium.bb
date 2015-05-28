// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_DOCUMENT_LOADER_H_
#define PDF_DOCUMENT_LOADER_H_

#include <list>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "pdf/chunk_stream.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/utility/completion_callback_factory.h"

#define kDefaultRequestSize 32768u

namespace chrome_pdf {

class DocumentLoader {
 public:
  class Client {
   public:
    virtual ~Client();

    // Gets the pp::Instance object.
    virtual pp::Instance* GetPluginInstance() = 0;
    // Creates new URLLoader based on client settings.
    virtual pp::URLLoader CreateURLLoader() = 0;
    // Notification called when partial information about document is available.
    // Only called for urls that returns full content size and supports byte
    // range requests.
    virtual void OnPartialDocumentLoaded() = 0;
    // Notification called when all outstanding pending requests are complete.
    virtual void OnPendingRequestComplete() = 0;
    // Notification called when new data is available.
    virtual void OnNewDataAvailable() = 0;
    // Notification called when document is fully loaded.
    virtual void OnDocumentComplete() = 0;
  };

  explicit DocumentLoader(Client* client);
  ~DocumentLoader();

  bool Init(const pp::URLLoader& loader,
            const std::string& url,
            const std::string& headers);

  // Data access interface. Return true is sucessful.
  bool GetBlock(uint32_t position, uint32_t size, void* buf) const;

  // Data availability interface. Return true data avaialble.
  bool IsDataAvailable(uint32_t position, uint32_t size) const;

  // Data availability interface. Return true data avaialble.
  void RequestData(uint32_t position, uint32_t size);

  bool IsDocumentComplete() const;
  uint32_t document_size() const { return document_size_; }

  // Return number of bytes available.
  uint32_t GetAvailableData() const;

  // Clear pending requests from the queue.
  void ClearPendingRequests();

  bool is_partial_document() const { return partial_document_; }

 private:
  // Called by the completion callback of the document's URLLoader.
  void DidOpen(int32_t result);
  // Call to read data from the document's URLLoader.
  void ReadMore();
  // Called by the completion callback of the document's URLLoader.
  void DidRead(int32_t result);

  // Called when we detect that partial document load is possible.
  void LoadPartialDocument();
  // Called when we have to load full document.
  void LoadFullDocument();
  // Download pending requests.
  void DownloadPendingRequests();
  // Called when we complete server request and read all data from it.
  void ReadComplete();
  // Creates request to download size byte of data data starting from position.
  pp::URLRequestInfo GetRequest(uint32_t position, uint32_t size) const;
  // Returns current request size in bytes.
  uint32_t GetRequestSize() const;

  Client* client_;
  std::string url_;
  pp::URLLoader loader_;
  pp::CompletionCallbackFactory<DocumentLoader> loader_factory_;
  ChunkStream chunk_stream_;
  bool partial_document_;
  bool request_pending_;
  typedef std::list<std::pair<size_t, size_t> > PendingRequests;
  PendingRequests pending_requests_;
  char buffer_[kDefaultRequestSize];
  uint32_t current_pos_;
  uint32_t current_chunk_size_;
  uint32_t current_chunk_read_;
  uint32_t document_size_;
  bool header_request_;
  bool is_multipart_;
  std::string multipart_boundary_;
  uint32_t requests_count_;
  std::list<std::vector<unsigned char> > chunk_buffer_;
};

}  // namespace chrome_pdf

#endif  // PDF_DOCUMENT_LOADER_H_
