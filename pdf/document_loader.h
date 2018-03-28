// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_DOCUMENT_LOADER_H_
#define PDF_DOCUMENT_LOADER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "pdf/chunk_stream.h"
#include "ppapi/utility/completion_callback_factory.h"

namespace pp {
class Instance;
}

namespace chrome_pdf {

class URLLoaderWrapper;

class DocumentLoader {
 public:
  // Number was chosen in crbug.com/78264#c8
  static constexpr uint32_t kDefaultRequestSize = 65536;

  class Client {
   public:
    virtual ~Client() = default;

    // Gets the pp::Instance object.
    virtual pp::Instance* GetPluginInstance() = 0;
    // Creates new URLLoader based on client settings.
    virtual std::unique_ptr<URLLoaderWrapper> CreateURLLoader() = 0;
    // Notification called when all outstanding pending requests are complete.
    virtual void OnPendingRequestComplete() = 0;
    // Notification called when new data is available.
    virtual void OnNewDataReceived() = 0;
    // Notification called when document is fully loaded.
    virtual void OnDocumentComplete() = 0;
    // Notification called when document loading is canceled.
    virtual void OnDocumentCanceled() = 0;
    // Called when initial loader was closed.
    virtual void CancelBrowserDownload() = 0;
  };

  explicit DocumentLoader(Client* client);
  ~DocumentLoader();

  bool Init(std::unique_ptr<URLLoaderWrapper> loader, const std::string& url);

  // Data access interface. Return true if successful.
  bool GetBlock(uint32_t position, uint32_t size, void* buf) const;

  // Data availability interface. Return true if data is available.
  bool IsDataAvailable(uint32_t position, uint32_t size) const;

  // Data request interface.
  void RequestData(uint32_t position, uint32_t size);

  bool IsDocumentComplete() const;
  void SetDocumentSize(uint32_t size);
  uint32_t GetDocumentSize() const;
  uint32_t bytes_received() const { return bytes_received_; }

  // Clear pending requests from the queue.
  void ClearPendingRequests();

  // Exposed for unit tests.
  void SetPartialLoadingEnabled(bool enabled);
  bool is_partial_loader_active() const { return is_partial_loader_active_; }

 private:
  using DataStream = ChunkStream<kDefaultRequestSize>;
  struct Chunk {
    Chunk();
    ~Chunk();

    void Clear();

    uint32_t chunk_index = 0;
    uint32_t data_size = 0;
    std::unique_ptr<DataStream::ChunkData> chunk_data;
  };

  // Called by the completion callback of the document's URLLoader.
  void DidOpenPartial(int32_t result);
  // Call to read data from the document's URLLoader.
  void ReadMore();
  // Called by the completion callback of the document's URLLoader.
  void DidRead(int32_t result);

  bool ShouldCancelLoading() const;
  void ContinueDownload();
  // Called when we complete server request.
  void ReadComplete();

  bool SaveBuffer(char* input, uint32_t input_size);
  void SaveChunkData();

  uint32_t EndOfCurrentChunk() const;

  Client* const client_;
  std::string url_;
  std::unique_ptr<URLLoaderWrapper> loader_;

  pp::CompletionCallbackFactory<DocumentLoader> loader_factory_;

  DataStream chunk_stream_;
  bool partial_loading_enabled_ = true;
  bool is_partial_loader_active_ = false;

  static constexpr uint32_t kReadBufferSize = 256 * 1024;
  char buffer_[kReadBufferSize];

  // The current chunk DocumentLoader is working with.
  Chunk chunk_;

  // In units of Chunks.
  RangeSet pending_requests_;

  uint32_t bytes_received_ = 0;

  DISALLOW_COPY_AND_ASSIGN(DocumentLoader);
};

}  // namespace chrome_pdf

#endif  // PDF_DOCUMENT_LOADER_H_
