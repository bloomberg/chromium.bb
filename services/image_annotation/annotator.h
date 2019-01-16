// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IMAGE_ANNOTATION_ANNOTATOR_H_
#define SERVICES_IMAGE_ANNOTATION_ANNOTATOR_H_

#include <list>
#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

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
  Annotator(GURL server_url,
            scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
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

  // A map from source ID to URL loader.
  using URLLoaderMap =
      std::map<std::string, std::unique_ptr<network::SimpleURLLoader>>;

  // Removes the given request, reassigning local processing if its associated
  // image processor had some ongoing.
  void RemoveRequestInfo(const std::string& source_id,
                         RequestInfoList::iterator request_info_it,
                         mojom::AnnotateImageError error);

  // Called when a local handler returns compressed image data for the given
  // source ID.
  void OnJpgImageDataReceived(const std::string& source_id,
                              RequestInfoList::iterator request_info_it,
                              const std::vector<uint8_t>& image_bytes);

  // Called when the image annotation server responds with annotations for the
  // given source ID.
  void OnServerResponseReceived(const std::string& source_id,
                                URLLoaderMap::iterator url_loader_it,
                                std::unique_ptr<std::string> json_response);

  // Maps from source ID to previously-obtained OCR result.
  // TODO(crbug.com/916420): periodically clear entries from this cache.
  std::map<std::string, std::string> cached_results_;

  // Maps from source ID to the list of request info (i.e. info of clients that
  // have made requests) for that source.
  std::map<std::string, RequestInfoList> request_infos_;

  // Maps from the source IDs of images currently being locally processed to the
  // ImageProcessors responsible for their processing.
  //
  // The value is a weak pointer to an entry in the RequestInfoList for the
  // given source.
  std::map<std::string, mojom::ImageProcessorPtr*> local_processors_;

  // A map from source ID to the currently-ongoing HTTP request to the image
  // annotation server (if any) for that source.
  URLLoaderMap url_loaders_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  mojo::BindingSet<mojom::Annotator> bindings_;

  const GURL server_url_;

  DISALLOW_COPY_AND_ASSIGN(Annotator);
};

}  // namespace image_annotation

#endif  // SERVICES_IMAGE_ANNOTATION_ANNOTATOR_H_
