// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IMAGE_ANNOTATION_ANNOTATOR_H_
#define SERVICES_IMAGE_ANNOTATION_ANNOTATOR_H_

#include <deque>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/data_decoder/public/mojom/json_parser.mojom.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace image_annotation {

// The annotator communicates with the external image annotation server to
// perform image labeling at the behest of clients.
//
// Clients make requests of the annotator by providing an image "source ID"
// (which is either an image URL or hash of an image data URI) and an associated
// ImageProcessor (the interface through which the annotator can obtain image
// pixels if necessary).
//
// The annotator maintains a cache of previously-computed results, and will
// compute new results either by sending image URLs (for publicly-crawled
// images) or image pixels to the external server.
class Annotator : public mojom::Annotator {
 public:
  // The HTTP request header in which the API key should be transmitted.
  static constexpr char kGoogApiKeyHeader[] = "X-Goog-Api-Key";

  // Constructs an annotator.
  //  |server_url|        : the URL of the server with which the annotator
  //                        communicates. The annotator gracefully handles (i.e.
  //                        returns errors when constructed with) an empty
  //                        server URL.
  //  |api_key|           : the Google API key used to authenticate
  //                        communication with the image annotation server. If
  //                        empty, no API key header will be sent.
  //  |throttle|          : the miminum amount of time to wait between sending
  //                        new HTTP requests to the image annotation server.
  //  |batch_size|        : The maximum number of image annotation requests that
  //                        should be batched into a single request to the
  //                        server.
  //  |min_ocr_confidence|: The minimum confidence value needed to return an OCR
  //                        result.
  Annotator(GURL server_url,
            std::string api_key,
            base::TimeDelta throttle,
            int batch_size,
            double min_ocr_confidence,
            scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
            service_manager::Connector* connector);
  ~Annotator() override;

  // Start providing behavior for the given Mojo request.
  void BindRequest(mojom::AnnotatorRequest request);

  // mojom::Annotator:
  void AnnotateImage(const std::string& source_id,
                     mojom::ImageProcessorPtr image,
                     AnnotateImageCallback callback) override;

 private:
  // A list of the information associated with each request made by a client
  // (i.e. a Chrome component) to this service. Each entry contains:
  //  1) the ImageProcessor to use for local processing, and
  //  2) the callback to execute when request processing has finished.
  using RequestInfoList =
      std::list<std::pair<mojom::ImageProcessorPtr, AnnotateImageCallback>>;

  // List of URL loader objects.
  using UrlLoaderList = std::list<std::unique_ptr<network::SimpleURLLoader>>;

  // A queue of the data needed to make HTTP requests to the image annotation
  // server. Each entry is a (source ID, image bytes) pair.
  using HttpRequestQueue =
      std::deque<std::pair<std::string, std::vector<uint8_t>>>;

  // Constructs and returns a JSON object containing an request for the
  // given images.
  static std::string FormatJsonRequest(HttpRequestQueue::iterator begin_it,
                                       HttpRequestQueue::iterator end_it);

  // Creates a URL loader that calls the image annotation server with an
  // annotation request for the given images.
  static std::unique_ptr<network::SimpleURLLoader> MakeRequestLoader(
      const GURL& server_url,
      const std::string& api_key,
      HttpRequestQueue::iterator begin_it,
      HttpRequestQueue::iterator end_it);

  // Create or reuse a connection to the data decoder service for safe JSON
  // parsing.
  data_decoder::mojom::JsonParser& GetJsonParser();

  // Removes the given request, reassigning local processing if its associated
  // image processor had some ongoing.
  void RemoveRequestInfo(const std::string& source_id,
                         RequestInfoList::iterator request_info_it,
                         bool canceled);

  // Called when a local handler returns compressed image data for the given
  // source ID.
  void OnJpgImageDataReceived(const std::string& source_id,
                              RequestInfoList::iterator request_info_it,
                              const std::vector<uint8_t>& image_bytes);

  // Called periodically to send the next batch of requests to the image
  // annotation server.
  void SendRequestBatchToServer();

  // Called when the image annotation server responds with annotations for the
  // given source IDs.
  void OnServerResponseReceived(const std::set<std::string>& source_ids,
                                UrlLoaderList::iterator http_request_it,
                                std::unique_ptr<std::string> json_response);

  // Called when the data decoder service provides parsed JSON data for a server
  // response.
  void OnResponseJsonParsed(const std::set<std::string>& source_ids,
                            base::Optional<base::Value> json_data,
                            const base::Optional<std::string>& error);

  // Adds the given results to the cache (if successful) and notifies clients.
  void ProcessResults(
      const std::set<std::string>& source_ids,
      const std::map<std::string, mojom::AnnotateImageResultPtr>& results);

  // Maps from source ID to previously-obtained annotation results.
  // TODO(crbug.com/916420): periodically clear entries from this cache.
  std::map<std::string, mojom::AnnotateImageResultPtr> cached_results_;

  // Maps from source ID to the list of request info (i.e. info of clients that
  // have made requests) for that source.
  std::map<std::string, RequestInfoList> request_infos_;

  // Maps from the source IDs of images currently being locally processed to the
  // ImageProcessors responsible for their processing.
  //
  // The value is a weak pointer to an entry in the RequestInfoList for the
  // given source.
  std::map<std::string, mojom::ImageProcessorPtr*> local_processors_;

  // A list of currently-ongoing HTTP requests to the image annotation server.
  UrlLoaderList http_requests_;

  // A queue of HTTP requests waiting to be made.
  HttpRequestQueue http_request_queue_;

  // The set of source IDs for which an HTTP request is either queued or
  // currently ongoing.
  std::set<std::string> pending_source_ids_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  service_manager::Connector* const connector_;

  mojo::BindingSet<mojom::Annotator> bindings_;

  // Should not be used directly; GetJsonParser() should be called instead.
  data_decoder::mojom::JsonParserPtr json_parser_;

  // A timer used to throttle HTTP request frequency.
  base::RepeatingTimer http_request_timer_;

  const GURL server_url_;

  const std::string api_key_;

  const int batch_size_;

  const double min_ocr_confidence_;

  DISALLOW_COPY_AND_ASSIGN(Annotator);
};

}  // namespace image_annotation

#endif  // SERVICES_IMAGE_ANNOTATION_ANNOTATOR_H_
