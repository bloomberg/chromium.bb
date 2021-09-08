/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/compiler/tf2tensorrt/utils/trt_shape_optimization_profiles.h"

#include <algorithm>
#include <functional>

#include "tensorflow/compiler/tf2tensorrt/convert/utils.h"

#if GOOGLE_CUDA && GOOGLE_TENSORRT
namespace tensorflow {
namespace tensorrt {

// Creates optimization profiles for a list of input shapes. The list of input
// shapes are stored in shapes_.
void TrtShapeOptimizationProfile::InitProfiles() {
  if (input_shapes_.size() == 0) {
    VLOG(1) << "Not creating profiles without input_shapes. "
               "You have to enable profile generation mode first (build).";
  } else {
    VLOG(1) << "Creating profiles with startegy of one profile "
            << "for each input (min=opt=max).";
  }
  for (auto& shape_vec : input_shapes_) {
    std::vector<nvinfer1::Dims> dimvec;
    for (auto& shape : shape_vec) {
      dimvec.push_back(TensorShapeToTrtDims(shape, false));
    }
    // We set min=opt=max.
    OptimizationProfileConfig profConfig{dimvec, dimvec, dimvec};
    profiles_.push_back(std::move(profConfig));
    VLOG(1) << "Created profile " << profiles_.back().DebugString();
  }
}

#if IS_TRT_VERSION_GE(6, 0, 0, 0)
Status TrtShapeOptimizationProfile::AddProfiles(
    nvinfer1::IBuilder* builder, nvinfer1::IBuilderConfig* config,
    const nvinfer1::INetworkDefinition* network) {
  // Create a vector of optimization profiles
  for (int i = 0; i < profiles_.size(); i++) {
    auto* optProfile = builder->createOptimizationProfile();
    Status status = profiles_[i].SetDimensions(network, optProfile);
    if (!status.ok()) {
      return status;
    }
    int idx = -1;
    if (optProfile->isValid()) {
      idx = config->addOptimizationProfile(optProfile);
    }
    if (idx >= 0) {
      if (i != idx) {
        return errors::Internal(
            "Profile index of engine config is different from resource profile "
            "index: ",
            i, " != ", idx);
      }
      VLOG(1) << "Added optimization profile " << profiles_[i].DebugString()
              << " to builder config.";
    } else {
      LOG(ERROR) << "Failed to add optimization profile "
                 << profiles_[i].DebugString()
                 << ". This usually happens when profile is invalid.";
    }
  }
  if (!profiles_.empty() && config->getNbOptimizationProfiles() == 0) {
    return errors::Internal("Failure in adding an optimization profile.");
  }
  // if TRT_VERSION < 6, then we do not need to add
  return Status::OK();
}
#endif

#if IS_TRT_VERSION_GE(6, 0, 0, 0)
Status TrtShapeOptimizationProfile::ConfigureBuilder(
    nvinfer1::IBuilder* builder, nvinfer1::IBuilderConfig* config,
    const nvinfer1::INetworkDefinition* network) {
  TF_RETURN_IF_ERROR(AddProfiles(builder, config, network));
  return Status::OK();
}
#endif

int TrtShapeOptimizationProfile::GetProfileNumber(
    std::vector<TensorShape> shapes) {
  for (int i = 0; i < profiles_.size(); i++) {
    if (profiles_[i].IncludesShapes(shapes)) {
      return i;
    }
  }
  VLOG(1) << "Profile not found for input shapes " << DebugString(shapes)
          << ".";
  return -1;
}

Status TrtShapeOptimizationProfile::CreateExecutionContexts(
    nvinfer1::ICudaEngine* engine,
    std::vector<TrtUniquePtrType<nvinfer1::IExecutionContext>>& exec_context) {
  int i = 0;
  // The following loop runs once if we have static shapes, to create a single
  // execution context without profiles. In dynamic mode we create one context
  // for each profile and set the corresponding optimization profile.
  do {
    VLOG(1) << "Creating execution context " << i;
    nvinfer1::IExecutionContext* ctx = engine->createExecutionContext();
    if (ctx == nullptr) {
      return errors::Internal("Failed to create execution context");
    }
    if (i > 0) {
      // This condition is needed for two reasons:
      // - using static shapes we do not have any profiles so we cannot call
      //   set optimizationprofiles.
      // - The 0th profile is set implicitly for the first execution context
      //   therefore we do not need to set.
#if IS_TRT_VERSION_GE(6, 0, 0, 0)
      bool stat = ctx->setOptimizationProfile(i);
      if (!stat) {
        ctx->destroy();
        return errors::Internal("Could not set TRT optimization profile.");
      }
#endif
    }
    exec_context.push_back(TrtUniquePtrType<nvinfer1::IExecutionContext>(ctx));
    i++;
  } while (i < profiles_.size());

  return Status::OK();
}

Status TrtShapeOptimizationProfile::RestoreProfiles(
    const nvinfer1::ICudaEngine* engine) {
#if IS_TRT_VERSION_GE(6, 0, 0, 0)
  if (!engine) {
    // We do not need to restore profiles for an empty engine
    return Status::OK();
  }
#if IS_TRT_VERSION_GE(7, 0, 0, 0)
  if (engine->hasImplicitBatchDimension()) {
    // Nothing to do, we cannot have profiles in implicit batch mode
    return Status::OK();
  }
#endif
  int n_profiles = engine->getNbOptimizationProfiles();
  int n_inputs = GetNumberOfEngineInputs(engine);
  VLOG(2) << "Attempting to restore " << n_profiles << " profiles, each with "
          << n_inputs << " inputs";
  for (int prof_idx = 0; prof_idx < n_profiles; prof_idx++) {
    OptimizationProfileConfig cfg;
    for (int j = 0; j < n_inputs; j++) {
      nvinfer1::Dims min = engine->getProfileDimensions(
          j, prof_idx, nvinfer1::OptProfileSelector::kMIN);
      nvinfer1::Dims max = engine->getProfileDimensions(
          j, prof_idx, nvinfer1::OptProfileSelector::kMAX);
      nvinfer1::Dims opt = engine->getProfileDimensions(
          j, prof_idx, nvinfer1::OptProfileSelector::kOPT);
      cfg.min.push_back(min);
      cfg.max.push_back(max);
      cfg.opt.push_back(opt);
    }
    VLOG(2) << "Restored profile " << cfg.DebugString();
    profiles_.push_back(std::move(cfg));
  }
#endif
  return Status::OK();
}

int TrtShapeOptimizationProfile::GetNumProfiles() const {
  return profiles_.size();
}

}  // namespace tensorrt
}  // namespace tensorflow
#endif  // GOOGLE_CUDA && GOOGLE_TENSORRT
