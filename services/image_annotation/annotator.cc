// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/image_annotation/annotator.h"

#include <algorithm>
#include <tuple>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/stl_util.h"
#include "components/google/core/common/google_util.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "url/gurl.h"

namespace image_annotation {

namespace {

constexpr size_t kMaxResponseSize = 1024 * 1024;  // 1MB.

// The server returns separate OCR results for each region of the image; we
// naively concatenate these into one response string.
//
// Returns a null pointer if there is any unexpected structure to the
// annotations message.
mojom::AnnotationPtr ParseJsonOcrAnnotation(const base::Value& ocr_engine,
                                            const double min_ocr_confidence) {
  // No OCR regions is valid - it just means there is no text.
  const base::Value* const ocr_regions = ocr_engine.FindKey("ocrRegions");
  if (!ocr_regions)
    return mojom::Annotation::New(mojom::AnnotationType::kOcr, 1.0 /* score */,
                                  std::string() /* text */);

  if (!ocr_regions->is_list())
    return mojom::AnnotationPtr(nullptr);

  std::string all_ocr_text;
  int word_count = 0;
  double word_confidence_sum = 0.0;
  for (const base::Value& ocr_region : ocr_regions->GetList()) {
    if (!ocr_region.is_dict())
      continue;

    const base::Value* const words = ocr_region.FindKey("words");
    if (!words || !words->is_list())
      continue;

    std::string region_ocr_text;
    for (const base::Value& word : words->GetList()) {
      if (!word.is_dict())
        continue;

      const base::Value* const detected_text = word.FindKey("detectedText");
      if (!detected_text || !detected_text->is_string())
        continue;

      // A confidence value of 0 or 1 is interpreted as an int and not a double.
      const base::Value* const confidence = word.FindKey("confidenceScore");
      if (!confidence || (!confidence->is_double() && !confidence->is_int()) ||
          confidence->GetDouble() < 0.0 || confidence->GetDouble() > 1.0)
        continue;

      if (confidence->GetDouble() < min_ocr_confidence)
        continue;

      const std::string& detected_text_str = detected_text->GetString();

      if (detected_text_str.empty())
        continue;

      if (!region_ocr_text.empty())
        region_ocr_text += " ";

      region_ocr_text += detected_text_str;
      ++word_count;
      word_confidence_sum += confidence->GetDouble();
    }

    if (!all_ocr_text.empty() && !region_ocr_text.empty())
      all_ocr_text += "\n";
    all_ocr_text += region_ocr_text;
  }

  return mojom::Annotation::New(
      mojom::AnnotationType::kOcr,
      word_count == 0 ? 1.0 : word_confidence_sum / word_count, all_ocr_text);
}

// Extracts annotations from the given description engine result into the second
// element of the return tuple.
//
// The first element of the return tuple will be true if the image was
// classified as containing adult content.
std::tuple<bool, std::vector<mojom::AnnotationPtr>> ParseJsonDescAnnotations(
    const base::Value& desc_engine) {
  constexpr char kAdultFailureReason[] = "ADULT";
  const std::map<std::string, mojom::AnnotationType> annotation_types = {
      {"OCR", mojom::AnnotationType::kOcr},
      {"CAPTION", mojom::AnnotationType::kCaption},
      {"LABEL", mojom::AnnotationType::kLabel}};

  const base::Value* const failure_reason =
      desc_engine.FindKey("failureReason");

  const bool adult = failure_reason && failure_reason->is_string() &&
                     failure_reason->GetString() == kAdultFailureReason;
  std::vector<mojom::AnnotationPtr> results;

  const base::Value* const desc_list_dict =
      desc_engine.FindKey("descriptionList");
  if (!desc_list_dict || !desc_list_dict->is_dict())
    return {adult, std::move(results)};

  const base::Value* const desc_list = desc_list_dict->FindKey("descriptions");
  if (!desc_list || !desc_list->is_list())
    return {adult, std::move(results)};

  for (const base::Value& desc : desc_list->GetList()) {
    if (!desc.is_dict())
      continue;

    const base::Value* const type = desc.FindKey("type");
    if (!type || !type->is_string())
      continue;

    const auto type_lookup = annotation_types.find(type->GetString());
    if (type_lookup == annotation_types.end())
      continue;

    const base::Value* const score = desc.FindKey("score");
    if (!score || (!score->is_double() && !score->is_int()) ||
        score->GetDouble() < 0.0 || score->GetDouble() > 1.0)
      continue;

    const base::Value* const text = desc.FindKey("text");
    if (!text || !text->is_string() || text->GetString().empty())
      continue;

    results.push_back(mojom::Annotation::New(
        type_lookup->second, score->GetDouble(), text->GetString()));
  }

  return {adult, std::move(results)};
}

// Attempts to extract annotation results from the server response, returning a
// map from each source ID to its annotations (if successfully extracted).
std::map<std::string, mojom::AnnotateImageResultPtr> UnpackJsonResponse(
    const base::Value& json_data,
    const double min_ocr_confidence) {
  if (!json_data.is_dict())
    return {};

  const base::Value* const results = json_data.FindKey("results");
  if (!results || !results->is_list())
    return {};

  std::map<std::string, mojom::AnnotateImageResultPtr> out;
  for (const base::Value& result : results->GetList()) {
    if (!result.is_dict())
      continue;

    const base::Value* const image_id = result.FindKey("imageId");
    if (!image_id || !image_id->is_string())
      continue;

    const base::Value* const engine_results = result.FindKey("engineResults");
    if (!engine_results || !engine_results->is_list())
      continue;

    // We expect the engine result list to have exactly two results: one for OCR
    // and one for image descriptions. However, we "robustly" handle missing
    // engines, unknown engines (by skipping them) and repetitions (by
    // overwriting data).
    bool adult = false;
    std::vector<mojom::AnnotationPtr> annotations;
    mojom::AnnotationPtr ocr_annotation;
    for (const base::Value& engine_result : engine_results->GetList()) {
      if (!engine_result.is_dict())
        continue;

      const base::Value* const desc_engine =
          engine_result.FindKey("descriptionEngine");
      const base::Value* const ocr_engine = engine_result.FindKey("ocrEngine");

      if (desc_engine && desc_engine->is_dict()) {
        // Add description annotations and update the adult image flag.
        std::tie(adult, annotations) = ParseJsonDescAnnotations(*desc_engine);
      } else if (ocr_engine && ocr_engine->is_dict()) {
        // Update the specialized OCR annotations.
        ocr_annotation =
            ParseJsonOcrAnnotation(*ocr_engine, min_ocr_confidence);
      }
    }

    // Remove any description OCR data (which is lower quality) if we have
    // specialized OCR results.
    if (!ocr_annotation.is_null()) {
      base::EraseIf(annotations, [](const mojom::AnnotationPtr& a) {
        return a->type == mojom::AnnotationType::kOcr;
      });
      annotations.push_back(std::move(ocr_annotation));
    }

    if (adult) {
      out[image_id->GetString()] = mojom::AnnotateImageResult::NewErrorCode(
          mojom::AnnotateImageError::kAdult);
    } else if (annotations.empty()) {
      out[image_id->GetString()] = mojom::AnnotateImageResult::NewErrorCode(
          mojom::AnnotateImageError::kFailure);
    } else {
      out[image_id->GetString()] =
          mojom::AnnotateImageResult::NewAnnotations(std::move(annotations));
    }
  }

  return out;
}

}  // namespace

constexpr char Annotator::kGoogApiKeyHeader[];

Annotator::Annotator(
    GURL server_url,
    std::string api_key,
    const base::TimeDelta throttle,
    const int batch_size,
    const double min_ocr_confidence,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    service_manager::Connector* const connector)
    : url_loader_factory_(std::move(url_loader_factory)),
      connector_(connector),
      http_request_timer_(
          FROM_HERE,
          throttle,
          base::BindRepeating(&Annotator::SendRequestBatchToServer,
                              base::Unretained(this))),
      server_url_(std::move(server_url)),
      api_key_(std::move(api_key)),
      batch_size_(batch_size),
      min_ocr_confidence_(min_ocr_confidence) {
  DCHECK(connector_);
}

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
    std::move(callback).Run(cache_lookup->second.Clone());
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
std::string Annotator::FormatJsonRequest(
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
    base::Value ocr_engine_params(base::Value::Type::DICTIONARY);
    ocr_engine_params.SetKey("ocrParameters",
                             base::Value(base::Value::Type::DICTIONARY));
    base::Value desc_engine_params(base::Value::Type::DICTIONARY);
    desc_engine_params.SetKey("descriptionParameters",
                              base::Value(base::Value::Type::DICTIONARY));

    base::Value engine_params_list(base::Value::Type::LIST);
    engine_params_list.GetList().push_back(std::move(ocr_engine_params));
    engine_params_list.GetList().push_back(std::move(desc_engine_params));

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
std::unique_ptr<network::SimpleURLLoader> Annotator::MakeRequestLoader(
    const GURL& server_url,
    const std::string& api_key,
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

  // Put API key in request's header if a key exists, and the endpoint is
  // trusted by Google.
  if (!api_key.empty() && server_url.SchemeIs(url::kHttpsScheme) &&
      google_util::IsGoogleAssociatedDomainUrl(server_url)) {
    resource_request->headers.SetHeader(kGoogApiKeyHeader, api_key);
  }

  // TODO(crbug.com/916420): update this annotation to be more general and to
  //                         reflect specfics of the UI when it is implemented.
  const net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("image_annotation", R"(
        semantics {
          sender: "Image Annotation"
          description:
            "Chrome can provide image labels (which include detected objects, "
            "extracted text and generated captions) to screen readers (for "
            "visually-impaired users) by sending images to Google's servers. "
            "If image labeling is enabled for a page, Chrome will send the "
            "URLs and pixels of all images on the page to Google's servers, "
            "which will return labels for content identified inside the "
            "images. This content is made accessible to screen reading "
            "software."
          trigger: "A page containing images is loaded for a user who has "
                   "automatic image labeling enabled."
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

  url_loader->AttachStringForUpload(FormatJsonRequest(begin_it, end_it),
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
  http_request_queue_.push_front({source_id, image_bytes});
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
  http_requests_.push_back(
      MakeRequestLoader(server_url_, api_key_, begin_it, end_it));
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

  if (!json_response) {
    DVLOG(1) << "HTTP request to image annotation server failed.";
    ProcessResults(source_ids, {});
    return;
  }

  // Send JSON string to a dedicated service for safe parsing.
  GetJsonParser().Parse(*json_response,
                        base::BindOnce(&Annotator::OnResponseJsonParsed,
                                       base::Unretained(this), source_ids));
}

void Annotator::OnResponseJsonParsed(
    const std::set<std::string>& source_ids,
    const base::Optional<base::Value> json_data,
    const base::Optional<std::string>& error) {
  // Extract annotation results for each source ID with valid results.
  std::map<std::string, mojom::AnnotateImageResultPtr> results;
  if (!json_data.has_value() || error.has_value()) {
    DVLOG(1) << "Parsing server response JSON failed with error: "
             << error.value_or("No reason reported.");
    ProcessResults(source_ids, {});
  } else {
    ProcessResults(source_ids,
                   UnpackJsonResponse(*json_data, min_ocr_confidence_));
  }
}

void Annotator::ProcessResults(
    const std::set<std::string>& source_ids,
    const std::map<std::string, mojom::AnnotateImageResultPtr>& results) {
  // Process each source ID for which we expect to have results.
  for (const std::string& source_id : source_ids) {
    pending_source_ids_.erase(source_id);

    // The lookup will be successful if there is a valid result (i.e. not an
    // error and not a malformed result) for this source ID.
    const auto result_lookup = results.find(source_id);

    // Populate the result struct for this image and copy it into the cache if
    // necessary.
    if (result_lookup != results.end())
      cached_results_.insert({source_id, result_lookup->second.Clone()});

    // This should not happen, since only this method removes entries of
    // |request_infos_|, and this method should only execute once per source ID.
    const auto request_info_it = request_infos_.find(source_id);
    if (request_info_it == request_infos_.end())
      continue;

    const auto image_result = result_lookup != results.end()
                                  ? result_lookup->second.Clone()
                                  : mojom::AnnotateImageResult::NewErrorCode(
                                        mojom::AnnotateImageError::kFailure);

    // Notify clients of success or failure.
    // TODO(crbug.com/916420): explore server retry strategies.
    for (auto& info : request_info_it->second) {
      std::move(info.second).Run(image_result.Clone());
    }
    request_infos_.erase(request_info_it);
  }
}

data_decoder::mojom::JsonParser& Annotator::GetJsonParser() {
  if (!json_parser_) {
    connector_->BindInterface(data_decoder::mojom::kServiceName,
                              mojo::MakeRequest(&json_parser_));
    json_parser_.set_connection_error_handler(base::BindOnce(
        [](Annotator* const annotator) { annotator->json_parser_.reset(); },
        base::Unretained(this)));
  }

  return *json_parser_;
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
