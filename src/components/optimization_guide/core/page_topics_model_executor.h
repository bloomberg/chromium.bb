// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_TOPICS_MODEL_EXECUTOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_TOPICS_MODEL_EXECUTOR_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/optimization_guide/core/bert_model_handler.h"
#include "components/optimization_guide/core/page_content_annotation_job.h"
#include "components/optimization_guide/core/page_content_annotation_job_executor.h"
#include "components/optimization_guide/core/page_content_annotations_common.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace optimization_guide {

// A BERT-based mode executor for page topics annotations. All the derived
// functionality of this class is exclusive to the UI thread, but may post
// things to the background task runner.
class PageTopicsModelExecutor : public PageContentAnnotationJobExecutor,
                                public BertModelHandler {
 public:
  PageTopicsModelExecutor(
      OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      const absl::optional<proto::Any>& model_metadata);
  ~PageTopicsModelExecutor() override;

  // PageContentAnnotationJobExecutor:
  void ExecuteJob(base::OnceClosure on_job_complete_callback,
                  std::unique_ptr<PageContentAnnotationJob> job) override;
  void ExecuteOnSingleInput(
      AnnotationType annotation_type,
      const std::string& raw_input,
      base::OnceCallback<void(const BatchAnnotationResult&)> callback) override;

  // BertModelHandler:
  void UnloadModel() override;
  void OnModelUpdated(proto::OptimizationTarget optimization_target,
                      const ModelInfo& model_info) override;

  // Creates a BatchAnnotationResult from the output of the model, calling
  // |ExtractCategoriesFromModelOutput| in the process.
  // Public for testing.
  void PostprocessCategoriesToBatchAnnotationResult(
      base::OnceCallback<void(const BatchAnnotationResult&)> callback,
      AnnotationType annotation_type,
      const std::string& raw_input,
      const absl::optional<std::vector<tflite::task::core::Category>>& output);

  // Extracts the scored categories from the output of the model.
  // Public for testing.
  absl::optional<std::vector<WeightedIdentifier>>
  ExtractCategoriesFromModelOutput(
      const std::vector<tflite::task::core::Category>& model_output) const;

 private:
  void OnOverrideListLoadAttemptDone(
      base::OnceClosure on_job_complete_callback,
      std::unique_ptr<PageContentAnnotationJob> job,
      absl::optional<
          std::unordered_map<std::string, std::vector<WeightedIdentifier>>>
          override_list);

  // Does the required preprocessing on a input domain.
  static std::string PreprocessHost(const std::string& host);

  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // Set whenever a valid override list file is passed along with the model file
  // update. This will be reset if the provided file is deemed invalid on the
  // first attempted load.
  // Used on the UI thread.
  absl::optional<base::FilePath> override_list_file_path_;

  // Set whenever an override list file is available and the model file is
  // loaded into memory. Reset whenever the model file is unloaded.
  // Used on the UI thread. Lookups in this mapping should have |PreprocessHost|
  // applied first.
  absl::optional<
      std::unordered_map<std::string, std::vector<WeightedIdentifier>>>
      override_list_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PageTopicsModelExecutor> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_TOPICS_MODEL_EXECUTOR_H_
