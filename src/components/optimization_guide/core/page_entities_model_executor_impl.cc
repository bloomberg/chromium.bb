// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/page_entities_model_executor_impl.h"

#include "base/metrics/histogram_functions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/timer/elapsed_timer.h"
#include "components/optimization_guide/core/entity_annotator_native_library.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/page_entities_model_metadata.pb.h"

namespace optimization_guide {

namespace {

const char kPageEntitiesModelMetadataTypeUrl[] =
    "type.googleapis.com/"
    "google.internal.chrome.optimizationguide.v1.PageEntitiesModelMetadata";

}  // namespace

EntityAnnotatorHolder::EntityAnnotatorHolder(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    scoped_refptr<base::SequencedTaskRunner> reply_task_runner)
    : background_task_runner_(background_task_runner),
      reply_task_runner_(reply_task_runner) {}

EntityAnnotatorHolder::~EntityAnnotatorHolder() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  if (features::ShouldResetPageEntitiesModelOnShutdown()) {
    ResetEntityAnnotator();
  }
}

void EntityAnnotatorHolder::
    InitializeEntityAnnotatorNativeLibraryOnBackgroundThread(
        base::OnceCallback<void(int32_t)> init_callback) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  DCHECK(!entity_annotator_native_library_);
  if (entity_annotator_native_library_) {
    // We should only be initialized once but in case someone does something
    // wrong in a non-debug build, we invoke the callback anyway.
    reply_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(init_callback),
            entity_annotator_native_library_->GetMaxSupportedFeatureFlag()));
    return;
  }

  entity_annotator_native_library_ = EntityAnnotatorNativeLibrary::Create();
  if (!entity_annotator_native_library_) {
    reply_task_runner_->PostTask(FROM_HERE,
                                 base::BindOnce(std::move(init_callback), -1));
    return;
  }

  int32_t max_supported_feature_flag =
      entity_annotator_native_library_->GetMaxSupportedFeatureFlag();
  reply_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(init_callback), max_supported_feature_flag));
}

void EntityAnnotatorHolder::ResetEntityAnnotator() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  if (entity_annotator_) {
    DCHECK(entity_annotator_native_library_);
    entity_annotator_native_library_->DeleteEntityAnnotator(entity_annotator_);

    entity_annotator_ = nullptr;
  }
}

void EntityAnnotatorHolder::CreateAndSetEntityAnnotatorOnBackgroundThread(
    const ModelInfo& model_info) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  if (!entity_annotator_native_library_) {
    return;
  }

  ResetEntityAnnotator();

  entity_annotator_ =
      entity_annotator_native_library_->CreateEntityAnnotator(model_info);
}

void EntityAnnotatorHolder::AnnotateEntitiesMetadataModelOnBackgroundThread(
    const std::string& text,
    PageEntitiesMetadataModelExecutedCallback callback) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  base::ElapsedThreadTimer annotate_timer;

  absl::optional<std::vector<ScoredEntityMetadata>> scored_md;
  if (entity_annotator_) {
    DCHECK(entity_annotator_native_library_);
    base::TimeTicks start_time = base::TimeTicks::Now();
    scored_md =
        entity_annotator_native_library_->AnnotateText(entity_annotator_, text);
    // The max of the below histograms is 1 hour because we want to understand
    // tail behavior and catch long running model executions.
    base::UmaHistogramLongTimes(
        "OptimizationGuide.PageContentAnnotationsService.ModelExecutionLatency."
        "PageEntities",
        base::TimeTicks::Now() - start_time);

    base::UmaHistogramLongTimes(
        "OptimizationGuide.PageContentAnnotationsService."
        "ModelThreadExecutionLatency.PageEntities",
        annotate_timer.Elapsed());
  }
  reply_task_runner_->PostTask(FROM_HERE,
                               base::BindOnce(std::move(callback), scored_md));
}

void EntityAnnotatorHolder::GetMetadataForEntityIdOnBackgroundThread(
    const std::string& entity_id,
    PageEntitiesModelExecutor::PageEntitiesModelEntityMetadataRetrievedCallback
        callback) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  absl::optional<EntityMetadata> entity_metadata;
  if (entity_annotator_) {
    DCHECK(entity_annotator_native_library_);
    entity_metadata =
        entity_annotator_native_library_->GetEntityMetadataForEntityId(
            entity_annotator_, entity_id);
  }
  reply_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(entity_metadata)));
}

base::WeakPtr<EntityAnnotatorHolder>
EntityAnnotatorHolder::GetBackgroundWeakPtr() {
  return background_weak_ptr_factory_.GetWeakPtr();
}

PageEntitiesModelExecutorImpl::PageEntitiesModelExecutorImpl(
    OptimizationGuideModelProvider* optimization_guide_model_provider,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : background_task_runner_(background_task_runner),
      entity_annotator_holder_(std::make_unique<EntityAnnotatorHolder>(
          background_task_runner_,
          base::SequencedTaskRunnerHandle::Get())) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &EntityAnnotatorHolder::
              InitializeEntityAnnotatorNativeLibraryOnBackgroundThread,
          entity_annotator_holder_->GetBackgroundWeakPtr(),
          base::BindOnce(&PageEntitiesModelExecutorImpl::
                             OnEntityAnnotatorLibraryInitialized,
                         weak_ptr_factory_.GetWeakPtr(),
                         optimization_guide_model_provider)));
}

void PageEntitiesModelExecutorImpl::OnEntityAnnotatorLibraryInitialized(
    OptimizationGuideModelProvider* optimization_guide_model_provider,
    int32_t max_model_format_feature_flag) {
  if (max_model_format_feature_flag <= 0) {
    return;
  }

  proto::Any any_metadata;
  any_metadata.set_type_url(kPageEntitiesModelMetadataTypeUrl);
  proto::PageEntitiesModelMetadata model_metadata;
  model_metadata.set_max_model_format_feature_flag(
      max_model_format_feature_flag);
  model_metadata.SerializeToString(any_metadata.mutable_value());
  optimization_guide_model_provider->AddObserverForOptimizationTargetModel(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_PAGE_ENTITIES,
      any_metadata, this);
}

PageEntitiesModelExecutorImpl::~PageEntitiesModelExecutorImpl() {
  // |entity_annotator_holder_|'s  WeakPtrs are used on the background thread,
  // so that is also where the class must be destroyed.
  background_task_runner_->DeleteSoon(FROM_HERE,
                                      std::move(entity_annotator_holder_));
}

void PageEntitiesModelExecutorImpl::OnModelUpdated(
    proto::OptimizationTarget optimization_target,
    const ModelInfo& model_info) {
  if (optimization_target != proto::OPTIMIZATION_TARGET_PAGE_ENTITIES)
    return;

  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &EntityAnnotatorHolder::CreateAndSetEntityAnnotatorOnBackgroundThread,
          entity_annotator_holder_->GetBackgroundWeakPtr(), model_info));
}

void PageEntitiesModelExecutorImpl::HumanReadableExecuteModelWithInput(
    const std::string& text,
    PageEntitiesMetadataModelExecutedCallback callback) {
  if (text.empty()) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&EntityAnnotatorHolder::
                         AnnotateEntitiesMetadataModelOnBackgroundThread,
                     entity_annotator_holder_->GetBackgroundWeakPtr(), text,
                     std::move(callback)));
}

void PageEntitiesModelExecutorImpl::GetMetadataForEntityId(
    const std::string& entity_id,
    PageEntitiesModelEntityMetadataRetrievedCallback callback) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &EntityAnnotatorHolder::GetMetadataForEntityIdOnBackgroundThread,
          entity_annotator_holder_->GetBackgroundWeakPtr(), entity_id,
          std::move(callback)));
}

}  // namespace optimization_guide
