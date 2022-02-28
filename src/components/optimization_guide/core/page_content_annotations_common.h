// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_CONTENT_ANNOTATIONS_COMMON_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_CONTENT_ANNOTATIONS_COMMON_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "components/optimization_guide/core/entity_metadata.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace optimization_guide {

// The type of annotation that is being done on the given input.
enum class AnnotationType {
  kUnknown,

  // The input will be annotated with the topics on the page. These topics are
  // fairly high-level like "sports" or "news".
  kPageTopics,

  // The input will be annotated for the visibility of the content.
  kContentVisibility,

  // The input will be annotated with the entity IDs on the page, for example
  // listing the IDs of all the proper nouns on a page. To map the IDs back to
  // human-readable strings, use `EntityMetadataProvider`.
  kPageEntities,
};

std::string AnnotationTypeToString(AnnotationType type);

// A weighted string value.
class WeightedString {
 public:
  WeightedString(const std::string& value, double weight);
  WeightedString(const WeightedString&);
  ~WeightedString();

  std::string value() const { return value_; }
  double weight() const { return weight_; }

  std::string ToString() const;

  bool operator==(const WeightedString& other) const;

  friend std::ostream& operator<<(std::ostream& stream,
                                  const WeightedString& ws);

 private:
  std::string value_;

  // In the range of [0.0, 1.0].
  double weight_ = 0;
};

// The result of an execution, and all associated data.
class BatchAnnotationResult {
 public:
  // Creates a result for a page topics annotation.
  static BatchAnnotationResult CreatePageTopicsResult(
      const std::string& input,
      absl::optional<std::vector<WeightedString>> topics);

  // Creates a result for a page entities annotation.
  static BatchAnnotationResult CreatePageEntitiesResult(
      const std::string& input,
      absl::optional<std::vector<ScoredEntityMetadata>> entities);

  // Creates a result for a content visibility annotation.
  static BatchAnnotationResult CreateContentVisibilityResult(
      const std::string& input,
      absl::optional<double> visibility_score);

  // Creates a result where the AnnotationType and output are not set.
  static BatchAnnotationResult CreateEmptyAnnotationsResult(
      const std::string& input);

  BatchAnnotationResult(const BatchAnnotationResult&);
  ~BatchAnnotationResult();

  std::string input() const { return input_; }
  AnnotationType type() const { return type_; }
  absl::optional<std::vector<WeightedString>> topics() const { return topics_; }
  absl::optional<std::vector<ScoredEntityMetadata>> entities() const {
    return entities_;
  }
  absl::optional<double> visibility_score() const { return visibility_score_; }

  std::string ToString() const;

  bool operator==(const BatchAnnotationResult& other) const;

  friend std::ostream& operator<<(std::ostream& stream,
                                  const BatchAnnotationResult& result);

 private:
  BatchAnnotationResult();

  std::string input_;
  AnnotationType type_ = AnnotationType::kUnknown;

  // Output for page topics annotations, set only if the |type_| matches and the
  // execution was successful.
  absl::optional<std::vector<WeightedString>> topics_;

  // Output for page entities annotations, set only if the |type_| matches and
  // the execution was successful.
  absl::optional<std::vector<ScoredEntityMetadata>> entities_;

  // Output for visisbility score annotations, set only if the |type_| matches
  // and the execution was successful.
  absl::optional<double> visibility_score_;
};

using BatchAnnotationCallback =
    base::OnceCallback<void(const std::vector<BatchAnnotationResult>&)>;

// Creates a vector of |BatchAnnotationResult| from the given |inputs| where
// each result's status is set to |status|. Useful for creating an Annotation
// response with a single error.
std::vector<BatchAnnotationResult> CreateEmptyBatchAnnotationResults(
    const std::vector<std::string>& inputs);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_CONTENT_ANNOTATIONS_COMMON_H_
