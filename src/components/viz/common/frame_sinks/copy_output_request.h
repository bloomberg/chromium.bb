// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_FRAME_SINKS_COPY_OUTPUT_REQUEST_H_
#define COMPONENTS_VIZ_COMMON_FRAME_SINKS_COPY_OUTPUT_REQUEST_H_

#include <memory>

#include "base/callback.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/unguessable_token.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/common/viz_common_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

namespace viz {

namespace mojom {
class CopyOutputRequestDataView;
}

// Holds all the properties pertaining to a copy of a surface or layer.
// Implementations that execute these requests must provide the requested
// ResultFormat or else an "empty" result. Likewise, this means that any
// transient or permanent errors preventing the successful execution of a
// copy request will result in an "empty" result.
//
// Usage: Client code creates a CopyOutputRequest, optionally sets some/all of
// its properties, and then submits it to the compositing pipeline via one of a
// number of possible entry points (usually methods named RequestCopyOfOutput()
// or RequestCopyOfSurface()). Then, some time later, the given result callback
// will be run and the client processes the CopyOutputResult containing the
// image.
//
// Note: This should be used for one-off screen capture only, and NOT for video
// screen capture use cases (please use FrameSinkVideoCapturer instead).
class VIZ_COMMON_EXPORT CopyOutputRequest {
 public:
  using ResultFormat = CopyOutputResult::Format;

  using CopyOutputRequestCallback =
      base::OnceCallback<void(std::unique_ptr<CopyOutputResult> result)>;

  CopyOutputRequest(ResultFormat result_format,
                    CopyOutputRequestCallback result_callback);

  ~CopyOutputRequest();

  // Returns the requested result format.
  ResultFormat result_format() const { return result_format_; }

  // Requests that the result callback be run as a task posted to the given
  // |task_runner|. If this is not set, the result callback could be run from
  // any context.
  void set_result_task_runner(
      scoped_refptr<base::SequencedTaskRunner> task_runner) {
    result_task_runner_ = std::move(task_runner);
  }
  bool has_result_task_runner() const { return !!result_task_runner_; }

  // Optionally specify that the result should be scaled. |scale_from| and
  // |scale_to| describe the scale ratio in terms of relative sizes: Downscale
  // if |scale_from| > |scale_to|, upscale if |scale_from| < |scale_to|, and
  // no scaling if |scale_from| == |scale_to|. Neither argument may be zero.
  //
  // There are two setters: SetScaleRatio() allows for requesting an arbitrary
  // scale in each dimension, which is sometimes useful for minor "tweaks" that
  // optimize visual quality. SetUniformScaleRatio() scales both dimensions by
  // the same amount.
  void SetScaleRatio(const gfx::Vector2d& scale_from,
                     const gfx::Vector2d& scale_to);
  void SetUniformScaleRatio(int scale_from, int scale_to);
  const gfx::Vector2d& scale_from() const { return scale_from_; }
  const gfx::Vector2d& scale_to() const { return scale_to_; }
  bool is_scaled() const { return scale_from_ != scale_to_; }

  // Optionally specify the source of this copy request. This is used when the
  // client plans to make many similar copy requests over short periods of time.
  // It is used to: 1) auto-abort prior uncommitted copy requests to avoid
  // duplicate copies of the same frame; and 2) hint to the implementation to
  // cache resources for more-efficient execution of later copy requests.
  void set_source(const base::UnguessableToken& source) { source_ = source; }
  bool has_source() const { return source_.has_value(); }
  const base::UnguessableToken& source() const { return *source_; }

  // Optionally specify the clip rect; meaning that just a portion of the entire
  // surface (or layer's subtree output) should be scanned to produce a result.
  // This rect is in the same space as the RenderPass output rect, pre-scaling.
  // This is related to set_result_selection() (see below).
  void set_area(const gfx::Rect& area) { area_ = area; }
  bool has_area() const { return area_.has_value(); }
  const gfx::Rect& area() const { return *area_; }

  // Optionally specify that only a portion of the result be generated. The
  // selection rect will be clamped to the result bounds, which always starts at
  // 0,0 and spans the post-scaling size of the copy area (see set_area()
  // above).
  void set_result_selection(const gfx::Rect& selection) {
    result_selection_ = selection;
  }
  bool has_result_selection() const { return result_selection_.has_value(); }
  const gfx::Rect& result_selection() const { return *result_selection_; }

  // Sends the result from executing this request. Called by the internal
  // implementation, usually a DirectRenderer.
  void SendResult(std::unique_ptr<CopyOutputResult> result);

  // Returns true if SendResult() will deliver the CopyOutputResult using the
  // same TaskRunner as that to which the current task was posted.
  bool SendsResultsInCurrentSequence() const;

  // Creates a RGBA_BITMAP request that ignores results, for testing purposes.
  static std::unique_ptr<CopyOutputRequest> CreateStubForTesting();

 private:
  // Note: The StructTraits may "steal" the |result_callback_|, to allow it to
  // outlive this CopyOutputRequest (and wait for the result from another
  // process).
  friend struct mojo::StructTraits<mojom::CopyOutputRequestDataView,
                                   std::unique_ptr<CopyOutputRequest>>;

  const ResultFormat result_format_;
  CopyOutputRequestCallback result_callback_;
  scoped_refptr<base::SequencedTaskRunner> result_task_runner_;
  gfx::Vector2d scale_from_;
  gfx::Vector2d scale_to_;
  base::Optional<base::UnguessableToken> source_;
  base::Optional<gfx::Rect> area_;
  base::Optional<gfx::Rect> result_selection_;

  DISALLOW_COPY_AND_ASSIGN(CopyOutputRequest);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_FRAME_SINKS_COPY_OUTPUT_REQUEST_H_
