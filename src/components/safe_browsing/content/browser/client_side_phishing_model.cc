// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/client_side_phishing_model.h"

#include <stdint.h>
#include <memory>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/safe_browsing/core/common/fbs/client_model_generated.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace safe_browsing {

namespace {

// Command-line flag that can be used to override the current CSD model. Must be
// provided with an absolute path.
const char kOverrideCsdModelFlag[] = "csd-model-override-path";

std::string ReadFileIntoString(base::FilePath path) {
  if (path.empty())
    return std::string();

  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid())
    return std::string();

  std::vector<char> model_data(file.GetLength());
  if (file.ReadAtCurrentPos(model_data.data(), model_data.size()) == -1)
    return std::string();

  return std::string(model_data.begin(), model_data.end());
}

}  // namespace

using base::AutoLock;

struct ClientSidePhishingModelSingletonTrait
    : public base::DefaultSingletonTraits<ClientSidePhishingModel> {
  static ClientSidePhishingModel* New() {
    ClientSidePhishingModel* instance = new ClientSidePhishingModel();
    return instance;
  }
};

// --- ClientSidePhishingModel methods ---

// static
ClientSidePhishingModel* ClientSidePhishingModel::GetInstance() {
  return base::Singleton<ClientSidePhishingModel,
                         ClientSidePhishingModelSingletonTrait>::get();
}

ClientSidePhishingModel::ClientSidePhishingModel() {
  MaybeOverrideModel();
}

ClientSidePhishingModel::~ClientSidePhishingModel() {
  AutoLock lock(lock_);  // DCHECK fail if the lock is held.
}

base::CallbackListSubscription ClientSidePhishingModel::RegisterCallback(
    base::RepeatingCallback<void()> callback) {
  AutoLock lock(lock_);
  return callbacks_.Add(std::move(callback));
}

bool ClientSidePhishingModel::IsEnabled() const {
  return (model_type_ == CSDModelType::kFlatbuffer &&
          mapped_region_.IsValid()) ||
         (model_type_ == CSDModelType::kProtobuf && !model_str_.empty()) ||
         visual_tflite_model_.IsValid();
}

const std::string& ClientSidePhishingModel::GetModelStr() const {
  DCHECK(model_type_ != CSDModelType::kFlatbuffer);
  return model_str_;
}

const base::File& ClientSidePhishingModel::GetVisualTfLiteModel() const {
  return visual_tflite_model_;
}

CSDModelType ClientSidePhishingModel::GetModelType() const {
  return model_type_;
}

base::ReadOnlySharedMemoryRegion
ClientSidePhishingModel::GetModelSharedMemoryRegion() const {
  return mapped_region_.region.Duplicate();
}

void ClientSidePhishingModel::PopulateFromDynamicUpdate(
    const std::string& model_str,
    base::File visual_tflite_model) {
  AutoLock lock(lock_);

  bool model_valid = false;
  int model_version_field = 0;

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          kOverrideCsdModelFlag) &&
      !model_str.empty()) {
    model_type_ = CSDModelType::kNone;
    if (base::FeatureList::IsEnabled(kClientSideDetectionModelIsFlatBuffer)) {
      flatbuffers::Verifier verifier(
          reinterpret_cast<const uint8_t*>(model_str.data()),
          model_str.length());
      model_valid = flat::VerifyClientSideModelBuffer(verifier);
      if (model_valid) {
        mapped_region_ =
            base::ReadOnlySharedMemoryRegion::Create(model_str.length());
        if (mapped_region_.IsValid()) {
          model_type_ = CSDModelType::kFlatbuffer;
          model_version_field =
              flat::GetClientSideModel(model_str.data())->version();
          memcpy(mapped_region_.mapping.memory(), model_str.data(),
                 model_str.length());
        } else {
          model_valid = false;
        }
        base::UmaHistogramBoolean(
            "SBClientPhishing.FlatBufferMappedRegionValid",
            mapped_region_.IsValid());
      }
    } else {
      ClientSideModel model_proto;
      model_valid = model_proto.ParseFromString(model_str);
      if (model_valid) {
        model_type_ = CSDModelType::kProtobuf;
        model_version_field = model_proto.version();
        model_str_ = model_str;
      }
    }

    base::UmaHistogramBoolean("SBClientPhishing.ModelDynamicUpdateSuccess",
                              model_valid);

    if (model_valid) {
      // At time of writing, versions go up to 25. We set a max version of 100
      // to give some room.
      const int kMaxVersion = 100;
      base::UmaHistogramExactLinear(
          "SBClientPhishing.ModelDynamicUpdateVersion", model_version_field,
          kMaxVersion + 1);
    }
  }

  bool tflite_valid = visual_tflite_model.IsValid();
  if (tflite_valid) {
    visual_tflite_model_ = std::move(visual_tflite_model);
  }

  if (model_valid || tflite_valid) {
    // Unretained is safe because this is a singleton.
    base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                   base::BindOnce(&ClientSidePhishingModel::NotifyCallbacksOnUI,
                                  base::Unretained(this)));
  }
}

void ClientSidePhishingModel::NotifyCallbacksOnUI() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  callbacks_.Notify();
}

void ClientSidePhishingModel::SetModelStrForTesting(
    const std::string& model_str) {
  AutoLock lock(lock_);
  model_str_ = model_str;
}

void ClientSidePhishingModel::SetVisualTfLiteModelForTesting(base::File file) {
  AutoLock lock(lock_);
  visual_tflite_model_ = std::move(file);
}

void ClientSidePhishingModel::SetModelTypeForTesting(CSDModelType model_type) {
  AutoLock lock(lock_);
  model_type_ = model_type;
}

void ClientSidePhishingModel::ClearMappedRegionForTesting() {
  AutoLock lock(lock_);
  mapped_region_.mapping = base::WritableSharedMemoryMapping();
  mapped_region_.region = base::ReadOnlySharedMemoryRegion();
}

void* ClientSidePhishingModel::GetFlatBufferMemoryAddressForTesting() {
  return mapped_region_.mapping.memory();
}

void ClientSidePhishingModel::MaybeOverrideModel() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kOverrideCsdModelFlag)) {
    base::FilePath overriden_model_path =
        base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            kOverrideCsdModelFlag);
    CSDModelType model_type =
        base::FeatureList::IsEnabled(kClientSideDetectionModelIsFlatBuffer)
            ? CSDModelType::kFlatbuffer
            : CSDModelType::kProtobuf;
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&ReadFileIntoString, overriden_model_path),
        // base::Unretained is safe because this is a singleton.
        base::BindOnce(&ClientSidePhishingModel::OnGetOverridenModelData,
                       base::Unretained(this), model_type));
  }
}

void ClientSidePhishingModel::OnGetOverridenModelData(
    CSDModelType model_type,
    const std::string& model_data) {
  if (model_data.empty()) {
    VLOG(2) << "Overriden model data is empty";
    return;
  }

  switch (model_type) {
    case CSDModelType::kProtobuf: {
      std::unique_ptr<ClientSideModel> model =
          std::make_unique<ClientSideModel>();
      if (!model->ParseFromArray(model_data.data(), model_data.size())) {
        VLOG(2) << "Overriden model data is not a valid ClientSideModel proto";
        return;
      }
      model_type_ = model_type;
      model_str_ = model_data;
      break;
    }
    case CSDModelType::kFlatbuffer: {
      flatbuffers::Verifier verifier(
          reinterpret_cast<const uint8_t*>(model_data.data()),
          model_data.length());
      if (!flat::VerifyClientSideModelBuffer(verifier)) {
        VLOG(2)
            << "Overriden model data is not a valid ClientSideModel flatbuffer";
        return;
      }
      mapped_region_ =
          base::ReadOnlySharedMemoryRegion::Create(model_data.length());
      if (!mapped_region_.IsValid()) {
        VLOG(2) << "Could not create shared memory region for flatbuffer";
        return;
      }
      memcpy(mapped_region_.mapping.memory(), model_data.data(),
             model_data.length());
      model_type_ = model_type;
      break;
    }
    case CSDModelType::kNone:
      VLOG(2) << "Model type should have been either proto or flatbuffer";
      return;
  }

  VLOG(2) << "Model overriden successfully";

  // Unretained is safe because this is a singleton.
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&ClientSidePhishingModel::NotifyCallbacksOnUI,
                                base::Unretained(this)));
}

}  // namespace safe_browsing
