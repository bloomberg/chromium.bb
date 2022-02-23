// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_PICTURE_LAYER_H_
#define CC_LAYERS_PICTURE_LAYER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "cc/base/devtools_instrumentation.h"
#include "cc/base/invalidation_region.h"
#include "cc/benchmarks/micro_benchmark_controller.h"
#include "cc/layers/layer.h"

namespace cc {

class ContentLayerClient;
class DisplayItemList;
class RecordingSource;

class CC_EXPORT PictureLayer : public Layer {
 public:
  static scoped_refptr<PictureLayer> Create(ContentLayerClient* client);

  PictureLayer(const PictureLayer&) = delete;
  PictureLayer& operator=(const PictureLayer&) = delete;

  void ClearClient();

  void SetNearestNeighbor(bool nearest_neighbor);
  bool nearest_neighbor() const {
    return picture_layer_inputs_.nearest_neighbor;
  }

  void SetIsBackdropFilterMask(bool is_backdrop_filter_mask);
  bool is_backdrop_filter_mask() const {
    return picture_layer_inputs_.is_backdrop_filter_mask;
  }

  // Layer interface.
  std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;
  void SetLayerTreeHost(LayerTreeHost* host) override;
  void PushPropertiesTo(LayerImpl* layer,
                        const CommitState& commit_state,
                        const ThreadUnsafeCommitState& unsafe_state) override;
  void SetNeedsDisplayRect(const gfx::Rect& layer_rect) override;
  sk_sp<const SkPicture> GetPicture() const override;
  bool Update() override;
  void RunMicroBenchmark(MicroBenchmark* benchmark) override;
  void CaptureContent(const gfx::Rect& rect,
                      std::vector<NodeInfo>* content) const override;

  ContentLayerClient* client() { return picture_layer_inputs_.client; }

  RecordingSource* GetRecordingSourceForTesting() const {
    return recording_source_.get();
  }

  const DisplayItemList* GetDisplayItemList() const;

 protected:
  // Encapsulates all data, callbacks or interfaces received from the embedder.
  struct PictureLayerInputs {
    PictureLayerInputs();
    ~PictureLayerInputs();

    raw_ptr<ContentLayerClient> client = nullptr;
    bool nearest_neighbor = false;
    bool is_backdrop_filter_mask = false;
    scoped_refptr<DisplayItemList> display_list;
    absl::optional<gfx::Size> directly_composited_image_size = absl::nullopt;
  };

  explicit PictureLayer(ContentLayerClient* client);
  // Allow tests to inject a recording source.
  PictureLayer(ContentLayerClient* client,
               std::unique_ptr<RecordingSource> source);
  ~PictureLayer() override;

  bool HasDrawableContent() const override;

  PictureLayerInputs picture_layer_inputs_;

 private:
  friend class TestSerializationPictureLayer;

  // Called on impl thread
  void DropRecordingSourceContentIfInvalid(int source_frame_number);

  std::unique_ptr<RecordingSource> recording_source_;
  devtools_instrumentation::
      ScopedLayerObjectTracker instrumentation_object_tracker_;

  Region last_updated_invalidation_;

  int update_source_frame_number_;
};

}  // namespace cc

#endif  // CC_LAYERS_PICTURE_LAYER_H_
