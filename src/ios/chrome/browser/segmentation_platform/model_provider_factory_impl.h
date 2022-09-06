// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_MODEL_PROVIDER_FACTORY_IMPL_H_
#define IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_MODEL_PROVIDER_FACTORY_IMPL_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
}

namespace segmentation_platform {

// Implements model provider factory for segmentation, which is the source of
// models for segmentation service.
class ModelProviderFactoryImpl : public ModelProviderFactory {
 public:
  ModelProviderFactoryImpl(
      optimization_guide::OptimizationGuideModelProvider*
          optimization_guide_provider,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);

  ~ModelProviderFactoryImpl() override;

  ModelProviderFactoryImpl(ModelProviderFactoryImpl&) = delete;
  ModelProviderFactoryImpl& operator=(ModelProviderFactoryImpl&) = delete;

  // ModelProviderFactory impl:
  std::unique_ptr<ModelProvider> CreateProvider(
      proto::SegmentId segment_id) override;
  std::unique_ptr<ModelProvider> CreateDefaultProvider(
      proto::SegmentId segment_id) override;

 private:
  raw_ptr<optimization_guide::OptimizationGuideModelProvider>
      optimization_guide_provider_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
};

}  // namespace segmentation_platform

#endif  // IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_MODEL_PROVIDER_FACTORY_IMPL_H_
