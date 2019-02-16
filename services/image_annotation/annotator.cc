// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/image_annotation/annotator.h"

#include <algorithm>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace image_annotation {

namespace {

constexpr size_t kMaxResponseSize = 1024 * 1024;  // 1MB.

// The server returns separate OCR results for each region of the image; we
// naively concatenate these into one response string.
//
// Returns nullopt if there is any unexpected structure to the annotations
// message.
base::Optional<std::string> ParseJsonOcrAnnotation(
    const base::Value& ocr_engine,
    const double min_ocr_confidence) {
  const base::Value* const ocr_regions = ocr_engine.FindKey("ocrRegions");
  // No OCR regions is valid - it just means there is no text.
  if (!ocr_regions)
    return std::string();

  if (!ocr_regions->is_list())
    return base::nullopt;

  std::string all_ocr_text;
  for (const base::Value& ocr_region : ocr_regions->GetList()) {
    if (!ocr_region.is_dict())
      return base::nullopt;

    const base::Value* const words = ocr_region.FindKey("words");
    if (!words || !words->is_list())
      return base::nullopt;

    std::string region_ocr_text;
    for (const base::Value& word : words->GetList()) {
      if (!word.is_dict())
        return base::nullopt;

      const base::Value* const detected_text = word.FindKey("detectedText");
      if (!detected_text || !detected_text->is_string())
        return base::nullopt;

      // A confidence value of 0 or 1 is interpreted as an int and not a double.
      const base::Value* const confidence = word.FindKey("confidenceScore");
      if (!confidence || (!confidence->is_double() && !confidence->is_int()) ||
          confidence->GetDouble() < 0.0 || confidence->GetDouble() > 1.0)
        return base::nullopt;

      if (confidence->GetDouble() < min_ocr_confidence)
        continue;

      const std::string& detected_text_str = detected_text->GetString();

      if (!region_ocr_text.empty() && !detected_text_str.empty())
        region_ocr_text += " ";
      region_ocr_text += detected_text_str;
    }

    if (!all_ocr_text.empty() && !region_ocr_text.empty())
      all_ocr_text += "\n";
    all_ocr_text += region_ocr_text;
  }

  return all_ocr_text;
}

// Attempts to extract OCR results from the server response, returning a map
// from each source ID to its OCR text (if successfully extracted).
std::map<std::string, std::string> ParseJsonOcrResponse(
    const std::string* const json_response,
    const double min_ocr_confidence) {
  if (!json_response)
    return {};

  const std::unique_ptr<base::Value> response =
      base::JSONReader::ReadDeprecated(*json_response);
  if (!response || !response->is_dict())
    return {};

  const base::Value* const results = response->FindKey("results");
  if (!results || !results->is_list())
    return {};

  std::map<std::string, std::string> out;
  for (const base::Value& result : results->GetList()) {
    if (!result.is_dict())
      continue;

    const base::Value* const image_id = result.FindKey("imageId");
    if (!image_id || !image_id->is_string())
      continue;

    const base::Value* const engine_results = result.FindKey("engineResults");
    if (!engine_results || !engine_results->is_list() ||
        engine_results->GetList().size() != 1)
      continue;

    const base::Value& engine_result = engine_results->GetList()[0];
    if (!engine_result.is_dict())
      continue;

    const base::Value* const ocr_engine = engine_result.FindKey("ocrEngine");
    if (!ocr_engine || !ocr_engine->is_dict())
      continue;

    const base::Optional<std::string> ocr_text =
        ParseJsonOcrAnnotation(*ocr_engine, min_ocr_confidence);
    if (!ocr_text.has_value())
      continue;

    out[image_id->GetString()] = *ocr_text;
  }

  return out;
}

}  // namespace

Annotator::Annotator(
    GURL server_url,
    const base::TimeDelta throttle,
    const int batch_size,
    const double min_ocr_confidence,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)),
      http_request_timer_(
          FROM_HERE,
          throttle,
          base::BindRepeating(&Annotator::SendRequestBatchToServer,
                              base::Unretained(this))),
      server_url_(std::move(server_url)),
      batch_size_(batch_size),
      min_ocr_confidence_(min_ocr_confidence) {}

Annotator::~Annotator() {}

void Annotator::BindRequest(mojom::AnnotatorRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void Annotator::AnnotateImage(const std::string& source_id,
                              mojom::ImageProcessorPtr image_processor,
                              AnnotateImageCallback callback) {
  // Return cached results if they exist.
  const auto cache_lookup = cached_results_.find(source_id);
  if (cache_lookup != cached_results_.end()) {
    std::move(callback).Run(
        mojom::AnnotateImageResult::NewOcrText(cache_lookup->second));
    return;
  }

  // Register the ImageProcessor and callback to be used for this request.
  RequestInfoList& request_info_list = request_infos_[source_id];
  request_info_list.push_back(
      {std::move(image_processor), std::move(callback)});

  // If the image processor dies: automatically delete the request info and
  // reassign local processing (for other interested clients) if the dead image
  // processor was responsible for some ongoing work.
  request_info_list.back().first.set_connection_error_handler(base::BindOnce(
      &Annotator::RemoveRequestInfo, base::Unretained(this), source_id,
      --request_info_list.end(), mojom::AnnotateImageError::kCanceled));

  // Don't start local work if it would duplicate some ongoing or already-
  // completed work.
  if (base::ContainsKey(local_processors_, source_id) ||
      base::ContainsKey(pending_source_ids_, source_id))
    return;

  local_processors_.insert(
      std::make_pair(source_id, &request_info_list.back().first));

  // TODO(crbug.com/916420): first query the public result cache by URL to
  // improve latency.

  request_info_list.back().first->GetJpgImageData(
      base::BindOnce(&Annotator::OnJpgImageDataReceived, base::Unretained(this),
                     source_id, --request_info_list.end()));
}

// static
std::string Annotator::FormatJsonOcrRequest(
    const HttpRequestQueue::iterator begin_it,
    const HttpRequestQueue::iterator end_it) {
  base::Value image_request_list(base::Value::Type::LIST);
  for (HttpRequestQueue::iterator it = begin_it; it != end_it; ++it) {
    // Re-encode image bytes into base64, which can be represented in JSON.
    std::string base64_data;
    Base64Encode(
        base::StringPiece(reinterpret_cast<const char*>(it->second.data()),
                          it->second.size()),
        &base64_data);

    // TODO(crbug.com/916420): accept and propagate page language info to
    //                         improve OCR accuracy.
    base::Value engine_params(base::Value::Type::DICTIONARY);
    engine_params.SetKey("ocrParameters",
                         base::Value(base::Value::Type::DICTIONARY));

    base::Value engine_params_list(base::Value::Type::LIST);
    engine_params_list.GetList().push_back(std::move(engine_params));

    base::Value image_request(base::Value::Type::DICTIONARY);
    image_request.SetKey("imageId", base::Value(it->first));
    image_request.SetKey("imageBytes", base::Value(std::move(base64_data)));
    image_request.SetKey("engineParameters", std::move(engine_params_list));

    image_request_list.GetList().push_back(std::move(image_request));
  }

  base::Value request(base::Value::Type::DICTIONARY);
  request.SetKey("imageRequests", std::move(image_request_list));

  std::string json_request;
  base::JSONWriter::Write(request, &json_request);

  return json_request;
}

// static
std::unique_ptr<network::SimpleURLLoader> Annotator::MakeOcrRequestLoader(
    const GURL& server_url,
    const HttpRequestQueue::iterator begin_it,
    const HttpRequestQueue::iterator end_it) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "POST";

  // TODO(crbug.com/916420): accept and pass API key when the server is
  //                         configured to require it.
  resource_request->url = server_url;

  resource_request->load_flags = net::LOAD_DO_NOT_SAVE_COOKIES |
                                 net::LOAD_DO_NOT_SEND_COOKIES |
                                 net::LOAD_DO_NOT_SEND_AUTH_DATA;

  // TODO(crbug.com/916420): update this annotation to be more general and to
  //                         reflect specfics of the UI when it is implemented.
  const net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("image_annotation", R"(
        semantics {
          sender: "Image Annotation"
          description:
            "Chrome can identify text inside images and provide this text to "
            "screen readers (for visually-impaired users) by sending images to "
            "Google's servers. If image text extraction is enabled for a page, "
            "Chrome will send the URLs and pixels of all images on the "
            "page to Google's servers, which will return any textual content "
            "identified inside the images. This content is made accessible to "
            "screen reading software."
          trigger: "A page containing images is loaded for a user who has "
                   "automatic image text extraction enabled."
          data: "Image pixels and URLs. No user identifier is sent along with "
                "the data."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "You can enable or disable this feature via the context menu "
            "for images, or via 'Image Labeling' in Chrome's settings under "
            "Accessibility. This feature is disabled by default."
          policy_exception_justification: "Policy to come; feature not yet "
                                          "complete."
        })");

  auto url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);

  url_loader->AttachStringForUpload(FormatJsonOcrRequest(begin_it, end_it),
                                    "application/json");

  return url_loader;
}

void Annotator::OnJpgImageDataReceived(
    const std::string& source_id,
    const RequestInfoList::iterator request_info_it,
    const std::vector<uint8_t>& image_bytes) {
  // Failed to retrieve bytes from local processor; remove dead processor and
  // reschedule processing.
  if (image_bytes.empty()) {
    RemoveRequestInfo(source_id, request_info_it,
                      mojom::AnnotateImageError::kFailure);
    return;
  }

  // Local processing is no longer ongoing.
  local_processors_.erase(source_id);

  // Schedule an HTTP request for this image.
  http_request_queue_.push_back({source_id, image_bytes});
  pending_source_ids_.insert(source_id);

  // Start sending batches to the server.
  if (!http_request_timer_.IsRunning())
    http_request_timer_.Reset();
}

void Annotator::SendRequestBatchToServer() {
  if (http_request_queue_.empty()) {
    http_request_timer_.Stop();
    return;
  }

  // Take last n elements (or all elements if there are less than n).
  const auto begin_it =
      http_request_queue_.end() -
      std::min<size_t>(http_request_queue_.size(), batch_size_);
  const auto end_it = http_request_queue_.end();

  // The set of source IDs relevant for this request.
  std::set<std::string> source_ids;
  for (HttpRequestQueue::iterator it = begin_it; it != end_it; it++) {
    source_ids.insert(it->first);
  }

  // Kick off server communication.
  http_requests_.push_back(MakeOcrRequestLoader(server_url_, begin_it, end_it));
  http_requests_.back()->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&Annotator::OnServerResponseReceived,
                     base::Unretained(this), source_ids,
                     --http_requests_.end()),
      kMaxResponseSize);

  http_request_queue_.erase(begin_it, end_it);
}

void Annotator::OnServerResponseReceived(
    const std::set<std::string>& source_ids,
    const UrlLoaderList::iterator http_request_it,
    const std::unique_ptr<std::string> json_response) {
  http_requests_.erase(http_request_it);

  // Extract OCR results for each source ID with valid results.
  const std::map<std::string, std::string> ocr_results =
      ParseJsonOcrResponse(json_response.get(), min_ocr_confidence_);

  // Process each source ID for which we expect to have results.
  for (const std::string& source_id : source_ids) {
    pending_source_ids_.erase(source_id);

    // The lookup will be successful if there is a valid result (i.e. not an
    // error and not a malformed result) for this source ID.
    const auto result_lookup = ocr_results.find(source_id);

    if (result_lookup != ocr_results.end())
      cached_results_.insert({source_id, result_lookup->second});

    // This should not happen, since only this method removes entries of
    // |request_infos_|, and this method should only execute once per source ID.
    const auto request_info_it = request_infos_.find(source_id);
    if (request_info_it == request_infos_.end())
      continue;

    // Notify clients of success or failure.
    // TODO(crbug.com/916420): explore server retry strategies.
    const auto result =
        result_lookup != ocr_results.end()
            ? mojom::AnnotateImageResult::NewOcrText(result_lookup->second)
            : mojom::AnnotateImageResult::NewErrorCode(
                  mojom::AnnotateImageError::kFailure);
    for (auto& info : request_info_it->second) {
      std::move(info.second).Run(result.Clone());
    }
    request_infos_.erase(request_info_it);
  }
}

void Annotator::RemoveRequestInfo(
    const std::string& source_id,
    const RequestInfoList::iterator request_info_it,
    const mojom::AnnotateImageError error) {
  // Check whether we are deleting the ImageProcessor responsible for current
  // local processing.
  auto lookup = local_processors_.find(source_id);
  const bool should_reassign = lookup != local_processors_.end() &&
                               lookup->second == &request_info_it->first;

  // Notify client of cancellation / failure.
  std::move(request_info_it->second)
      .Run(mojom::AnnotateImageResult::NewErrorCode(error));

  // Delete the specified ImageProcessor.
  RequestInfoList& request_info_list = request_infos_[source_id];
  request_info_list.erase(request_info_it);

  // If necessary, reassign local processing.
  if (!request_info_list.empty() && should_reassign) {
    lookup->second = &request_info_list.front().first;

    request_info_list.front().first->GetJpgImageData(base::BindOnce(
        &Annotator::OnJpgImageDataReceived, base::Unretained(this), source_id,
        request_info_list.begin()));
  }
}

}  // namespace image_annotation
