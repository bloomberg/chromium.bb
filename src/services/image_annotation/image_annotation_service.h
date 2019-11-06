// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IMAGE_ANNOTATION_IMAGE_ANNOTATION_SERVICE_H_
#define SERVICES_IMAGE_ANNOTATION_IMAGE_ANNOTATION_SERVICE_H_

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "services/image_annotation/annotator.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace image_annotation {

class ImageAnnotationService : public service_manager::Service {
 public:
  // Whether or not to override service parameters for experimentation.
  static const base::Feature kExperiment;

  ImageAnnotationService(
      service_manager::mojom::ServiceRequest request,
      std::string api_key,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory);
  ~ImageAnnotationService() override;

 private:
  // Service params:

  static constexpr base::FeatureParam<std::string> kServerUrl{
      &kExperiment, "server_url",
      "https://ckintersect-pa.googleapis.com/v1/intersect/pixels"};
  // An override Google API key. If empty, the API key with which the browser
  // was built (if any) will be used instead.
  static constexpr base::FeatureParam<std::string> kApiKey{&kExperiment,
                                                           "api_key", ""};
  static constexpr base::FeatureParam<int> kThrottleMs{&kExperiment,
                                                       "throttle_ms", 300};
  static constexpr base::FeatureParam<int> kBatchSize{&kExperiment,
                                                      "batch_size", 10};
  static constexpr base::FeatureParam<double> kMinOcrConfidence{
      &kExperiment, "min_ocr_confidence", 0.7};

  // service_manager::Service:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;
  void OnStart() override;

  service_manager::BinderRegistry registry_;
  service_manager::ServiceBinding service_binding_;

  Annotator annotator_;

  DISALLOW_COPY_AND_ASSIGN(ImageAnnotationService);
};

}  // namespace image_annotation

#endif  // SERVICES_IMAGE_ANNOTATION_IMAGE_ANNOTATION_SERVICE_H_
