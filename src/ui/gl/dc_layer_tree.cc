// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <d3d11_1.h>

#include "ui/gl/dc_layer_tree.h"

#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "ui/gl/direct_composition_child_surface_win.h"
#include "ui/gl/direct_composition_surface_win.h"
#include "ui/gl/swap_chain_presenter.h"

namespace gl {
namespace {
bool SizeContains(const gfx::Size& a, const gfx::Size& b) {
  return gfx::Rect(a).Contains(gfx::Rect(b));
}
}  // namespace

DCLayerTree::DCLayerTree(bool disable_nv12_dynamic_textures,
                         bool disable_vp_scaling,
                         bool reset_vp_when_colorspace_changes)
    : disable_nv12_dynamic_textures_(disable_nv12_dynamic_textures),
      disable_vp_scaling_(disable_vp_scaling),
      reset_vp_when_colorspace_changes_(reset_vp_when_colorspace_changes) {}

DCLayerTree::~DCLayerTree() = default;

bool DCLayerTree::Initialize(
    HWND window,
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
    Microsoft::WRL::ComPtr<IDCompositionDevice2> dcomp_device) {
  DCHECK(window);
  window_ = window;
  DCHECK(d3d11_device);
  d3d11_device_ = std::move(d3d11_device);
  DCHECK(dcomp_device);
  dcomp_device_ = std::move(dcomp_device);

  Microsoft::WRL::ComPtr<IDCompositionDesktopDevice> desktop_device;
  dcomp_device_.As(&desktop_device);
  DCHECK(desktop_device);

  HRESULT hr =
      desktop_device->CreateTargetForHwnd(window_, TRUE, &dcomp_target_);
  if (FAILED(hr)) {
    DLOG(ERROR) << "CreateTargetForHwnd failed with error 0x" << std::hex << hr;
    return false;
  }

  dcomp_device_->CreateVisual(&dcomp_root_visual_);
  DCHECK(dcomp_root_visual_);
  dcomp_target_->SetRoot(dcomp_root_visual_.Get());
  // A visual inherits the interpolation mode of the parent visual by default.
  // If no visuals set the interpolation mode, the default for the entire visual
  // tree is nearest neighbor interpolation.
  // Set the interpolation mode to Linear to get a better upscaling quality.
  dcomp_root_visual_->SetBitmapInterpolationMode(
      DCOMPOSITION_BITMAP_INTERPOLATION_MODE_LINEAR);

  hdr_metadata_helper_ = std::make_unique<HDRMetadataHelperWin>(d3d11_device_);

  return true;
}

bool DCLayerTree::InitializeVideoProcessor(
    const gfx::Size& input_size,
    const gfx::Size& output_size,
    const gfx::ColorSpace& input_color_space,
    const gfx::ColorSpace& output_color_space,
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain,
    bool is_yuv_swapchain) {
  if (!video_device_) {
    // This can fail if the D3D device is "Microsoft Basic Display Adapter".
    if (FAILED(d3d11_device_.As(&video_device_))) {
      DLOG(ERROR) << "Failed to retrieve video device from D3D11 device";
      DCHECK(false);
      DirectCompositionSurfaceWin::DisableOverlays();
      return false;
    }
    DCHECK(video_device_);

    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    d3d11_device_->GetImmediateContext(&context);
    DCHECK(context);
    context.As(&video_context_);
    DCHECK(video_context_);
  }

  bool colorspace_changed = !(input_color_space == video_input_color_space_ &&
                              output_color_space == video_output_color_space_ &&
                              is_yuv_video_output_ == is_yuv_swapchain);
  if (video_processor_ && SizeContains(video_input_size_, input_size) &&
      SizeContains(video_output_size_, output_size) &&
      !(colorspace_changed && reset_vp_when_colorspace_changes_)) {
    if (colorspace_changed) {
      SetColorSpaceForVideoProcessor(input_color_space, output_color_space,
                                     std::move(swap_chain), is_yuv_swapchain);
    }
    return true;
  }
  TRACE_EVENT2("gpu", "DCLayerTree::InitializeVideoProcessor", "input_size",
               input_size.ToString(), "output_size", output_size.ToString());
  video_input_size_ = input_size;
  video_output_size_ = output_size;

  video_processor_.Reset();
  video_processor_enumerator_.Reset();
  D3D11_VIDEO_PROCESSOR_CONTENT_DESC desc = {};
  desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
  desc.InputFrameRate.Numerator = 60;
  desc.InputFrameRate.Denominator = 1;
  desc.InputWidth = input_size.width();
  desc.InputHeight = input_size.height();
  desc.OutputFrameRate.Numerator = 60;
  desc.OutputFrameRate.Denominator = 1;
  desc.OutputWidth = output_size.width();
  desc.OutputHeight = output_size.height();
  desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;
  HRESULT hr = video_device_->CreateVideoProcessorEnumerator(
      &desc, &video_processor_enumerator_);
  base::UmaHistogramSparse(
      "GPU.DirectComposition.CreateVideoProcessorEnumerator", hr);
  if (FAILED(hr)) {
    DLOG(ERROR) << "CreateVideoProcessorEnumerator failed with error 0x"
                << std::hex << hr;
    // It might fail again next time. Disable overlay support so
    // overlay processor will stop sending down overlay frames.
    DirectCompositionSurfaceWin::DisableOverlays();
    return false;
  }

  hr = video_device_->CreateVideoProcessor(video_processor_enumerator_.Get(), 0,
                                           &video_processor_);
  base::UmaHistogramSparse(
      "GPU.DirectComposition.VideoDeviceCreateVideoProcessor", hr);
  if (FAILED(hr)) {
    DLOG(ERROR) << "CreateVideoProcessor failed with error 0x" << std::hex
                << hr;
    // It might fail again next time. Disable overlay support so
    // overlay processor will stop sending down overlay frames.
    DirectCompositionSurfaceWin::DisableOverlays();
    return false;
  }
  // Auto stream processing (the default) can hurt power consumption.
  video_context_->VideoProcessorSetStreamAutoProcessingMode(
      video_processor_.Get(), 0, FALSE);
  SetColorSpaceForVideoProcessor(input_color_space, output_color_space,
                                 std::move(swap_chain), is_yuv_swapchain);
  return true;
}

void DCLayerTree::SetColorSpaceForVideoProcessor(
    const gfx::ColorSpace& input_color_space,
    const gfx::ColorSpace& output_color_space,
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain,
    bool is_yuv_swapchain) {
  Microsoft::WRL::ComPtr<IDXGISwapChain3> swap_chain3;
  Microsoft::WRL::ComPtr<ID3D11VideoContext1> context1;
  if (SUCCEEDED(swap_chain.As(&swap_chain3)) &&
      SUCCEEDED(video_context_.As(&context1))) {
    DCHECK(swap_chain3);
    DCHECK(context1);
    // Set input color space.
    context1->VideoProcessorSetStreamColorSpace1(
        video_processor_.Get(), 0,
        gfx::ColorSpaceWin::GetDXGIColorSpace(input_color_space));
    // Set output color space.
    DXGI_COLOR_SPACE_TYPE output_dxgi_color_space =
        gfx::ColorSpaceWin::GetDXGIColorSpace(output_color_space,
                                              /*force_yuv=*/is_yuv_swapchain);

    if (SUCCEEDED(swap_chain3->SetColorSpace1(output_dxgi_color_space))) {
      context1->VideoProcessorSetOutputColorSpace1(video_processor_.Get(),
                                                   output_dxgi_color_space);
    }
  } else {
    // This can't handle as many different types of color spaces, so use it
    // only if ID3D11VideoContext1 isn't available.
    D3D11_VIDEO_PROCESSOR_COLOR_SPACE src_d3d11_color_space =
        gfx::ColorSpaceWin::GetD3D11ColorSpace(input_color_space);
    video_context_->VideoProcessorSetStreamColorSpace(video_processor_.Get(), 0,
                                                      &src_d3d11_color_space);
    D3D11_VIDEO_PROCESSOR_COLOR_SPACE output_d3d11_color_space =
        gfx::ColorSpaceWin::GetD3D11ColorSpace(output_color_space);
    video_context_->VideoProcessorSetOutputColorSpace(
        video_processor_.Get(), &output_d3d11_color_space);
  }
  video_input_color_space_ = input_color_space;
  video_output_color_space_ = output_color_space;
  is_yuv_video_output_ = is_yuv_swapchain;
}

Microsoft::WRL::ComPtr<IDXGISwapChain1>
DCLayerTree::GetLayerSwapChainForTesting(size_t index) const {
  if (index < video_swap_chains_.size())
    return video_swap_chains_[index]->swap_chain();
  return nullptr;
}

void DCLayerTree::GetSwapChainVisualInfoForTesting(size_t index,
                                                   gfx::Transform* transform,
                                                   gfx::Point* offset,
                                                   gfx::Rect* clip_rect) const {
  if (index < video_swap_chains_.size()) {
    video_swap_chains_[index]->GetSwapChainVisualInfoForTesting(  // IN-TEST
        transform, offset, clip_rect);
  }
}

bool DCLayerTree::CommitAndClearPendingOverlays(
    DirectCompositionChildSurfaceWin* root_surface) {
  TRACE_EVENT1("gpu", "DCLayerTree::CommitAndClearPendingOverlays",
               "num_pending_overlays", pending_overlays_.size());
  DCHECK(!needs_rebuild_visual_tree_);
  bool needs_commit = false;
  // Check if root surface visual needs a commit first.
  if (!root_surface_visual_) {
    dcomp_device_->CreateVisual(&root_surface_visual_);
    needs_rebuild_visual_tree_ = true;
  }

  if (root_surface->swap_chain() != root_swap_chain_ ||
      root_surface->dcomp_surface() != root_dcomp_surface_) {
    root_swap_chain_ = root_surface->swap_chain();
    root_dcomp_surface_ = root_surface->dcomp_surface();
    root_surface_visual_->SetContent(
        root_swap_chain_ ? static_cast<IUnknown*>(root_swap_chain_.Get())
                         : static_cast<IUnknown*>(root_dcomp_surface_.Get()));
    needs_rebuild_visual_tree_ = true;
  }

  // dcomp_surface data is updated. But visual tree is not affected.
  // Just needs a commit.
  if (root_surface->dcomp_surface_serial() != root_dcomp_surface_serial_) {
    root_dcomp_surface_serial_ = root_surface->dcomp_surface_serial();
    needs_commit = true;
  }

  std::vector<std::unique_ptr<ui::DCRendererLayerParams>> overlays;
  std::swap(pending_overlays_, overlays);

  // Sort layers by z-order.
  std::sort(overlays.begin(), overlays.end(),
            [](const auto& a, const auto& b) -> bool {
              return a->z_order < b->z_order;
            });

  // If we need to grow or shrink swap chain presenters, we'll need to add or
  // remove visuals.
  if (video_swap_chains_.size() != overlays.size()) {
    // Grow or shrink list of swap chain presenters to match pending overlays.
    std::vector<std::unique_ptr<SwapChainPresenter>> new_video_swap_chains;
    for (size_t i = 0; i < overlays.size(); ++i) {
      // TODO(sunnyps): Try to find a matching swap chain based on size, type of
      // swap chain, gl image, etc.
      if (i < video_swap_chains_.size()) {
        new_video_swap_chains.emplace_back(std::move(video_swap_chains_[i]));
      } else {
        new_video_swap_chains.emplace_back(std::make_unique<SwapChainPresenter>(
            this, window_, d3d11_device_, dcomp_device_));
        if (frame_rate_ > 0)
          new_video_swap_chains.back()->SetFrameRate(frame_rate_);
      }
    }
    video_swap_chains_.swap(new_video_swap_chains);
    needs_rebuild_visual_tree_ = true;
  }

  // Present to each swap chain.
  for (size_t i = 0; i < overlays.size(); ++i) {
    auto& video_swap_chain = video_swap_chains_[i];
    if (!video_swap_chain->PresentToSwapChain(*overlays[i])) {
      DLOG(ERROR) << "PresentToSwapChain failed";
      return false;
    }
  }

  // Rebuild visual tree and commit if any visual changed.
  // Note: needs_rebuild_visual_tree_ might be set in this function and in
  // SetNeedsRebuildVisualTree() during video_swap_chain->PresentToSwapChain()
  if (needs_rebuild_visual_tree_) {
    TRACE_EVENT0(
        "gpu", "DCLayerTree::CommitAndClearPendingOverlays::ReBuildVisualTree");
    needs_rebuild_visual_tree_ = false;
    dcomp_root_visual_->RemoveAllVisuals();

    // Add layers with negative z-order first.
    size_t i = 0;
    for (; i < overlays.size() && overlays[i]->z_order < 0; ++i) {
      IDCompositionVisual2* visual = video_swap_chains_[i]->visual().Get();
      // We call AddVisual with insertAbove FALSE and referenceVisual nullptr
      // which is equivalent to saying that the visual should be below no other
      // visual, or in other words it should be above all other visuals.
      dcomp_root_visual_->AddVisual(visual, FALSE, nullptr);
    }

    // Add root surface visual at z-order 0.
    dcomp_root_visual_->AddVisual(root_surface_visual_.Get(), FALSE, nullptr);

    // Add visuals with positive z-order.
    for (; i < overlays.size(); ++i) {
      // There shouldn't be a layer with z-order 0.  Otherwise, we can't tell
      // its order with respect to root surface.
      DCHECK_GT(overlays[i]->z_order, 0);
      IDCompositionVisual2* visual = video_swap_chains_[i]->visual().Get();
      dcomp_root_visual_->AddVisual(visual, FALSE, nullptr);
    }
    needs_commit = true;
  }

  if (needs_commit) {
    TRACE_EVENT0("gpu", "DCLayerTree::CommitAndClearPendingOverlays::Commit");
    HRESULT hr = dcomp_device_->Commit();
    if (FAILED(hr)) {
      DLOG(ERROR) << "Commit failed with error 0x" << std::hex << hr;
      return false;
    }
  }

  return true;
}

bool DCLayerTree::ScheduleDCLayer(const ui::DCRendererLayerParams& params) {
  pending_overlays_.push_back(
      std::make_unique<ui::DCRendererLayerParams>(params));
  return true;
}

void DCLayerTree::SetFrameRate(float frame_rate) {
  frame_rate_ = frame_rate;
  for (size_t ii = 0; ii < video_swap_chains_.size(); ++ii)
    video_swap_chains_[ii]->SetFrameRate(frame_rate);
}

}  // namespace gl
