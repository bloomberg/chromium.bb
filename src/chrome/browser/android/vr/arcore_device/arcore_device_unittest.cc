// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/arcore/arcore_device.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/android/vr/arcore_device/fake_arcore.h"
#include "components/webxr/mailbox_to_surface_bridge_impl.h"
#include "device/vr/android/arcore/ar_image_transport.h"
#include "device/vr/android/arcore/arcore_gl.h"
#include "device/vr/android/arcore/arcore_session_utils.h"
#include "device/vr/public/cpp/xr_frame_sink_client.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class StubArImageTransport : public ArImageTransport {
 public:
  explicit StubArImageTransport(
      std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge)
      : ArImageTransport(std::move(mailbox_bridge)) {}

  void Initialize(WebXrPresentationState*,
                  base::OnceClosure callback) override {
    std::move(callback).Run();
  }

  // TODO(lincolnfrog): test verify this somehow.
  GLuint GetCameraTextureId() override { return CAMERA_TEXTURE_ID; }

  // This transfers whatever the contents of the texture specified
  // by GetCameraTextureId() is at the time it is called and returns
  // a gpu::MailboxHolder with that texture copied to a shared buffer.
  gpu::MailboxHolder TransferFrame(
      WebXrPresentationState*,
      const gfx::Size& frame_size,
      const gfx::Transform& uv_transform) override {
    return gpu::MailboxHolder();
  }
  gpu::MailboxHolder TransferCameraImageFrame(
      WebXrPresentationState*,
      const gfx::Size& frame_size,
      const gfx::Transform& uv_transform) override {
    return gpu::MailboxHolder();
  }

  std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge_;
  const GLuint CAMERA_TEXTURE_ID = 10;
};

class StubArImageTransportFactory : public ArImageTransportFactory {
 public:
  ~StubArImageTransportFactory() override = default;

  std::unique_ptr<ArImageTransport> Create(
      std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge) override {
    return std::make_unique<StubArImageTransport>(std::move(mailbox_bridge));
  }
};

class StubMailboxToSurfaceBridge : public webxr::MailboxToSurfaceBridgeImpl {
 public:
  StubMailboxToSurfaceBridge() = default;

  void CreateAndBindContextProvider(base::OnceClosure callback) override {
    callback_ = std::move(callback);
  }

  bool IsConnected() override { return true; }

  const uint32_t TEXTURE_ID = 1;

 private:
  base::OnceClosure callback_;
};

class StubMailboxToSurfaceBridgeFactory : public MailboxToSurfaceBridgeFactory {
 public:
  std::unique_ptr<device::MailboxToSurfaceBridge> Create() const override {
    return std::make_unique<StubMailboxToSurfaceBridge>();
  }
};

class StubArCoreSessionUtils : public ArCoreSessionUtils {
 public:
  StubArCoreSessionUtils() = default;

  void RequestArSession(int render_process_id,
                        int render_frame_id,
                        bool use_overlay,
                        bool can_render_dom_content,
                        SurfaceReadyCallback ready_callback,
                        SurfaceTouchCallback touch_callback,
                        SurfaceDestroyedCallback destroyed_callback) override {
    // Return arbitrary screen geometry as stand-in for the expected
    // drawing surface. It's not actually a surface, hence the nullptr
    // instead of a WindowAndroid.
    std::move(ready_callback)
        .Run(nullptr, gpu::kNullSurfaceHandle, nullptr,
             display::Display::Rotation::ROTATE_0, {1024, 512});
  }
  void EndSession() override {}

  bool EnsureLoaded() override { return true; }

  base::android::ScopedJavaLocalRef<jobject> GetApplicationContext() override {
    JNIEnv* env = base::android::AttachCurrentThread();
    jclass activityThread = env->FindClass("android/app/ActivityThread");
    jmethodID currentActivityThread =
        env->GetStaticMethodID(activityThread, "currentActivityThread",
                               "()Landroid/app/ActivityThread;");
    jobject at =
        env->CallStaticObjectMethod(activityThread, currentActivityThread);
    jmethodID getApplication = env->GetMethodID(
        activityThread, "getApplication", "()Landroid/app/Application;");
    jobject context = env->CallObjectMethod(at, getApplication);
    return base::android::ScopedJavaLocalRef<jobject>(env, context);
  }
};

// Note that this must be created and destroyed on the same thread as the mojo
// bindings were originally opened on. If we don't allow UnassociatedUsage of
// the AssociatedReceiver's, we get a DCHECK in the product code that the
// Receiver's still have a pending association. However, it appears that once we
// call EnableUnassociatedUsage, both ends of the pipe must be destroyed on the
// thread that EnableUnassociatedUsage was called on.
class StubCompositorFrameSink
    : public viz::mojom::DisplayPrivate,
      public viz::mojom::CompositorFrameSink,
      public viz::mojom::ExternalBeginFrameController {
 public:
  StubCompositorFrameSink(
      viz::mojom::RootCompositorFrameSinkParamsPtr root_params)
      : sink_client_(std::move(root_params->compositor_frame_sink_client)),
        display_client_(std::move(root_params->display_client)),
        task_runner_(base::ThreadTaskRunnerHandle::Get()) {
    root_params->compositor_frame_sink.EnableUnassociatedUsage();
    root_params->display_private.EnableUnassociatedUsage();
    root_params->external_begin_frame_controller.EnableUnassociatedUsage();

    frame_sink_.Bind(std::move(root_params->compositor_frame_sink));
    display_private_.Bind(std::move(root_params->display_private));
    frame_controller_.Bind(
        std::move(root_params->external_begin_frame_controller));
  }
  ~StubCompositorFrameSink() override {
    // See class comment for explanation.
    DCHECK(task_runner_->BelongsToCurrentThread());
  }

  // mojom::DisplayPrivate:
  void SetDisplayVisible(bool visible) override {}
  void Resize(const gfx::Size& size) override {}
  void SetDisplayColorMatrix(const gfx::Transform& color_matrix) override {}
  void SetDisplayColorSpaces(
      const gfx::DisplayColorSpaces& display_color_spaces) override {}
  void SetOutputIsSecure(bool secure) override {}
  void SetDisplayVSyncParameters(base::TimeTicks timebase,
                                 base::TimeDelta interval) override {}
  void ForceImmediateDrawAndSwapIfPossible() override {}
  void SetVSyncPaused(bool paused) override {}
  void UpdateRefreshRate(float refresh_rate) override {}
  void SetSupportedRefreshRates(
      const std::vector<float>& supported_refresh_rates) override {}
  void PreserveChildSurfaceControls() override {}
  void AddVSyncParameterObserver(
      mojo::PendingRemote<viz::mojom::VSyncParameterObserver> observer)
      override {}
  void SetDelegatedInkPointRenderer(
      mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer> receiver)
      override {}
  void SetSwapCompletionCallbackEnabled(bool enable) override {}

  // mojom::CompositorFrameSink:
  void SetNeedsBeginFrame(bool needs_begin_frame) override {}
  void SetWantsAnimateOnlyBeginFrames() override {}
  void SubmitCompositorFrame(
      const viz::LocalSurfaceId& local_surface_id,
      viz::CompositorFrame frame,
      absl::optional<viz::HitTestRegionList> hit_test_region_list,
      uint64_t submit_time) override {}
  void DidNotProduceFrame(const viz::BeginFrameAck& begin_frame_ack) override {}
  void DidAllocateSharedBitmap(base::ReadOnlySharedMemoryRegion region,
                               const viz::SharedBitmapId& id) override {}
  void DidDeleteSharedBitmap(const viz::SharedBitmapId& id) override {}
  void SubmitCompositorFrameSync(
      const viz::LocalSurfaceId& local_surface_id,
      viz::CompositorFrame frame,
      absl::optional<viz::HitTestRegionList> hit_test_region_list,
      uint64_t submit_time,
      SubmitCompositorFrameSyncCallback callback) override {}
  void InitializeCompositorFrameSinkType(
      viz::mojom::CompositorFrameSinkType type) override {}
  void SetThreadIds(const std::vector<int32_t>& thread_ids) override {}

  // mojom::ExternalBeginFrameController implementation.
  void IssueExternalBeginFrame(
      const viz::BeginFrameArgs& args,
      bool force,
      base::OnceCallback<void(const viz::BeginFrameAck&)> callback) override {
    std::move(callback).Run({args, false});
  }

 private:
  mojo::Remote<viz::mojom::CompositorFrameSinkClient> sink_client_;
  mojo::Remote<viz::mojom::DisplayClient> display_client_;
  mojo::AssociatedReceiver<viz::mojom::CompositorFrameSink> frame_sink_{this};
  mojo::AssociatedReceiver<viz::mojom::DisplayPrivate> display_private_{this};
  mojo::AssociatedReceiver<viz::mojom::ExternalBeginFrameController>
      frame_controller_{this};

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

class StubXrFrameSinkClient : public XrFrameSinkClient {
 public:
  StubXrFrameSinkClient() = default;
  ~StubXrFrameSinkClient() override {
    if (mojo_thread_task_runner_) {
      mojo_thread_task_runner_->DeleteSoon(FROM_HERE,
                                           std::move(compositor_frame_sink_));
    }
  }

  // device::XrFrameSinkClient
  void InitializeRootCompositorFrameSink(
      viz::mojom::RootCompositorFrameSinkParamsPtr root_params,
      DomOverlaySetup dom_setup,
      base::OnceClosure on_initialized) override {
    // The StubCompositorFrameSink must be created/destroyed on the same thread
    // as the mojo bindings in RootCompositorFrameSinkParamsPtr were on. Since
    // this call comes from the ArCompositorFrameSink, which only runs on the Gl
    // thread, we know that the mojo bindings were opened on this thread. So,
    // we make this the thread to create/destroy the StubCompositorFrameSink on.
    mojo_thread_task_runner_ = base::ThreadTaskRunnerHandle::Get();
    compositor_frame_sink_ =
        std::make_unique<StubCompositorFrameSink>(std::move(root_params));
    std::move(on_initialized).Run();
  }
  void SurfaceDestroyed() override {}
  absl::optional<viz::SurfaceId> GetDOMSurface() override {
    return absl::nullopt;
  }
  viz::FrameSinkId FrameSinkId() override { return {}; }

 private:
  std::unique_ptr<StubCompositorFrameSink> compositor_frame_sink_;
  scoped_refptr<base::SingleThreadTaskRunner> mojo_thread_task_runner_;
};

std::unique_ptr<XrFrameSinkClient> FrameSinkClientFactory(int32_t, int32_t) {
  return std::make_unique<StubXrFrameSinkClient>();
}

class ArCoreDeviceTest : public testing::Test {
 public:
  ArCoreDeviceTest() {}
  ~ArCoreDeviceTest() override {}

  void OnSessionCreated(mojom::XRRuntimeSessionResultPtr session_result) {
    DVLOG(1) << __func__;
    session_ = std::move(session_result->session);
    controller_.Bind(std::move(session_result->controller));
    // TODO(crbug.com/837834): verify that things fail if restricted.
    // We should think through the right result here for javascript.
    // If an AR page tries to hittest while not focused, should it
    // get no results or fail?
    controller_->SetFrameDataRestricted(false);

    frame_provider.Bind(std::move(session_->data_provider));
    frame_provider->GetEnvironmentIntegrationProvider(
        environment_provider.BindNewEndpointAndPassReceiver());
    std::move(quit_closure).Run();
  }

  raw_ptr<StubArCoreSessionUtils> session_utils;
  mojo::Remote<mojom::XRFrameDataProvider> frame_provider;
  mojo::AssociatedRemote<mojom::XREnvironmentIntegrationProvider>
      environment_provider;
  std::unique_ptr<base::RunLoop> run_loop;
  base::OnceClosure quit_closure;

 protected:
  void SetUp() override {
    std::unique_ptr<StubArCoreSessionUtils> session_utils_ptr =
        std::make_unique<StubArCoreSessionUtils>();
    session_utils = session_utils_ptr.get();
    device_ = std::make_unique<ArCoreDevice>(
        std::make_unique<FakeArCoreFactory>(),
        std::make_unique<StubArImageTransportFactory>(),
        std::make_unique<StubMailboxToSurfaceBridgeFactory>(),
        std::move(session_utils_ptr),
        base::BindRepeating(&FrameSinkClientFactory));
  }

  void CreateSession() {
    mojom::XRRuntimeSessionOptionsPtr options =
        mojom::XRRuntimeSessionOptions::New();
    options->mode = mojom::XRSessionMode::kImmersiveAr;
    device()->RequestSession(std::move(options),
                             base::BindOnce(&ArCoreDeviceTest::OnSessionCreated,
                                            base::Unretained(this)));

    // TODO(https://crbug.com/837834): figure out how to make this work
    // EXPECT_CALL(*bridge,
    // DoCreateUnboundContextProvider(testing::_)).Times(1);

    run_loop = std::make_unique<base::RunLoop>();
    quit_closure = run_loop->QuitClosure();
    run_loop->Run();

    EXPECT_TRUE(environment_provider);
    EXPECT_TRUE(session_);
  }

  mojom::XRFrameDataPtr GetFrameData() {
    run_loop = std::make_unique<base::RunLoop>();
    quit_closure = run_loop->QuitClosure();

    mojom::XRFrameDataPtr frame_data;
    auto callback = [](base::OnceClosure quit_closure,
                       mojom::XRFrameDataPtr* frame_data,
                       mojom::XRFrameDataPtr data) {
      *frame_data = std::move(data);
      std::move(quit_closure).Run();
    };

    // TODO(https://crbug.com/837834): verify GetFrameData fails if we
    // haven't resolved the Mailbox.
    frame_provider->GetFrameData(
        nullptr,
        base::BindOnce(callback, std::move(quit_closure), &frame_data));
    run_loop->Run();
    EXPECT_TRUE(frame_data);

    return frame_data;
  }

  VRDeviceBase* device() { return device_.get(); }

 private:
  std::unique_ptr<ArCoreDevice> device_;
  mojom::XRSessionPtr session_;
  mojo::Remote<mojom::XRSessionController> controller_;
};

TEST_F(ArCoreDeviceTest, RequestSession) {
  CreateSession();
}

TEST_F(ArCoreDeviceTest, GetFrameData) {
  CreateSession();
  GetFrameData();
}

}  // namespace device
