// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/direct_composition_surface_win.h"

#include "base/bind_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_select_object.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/win/hidden_window.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gdi_util.h"
#include "ui/gfx/transform.h"
#include "ui/gl/dc_renderer_layer_params.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_dxgi.h"
#include "ui/gl/gl_image_ref_counted_memory.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/win/win_window.h"

namespace gpu {
namespace {

bool CheckIfDCSupported() {
  if (!gl::QueryDirectCompositionDevice(
          gl::QueryD3D11DeviceObjectFromANGLE())) {
    LOG(WARNING)
        << "GL implementation not using DirectComposition, skipping test.";
    return false;
  }
  return true;
}

class TestImageTransportSurfaceDelegate
    : public ImageTransportSurfaceDelegate,
      public base::SupportsWeakPtr<TestImageTransportSurfaceDelegate> {
 public:
  TestImageTransportSurfaceDelegate()
      : feature_info_(new gpu::gles2::FeatureInfo()) {}

  ~TestImageTransportSurfaceDelegate() override {}

  // ImageTransportSurfaceDelegate implementation.
  void DidCreateAcceleratedSurfaceChildWindow(
      SurfaceHandle parent_window,
      SurfaceHandle child_window) override {
    if (parent_window)
      ::SetParent(child_window, parent_window);
  }
  void DidSwapBuffersComplete(SwapBuffersCompleteParams params) override {}
  const gles2::FeatureInfo* GetFeatureInfo() const override {
    return feature_info_.get();
  }
  const GpuPreferences& GetGpuPreferences() const override {
    return gpu_preferences_;
  }
  void BufferPresented(const gfx::PresentationFeedback& feedback) override {}
  void AddFilter(IPC::MessageFilter* message_filter) override {}
  int32_t GetRouteID() const override { return 0; }

 private:
  scoped_refptr<gpu::gles2::FeatureInfo> feature_info_;
  GpuPreferences gpu_preferences_;
};

class TestPlatformDelegate : public ui::PlatformWindowDelegate {
 public:
  // ui::PlatformWindowDelegate implementation.
  void OnBoundsChanged(const gfx::Rect& new_bounds) override {}
  void OnDamageRect(const gfx::Rect& damaged_region) override {}
  void DispatchEvent(ui::Event* event) override {}
  void OnCloseRequest() override {}
  void OnClosed() override {}
  void OnWindowStateChanged(ui::PlatformWindowState new_state) override {}
  void OnLostCapture() override {}
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) override {}
  void OnAcceleratedWidgetDestroyed() override {}
  void OnActivationChanged(bool active) override {}
};

void RunPendingTasks(scoped_refptr<base::TaskRunner> task_runner) {
  base::WaitableEvent done(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  task_runner->PostTask(FROM_HERE,
                        Bind(&base::WaitableEvent::Signal, Unretained(&done)));
  done.Wait();
}

void DestroySurface(scoped_refptr<DirectCompositionSurfaceWin> surface) {
  scoped_refptr<base::TaskRunner> task_runner =
      surface->GetWindowTaskRunnerForTesting();
  DCHECK(surface->HasOneRef());

  surface = nullptr;

  // Ensure that the ChildWindowWin posts the task to delete the thread to the
  // main loop before doing RunUntilIdle. Otherwise the child threads could
  // outlive the main thread.
  RunPendingTasks(task_runner);

  base::RunLoop().RunUntilIdle();
}

Microsoft::WRL::ComPtr<ID3D11Texture2D> CreateNV12Texture(
    const Microsoft::WRL::ComPtr<ID3D11Device>& d3d11_device,
    const gfx::Size& size,
    bool shared) {
  D3D11_TEXTURE2D_DESC desc = {};
  desc.Width = size.width();
  desc.Height = size.height();
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_NV12;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.SampleDesc.Count = 1;
  desc.BindFlags = 0;
  if (shared) {
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX |
                     D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
  }

  std::vector<char> image_data(size.width() * size.height() * 3 / 2);
  // Y, U, and V should all be 160. Output color should be pink.
  memset(&image_data[0], 160, size.width() * size.height() * 3 / 2);

  D3D11_SUBRESOURCE_DATA data = {};
  data.pSysMem = (const void*)&image_data[0];
  data.SysMemPitch = size.width();

  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  HRESULT hr =
      d3d11_device->CreateTexture2D(&desc, &data, texture.GetAddressOf());
  CHECK(SUCCEEDED(hr));
  return texture;
}

TEST(DirectCompositionSurfaceTest, TestMakeCurrent) {
  if (!CheckIfDCSupported())
    return;

  TestImageTransportSurfaceDelegate delegate;

  scoped_refptr<DirectCompositionSurfaceWin> surface1(
      new DirectCompositionSurfaceWin(nullptr, delegate.AsWeakPtr(),
                                      ui::GetHiddenWindow()));
  EXPECT_TRUE(surface1->Initialize(gl::GLSurfaceFormat()));

  scoped_refptr<gl::GLContext> context1 = gl::init::CreateGLContext(
      nullptr, surface1.get(), gl::GLContextAttribs());
  EXPECT_TRUE(context1->MakeCurrent(surface1.get()));

  surface1->SetEnableDCLayers(true);
  EXPECT_TRUE(surface1->Resize(gfx::Size(100, 100), 1.0,
                               gl::GLSurface::ColorSpace::UNSPECIFIED, true));

  // First SetDrawRectangle must be full size of surface.
  EXPECT_FALSE(surface1->SetDrawRectangle(gfx::Rect(0, 0, 50, 50)));
  EXPECT_TRUE(surface1->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));

  // SetDrawRectangle can't be called again until swap.
  EXPECT_FALSE(surface1->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface1->SwapBuffers(base::DoNothing()));

  EXPECT_TRUE(context1->IsCurrent(surface1.get()));

  // SetDrawRectangle must be contained within surface.
  EXPECT_FALSE(surface1->SetDrawRectangle(gfx::Rect(0, 0, 101, 101)));
  EXPECT_TRUE(surface1->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));
  EXPECT_TRUE(context1->IsCurrent(surface1.get()));

  EXPECT_TRUE(surface1->Resize(gfx::Size(50, 50), 1.0,
                               gl::GLSurface::ColorSpace::UNSPECIFIED, true));
  EXPECT_TRUE(context1->IsCurrent(surface1.get()));
  EXPECT_TRUE(surface1->SetDrawRectangle(gfx::Rect(0, 0, 50, 50)));
  EXPECT_TRUE(context1->IsCurrent(surface1.get()));

  scoped_refptr<DirectCompositionSurfaceWin> surface2(
      new DirectCompositionSurfaceWin(nullptr, delegate.AsWeakPtr(),
                                      ui::GetHiddenWindow()));
  EXPECT_TRUE(surface2->Initialize(gl::GLSurfaceFormat()));

  scoped_refptr<gl::GLContext> context2 = gl::init::CreateGLContext(
      nullptr, surface2.get(), gl::GLContextAttribs());
  EXPECT_TRUE(context2->MakeCurrent(surface2.get()));

  surface2->SetEnableDCLayers(true);
  EXPECT_TRUE(surface2->Resize(gfx::Size(100, 100), 1.0,
                               gl::GLSurface::ColorSpace::UNSPECIFIED, true));
  // The previous IDCompositionSurface should be suspended when another
  // surface is being drawn to.
  EXPECT_TRUE(surface2->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));
  EXPECT_TRUE(context2->IsCurrent(surface2.get()));

  // It should be possible to switch back to the previous surface and
  // unsuspend it.
  EXPECT_TRUE(context1->MakeCurrent(surface1.get()));
  context2 = nullptr;
  context1 = nullptr;

  DestroySurface(std::move(surface1));
  DestroySurface(std::move(surface2));
}

// Tests that switching using EnableDCLayers works.
TEST(DirectCompositionSurfaceTest, DXGIDCLayerSwitch) {
  if (!CheckIfDCSupported())
    return;

  TestImageTransportSurfaceDelegate delegate;

  scoped_refptr<DirectCompositionSurfaceWin> surface(
      new DirectCompositionSurfaceWin(nullptr, delegate.AsWeakPtr(),
                                      ui::GetHiddenWindow()));
  EXPECT_TRUE(surface->Initialize(gl::GLSurfaceFormat()));

  scoped_refptr<gl::GLContext> context =
      gl::init::CreateGLContext(nullptr, surface.get(), gl::GLContextAttribs());
  EXPECT_TRUE(context->MakeCurrent(surface.get()));

  EXPECT_TRUE(surface->Resize(gfx::Size(100, 100), 1.0,
                              gl::GLSurface::ColorSpace::UNSPECIFIED, true));
  EXPECT_FALSE(surface->GetBackbufferSwapChainForTesting());

  // First SetDrawRectangle must be full size of surface for DXGI swapchain.
  EXPECT_FALSE(surface->SetDrawRectangle(gfx::Rect(0, 0, 50, 50)));
  EXPECT_TRUE(surface->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));
  EXPECT_TRUE(surface->GetBackbufferSwapChainForTesting());

  // SetDrawRectangle and SetEnableDCLayers can't be called again until swap.
  EXPECT_FALSE(surface->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, surface->SwapBuffers(base::DoNothing()));
  EXPECT_TRUE(context->IsCurrent(surface.get()));

  surface->SetEnableDCLayers(true);

  // Surface switched to use IDCompositionSurface, so must draw to entire
  // surface.
  EXPECT_FALSE(surface->SetDrawRectangle(gfx::Rect(0, 0, 50, 50)));
  EXPECT_TRUE(surface->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));
  EXPECT_FALSE(surface->GetBackbufferSwapChainForTesting());

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, surface->SwapBuffers(base::DoNothing()));
  EXPECT_TRUE(context->IsCurrent(surface.get()));

  surface->SetEnableDCLayers(false);

  // Surface switched to use IDXGISwapChain, so must draw to entire surface.
  EXPECT_FALSE(surface->SetDrawRectangle(gfx::Rect(0, 0, 50, 50)));
  EXPECT_TRUE(surface->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));
  EXPECT_TRUE(surface->GetBackbufferSwapChainForTesting());

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, surface->SwapBuffers(base::DoNothing()));
  EXPECT_TRUE(context->IsCurrent(surface.get()));

  context = nullptr;
  DestroySurface(std::move(surface));
}

// Ensure that the swapchain's alpha is correct.
TEST(DirectCompositionSurfaceTest, SwitchAlpha) {
  if (!CheckIfDCSupported())
    return;

  TestImageTransportSurfaceDelegate delegate;

  scoped_refptr<DirectCompositionSurfaceWin> surface(
      new DirectCompositionSurfaceWin(nullptr, delegate.AsWeakPtr(),
                                      ui::GetHiddenWindow()));
  EXPECT_TRUE(surface->Initialize(gl::GLSurfaceFormat()));

  scoped_refptr<gl::GLContext> context =
      gl::init::CreateGLContext(nullptr, surface.get(), gl::GLContextAttribs());
  EXPECT_TRUE(context->MakeCurrent(surface.get()));

  EXPECT_TRUE(surface->Resize(gfx::Size(100, 100), 1.0,
                              gl::GLSurface::ColorSpace::UNSPECIFIED, true));
  EXPECT_FALSE(surface->GetBackbufferSwapChainForTesting());

  EXPECT_TRUE(surface->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      surface->GetBackbufferSwapChainForTesting();
  ASSERT_TRUE(swap_chain);
  DXGI_SWAP_CHAIN_DESC1 desc;
  swap_chain->GetDesc1(&desc);
  EXPECT_EQ(DXGI_ALPHA_MODE_PREMULTIPLIED, desc.AlphaMode);

  // Resize to the same parameters should have no effect.
  EXPECT_TRUE(surface->Resize(gfx::Size(100, 100), 1.0,
                              gl::GLSurface::ColorSpace::UNSPECIFIED, true));
  EXPECT_TRUE(surface->GetBackbufferSwapChainForTesting());

  EXPECT_TRUE(surface->Resize(gfx::Size(100, 100), 1.0,
                              gl::GLSurface::ColorSpace::UNSPECIFIED, false));
  EXPECT_FALSE(surface->GetBackbufferSwapChainForTesting());

  EXPECT_TRUE(surface->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));

  swap_chain = surface->GetBackbufferSwapChainForTesting();
  ASSERT_TRUE(swap_chain);
  swap_chain->GetDesc1(&desc);
  EXPECT_EQ(DXGI_ALPHA_MODE_IGNORE, desc.AlphaMode);

  context = nullptr;
  DestroySurface(std::move(surface));
}

// Ensure that the GLImage isn't presented again unless it changes.
TEST(DirectCompositionSurfaceTest, NoPresentTwice) {
  if (!CheckIfDCSupported())
    return;

  TestImageTransportSurfaceDelegate delegate;
  scoped_refptr<DirectCompositionSurfaceWin> surface(
      new DirectCompositionSurfaceWin(nullptr, delegate.AsWeakPtr(),
                                      ui::GetHiddenWindow()));
  EXPECT_TRUE(surface->Initialize(gl::GLSurfaceFormat()));

  scoped_refptr<gl::GLContext> context =
      gl::init::CreateGLContext(nullptr, surface.get(), gl::GLContextAttribs());
  EXPECT_TRUE(context->MakeCurrent(surface.get()));

  surface->SetEnableDCLayers(true);

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      gl::QueryD3D11DeviceObjectFromANGLE();

  gfx::Size texture_size(50, 50);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size, false);

  scoped_refptr<gl::GLImageDXGI> image_dxgi(
      new gl::GLImageDXGI(texture_size, nullptr));
  image_dxgi->SetTexture(texture, 0);
  image_dxgi->SetColorSpace(gfx::ColorSpace::CreateREC709());

  ui::DCRendererLayerParams params;
  params.y_image = image_dxgi;
  params.uv_image = image_dxgi;
  params.content_rect = gfx::Rect(texture_size);
  params.quad_rect = gfx::Rect(100, 100);
  surface->ScheduleDCLayer(params);

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      surface->GetLayerSwapChainForTesting(0);
  ASSERT_FALSE(swap_chain);

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, surface->SwapBuffers(base::DoNothing()));

  swap_chain = surface->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain);

  UINT last_present_count = 0;
  EXPECT_TRUE(SUCCEEDED(swap_chain->GetLastPresentCount(&last_present_count)));

  // One present is normal, and a second present because it's the first frame
  // and the other buffer needs to be drawn to.
  EXPECT_EQ(2u, last_present_count);

  surface->ScheduleDCLayer(params);
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, surface->SwapBuffers(base::DoNothing()));

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain2 =
      surface->GetLayerSwapChainForTesting(0);
  EXPECT_EQ(swap_chain2.Get(), swap_chain.Get());

  // It's the same image, so it should have the same swapchain.
  EXPECT_TRUE(SUCCEEDED(swap_chain->GetLastPresentCount(&last_present_count)));
  EXPECT_EQ(2u, last_present_count);

  // The image changed, we should get a new present
  scoped_refptr<gl::GLImageDXGI> image_dxgi2(
      new gl::GLImageDXGI(texture_size, nullptr));
  image_dxgi2->SetTexture(texture, 0);
  image_dxgi2->SetColorSpace(gfx::ColorSpace::CreateREC709());

  params.y_image = image_dxgi2;
  params.uv_image = image_dxgi2;
  surface->ScheduleDCLayer(params);

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, surface->SwapBuffers(base::DoNothing()));

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain3 =
      surface->GetLayerSwapChainForTesting(0);
  EXPECT_TRUE(SUCCEEDED(swap_chain3->GetLastPresentCount(&last_present_count)));
  // the present count should increase with the new present
  EXPECT_EQ(3u, last_present_count);

  context = nullptr;
  DestroySurface(std::move(surface));
}

// Ensure the swapchain size is set to the correct size if HW overlay scaling
// is support - swapchain should be the minimum of the decoded
// video buffer size and the onscreen video size
TEST(DirectCompositionSurfaceTest, SwapchainSizeWithScaledOverlays) {
  if (!CheckIfDCSupported())
    return;

  TestImageTransportSurfaceDelegate delegate;
  scoped_refptr<DirectCompositionSurfaceWin> surface(
      new DirectCompositionSurfaceWin(nullptr, delegate.AsWeakPtr(),
                                      ui::GetHiddenWindow()));
  EXPECT_TRUE(surface->Initialize(gl::GLSurfaceFormat()));

  scoped_refptr<gl::GLContext> context =
      gl::init::CreateGLContext(nullptr, surface.get(), gl::GLContextAttribs());
  EXPECT_TRUE(context->MakeCurrent(surface.get()));

  surface->SetEnableDCLayers(true);

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      gl::QueryD3D11DeviceObjectFromANGLE();

  gfx::Size texture_size(64, 64);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size, false);

  scoped_refptr<gl::GLImageDXGI> image_dxgi(
      new gl::GLImageDXGI(texture_size, nullptr));
  image_dxgi->SetTexture(texture, 0);
  image_dxgi->SetColorSpace(gfx::ColorSpace::CreateREC709());

  // HW supports scaled overlays
  // The input texture size is maller than the window size.
  surface->SetScaledOverlaysSupportedForTesting(true);

  ui::DCRendererLayerParams params;
  params.y_image = image_dxgi;
  params.uv_image = image_dxgi;
  params.content_rect = gfx::Rect(texture_size);
  params.quad_rect = gfx::Rect(100, 100);
  surface->ScheduleDCLayer(params);

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, surface->SwapBuffers(base::DoNothing()));
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      surface->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain);

  DXGI_SWAP_CHAIN_DESC Desc;
  EXPECT_TRUE(SUCCEEDED(swap_chain->GetDesc(&Desc)));
  EXPECT_EQ((int)Desc.BufferDesc.Width, texture_size.width());
  EXPECT_EQ((int)Desc.BufferDesc.Height, texture_size.height());

  // Clear SwapChainPresenters
  // Must do Clear first because the swap chain won't resize immediately if
  // a new size is given unless this is the very first time after Clear.
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, surface->SwapBuffers(base::DoNothing()));

  // The input texture size is bigger than the window size.
  params.quad_rect = gfx::Rect(32, 48);

  surface->ScheduleDCLayer(params);
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, surface->SwapBuffers(base::DoNothing()));

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain2 =
      surface->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain2);

  EXPECT_TRUE(SUCCEEDED(swap_chain2->GetDesc(&Desc)));
  EXPECT_EQ((int)Desc.BufferDesc.Width, params.quad_rect.width());
  EXPECT_EQ((int)Desc.BufferDesc.Height, params.quad_rect.height());

  context = nullptr;
  DestroySurface(std::move(surface));
}

// Ensure the swapchain size is set to the correct size if HW overlay scaling
// is not support - swapchain should be the onscreen video size
TEST(DirectCompositionSurfaceTest, SwapchainSizeWithoutScaledOverlays) {
  if (!CheckIfDCSupported())
    return;

  TestImageTransportSurfaceDelegate delegate;
  scoped_refptr<DirectCompositionSurfaceWin> surface(
      new DirectCompositionSurfaceWin(nullptr, delegate.AsWeakPtr(),
                                      ui::GetHiddenWindow()));
  EXPECT_TRUE(surface->Initialize(gl::GLSurfaceFormat()));

  scoped_refptr<gl::GLContext> context =
      gl::init::CreateGLContext(nullptr, surface.get(), gl::GLContextAttribs());
  EXPECT_TRUE(context->MakeCurrent(surface.get()));

  surface->SetEnableDCLayers(true);

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      gl::QueryD3D11DeviceObjectFromANGLE();

  gfx::Size texture_size(80, 80);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size, false);

  scoped_refptr<gl::GLImageDXGI> image_dxgi(
      new gl::GLImageDXGI(texture_size, nullptr));
  image_dxgi->SetTexture(texture, 0);
  image_dxgi->SetColorSpace(gfx::ColorSpace::CreateREC709());

  // HW doesn't support scaled overlays
  // The input texture size is bigger than the window size.
  surface->SetScaledOverlaysSupportedForTesting(false);

  ui::DCRendererLayerParams params;
  params.y_image = image_dxgi;
  params.uv_image = image_dxgi;
  params.content_rect = gfx::Rect(texture_size);
  params.quad_rect = gfx::Rect(42, 42);
  surface->ScheduleDCLayer(params);

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, surface->SwapBuffers(base::DoNothing()));
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      surface->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain);

  DXGI_SWAP_CHAIN_DESC desc;
  EXPECT_TRUE(SUCCEEDED(swap_chain->GetDesc(&desc)));
  EXPECT_EQ((int)desc.BufferDesc.Width, params.quad_rect.width());
  EXPECT_EQ((int)desc.BufferDesc.Height, params.quad_rect.height());

  // The input texture size is smaller than the window size.
  params.quad_rect = gfx::Rect(124, 136);

  surface->ScheduleDCLayer(params);
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, surface->SwapBuffers(base::DoNothing()));

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain2 =
      surface->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain2);

  EXPECT_TRUE(SUCCEEDED(swap_chain2->GetDesc(&desc)));
  EXPECT_EQ((int)desc.BufferDesc.Width, params.quad_rect.width());
  EXPECT_EQ((int)desc.BufferDesc.Height, params.quad_rect.height());

  context = nullptr;
  DestroySurface(std::move(surface));
}

// Test protected video flags
TEST(DirectCompositionSurfaceTest, ProtectedVideos) {
  if (!CheckIfDCSupported())
    return;

  TestImageTransportSurfaceDelegate delegate;
  scoped_refptr<DirectCompositionSurfaceWin> surface(
      new DirectCompositionSurfaceWin(nullptr, delegate.AsWeakPtr(),
                                      ui::GetHiddenWindow()));
  EXPECT_TRUE(surface->Initialize(gl::GLSurfaceFormat()));

  scoped_refptr<gl::GLContext> context =
      gl::init::CreateGLContext(nullptr, surface.get(), gl::GLContextAttribs());
  EXPECT_TRUE(context->MakeCurrent(surface.get()));

  surface->SetEnableDCLayers(true);

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      gl::QueryD3D11DeviceObjectFromANGLE();

  gfx::Size texture_size(1280, 720);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size, false);

  scoped_refptr<gl::GLImageDXGI> image_dxgi(
      new gl::GLImageDXGI(texture_size, nullptr));
  image_dxgi->SetTexture(texture, 0);
  image_dxgi->SetColorSpace(gfx::ColorSpace::CreateREC709());
  gfx::Size window_size(640, 360);

  // Clear video
  {
    ui::DCRendererLayerParams params;
    params.y_image = image_dxgi;
    params.uv_image = image_dxgi;
    params.quad_rect = gfx::Rect(window_size);
    params.content_rect = gfx::Rect(texture_size);
    params.protected_video_type = ui::ProtectedVideoType::kClear;

    surface->ScheduleDCLayer(params);
    EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
              surface->SwapBuffers(base::DoNothing()));
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
        surface->GetLayerSwapChainForTesting(0);
    ASSERT_TRUE(swap_chain);

    DXGI_SWAP_CHAIN_DESC Desc;
    EXPECT_TRUE(SUCCEEDED(swap_chain->GetDesc(&Desc)));
    unsigned display_only_flag = Desc.Flags & DXGI_SWAP_CHAIN_FLAG_DISPLAY_ONLY;
    unsigned hw_protected_flag = Desc.Flags & DXGI_SWAP_CHAIN_FLAG_HW_PROTECTED;
    EXPECT_EQ(display_only_flag, (unsigned)0);
    EXPECT_EQ(hw_protected_flag, (unsigned)0);
  }

  // Software protected video
  {
    ui::DCRendererLayerParams params;
    params.y_image = image_dxgi;
    params.uv_image = image_dxgi;
    params.quad_rect = gfx::Rect(window_size);
    params.content_rect = gfx::Rect(texture_size);
    params.protected_video_type = ui::ProtectedVideoType::kSoftwareProtected;

    surface->ScheduleDCLayer(params);
    EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
              surface->SwapBuffers(base::DoNothing()));
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
        surface->GetLayerSwapChainForTesting(0);
    ASSERT_TRUE(swap_chain);

    DXGI_SWAP_CHAIN_DESC Desc;
    EXPECT_TRUE(SUCCEEDED(swap_chain->GetDesc(&Desc)));
    unsigned display_only_flag = Desc.Flags & DXGI_SWAP_CHAIN_FLAG_DISPLAY_ONLY;
    unsigned hw_protected_flag = Desc.Flags & DXGI_SWAP_CHAIN_FLAG_HW_PROTECTED;
    EXPECT_EQ(display_only_flag, (unsigned)DXGI_SWAP_CHAIN_FLAG_DISPLAY_ONLY);
    EXPECT_EQ(hw_protected_flag, (unsigned)0);
  }

  // TODO(magchen): Add a hardware protected video test when hardware procted
  // video support is enabled by defaut in the Intel driver and Chrome

  context = nullptr;
  DestroySurface(std::move(surface));
}

std::vector<SkColor> ReadBackWindow(HWND window, const gfx::Size& size) {
  base::win::ScopedCreateDC mem_hdc(::CreateCompatibleDC(nullptr));
  DCHECK(mem_hdc.IsValid());

  BITMAPV4HEADER hdr;
  gfx::CreateBitmapV4Header(size.width(), size.height(), &hdr);

  void* bits = nullptr;
  base::win::ScopedBitmap bitmap(
      ::CreateDIBSection(mem_hdc.Get(), reinterpret_cast<BITMAPINFO*>(&hdr),
                         DIB_RGB_COLORS, &bits, nullptr, 0));
  DCHECK(bitmap.is_valid());

  base::win::ScopedSelectObject select_object(mem_hdc.Get(), bitmap.get());

  // Grab a copy of the window. Use PrintWindow because it works even when the
  // window's partially occluded. The PW_RENDERFULLCONTENT flag is undocumented,
  // but works starting in Windows 8.1. It allows for capturing the contents of
  // the window that are drawn using DirectComposition.
  UINT flags = PW_CLIENTONLY | PW_RENDERFULLCONTENT;

  BOOL result = PrintWindow(window, mem_hdc.Get(), flags);
  if (!result)
    PLOG(ERROR) << "Failed to print window";

  GdiFlush();

  std::vector<SkColor> pixels(size.width() * size.height());
  memcpy(pixels.data(), bits, pixels.size() * sizeof(SkColor));
  return pixels;
}

SkColor ReadBackWindowPixel(HWND window, const gfx::Point& point) {
  gfx::Size size(point.x() + 1, point.y() + 1);
  auto pixels = ReadBackWindow(window, size);
  return pixels[size.width() * point.y() + point.x()];
}

class DirectCompositionPixelTest : public testing::Test {
 public:
  DirectCompositionPixelTest()
      : window_(&platform_delegate_, gfx::Rect(100, 100)) {}

  ~DirectCompositionPixelTest() override {
    context_ = nullptr;
    if (surface_)
      DestroySurface(std::move(surface_));
  }

 protected:
  void InitializeSurface() {
    static_cast<ui::PlatformWindow*>(&window_)->Show();

    surface_ = new DirectCompositionSurfaceWin(nullptr, delegate_.AsWeakPtr(),
                                               window_.hwnd());
    EXPECT_TRUE(surface_->Initialize(gl::GLSurfaceFormat()));
    context_ = gl::init::CreateGLContext(nullptr, surface_.get(),
                                         gl::GLContextAttribs());
    EXPECT_TRUE(context_->MakeCurrent(surface_.get()));
  }

  void PixelTestSwapChain(bool layers_enabled) {
    if (!CheckIfDCSupported())
      return;

    InitializeSurface();

    surface_->SetEnableDCLayers(layers_enabled);
    gfx::Size window_size(100, 100);
    EXPECT_TRUE(surface_->Resize(window_size, 1.0,
                                 gl::GLSurface::ColorSpace::UNSPECIFIED, true));
    EXPECT_TRUE(surface_->SetDrawRectangle(gfx::Rect(window_size)));

    glClearColor(1.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
              surface_->SwapBuffers(base::DoNothing()));

    // Ensure DWM swap completed.
    Sleep(1000);

    SkColor expected_color = SK_ColorRED;
    SkColor actual_color =
        ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75));
    EXPECT_EQ(expected_color, actual_color)
        << std::hex << "Expected " << expected_color << " Actual "
        << actual_color;

    EXPECT_TRUE(context_->IsCurrent(surface_.get()));
  }

  TestPlatformDelegate platform_delegate_;
  TestImageTransportSurfaceDelegate delegate_;
  ui::WinWindow window_;
  scoped_refptr<DirectCompositionSurfaceWin> surface_;
  scoped_refptr<gl::GLContext> context_;
};

TEST_F(DirectCompositionPixelTest, DCLayersEnabled) {
  PixelTestSwapChain(true);
}

TEST_F(DirectCompositionPixelTest, DCLayersDisabled) {
  PixelTestSwapChain(false);
}

bool AreColorsSimilar(int a, int b) {
  // The precise colors may differ depending on the video processor, so allow
  // a margin for error.
  const int kMargin = 10;
  return abs(SkColorGetA(a) - SkColorGetA(b)) < kMargin &&
         abs(SkColorGetR(a) - SkColorGetR(b)) < kMargin &&
         abs(SkColorGetG(a) - SkColorGetG(b)) < kMargin &&
         abs(SkColorGetB(a) - SkColorGetB(b)) < kMargin;
}

class DirectCompositionVideoPixelTest : public DirectCompositionPixelTest {
 protected:
  void TestVideo(const gfx::ColorSpace& color_space,
                 SkColor expected_color,
                 bool check_color) {
    if (!CheckIfDCSupported())
      return;
    InitializeSurface();
    surface_->SetEnableDCLayers(true);

    gfx::Size window_size(100, 100);
    EXPECT_TRUE(surface_->Resize(window_size, 1.0,
                                 gl::GLSurface::ColorSpace::UNSPECIFIED, true));

    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
        gl::QueryD3D11DeviceObjectFromANGLE();

    gfx::Size texture_size(50, 50);
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
        CreateNV12Texture(d3d11_device, texture_size, false);

    scoped_refptr<gl::GLImageDXGI> image_dxgi(
        new gl::GLImageDXGI(texture_size, nullptr));
    image_dxgi->SetTexture(texture, 0);
    image_dxgi->SetColorSpace(color_space);

    ui::DCRendererLayerParams params;
    params.y_image = image_dxgi;
    params.uv_image = image_dxgi;
    params.content_rect = gfx::Rect(texture_size);
    params.quad_rect = gfx::Rect(texture_size);
    surface_->ScheduleDCLayer(params);

    EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
              surface_->SwapBuffers(base::DoNothing()));

    // Scaling up the swapchain with the same image should cause it to be
    // transformed again, but not presented again.
    params.quad_rect = gfx::Rect(window_size);

    surface_->ScheduleDCLayer(params);
    EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
              surface_->SwapBuffers(base::DoNothing()));
    Sleep(1000);

    if (check_color) {
      SkColor actual_color =
          ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75));
      EXPECT_TRUE(AreColorsSimilar(expected_color, actual_color))
          << std::hex << "Expected " << expected_color << " Actual "
          << actual_color;
    }
  }
};

TEST_F(DirectCompositionVideoPixelTest, BT601) {
  TestVideo(gfx::ColorSpace::CreateREC601(), SkColorSetRGB(0xdb, 0x81, 0xe8),
            true);
}

TEST_F(DirectCompositionVideoPixelTest, BT709) {
  TestVideo(gfx::ColorSpace::CreateREC709(), SkColorSetRGB(0xe1, 0x90, 0xeb),
            true);
}

TEST_F(DirectCompositionVideoPixelTest, SRGB) {
  // SRGB doesn't make sense on an NV12 input, but don't crash.
  TestVideo(gfx::ColorSpace::CreateSRGB(), SK_ColorTRANSPARENT, false);
}

TEST_F(DirectCompositionVideoPixelTest, SCRGBLinear) {
  // SCRGB doesn't make sense on an NV12 input, but don't crash.
  TestVideo(gfx::ColorSpace::CreateSCRGBLinear(), SK_ColorTRANSPARENT, false);
}

TEST_F(DirectCompositionVideoPixelTest, InvalidColorSpace) {
  // Invalid color space should be treated as BT.709
  TestVideo(gfx::ColorSpace(), SkColorSetRGB(0xe1, 0x90, 0xeb), true);
}

TEST_F(DirectCompositionPixelTest, SoftwareVideoSwapchain) {
  if (!CheckIfDCSupported())
    return;
  InitializeSurface();
  surface_->SetEnableDCLayers(true);

  gfx::Size window_size(100, 100);
  EXPECT_TRUE(surface_->Resize(window_size, 1.0,
                               gl::GLSurface::ColorSpace::UNSPECIFIED, true));

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      gl::QueryD3D11DeviceObjectFromANGLE();

  gfx::Size y_size(50, 50);
  gfx::Size uv_size(25, 25);
  size_t y_stride =
      gfx::RowSizeForBufferFormat(y_size.width(), gfx::BufferFormat::R_8, 0);
  size_t uv_stride =
      gfx::RowSizeForBufferFormat(uv_size.width(), gfx::BufferFormat::RG_88, 0);
  std::vector<uint8_t> y_data(y_stride * y_size.height(), 0xff);
  std::vector<uint8_t> uv_data(uv_stride * uv_size.height(), 0xff);
  auto y_image = base::MakeRefCounted<gl::GLImageRefCountedMemory>(y_size);

  y_image->Initialize(new base::RefCountedBytes(y_data),
                      gfx::BufferFormat::R_8);
  auto uv_image = base::MakeRefCounted<gl::GLImageRefCountedMemory>(uv_size);
  uv_image->Initialize(new base::RefCountedBytes(uv_data),
                       gfx::BufferFormat::RG_88);
  y_image->SetColorSpace(gfx::ColorSpace::CreateREC709());

  ui::DCRendererLayerParams params;
  params.y_image = y_image;
  params.uv_image = uv_image;
  params.content_rect = gfx::Rect(y_size);
  params.quad_rect = gfx::Rect(window_size);
  surface_->ScheduleDCLayer(params);

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing()));
  Sleep(1000);

  SkColor expected_color = SkColorSetRGB(0xff, 0xb7, 0xff);
  SkColor actual_color =
      ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75));
  EXPECT_TRUE(AreColorsSimilar(expected_color, actual_color))
      << std::hex << "Expected " << expected_color << " Actual "
      << actual_color;
}

TEST_F(DirectCompositionPixelTest, VideoHandleSwapchain) {
  if (!CheckIfDCSupported())
    return;
  InitializeSurface();
  surface_->SetEnableDCLayers(true);

  gfx::Size window_size(100, 100);
  EXPECT_TRUE(surface_->Resize(window_size, 1.0,
                               gl::GLSurface::ColorSpace::UNSPECIFIED, true));

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      gl::QueryD3D11DeviceObjectFromANGLE();

  gfx::Size texture_size(50, 50);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size, true);
  Microsoft::WRL::ComPtr<IDXGIResource1> resource;
  texture.CopyTo(resource.GetAddressOf());
  HANDLE handle = 0;
  resource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ, nullptr,
                               &handle);
  // The format doesn't matter, since we aren't binding.
  scoped_refptr<gl::GLImageDXGI> image_dxgi(
      new gl::GLImageDXGI(texture_size, nullptr));
  ASSERT_TRUE(image_dxgi->InitializeHandle(base::win::ScopedHandle(handle), 0,
                                           gfx::BufferFormat::RGBA_8888));

  ui::DCRendererLayerParams params;
  params.y_image = image_dxgi;
  params.uv_image = image_dxgi;
  params.content_rect = gfx::Rect(texture_size);
  params.quad_rect = gfx::Rect(window_size);
  surface_->ScheduleDCLayer(params);

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing()));

  Sleep(1000);

  SkColor expected_color = SkColorSetRGB(0xe1, 0x90, 0xeb);
  SkColor actual_color =
      ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75));
  EXPECT_TRUE(AreColorsSimilar(expected_color, actual_color))
      << std::hex << "Expected " << expected_color << " Actual "
      << actual_color;
}

TEST_F(DirectCompositionPixelTest, SkipVideoLayerEmptyBoundsRect) {
  if (!CheckIfDCSupported())
    return;
  InitializeSurface();
  surface_->SetEnableDCLayers(true);

  gfx::Size window_size(100, 100);
  EXPECT_TRUE(surface_->Resize(window_size, 1.0,
                               gl::GLSurface::ColorSpace::UNSPECIFIED, true));
  EXPECT_TRUE(surface_->SetDrawRectangle(gfx::Rect(window_size)));

  glClearColor(0.0, 0.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      gl::QueryD3D11DeviceObjectFromANGLE();

  gfx::Size texture_size(50, 50);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size, true);
  Microsoft::WRL::ComPtr<IDXGIResource1> resource;
  texture.CopyTo(resource.GetAddressOf());
  HANDLE handle = 0;
  resource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ, nullptr,
                               &handle);
  // The format doesn't matter, since we aren't binding.
  scoped_refptr<gl::GLImageDXGI> image_dxgi(
      new gl::GLImageDXGI(texture_size, nullptr));
  ASSERT_TRUE(image_dxgi->InitializeHandle(base::win::ScopedHandle(handle), 0,
                                           gfx::BufferFormat::RGBA_8888));

  // Layer with empty bounds rect.
  ui::DCRendererLayerParams params;
  params.y_image = image_dxgi;
  params.uv_image = image_dxgi;
  params.content_rect = gfx::Rect(texture_size);
  surface_->ScheduleDCLayer(params);

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing()));

  Sleep(1000);

  // No color is written since the visual committed to DirectComposition has no
  // content.
  SkColor expected_color = SK_ColorBLACK;
  SkColor actual_color =
      ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75));
  EXPECT_TRUE(AreColorsSimilar(expected_color, actual_color))
      << std::hex << "Expected " << expected_color << " Actual "
      << actual_color;
}

TEST_F(DirectCompositionPixelTest, SkipVideoLayerEmptyContentsRect) {
  if (!CheckIfDCSupported())
    return;
  InitializeSurface();
  // Swap chain size is overridden to content rect size only if scaled overlays
  // are supported.
  DirectCompositionSurfaceWin::SetScaledOverlaysSupportedForTesting(true);
  surface_->SetEnableDCLayers(true);

  gfx::Size window_size(100, 100);
  EXPECT_TRUE(surface_->Resize(window_size, 1.0,
                               gl::GLSurface::ColorSpace::UNSPECIFIED, true));
  EXPECT_TRUE(surface_->SetDrawRectangle(gfx::Rect(window_size)));

  glClearColor(0.0, 0.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      gl::QueryD3D11DeviceObjectFromANGLE();

  gfx::Size texture_size(50, 50);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size, true);
  Microsoft::WRL::ComPtr<IDXGIResource1> resource;
  texture.CopyTo(resource.GetAddressOf());
  HANDLE handle = 0;
  resource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ, nullptr,
                               &handle);
  // The format doesn't matter, since we aren't binding.
  scoped_refptr<gl::GLImageDXGI> image_dxgi(
      new gl::GLImageDXGI(texture_size, nullptr));
  ASSERT_TRUE(image_dxgi->InitializeHandle(base::win::ScopedHandle(handle), 0,
                                           gfx::BufferFormat::RGBA_8888));

  // Layer with empty content rect.
  ui::DCRendererLayerParams params;
  params.y_image = image_dxgi;
  params.uv_image = image_dxgi;
  params.quad_rect = gfx::Rect(window_size);
  surface_->ScheduleDCLayer(params);

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing()));

  Sleep(1000);

  // No color is written since the visual committed to DirectComposition has no
  // content.
  SkColor expected_color = SK_ColorBLACK;
  SkColor actual_color =
      ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75));
  EXPECT_TRUE(AreColorsSimilar(expected_color, actual_color))
      << std::hex << "Expected " << expected_color << " Actual "
      << actual_color;
}

TEST_F(DirectCompositionPixelTest, NV12SwapChain) {
  if (!CheckIfDCSupported())
    return;
  DirectCompositionSurfaceWin::SetPreferNV12OverlaysForTesting();
  InitializeSurface();

  surface_->SetEnableDCLayers(true);

  gfx::Size window_size(100, 100);
  EXPECT_TRUE(surface_->Resize(window_size, 1.0,
                               gl::GLSurface::ColorSpace::UNSPECIFIED, true));
  EXPECT_TRUE(surface_->SetDrawRectangle(gfx::Rect(window_size)));

  glClearColor(0.0, 0.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      gl::QueryD3D11DeviceObjectFromANGLE();

  gfx::Size texture_size(50, 50);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size, true);
  Microsoft::WRL::ComPtr<IDXGIResource1> resource;
  texture.CopyTo(resource.GetAddressOf());
  HANDLE handle = 0;
  resource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ, nullptr,
                               &handle);
  // The format doesn't matter, since we aren't binding.
  scoped_refptr<gl::GLImageDXGI> image_dxgi(
      new gl::GLImageDXGI(texture_size, nullptr));
  ASSERT_TRUE(image_dxgi->InitializeHandle(base::win::ScopedHandle(handle), 0,
                                           gfx::BufferFormat::RGBA_8888));

  // Pass content rect with odd with and height.  Surface should round up width
  // and height when creating swap chain.
  ui::DCRendererLayerParams params;
  params.y_image = image_dxgi;
  params.uv_image = image_dxgi;
  params.content_rect = gfx::Rect(0, 0, 49, 49);
  params.quad_rect = gfx::Rect(window_size);
  surface_->ScheduleDCLayer(params);

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing()));

  Sleep(1000);

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      surface_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain);

  DXGI_SWAP_CHAIN_DESC1 desc;
  EXPECT_TRUE(SUCCEEDED(swap_chain->GetDesc1(&desc)));
  EXPECT_EQ(desc.Format, DXGI_FORMAT_NV12);
  EXPECT_EQ(desc.Width, 50u);
  EXPECT_EQ(desc.Height, 50u);

  SkColor expected_color = SkColorSetRGB(0xe1, 0x90, 0xeb);
  SkColor actual_color =
      ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75));
  EXPECT_TRUE(AreColorsSimilar(expected_color, actual_color))
      << std::hex << "Expected " << expected_color << " Actual "
      << actual_color;
}

TEST_F(DirectCompositionPixelTest, NonZeroBoundsOffset) {
  if (!CheckIfDCSupported())
    return;
  InitializeSurface();
  // Swap chain size is overridden to content rect size only if scaled overlays
  // are supported.
  DirectCompositionSurfaceWin::SetScaledOverlaysSupportedForTesting(true);
  surface_->SetEnableDCLayers(true);

  gfx::Size window_size(100, 100);
  EXPECT_TRUE(surface_->Resize(window_size, 1.0,
                               gl::GLSurface::ColorSpace::UNSPECIFIED, true));
  EXPECT_TRUE(surface_->SetDrawRectangle(gfx::Rect(window_size)));

  glClearColor(0.0, 0.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      gl::QueryD3D11DeviceObjectFromANGLE();

  gfx::Size texture_size(50, 50);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size, true);
  Microsoft::WRL::ComPtr<IDXGIResource1> resource;
  texture.CopyTo(resource.GetAddressOf());
  HANDLE handle = 0;
  resource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ, nullptr,
                               &handle);
  // The format doesn't matter, since we aren't binding.
  scoped_refptr<gl::GLImageDXGI> image_dxgi(
      new gl::GLImageDXGI(texture_size, nullptr));
  ASSERT_TRUE(image_dxgi->InitializeHandle(base::win::ScopedHandle(handle), 0,
                                           gfx::BufferFormat::RGBA_8888));

  ui::DCRendererLayerParams params;
  params.y_image = image_dxgi;
  params.uv_image = image_dxgi;
  params.content_rect = gfx::Rect(texture_size);
  params.quad_rect = gfx::Rect(gfx::Point(25, 25), texture_size);
  surface_->ScheduleDCLayer(params);

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing()));

  Sleep(1000);

  SkColor video_color = SkColorSetRGB(0xe1, 0x90, 0xeb);
  struct {
    gfx::Point point;
    SkColor expected_color;
  } test_cases[] = {
      // Outside bounds
      {{24, 24}, SK_ColorBLACK},
      {{75, 75}, SK_ColorBLACK},
      // Inside bounds
      {{25, 25}, video_color},
      {{74, 74}, video_color},
  };

  auto pixels = ReadBackWindow(window_.hwnd(), window_size);

  for (const auto& test_case : test_cases) {
    const auto& point = test_case.point;
    const auto& expected_color = test_case.expected_color;
    SkColor actual_color = pixels[window_size.width() * point.y() + point.x()];
    EXPECT_TRUE(AreColorsSimilar(expected_color, actual_color))
        << std::hex << "Expected " << expected_color << " Actual "
        << actual_color << " at " << point.ToString();
  }
}

TEST_F(DirectCompositionPixelTest, ResizeVideoLayer) {
  if (!CheckIfDCSupported())
    return;
  InitializeSurface();
  surface_->SetEnableDCLayers(true);

  gfx::Size window_size(100, 100);
  EXPECT_TRUE(surface_->Resize(window_size, 1.0,
                               gl::GLSurface::ColorSpace::UNSPECIFIED, true));
  EXPECT_TRUE(surface_->SetDrawRectangle(gfx::Rect(window_size)));

  glClearColor(0.0, 0.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      gl::QueryD3D11DeviceObjectFromANGLE();

  gfx::Size texture_size(50, 50);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size, true);
  Microsoft::WRL::ComPtr<IDXGIResource1> resource;
  texture.CopyTo(resource.GetAddressOf());
  HANDLE handle = 0;
  resource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ, nullptr,
                               &handle);
  // The format doesn't matter, since we aren't binding.
  scoped_refptr<gl::GLImageDXGI> image_dxgi(
      new gl::GLImageDXGI(texture_size, nullptr));
  ASSERT_TRUE(image_dxgi->InitializeHandle(base::win::ScopedHandle(handle), 0,
                                           gfx::BufferFormat::RGBA_8888));

  {
    ui::DCRendererLayerParams params;
    params.y_image = image_dxgi;
    params.uv_image = image_dxgi;
    params.content_rect = gfx::Rect(texture_size);
    params.quad_rect = gfx::Rect(window_size);
    surface_->ScheduleDCLayer(params);

    EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
              surface_->SwapBuffers(base::DoNothing()));
  }

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      surface_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain);

  DXGI_SWAP_CHAIN_DESC1 desc;
  EXPECT_TRUE(SUCCEEDED(swap_chain->GetDesc1(&desc)));
  EXPECT_EQ(desc.Width, 50u);
  EXPECT_EQ(desc.Height, 50u);

  {
    ui::DCRendererLayerParams params;
    params.y_image = image_dxgi;
    params.uv_image = image_dxgi;
    params.content_rect = gfx::Rect(30, 30);
    params.quad_rect = gfx::Rect(window_size);
    surface_->ScheduleDCLayer(params);

    EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
              surface_->SwapBuffers(base::DoNothing()));
  }

  // Swap chain isn't recreated on resize.
  ASSERT_TRUE(surface_->GetLayerSwapChainForTesting(0));
  EXPECT_EQ(swap_chain.Get(), surface_->GetLayerSwapChainForTesting(0).Get());
  EXPECT_TRUE(SUCCEEDED(swap_chain->GetDesc1(&desc)));
  EXPECT_EQ(desc.Width, 30u);
  EXPECT_EQ(desc.Height, 30u);
}

}  // namespace
}  // namespace gpu
