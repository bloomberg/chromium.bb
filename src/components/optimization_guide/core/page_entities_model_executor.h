// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_ENTITIES_MODEL_EXECUTOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_ENTITIES_MODEL_EXECUTOR_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "components/optimization_guide/core/entity_metadata.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/category.h"

namespace optimization_guide {

// TODO(crbug/1278828): Remove this entirely.
class HumanReadablePageEntitiesModelExecutor {
 public:
  virtual ~HumanReadablePageEntitiesModelExecutor() = default;

  using PageEntitiesModelExecutedCallback = base::OnceCallback<void(
      const absl::optional<std::vector<tflite::task::core::Category>>&)>;

  // Annotates |text| with page entities likely represented on the page. Invokes
  // |callback| when done.
  virtual void ExecuteModelWithInput(
      const std::string& text,
      PageEntitiesModelExecutedCallback callback) = 0;
};

// The PageEntitiesModelExecutor is responsible for executing the PAGE_ENTITIES
// model.
class PageEntitiesModelExecutor {
 public:
  virtual ~PageEntitiesModelExecutor() = default;

  using PageEntitiesMetadataModelExecutedCallback = base::OnceCallback<void(
      const absl::optional<std::vector<ScoredEntityMetadata>>&)>;

  // Annotates |text| with page entities likely represented on the page,
  // returning the entity metadata in the reader's locale with associated score.
  // Invokes |callback| when done.
  virtual void HumanReadableExecuteModelWithInput(
      const std::string& text,
      PageEntitiesMetadataModelExecutedCallback callback) = 0;

  using PageEntitiesModelEntityMetadataRetrievedCallback =
      base::OnceCallback<void(const absl::optional<EntityMetadata>&)>;

  // Retrieves the metadata associated with |entity_id|. Invokes |callback|
  // when done.
  virtual void GetMetadataForEntityId(
      const std::string& entity_id,
      PageEntitiesModelEntityMetadataRetrievedCallback callback) = 0;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_ENTITIES_MODEL_EXECUTOR_H_
