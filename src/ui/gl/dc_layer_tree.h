// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_DC_LAYER_TREE_H_
#define UI_GL_DC_LAYER_TREE_H_

#include <windows.h>
#include <d3d11.h>
#include <dcomp.h>
#include <wrl/client.h>

#include <memory>

#include "base/containers/flat_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/gfx/color_space_win.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/dc_renderer_layer_params.h"
#include "ui/gl/delegated_ink_point_renderer_gpu.h"
#include "ui/gl/hdr_metadata_helper_win.h"

namespace gfx {
namespace mojom {
class DelegatedInkPointRenderer;
}  // namespace mojom
class DelegatedInkMetadata;
}  // namespace gfx

namespace gl {

class DirectCompositionChildSurfaceWin;
class SwapChainPresenter;

enum class VideoProcessorType { kSDR, kHDR };

// Cache video processor and its size.
struct VideoProcessorWrapper {
  VideoProcessorWrapper();
  ~VideoProcessorWrapper();
  VideoProcessorWrapper(VideoProcessorWrapper&& other);
  VideoProcessorWrapper& operator=(VideoProcessorWrapper&& other);
  VideoProcessorWrapper(const VideoProcessorWrapper&) = delete;
  VideoProcessorWrapper& operator=(VideoProcessorWrapper& other) = delete;

  // Input and output size of video processor .
  gfx::Size video_input_size;
  gfx::Size video_output_size;

  // The video processor is cached so SwapChains don't have to recreate it
  // whenever they're created.
  Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device;
  Microsoft::WRL::ComPtr<ID3D11VideoContext> video_context;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessor> video_processor;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator>
      video_processor_enumerator;
};

// DCLayerTree manages a tree of direct composition visuals, and associated
// swap chains for given overlay layers.  It maintains a list of pending layers
// submitted using ScheduleDCLayer() that are presented and committed in
// CommitAndClearPendingOverlays().
class DCLayerTree {
 public:
  using VideoProcessorMap =
      base::flat_map<VideoProcessorType, VideoProcessorWrapper>;
  using DelegatedInkRenderer =
      DelegatedInkPointRendererGpu<IDCompositionInkTrailDevice,
                                   IDCompositionDelegatedInkTrail,
                                   DCompositionInkTrailPoint>;

  DCLayerTree(bool disable_nv12_dynamic_textures,
              bool disable_vp_scaling,
              bool no_downscaled_overlay_promotion);

  DCLayerTree(const DCLayerTree&) = delete;
  DCLayerTree& operator=(const DCLayerTree&) = delete;

  ~DCLayerTree();

  // Returns true on success.
  bool Initialize(HWND window);

  // Present pending overlay layers, and perform a direct composition commit if
  // necessary.  Returns true if presentation and commit succeeded.
  bool CommitAndClearPendingOverlays(
      DirectCompositionChildSurfaceWin* root_surface);

  // Schedule an overlay layer for the next CommitAndClearPendingOverlays call.
  bool ScheduleDCLayer(std::unique_ptr<ui::DCRendererLayerParams> params);

  // Called by SwapChainPresenter to initialize video processor that can handle
  // at least given input and output size.  The video processor is shared across
  // layers so the same one can be reused if it's large enough.  Returns true on
  // success.
  VideoProcessorWrapper* InitializeVideoProcessor(const gfx::Size& input_size,
                                                  const gfx::Size& output_size,
                                                  bool is_hdr_output);

  void SetNeedsRebuildVisualTree() { needs_rebuild_visual_tree_ = true; }

  bool disable_nv12_dynamic_textures() const {
    return disable_nv12_dynamic_textures_;
  }

  bool disable_vp_scaling() const { return disable_vp_scaling_; }

  bool no_downscaled_overlay_promotion() const {
    return no_downscaled_overlay_promotion_;
  }

  VideoProcessorWrapper& GetOrCreateVideoProcessor(bool is_hdr);

  Microsoft::WRL::ComPtr<IDXGISwapChain1> GetLayerSwapChainForTesting(
      size_t index) const;

  void GetSwapChainVisualInfoForTesting(size_t index,
                                        gfx::Transform* transform,
                                        gfx::Point* offset,
                                        gfx::Rect* clip_rect) const;

  void SetFrameRate(float frame_rate);

  const std::unique_ptr<HDRMetadataHelperWin>& GetHDRMetadataHelper() {
    return hdr_metadata_helper_;
  }

  HWND window() const { return window_; }

  bool SupportsDelegatedInk();

  void SetDelegatedInkTrailStartPoint(
      std::unique_ptr<gfx::DelegatedInkMetadata>);

  void InitDelegatedInkPointRendererReceiver(
      mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer>
          pending_receiver);

  DelegatedInkRenderer* GetInkRendererForTesting() const {
    return ink_renderer_.get();
  }

 private:
  // This will add an ink visual to the visual tree to enable delegated ink
  // trails. This will initially always be called directly before an OS
  // delegated ink API is used. After that, it can also be added anytime the
  // visual tree is rebuilt.
  void AddDelegatedInkVisualToTree();

  // The ink renderer must be initialized before an OS API is used in order to
  // set up the delegated ink visual and delegated ink trail object.
  bool InitializeInkRenderer();

  const bool disable_nv12_dynamic_textures_;
  const bool disable_vp_scaling_;
  const bool no_downscaled_overlay_promotion_;

  HWND window_;
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  Microsoft::WRL::ComPtr<IDCompositionDevice2> dcomp_device_;
  Microsoft::WRL::ComPtr<IDCompositionTarget> dcomp_target_;

  // Store video processor for SDR/HDR mode separately, which could avoid
  // problem in (http://crbug.com/1121061).
  VideoProcessorMap video_processor_map_;

  // Current video processor input and output colorspace.
  gfx::ColorSpace video_input_color_space_;
  gfx::ColorSpace video_output_color_space_;

  // Set to true if a direct composition visual tree needs rebuild.
  bool needs_rebuild_visual_tree_ = false;

  // Set if root surface is using a swap chain currently.
  Microsoft::WRL::ComPtr<IDXGISwapChain1> root_swap_chain_;

  // Set if root surface is using a direct composition surface currently.
  Microsoft::WRL::ComPtr<IDCompositionSurface> root_dcomp_surface_;
  uint64_t root_dcomp_surface_serial_;

  // Direct composition visual for root surface.
  Microsoft::WRL::ComPtr<IDCompositionVisual2> root_surface_visual_;

  // Root direct composition visual for window dcomp target.
  Microsoft::WRL::ComPtr<IDCompositionVisual2> dcomp_root_visual_;

  // List of pending overlay layers from ScheduleDCLayer().
  std::vector<std::unique_ptr<ui::DCRendererLayerParams>> pending_overlays_;

  // List of swap chain presenters for previous frame.
  std::vector<std::unique_ptr<SwapChainPresenter>> video_swap_chains_;

  // Number of frames per second.
  float frame_rate_ = 0.f;

  // dealing with hdr metadata
  std::unique_ptr<HDRMetadataHelperWin> hdr_metadata_helper_;

  // Renderer for drawing delegated ink trails using OS APIs. This is created
  // when the DCLayerTree is created, but can only be queried to check if the
  // platform supports delegated ink trails. It must be initialized via the
  // Initialize() method in order to be used for drawing delegated ink trails.
  std::unique_ptr<DelegatedInkRenderer> ink_renderer_;
};

}  // namespace gl

#endif  // UI_GL_DC_LAYER_TREE_H_
