// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_UPDATED_SCRIPT_LOADER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_UPDATED_SCRIPT_LOADER_H_

#include "base/macros.h"
#include "content/browser/service_worker/service_worker_cache_writer.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/net_adapters.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "url/gurl.h"

namespace blink {
class ThrottlingURLLoader;
}  // namespace blink

namespace content {

class BrowserContext;
class ServiceWorkerVersion;

// Used only for ServiceWorkerImportedScriptUpdateCheck.
//
// This is the URLLoader used for loading scripts for a new (installing) service
// worker. This is used for a script which has an update on the script during
// update checking in the browser process. Also used when the request was a
// network error during the update checking so that the same network error can
// be observed at the initial script evaluation in the renderer.
//
// This loader works as follows:
//   1. The ServiceWorkerCacheWriter used in the update check is resumed. After
//      that, the writer starts to make a new resource by copying the header and
//      body which has already been provided by the update checker. This loader
//      observes the copy (WillWriteInfo()/WillWriteData()), and sends the data
//      to |client_|.
//   2. After the copy has done, it resumes the network load. The rest of the
//      script body is responded to the |client_| and also written to service
//      worker storage.
//   3. When OnComplete() is called and the Mojo data pipe for the script body
//      is closed, it calls CommitCompleted() and closes the connections with
//      the network service and the renderer process.
//
// NOTE: To perform the network request, this class uses |loader_factory_| which
// may internally use a non-NetworkService factory if URL has a non-http(s)
// scheme, e.g., a chrome-extension:// URL. Regardless, that is still called a
// "network" request in comments and naming. "network" is meant to distinguish
// from the load this URLLoader does for its client:
//     "network" <------> SWUpdatedScriptLoader <------> client
class CONTENT_EXPORT ServiceWorkerUpdatedScriptLoader final
    : public network::mojom::URLLoader,
      public network::mojom::URLLoaderClient,
      public ServiceWorkerCacheWriter::WriteObserver {
 public:
  enum class LoaderState {
    kNotStarted,
    kLoadingHeader,
    kWaitingForBody,
    kLoadingBody,
    kCompleted,
  };

  enum class WriterState { kNotStarted, kWriting, kCompleted };

  using BrowserContextGetter = base::RepeatingCallback<BrowserContext*(void)>;

  // A wrapper to use ThrottlingURLLoader on the core thread.
  // TODO(crbug.com/824858): Remove this once core is moved to UI thread.
  class ThrottlingURLLoaderCoreWrapper {
   public:
    // Creates a ThrottlingURLLoader and starts the request.
    // Called on the core thread.
    static std::unique_ptr<ThrottlingURLLoaderCoreWrapper> CreateLoaderAndStart(
        std::unique_ptr<network::PendingSharedURLLoaderFactory>
            pending_loader_factory,
        BrowserContextGetter browser_context_getter,
        int32_t routing_id,
        int32_t request_id,
        uint32_t options,
        const network::ResourceRequest& resource_request,
        mojo::PendingRemote<network::mojom::URLLoaderClient> client,
        const net::NetworkTrafficAnnotationTag& traffic_annotation);

    // Called on the core thread.
    void SetPriority(net::RequestPriority priority,
                     int32_t intra_priority_value);
    void PauseReadingBodyFromNet();
    void ResumeReadingBodyFromNet();

    ~ThrottlingURLLoaderCoreWrapper();

   private:
    ThrottlingURLLoaderCoreWrapper();

    // The real loader to be used in ThrottlingURLLoaderCoreWrapper.
    // Created and deleted on the UI thread via BrowserThread::DeleteOnUIThread
    // to ensure the order of posted tasks and destruction of this instance.
    struct LoaderOnUI {
      LoaderOnUI();
      ~LoaderOnUI();

      std::unique_ptr<blink::ThrottlingURLLoader> loader;
      mojo::Remote<network::mojom::URLLoaderClient> client;
    };

    static void StartInternalOnUI(
        std::unique_ptr<network::PendingSharedURLLoaderFactory>
            pending_loader_factory,
        BrowserContextGetter browser_context_getter,
        int32_t routing_id,
        int32_t request_id,
        uint32_t options,
        network::ResourceRequest resource_request,
        mojo::PendingRemote<network::mojom::URLLoaderClient> client,
        net::NetworkTrafficAnnotationTag traffic_annotation,
        LoaderOnUI* loader_on_ui);

    std::unique_ptr<LoaderOnUI, BrowserThread::DeleteOnUIThread> loader_on_ui_;
  };

  // Creates a loader to continue downloading of a script paused during update
  // check.
  static std::unique_ptr<ServiceWorkerUpdatedScriptLoader> CreateAndStart(
      uint32_t options,
      const network::ResourceRequest& original_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      scoped_refptr<ServiceWorkerVersion> version);

  ~ServiceWorkerUpdatedScriptLoader() override;

  // network::mojom::URLLoader:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const base::Optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  // network::mojom::URLLoaderClient for the network load:
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr response_head) override;
  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr response_head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override;
  void OnReceiveCachedMetadata(mojo_base::BigBuffer data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  // Implements ServiceWorkerCacheWriter::WriteObserver.
  int WillWriteResponseHead(
      const network::mojom::URLResponseHead& response_head) override;
  int WillWriteData(scoped_refptr<net::IOBuffer> data,
                    int length,
                    base::OnceCallback<void(net::Error)> callback) override;

  // Buffer size for reading script data from network.
  const static uint32_t kReadBufferSize;

 private:
  class WrappedIOBuffer;

  ServiceWorkerUpdatedScriptLoader(
      uint32_t options,
      const network::ResourceRequest& original_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      scoped_refptr<ServiceWorkerVersion> version);

  // Called when |network_consumer_| is ready to be read. Can be called multiple
  // times.
  void OnNetworkDataAvailable(MojoResult);

  // Writes the given data into the service worker script storage.
  void WriteData(scoped_refptr<network::MojoToNetPendingBuffer> pending_buffer,
                 uint32_t bytes_available);
  void OnWriteDataComplete(
      scoped_refptr<network::MojoToNetPendingBuffer> pending_buffer,
      uint32_t bytes_written,
      net::Error error);

  // This is the last method that is called on this class. Notifies the final
  // result to |client_| and clears all mojo connections etc.
  void CommitCompleted(const network::URLLoaderCompletionStatus& status,
                       const std::string& status_message);

  // Called when |client_producer_| is writable. It writes |data_to_send_|
  // to |client_producer_|. If all data is written, the observer has completed
  // its work and |write_observer_complete_callback_| is called. Otherwise,
  // |client_producer_watcher_| is armed to wait for |client_producer_| to be
  // writable again.
  void OnClientWritable(MojoResult);

  // Called when ServiceWorkerCacheWriter::Resume() completes its work.
  // If not all data are received, it continues to download from network.
  void OnCacheWriterResumed(
      scoped_refptr<network::MojoToNetPendingBuffer> pending_network_buffer,
      uint32_t consumed_bytes,
      net::Error error);

  const GURL request_url_;

  // This is network::mojom::RequestDestination::kServiceWorker for the main
  // script or network::mojom::RequestDestination::kScript for an imported
  // script.
  const network::mojom::RequestDestination request_destination_;

  // Loader options to pass to the network loader.
  const uint32_t options_;

  scoped_refptr<ServiceWorkerVersion> version_;

  std::unique_ptr<ServiceWorkerCacheWriter> cache_writer_;

  // Used for fetching the script from network (or other loaders like extensions
  // sometimes).
  std::unique_ptr<ThrottlingURLLoaderCoreWrapper> network_loader_;

  mojo::Receiver<network::mojom::URLLoaderClient> network_client_receiver_{
      this};
  mojo::ScopedDataPipeConsumerHandle network_consumer_;
  mojo::SimpleWatcher network_watcher_;

  // Used for responding with the fetched script to this loader's client.
  mojo::Remote<network::mojom::URLLoaderClient> client_;
  mojo::ScopedDataPipeProducerHandle client_producer_;

  // Represents the state of |network_loader_|.
  // Corresponds to the steps of calls as a URLLoaderClient.
  //
  // CreateLoaderAndStart(): kNotStarted -> kLoadingHeader
  // OnReceiveResponse(): kLoadingHeader -> kWaitingForBody
  // OnStartLoadingResponseBody(): kWaitingForBody -> kLoadingBody
  // OnComplete(): kLoadingBody -> kCompleted
  //
  // When this loader is created, the state should be kLoadingBody or kCompleted
  // because the update checking pauses the cache writer during loading the
  // body for byte-for-byte comparison.
  LoaderState network_loader_state_ = LoaderState::kNotStarted;

  // State of the cache writer.
  // kNotStarted: not started to write the body.
  // kWriting: writing the body into the cache writer.
  // kCompleted: all body has been written into the cache writer.
  //
  // When this loader is created, |body_writer_state_| should be kWriting or
  // kCompleted because the response body should have started to be read during
  // update checking.
  WriterState body_writer_state_ = WriterState::kNotStarted;

  mojo::SimpleWatcher client_producer_watcher_;
  const base::TimeTicks request_start_;
  mojo::PendingReceiver<network::mojom::URLLoaderClient>
      pending_network_client_receiver_;

  // This is the data notified by OnBeforeWriteData() which would be sent
  // to |client_|.
  scoped_refptr<net::IOBuffer> data_to_send_;

  // Length of |data_to_send_| in bytes.
  int data_length_ = 0;

  // Length of data in |data_to_send_| already sent to |client_|.
  int bytes_sent_to_client_ = 0;

  // Run this to notify ServiceWorkerCacheWriter that the observer completed
  // its work. net::OK means all |data_to_send_| has been sent to |client_|.
  base::OnceCallback<void(net::Error)> write_observer_complete_callback_;

  base::WeakPtrFactory<ServiceWorkerUpdatedScriptLoader> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerUpdatedScriptLoader);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_UPDATED_SCRIPT_LOADER_H_
