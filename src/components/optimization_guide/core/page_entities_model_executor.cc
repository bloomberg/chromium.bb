// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/page_entities_model_executor.h"

#include "base/bind.h"
#include "base/callback.h"

namespace optimization_guide {

void PageEntitiesModelExecutor::ExecuteOnSingleInput(
    AnnotationType annotation_type,
    const std::string& input,
    base::OnceCallback<void(const BatchAnnotationResult&)> callback) {
  DCHECK_EQ(annotation_type, AnnotationType::kPageEntities);

  ExecuteModelWithInput(
      input, base::BindOnce(
                 [](const std::string& input,
                    base::OnceCallback<void(const BatchAnnotationResult&)>
                        pca_callback,
                    const absl::optional<std::vector<ScoredEntityMetadata>>&
                        entity_metadata) {
                   std::move(pca_callback)
                       .Run(BatchAnnotationResult::CreatePageEntitiesResult(
                           input, entity_metadata));
                 },
                 input, std::move(callback)));
}

}  // namespace optimization_guide
