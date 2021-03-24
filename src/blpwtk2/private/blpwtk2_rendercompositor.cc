/*
 * Copyright (C) 2018 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <blpwtk2_rendercompositor.h>

#include <blpwtk2_profileimpl.h>

#include <algorithm>

#include <base/debug/alias.h>
#include <base/memory/ref_counted.h>
#include <components/viz/common/display/renderer_settings.h>
#include <components/viz/common/surfaces/frame_sink_id.h>
#include <components/viz/common/surfaces/frame_sink_id_allocator.h>
#include <components/viz/common/surfaces/parent_local_surface_id_allocator.h>
#include <components/viz/host/host_display_client.h>
#include <components/viz/host/host_frame_sink_client.h>
#include <components/viz/host/host_frame_sink_manager.h>
#include <components/viz/host/renderer_settings_creation.h>
#include <components/viz/service/display/output_surface.h>
#include <components/viz/service/display/output_surface_client.h>
#include <components/viz/service/display/output_surface_frame.h>
#include <components/viz/service/display_embedder/output_device_backing.h>
#include <components/viz/service/display_embedder/output_surface_provider.h>
#include <components/viz/service/display_embedder/server_shared_bitmap_manager.h>
#include <components/viz/service/display_embedder/software_output_device_win.h>
#include <components/viz/service/display_embedder/software_output_surface.h>
#include <components/viz/service/frame_sinks/frame_sink_manager_impl.h>
#include <components/viz/service/frame_sinks/root_compositor_frame_sink_impl.h>
#include <cc/mojom/render_frame_metadata.mojom.h>
#include <content/public/common/gpu_stream_constants.h>
#include <content/renderer/render_thread_impl.h>
#include <content/renderer/renderer_blink_platform_impl.h>
#include <gpu/command_buffer/client/context_support.h>
#include <gpu/command_buffer/client/gles2_interface.h>
#include <gpu/command_buffer/common/swap_buffers_complete_params.h>
#include <gpu/command_buffer/common/swap_buffers_flags.h>
#include <gpu/ipc/client/command_buffer_proxy_impl.h>
#include <services/service_manager/public/cpp/connector.h>
#include <services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h>
#include <services/viz/public/cpp/gpu/context_provider_command_buffer.h>
#include <ui/gfx/buffer_format_util.h>
#include <ui/gfx/overlay_transform_utils.h>

namespace blpwtk2 {

class RenderCompositorFrameSinkImpl;

class RenderFrameSinkProviderImpl :  public viz::OutputSurfaceProvider {
    mojo::Remote<blink::mojom::WidgetHost> d_default_frame_sink_provider;

    scoped_refptr<base::SingleThreadTaskRunner> d_main_task_runner, d_compositor_task_runner;
    gpu::GpuMemoryBufferManager *d_gpu_memory_buffer_manager = nullptr;

    std::unique_ptr<viz::SharedBitmapManager> d_shared_bitmap_manager;
    std::unique_ptr<viz::FrameSinkManagerImpl> d_frame_sink_manager;
    std::unique_ptr<viz::HostFrameSinkManager> d_host_frame_sink_manager;
    std::unique_ptr<viz::FrameSinkIdAllocator> d_frame_sink_id_allocator;
    std::unique_ptr<viz::RendererSettings> d_renderer_settings;
    std::unique_ptr<viz::OutputDeviceBacking> d_software_output_device_backing;

    scoped_refptr<gpu::GpuChannelHost> d_gpu_channel;

    viz::DebugRendererSettings debug_renderer_settings_;

    struct CompositorData {
        CompositorData(gpu::SurfaceHandle gpu_surface_handle)
        : gpu_surface_handle(gpu_surface_handle) {}

        bool invalidated = false;
        gpu::SurfaceHandle gpu_surface_handle;
        std::unique_ptr<RenderCompositorFrameSinkImpl> compositor_frame_sink_impl;
        bool visible = false;
        gfx::Size size;
    };

    using CompositorDataMap = std::map<int32_t, CompositorData>;

    CompositorDataMap d_compositor_data;
    std::map<gpu::SurfaceHandle, CompositorDataMap::iterator> d_compositor_data_by_surface_handle;

    void EstablishGpuChannelSyncOnMain(gpu::GpuChannelEstablishedCallback callback);

    void CreateForWidgetOnCompositor(
        int32_t compositor_id,
        mojo::PendingReceiver<::viz::mojom::CompositorFrameSink> compositor_frame_sink_receiver,
        mojo::PendingRemote<::viz::mojom::CompositorFrameSinkClient> compositor_frame_sink_client,
        scoped_refptr<gpu::GpuChannelHost> gpu_channel);

    void DelegateToDefaultFrameSinkProviderOnMain(
        mojo::PendingReceiver<::viz::mojom::CompositorFrameSink> compositor_frame_sink_receiver,
        mojo::PendingRemote<::viz::mojom::CompositorFrameSinkClient> compositor_frame_sink_client);

public:

    RenderFrameSinkProviderImpl();
    ~RenderFrameSinkProviderImpl() override;

    void Init(
        gpu::GpuMemoryBufferManager *gpu_memory_buffer_manager);

    gpu::GpuMemoryBufferManager *gpu_memory_buffer_manager() {
        return d_gpu_memory_buffer_manager;
    }

    viz::SharedBitmapManager *shared_bitmap_manager() {
        return d_shared_bitmap_manager.get();
    }

    viz::FrameSinkManagerImpl *frame_sink_manager() {
        return d_frame_sink_manager.get();
    }

    viz::HostFrameSinkManager *host_frame_sink_manager() {
        return d_host_frame_sink_manager.get();
    }

    viz::FrameSinkIdAllocator *frame_sink_id_allocator() {
        return d_frame_sink_id_allocator.get();
    }

    viz::RendererSettings *renderer_settings() {
        return d_renderer_settings.get();
    }

    viz::OutputDeviceBacking *software_output_device_backing() {
        return d_software_output_device_backing.get();
    }

    void Bind(mojo::PendingReceiver<blink::mojom::WidgetHost> receiver);
    void Unbind();

    void CreateCompositorFrameSink(int32_t compositor_id,
        mojo::PendingReceiver<viz::mojom::CompositorFrameSink>
        compositor_frame_sink_receiver,
        mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient>
        compositor_frame_sink_client);
    void RegisterCompositor(int32_t compositor_id, gpu::SurfaceHandle gpu_surface_handle);
    void UnregisterCompositor(int32_t compositor_id);
    void InvalidateCompositor(int32_t compositor_id);
    void SetCompositorVisible(int32_t compositor_id, bool visible);
    void ResizeCompositor(int32_t compositor_id, gfx::Size size, base::WaitableEvent *event);

    void CompositingModeFallbackToSoftwareOnMain();

    // viz::OutputSurfaceProvider overrides:
    std::unique_ptr<viz::OutputSurface> CreateOutputSurface(
        gpu::SurfaceHandle surface_handle,
        bool gpu_compositing,
        viz::mojom::DisplayClient* display_client,
        viz::DisplayCompositorMemoryAndTaskController* gpu_dependency,
        const viz::RendererSettings& renderer_settings,
        const viz::DebugRendererSettings* debug_settings) override;

    std::unique_ptr<viz::DisplayCompositorMemoryAndTaskController> CreateGpuDependency(
      bool gpu_compositing,
      gpu::SurfaceHandle surface_handle,
      const viz::RendererSettings& renderer_settings) override;
};

class GpuOutputSurface : public viz::OutputSurface {
public:
    GpuOutputSurface(scoped_refptr<viz::ContextProvider> context_provider,
                     gpu::SurfaceHandle surface_handle);
    ~GpuOutputSurface() override;

    // OutputSurface implementation
    void BindToClient(viz::OutputSurfaceClient* client) override;
    void EnsureBackbuffer() override;
    void DiscardBackbuffer() override;
    void BindFramebuffer() override;
    void SetDrawRectangle(const gfx::Rect& draw_rectangle) override;
    void Reshape(const gfx::Size& size,
                float device_scale_factor,
                const gfx::ColorSpace& color_space,
                gfx::BufferFormat format,
                bool use_stencil) override;
    void SwapBuffers(viz::OutputSurfaceFrame frame) override;
    uint32_t GetFramebufferCopyTextureFormat() override;
    bool IsDisplayedAsOverlayPlane() const override;
    unsigned GetOverlayTextureId() const override;
    bool HasExternalStencilTest() const override;
    void ApplyExternalStencil() override;
    unsigned UpdateGpuFence() override;
    void SetNeedsSwapSizeNotifications(
        bool needs_swap_size_notifications) override;
    void SetUpdateVSyncParametersCallback(
        viz::UpdateVSyncParametersCallback callback) override;
    void SetDisplayTransformHint(gfx::OverlayTransform transform) override {}
    gfx::OverlayTransform GetDisplayTransform() override;

    gpu::SurfaceHandle GetSurfaceHandle() const override;

protected:

    viz::OutputSurfaceClient* client() const { return client_; }
    ui::LatencyTracker* latency_tracker() { return &latency_tracker_; }
    bool needs_swap_size_notifications() { return needs_swap_size_notifications_; }

    // Called when a swap completion is signaled from ImageTransportSurface.
    virtual void DidReceiveSwapBuffersAck(const gfx::SwapResponse& response);

    // Called in SwapBuffers() when a swap is determined to be partial. Subclasses
    // might override this method because different platforms handle partial swaps
    // differently.
    virtual void HandlePartialSwap(
        const gfx::Rect& sub_buffer_rect,
        uint32_t flags,
        gpu::ContextSupport::SwapCompletedCallback swap_callback,
        gpu::ContextSupport::PresentationCallback presentation_callback);

private:

    // Called when a swap completion is signaled from ImageTransportSurface.
    void OnGpuSwapBuffersCompleted(std::vector<ui::LatencyInfo> latency_info,
                                    bool top_controls_visible_height_changed,
                                    const gfx::Size& pixel_size,
                                    const gpu::SwapBuffersCompleteParams& params);
    void OnPresentation(const gfx::PresentationFeedback& feedback);
    void OnUpdateVSyncParameters(base::TimeTicks timebase, base::TimeDelta interval);
    gfx::Rect ApplyDisplayInverse(const gfx::Rect& input);

    //scoped_refptr<VizProcessContextProvider> viz_context_provider_;
    viz::OutputSurfaceClient* client_ = nullptr;
    viz::UpdateVSyncParametersCallback update_vsync_parameters_callback_;
    bool wants_vsync_parameter_updates_ = false;
    ui::LatencyTracker latency_tracker_;

    const gpu::SurfaceHandle surface_handle_;

    bool set_draw_rectangle_for_frame_ = false;
    // True if the draw rectangle has been set at all since the last resize.
    bool has_set_draw_rectangle_since_last_resize_ = false;
    gfx::Size size_;
    bool use_gpu_fence_;
    unsigned gpu_fence_id_ = 0;
    // Whether to send viz::OutputSurfaceClient::DidSwapWithSize notifications.
    bool needs_swap_size_notifications_ = false;

    base::WeakPtrFactory<GpuOutputSurface> weak_ptr_factory_{this};
};

class RenderCompositorFrameSinkImpl : public viz::mojom::CompositorFrameSink
                                     , private viz::HostFrameSinkClient {
    RenderFrameSinkProviderImpl& d_context;

    mojo::Receiver<viz::mojom::CompositorFrameSink> d_binding; //d_receiver
    mojo::Remote<viz::mojom::CompositorFrameSinkClient> d_client;

    viz::FrameSinkId d_frame_sink_id;

    std::unique_ptr<viz::HostDisplayClient> d_display_client;
    //mojo::AssociatedRemote<viz::mojom::DisplayPrivate> d_display_private;

    std::unique_ptr<viz::RootCompositorFrameSinkImpl> d_root_compositor_frame_sink;

public:

    RenderCompositorFrameSinkImpl(
        RenderFrameSinkProviderImpl& context,
        gpu::SurfaceHandle gpu_surface_handle,
        scoped_refptr<gpu::GpuChannelHost> gpu_channel,
        scoped_refptr<base::SingleThreadTaskRunner> task_runner,
        gfx::Size size,
        bool visible,
        mojo::PendingReceiver<::viz::mojom::CompositorFrameSink> compositor_frame_sink_receiver,
        mojo::PendingRemote<::viz::mojom::CompositorFrameSinkClient> compositor_frame_sink_client,
        const viz::DebugRendererSettings* debug_settings);
    ~RenderCompositorFrameSinkImpl() override;

    void SetVisible(bool visible);
    void Resize(gfx::Size size);

    // viz::mojom::CompositorFrameSink overrides:
    void SetNeedsBeginFrame(bool needs_begin_frame) override;
    void SetWantsAnimateOnlyBeginFrames() override;
    void SubmitCompositorFrame(
        const viz::LocalSurfaceId& local_surface_id,
        viz::CompositorFrame frame,
        base::Optional<viz::HitTestRegionList> hit_test_region_list,
        uint64_t submit_time) override;
    void SubmitCompositorFrameSync(
        const viz::LocalSurfaceId& local_surface_id,
        viz::CompositorFrame frame,
        base::Optional<viz::HitTestRegionList> hit_test_region_list,
        uint64_t submit_time,
        SubmitCompositorFrameSyncCallback callback) override;
    void DidNotProduceFrame(const viz::BeginFrameAck& ack) override;
    void DidAllocateSharedBitmap(base::ReadOnlySharedMemoryRegion region,
                                 const gpu::Mailbox& id) override;
    void DidDeleteSharedBitmap(const viz::SharedBitmapId& id) override;
    void InitializeCompositorFrameSinkType(viz::mojom::CompositorFrameSinkType type) override;

    // viz::HostFrameSinkClient overrides:
    void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override {}
    void OnFrameTokenChanged(uint32_t frame_token) override {}
};

//
namespace {

base::LazyInstance<RenderCompositorFactory>::DestructorAtExit
    s_render_frame_sink_provider_instance = LAZY_INSTANCE_INITIALIZER;

} // close namespace

RenderCompositorFactory* RenderCompositorFactory::GetInstance()
{
    return s_render_frame_sink_provider_instance.Pointer();
}

void RenderCompositorFactory::Terminate()
{
    if (!s_render_frame_sink_provider_instance.IsCreated()) {
        return;
    }
}

RenderCompositorFactory::RenderCompositorFactory()
: d_frame_sink_provider(new RenderFrameSinkProviderImpl())
{
    content::RenderThreadImpl *render_thread =
        content::RenderThreadImpl::current();

    d_compositor_task_runner = render_thread->compositor_task_runner();

    d_compositor_task_runner->
        PostTask(
            FROM_HERE,
            base::BindOnce(&RenderFrameSinkProviderImpl::Init,
                base::Unretained(d_frame_sink_provider.get()),
                render_thread->GetGpuMemoryBufferManager()));
}

RenderCompositorFactory::~RenderCompositorFactory()
{
    d_compositor_task_runner->
        DeleteSoon(
            FROM_HERE,
            std::move(d_frame_sink_provider));
}


std::unique_ptr<RenderCompositor> RenderCompositorFactory::CreateCompositor(
    int32_t compositor_id,
    gpu::SurfaceHandle gpu_surface_handle,
    blpwtk2::ProfileImpl *profile)
{
    auto it = d_compositors_by_compositor_id.find(compositor_id);
    DCHECK(it == d_compositors_by_compositor_id.end());

    std::unique_ptr<RenderCompositor> render_compositor(
        new RenderCompositor(
            *this, compositor_id,
            gpu_surface_handle, profile));

    return render_compositor;
}

//
RenderCompositor::RenderCompositor(
    RenderCompositorFactory& factory,
    int32_t compositor_id,
    gpu::SurfaceHandle gpu_surface_handle,
    blpwtk2::ProfileImpl *profile)
: d_factory(factory)
, d_compositor_id(compositor_id)
, d_gpu_surface_handle(gpu_surface_handle)
, d_profile(profile)
, d_compositor_task_runner(d_factory.d_compositor_task_runner)
, d_local_surface_id_allocator(new viz::ParentLocalSurfaceIdAllocator())
{
    d_factory.d_compositors_by_compositor_id.insert(
        std::make_pair(d_compositor_id, this));

    d_profile->registerNativeViewForComposition(d_gpu_surface_handle);

    d_compositor_task_runner->
        PostTask(
            FROM_HERE,
            base::BindOnce(&RenderFrameSinkProviderImpl::RegisterCompositor,
                base::Unretained(d_factory.d_frame_sink_provider.get()),
                d_compositor_id,
                d_gpu_surface_handle));
}

void RenderCompositor::CreateFrameSink(
    mojo::PendingReceiver<viz::mojom::CompositorFrameSink>
        compositor_frame_sink_receiver,
    mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient>
        compositor_frame_sink_client)
{
    d_factory.d_frame_sink_provider.get()->CreateCompositorFrameSink(d_compositor_id, std::move(compositor_frame_sink_receiver), std::move(compositor_frame_sink_client));
}

RenderCompositor::~RenderCompositor()
{
    auto it = d_factory.d_compositors_by_compositor_id.find(d_compositor_id);
    DCHECK(it != d_factory.d_compositors_by_compositor_id.end());

    d_factory.d_compositors_by_compositor_id.erase(it);

    d_profile->unregisterNativeViewForComposition(d_gpu_surface_handle);

    d_compositor_task_runner->
        PostTask(
            FROM_HERE,
            base::BindOnce(&RenderFrameSinkProviderImpl::UnregisterCompositor,
                base::Unretained(d_factory.d_frame_sink_provider.get()),
                d_compositor_id));
}

viz::LocalSurfaceId RenderCompositor::GetLocalSurfaceId()
{
    return d_local_surface_id_allocator->GetCurrentLocalSurfaceId();
}

void RenderCompositor::Invalidate()
{
    d_visible = false;
}

void RenderCompositor::SetVisible(bool visible)
{
    d_visible = visible;

    d_compositor_task_runner->
        PostTask(
            FROM_HERE,
            base::BindOnce(&RenderFrameSinkProviderImpl::SetCompositorVisible,
                base::Unretained(d_factory.d_frame_sink_provider.get()),
                d_compositor_id,
                d_visible));
}

void RenderCompositor::Resize(const gfx::Size& size)
{
    if (d_size == size) {
        return;
    }

    d_size = size;

    d_local_surface_id_allocator->GenerateId();

    base::WaitableEvent event(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);

    d_compositor_task_runner->
        PostTask(
            FROM_HERE,
            base::BindOnce(&RenderFrameSinkProviderImpl::ResizeCompositor,
                base::Unretained(d_factory.d_frame_sink_provider.get()),
                d_compositor_id,
                d_size,
                &event));

    event.Wait();
}

//
RenderFrameSinkProviderImpl::RenderFrameSinkProviderImpl()
{
    content::RenderThreadImpl *render_thread =
        content::RenderThreadImpl::current();

    d_main_task_runner = base::ThreadTaskRunnerHandle::Get();
    d_compositor_task_runner = render_thread->compositor_task_runner();

    render_thread->BindHostReceiver(d_default_frame_sink_provider.BindNewPipeAndPassReceiver());
}

RenderFrameSinkProviderImpl::~RenderFrameSinkProviderImpl()
{
}

void RenderFrameSinkProviderImpl::Init(
    gpu::GpuMemoryBufferManager *gpu_memory_buffer_manager)
{
    d_gpu_memory_buffer_manager = gpu_memory_buffer_manager;

    d_shared_bitmap_manager = std::make_unique<viz::ServerSharedBitmapManager>();

    d_frame_sink_manager = std::make_unique<viz::FrameSinkManagerImpl>(d_shared_bitmap_manager.get(), this);
    d_host_frame_sink_manager = std::make_unique<viz::HostFrameSinkManager>();

    d_host_frame_sink_manager->SetLocalManager(d_frame_sink_manager.get());
    d_frame_sink_manager->SetLocalClient(d_host_frame_sink_manager.get());

    d_frame_sink_id_allocator = std::make_unique<viz::FrameSinkIdAllocator>(0);

    d_renderer_settings = std::make_unique<viz::RendererSettings>(viz::CreateRendererSettings());
    d_software_output_device_backing = std::make_unique<viz::OutputDeviceBacking>();

    debug_renderer_settings_ = viz::CreateDefaultDebugRendererSettings();
}

void RenderFrameSinkProviderImpl::RegisterCompositor(
    int32_t compositor_id, gpu::SurfaceHandle gpu_surface_handle)
{
    auto it = d_compositor_data.find(compositor_id);
    DCHECK(it == d_compositor_data.end());

    CompositorData compositor_data(gpu_surface_handle);

    it = d_compositor_data.insert({ compositor_id, std::move(compositor_data) }).first;
    d_compositor_data_by_surface_handle.insert({ gpu_surface_handle, it });
}

void RenderFrameSinkProviderImpl::InvalidateCompositor(
    int32_t compositor_id)
{
    SetCompositorVisible(compositor_id, false);

    auto it = d_compositor_data.find(compositor_id);
    DCHECK(it != d_compositor_data.end());

    it->second.invalidated = true;
}

void RenderFrameSinkProviderImpl::UnregisterCompositor(
    int32_t compositor_id)
{
    auto it = d_compositor_data.find(compositor_id);
    DCHECK(it != d_compositor_data.end());

    d_compositor_data_by_surface_handle.erase(it->second.gpu_surface_handle);
    d_compositor_data.erase(compositor_id);
}

void RenderFrameSinkProviderImpl::SetCompositorVisible(int32_t compositor_id, bool visible)
{
    auto it = d_compositor_data.find(compositor_id);
    DCHECK(it != d_compositor_data.end());

    it->second.visible = visible;

    if (it->second.compositor_frame_sink_impl) {
        it->second.compositor_frame_sink_impl->SetVisible(it->second.visible);
    }
}

void RenderFrameSinkProviderImpl::ResizeCompositor(int32_t compositor_id, gfx::Size size, base::WaitableEvent *event)
{
    auto it = d_compositor_data.find(compositor_id);
    DCHECK(it != d_compositor_data.end());

    it->second.size = size;

    if (it->second.compositor_frame_sink_impl) {
        it->second.compositor_frame_sink_impl->Resize(it->second.size);
    }

    event->Signal();
}

void RenderFrameSinkProviderImpl::CompositingModeFallbackToSoftwareOnMain()
{
    content::RenderThreadImpl::current()->CompositingModeFallbackToSoftware();
}

void RenderFrameSinkProviderImpl::CreateCompositorFrameSink(
    int32_t compositor_id,
    mojo::PendingReceiver<::viz::mojom::CompositorFrameSink> compositor_frame_sink_receiver,
    mojo::PendingRemote<::viz::mojom::CompositorFrameSinkClient> compositor_frame_sink_client)
{
    d_main_task_runner->
        PostTask(
            FROM_HERE,
            base::BindOnce(
                &RenderFrameSinkProviderImpl::EstablishGpuChannelSyncOnMain,
                base::Unretained(this),
                base::BindOnce(
                    &RenderFrameSinkProviderImpl::CreateForWidgetOnCompositor,
                    base::Unretained(this),
                    compositor_id,
                    std::move(compositor_frame_sink_receiver),
                    std::move(compositor_frame_sink_client))));
}

std::unique_ptr<viz::OutputSurface> RenderFrameSinkProviderImpl::CreateOutputSurface(
    gpu::SurfaceHandle surface_handle,
    bool gpu_compositing,
    viz::mojom::DisplayClient* display_client,
    viz::DisplayCompositorMemoryAndTaskController* gpu_dependency,
    const viz::RendererSettings& renderer_settings,
    const viz::DebugRendererSettings* debug_settings)
{
    std::unique_ptr<viz::OutputSurface> output_surface;

    auto it = d_compositor_data_by_surface_handle.find(surface_handle);
    if (it == d_compositor_data_by_surface_handle.end() ||
        it->second->second.invalidated) {
        return output_surface;
    }

    if (gpu_compositing) {
        constexpr bool automatic_flushes = false;
        constexpr bool support_locking   = false;
        constexpr bool support_gles2_interface = true;
        constexpr bool support_raster_interface = false;
        constexpr bool support_grcontext = true;

        gpu::ContextCreationAttribs attributes;
        attributes.alpha_size                      = -1;
        attributes.depth_size                      = 0;
        attributes.stencil_size                    = 0;
        attributes.samples                         = 0;
        attributes.sample_buffers                  = 0;
        attributes.bind_generates_resource         = false;
        attributes.lose_context_when_out_of_memory = true;
        attributes.buffer_preserved                = false;
        attributes.enable_gles2_interface          = support_gles2_interface;
        attributes.enable_raster_interface         = support_raster_interface;

        scoped_refptr<viz::ContextProviderCommandBuffer> context_provider =
            new viz::ContextProviderCommandBuffer(
                d_gpu_channel,
                d_gpu_memory_buffer_manager,
                content::kGpuStreamIdDefault,
                content::kGpuStreamPriorityUI,
                surface_handle,
                GURL("chrome://gpu/RenderFrameSinkProviderImpl::CreateOutputSurface"),
                automatic_flushes,
                support_locking,
                support_grcontext,
                gpu::SharedMemoryLimits(),
                attributes,
                viz::command_buffer_metrics::ContextType::BROWSER_COMPOSITOR);

        gpu::ContextResult result = context_provider->BindToCurrentThread();

        if (result == gpu::ContextResult::kSuccess) {
            output_surface = std::make_unique<GpuOutputSurface>(context_provider, surface_handle);
        }
        else {
            LOG(ERROR) << "failed to bind GL context, falling back to software compositing";

            d_main_task_runner->
                PostTask(
                    FROM_HERE,
                    base::BindOnce(
                        &RenderFrameSinkProviderImpl::CompositingModeFallbackToSoftwareOnMain,
                        base::Unretained(this)));
        }
    }

    if (!output_surface) {
        output_surface = std::make_unique<viz::SoftwareOutputSurface>(
            viz::CreateSoftwareOutputDeviceWin(surface_handle,
                d_software_output_device_backing.get(), display_client));
    }

    return output_surface;
}

std::unique_ptr<viz::DisplayCompositorMemoryAndTaskController> RenderFrameSinkProviderImpl::CreateGpuDependency(
      bool gpu_compositing,
      gpu::SurfaceHandle surface_handle,
      const viz::RendererSettings& renderer_settings)
{
    return nullptr;
}

void RenderFrameSinkProviderImpl::EstablishGpuChannelSyncOnMain(
    gpu::GpuChannelEstablishedCallback callback)
{
    content::RenderThreadImpl *render_thread =
        content::RenderThreadImpl::current();

    scoped_refptr<gpu::GpuChannelHost> gpu_channel =
        !render_thread->IsGpuCompositingDisabled() ?
        render_thread->EstablishGpuChannelSync()   :
        nullptr;

    d_compositor_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), gpu_channel));
}

void RenderFrameSinkProviderImpl::CreateForWidgetOnCompositor(
    int32_t compositor_id,
    mojo::PendingReceiver<::viz::mojom::CompositorFrameSink> compositor_frame_sink_receiver,
    mojo::PendingRemote<::viz::mojom::CompositorFrameSinkClient> compositor_frame_sink_client,
    scoped_refptr<gpu::GpuChannelHost> gpu_channel)
{
    auto it = d_compositor_data.find(compositor_id);
    if (it == d_compositor_data.end()) {
        d_main_task_runner->
            PostTask(
                FROM_HERE,
                base::BindOnce(
                    &RenderFrameSinkProviderImpl::DelegateToDefaultFrameSinkProviderOnMain,
                    base::Unretained(this),
                    std::move(compositor_frame_sink_receiver),
                    std::move(compositor_frame_sink_client)));
        return;
    }

    if (it->second.compositor_frame_sink_impl) {
        it->second.compositor_frame_sink_impl.reset();
    }

    d_gpu_channel = gpu_channel;

    it->second.compositor_frame_sink_impl =
        std::make_unique<RenderCompositorFrameSinkImpl>(
            *this,
            it->second.gpu_surface_handle,
            d_gpu_channel,
            d_compositor_task_runner,
            it->second.size,
            it->second.visible,
            std::move(compositor_frame_sink_receiver),
            std::move(compositor_frame_sink_client),
            &debug_renderer_settings_);
}

void RenderFrameSinkProviderImpl::DelegateToDefaultFrameSinkProviderOnMain(
    mojo::PendingReceiver<::viz::mojom::CompositorFrameSink> compositor_frame_sink_receiver,
    mojo::PendingRemote<::viz::mojom::CompositorFrameSinkClient> compositor_frame_sink_client)
{
    d_default_frame_sink_provider->CreateFrameSink(
        std::move(compositor_frame_sink_receiver),
        std::move(compositor_frame_sink_client));
}

//
GpuOutputSurface::GpuOutputSurface(scoped_refptr<viz::ContextProvider> context_provider,
                                   gpu::SurfaceHandle surface_handle)
: OutputSurface(context_provider)
//, viz_context_provider_(context_provider)
, surface_handle_(surface_handle)
, use_gpu_fence_(
        context_provider->ContextCapabilities().chromium_gpu_fence &&
        context_provider->ContextCapabilities()
            .use_gpu_fences_for_overlay_planes)
{
    const auto& context_capabilities = context_provider->ContextCapabilities();
    capabilities_.output_surface_origin = context_capabilities.surface_origin;
    capabilities_.supports_stencil = context_capabilities.num_stencil_bits > 0;
    // Since one of the buffers is used by the surface for presentation, there can
    // be at most |num_surface_buffers - 1| pending buffers that the compositor
    // can use.
    capabilities_.max_frames_pending =
        context_capabilities.num_surface_buffers - 1;
    capabilities_.supports_gpu_vsync = context_capabilities.gpu_vsync;
    capabilities_.supports_dc_layers = context_capabilities.dc_layers;
    capabilities_.supports_surfaceless = context_capabilities.surfaceless;
    capabilities_.android_surface_control_feature_enabled =
        context_provider->GetGpuFeatureInfo()
            .status_values[gpu::GPU_FEATURE_TYPE_ANDROID_SURFACE_CONTROL] ==
        gpu::kGpuFeatureStatusEnabled;
}

GpuOutputSurface::~GpuOutputSurface()
{
    if (gpu_fence_id_ > 0) {
        context_provider()->ContextGL()->DestroyGpuFenceCHROMIUM(gpu_fence_id_);
    }
}

void GpuOutputSurface::BindToClient(viz::OutputSurfaceClient* client)
{
    DCHECK(client);
    DCHECK(!client_);
    client_ = client;

    viz::ContextProviderCommandBuffer* provider_command_buffer =
        static_cast<viz::ContextProviderCommandBuffer*>(context_provider_.get());
    gpu::CommandBufferProxyImpl* command_buffer_proxy =
        provider_command_buffer->GetCommandBufferProxy();

    command_buffer_proxy->SetUpdateVSyncParametersCallback(
        base::BindRepeating(&GpuOutputSurface::OnUpdateVSyncParameters,
        weak_ptr_factory_.GetWeakPtr()));
}

void GpuOutputSurface::EnsureBackbuffer()
{
}

void GpuOutputSurface::DiscardBackbuffer()
{
    context_provider()->ContextGL()->DiscardBackbufferCHROMIUM();
}

void GpuOutputSurface::BindFramebuffer()
{
    context_provider()->ContextGL()->BindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GpuOutputSurface::SetDrawRectangle(const gfx::Rect& rect)
{
    DCHECK(capabilities_.supports_dc_layers);

    if (set_draw_rectangle_for_frame_)
    return;
    DCHECK(gfx::Rect(size_).Contains(rect));
    DCHECK(has_set_draw_rectangle_since_last_resize_ ||
            (gfx::Rect(size_) == rect));
    set_draw_rectangle_for_frame_ = true;
    has_set_draw_rectangle_since_last_resize_ = true;
    context_provider()->ContextGL()->SetDrawRectangleCHROMIUM(
        rect.x(), rect.y(), rect.width(), rect.height());
}

void GpuOutputSurface::Reshape(const gfx::Size& size,
                              float device_scale_factor,
                              const gfx::ColorSpace& color_space,
                              gfx::BufferFormat format,
                              bool use_stencil) {
    size_ = size;
    has_set_draw_rectangle_since_last_resize_ = false;
    context_provider()->ContextGL()->ResizeCHROMIUM(
        size.width(), size.height(), device_scale_factor,
        color_space.AsGLColorSpace(), gfx::AlphaBitsForBufferFormat(format));
}

void GpuOutputSurface::SwapBuffers(viz::OutputSurfaceFrame frame)
{
    DCHECK(context_provider_);

    uint32_t flags = 0;
    if (wants_vsync_parameter_updates_) {
        flags |= gpu::SwapBuffersFlags::kVSyncParams;
    }

    // The |swap_size| here should always be in the UI's logical screen space
    // since it is forwarded to the client code which is unaware of the display
    // transform optimization.
    gfx::Size swap_size = ApplyDisplayInverse(gfx::Rect(size_)).size();
    auto swap_callback = base::BindOnce(
        &GpuOutputSurface::OnGpuSwapBuffersCompleted,
        weak_ptr_factory_.GetWeakPtr(), std::move(frame.latency_info),
        frame.top_controls_visible_height_changed, swap_size);
    gpu::ContextSupport::PresentationCallback presentation_callback;
    presentation_callback = base::BindOnce(&GpuOutputSurface::OnPresentation,
                                            weak_ptr_factory_.GetWeakPtr());

    set_draw_rectangle_for_frame_ = false;
    if (frame.sub_buffer_rect) {
        HandlePartialSwap(*frame.sub_buffer_rect, flags, std::move(swap_callback),
                          std::move(presentation_callback));
    }
    else if (!frame.content_bounds.empty()) {
        context_provider_->ContextSupport()->SwapWithBounds(
            frame.content_bounds, flags, std::move(swap_callback),
            std::move(presentation_callback));
    }
    else {
        context_provider_->ContextSupport()->Swap(flags, std::move(swap_callback),
                                                    std::move(presentation_callback));
    }
}

uint32_t GpuOutputSurface::GetFramebufferCopyTextureFormat()
{
    auto* gl = static_cast<viz::ContextProviderCommandBuffer*>(context_provider());
    return gl->GetCopyTextureInternalFormat();
}

bool GpuOutputSurface::IsDisplayedAsOverlayPlane() const
{
    return false;
}

unsigned GpuOutputSurface::GetOverlayTextureId() const
{
    return 0;
}

bool GpuOutputSurface::HasExternalStencilTest() const
{
    return false;
}

void GpuOutputSurface::ApplyExternalStencil() {
}

void GpuOutputSurface::DidReceiveSwapBuffersAck(const gfx::SwapResponse& response)
{
    client_->DidReceiveSwapBuffersAck(response.timings);
}

void GpuOutputSurface::HandlePartialSwap(const gfx::Rect& sub_buffer_rect,
                                        uint32_t flags,
                                        gpu::ContextSupport::SwapCompletedCallback swap_callback,
                                        gpu::ContextSupport::PresentationCallback presentation_callback)
{
    context_provider_->ContextSupport()->PartialSwapBuffers(
        sub_buffer_rect, flags, std::move(swap_callback),
        std::move(presentation_callback));
}

void GpuOutputSurface::OnGpuSwapBuffersCompleted(
    std::vector<ui::LatencyInfo> latency_info,
    bool top_controls_visible_height_changed,
    const gfx::Size& pixel_size,
    const gpu::SwapBuffersCompleteParams& params)
{
    if (!params.texture_in_use_responses.empty()) {
        client_->DidReceiveTextureInUseResponses(params.texture_in_use_responses);
    }

    if (!params.ca_layer_params.is_empty) {
        client_->DidReceiveCALayerParams(params.ca_layer_params);
    }

    DidReceiveSwapBuffersAck(params.swap_response);

    UpdateLatencyInfoOnSwap(params.swap_response, &latency_info);
    latency_tracker_.OnGpuSwapBuffersCompleted(
        latency_info, top_controls_visible_height_changed);

    if (needs_swap_size_notifications_) {
        client_->DidSwapWithSize(pixel_size);
    }
}

void GpuOutputSurface::OnPresentation(const gfx::PresentationFeedback& feedback)
{
    client_->DidReceivePresentationFeedback(feedback);
}

void GpuOutputSurface::OnUpdateVSyncParameters(base::TimeTicks timebase, base::TimeDelta interval)
{
    if (!update_vsync_parameters_callback_) {
        return;
    }

    update_vsync_parameters_callback_.Run(timebase, interval);
}

unsigned GpuOutputSurface::UpdateGpuFence()
{
    if (!use_gpu_fence_) {
        return 0;
    }

    if (gpu_fence_id_ > 0) {
        context_provider()->ContextGL()->DestroyGpuFenceCHROMIUM(gpu_fence_id_);
    }

    gpu_fence_id_ = context_provider()->ContextGL()->CreateGpuFenceCHROMIUM();

    return gpu_fence_id_;
}

void GpuOutputSurface::SetNeedsSwapSizeNotifications(bool needs_swap_size_notifications)
{
    needs_swap_size_notifications_ = needs_swap_size_notifications;
}

void GpuOutputSurface::SetUpdateVSyncParametersCallback(viz::UpdateVSyncParametersCallback callback)
{
    update_vsync_parameters_callback_ = std::move(callback);
    wants_vsync_parameter_updates_ = !update_vsync_parameters_callback_.is_null();
}

gfx::OverlayTransform GpuOutputSurface::GetDisplayTransform()
{
    return gfx::OVERLAY_TRANSFORM_NONE;
}

gfx::Rect GpuOutputSurface::ApplyDisplayInverse(const gfx::Rect& input)
{
    gfx::Transform display_inverse = gfx::OverlayTransformToTransform(
        gfx::InvertOverlayTransform(GetDisplayTransform()), gfx::SizeF(size_));

    return cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
        display_inverse, input);
}

gpu::SurfaceHandle GpuOutputSurface::GetSurfaceHandle() const
{
    return surface_handle_;
}

//
RenderCompositorFrameSinkImpl::RenderCompositorFrameSinkImpl(
    RenderFrameSinkProviderImpl& context,
    gpu::SurfaceHandle gpu_surface_handle,
    scoped_refptr<gpu::GpuChannelHost> gpu_channel,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    gfx::Size size,
    bool visible,
    mojo::PendingReceiver<::viz::mojom::CompositorFrameSink> compositor_frame_sink_receiver,
    mojo::PendingRemote<::viz::mojom::CompositorFrameSinkClient> compositor_frame_sink_client,
    const viz::DebugRendererSettings* debug_settings)
: d_context(context)
, d_binding(this, std::move(compositor_frame_sink_receiver))
{
    d_frame_sink_id = d_context.frame_sink_id_allocator()->NextFrameSinkId();

    d_context.host_frame_sink_manager()->RegisterFrameSinkId(
        d_frame_sink_id, this, viz::ReportFirstSurfaceActivation::kNo);

    //
    bool gpu_compositing = (bool)gpu_channel;

    d_display_client = std::make_unique<viz::HostDisplayClient>(gpu_surface_handle);

    //
    auto root_params = viz::mojom::RootCompositorFrameSinkParams::New();

    root_params->frame_sink_id = d_frame_sink_id;
    root_params->widget = gpu_surface_handle;
    root_params->gpu_compositing = gpu_compositing;
    root_params->renderer_settings = viz::CreateRendererSettings();
    //root_params->compositor_frame_sink isn't set
    root_params->compositor_frame_sink_client = std::move(compositor_frame_sink_client);
    //root_params->display_private isn't set
    root_params->display_client = d_display_client->GetBoundRemote(task_runner);

    auto root_compositor_frame_sink = viz::RootCompositorFrameSinkImpl::Create(
        std::move(root_params),
        d_context.frame_sink_manager(), &d_context,
        viz::BeginFrameSource::kNotRestartableId, false, debug_settings);

    d_root_compositor_frame_sink = std::move(root_compositor_frame_sink);

    d_root_compositor_frame_sink->Resize(size);
    d_root_compositor_frame_sink->SetOutputIsSecure(false);
    d_root_compositor_frame_sink->SetDisplayVisible(visible);
}

RenderCompositorFrameSinkImpl::~RenderCompositorFrameSinkImpl()
{
    d_context.host_frame_sink_manager()->InvalidateFrameSinkId(d_frame_sink_id);
}

void RenderCompositorFrameSinkImpl::SetVisible(bool visible)
{
    d_root_compositor_frame_sink->SetDisplayVisible(visible);
}

void RenderCompositorFrameSinkImpl::Resize(gfx::Size size)
{
    d_root_compositor_frame_sink->Resize(size);
}

// viz::mojom::CompositorFrameSink overrides:
void RenderCompositorFrameSinkImpl::SetNeedsBeginFrame(bool needs_begin_frame)
{
    d_root_compositor_frame_sink->SetNeedsBeginFrame(needs_begin_frame);
}

void RenderCompositorFrameSinkImpl::SetWantsAnimateOnlyBeginFrames()
{
    d_root_compositor_frame_sink->SetWantsAnimateOnlyBeginFrames();
}

void RenderCompositorFrameSinkImpl::SubmitCompositorFrame(
    const viz::LocalSurfaceId& local_surface_id,
    viz::CompositorFrame frame,
    base::Optional<viz::HitTestRegionList> hit_test_region_list,
    uint64_t submit_time)
{
    d_root_compositor_frame_sink->SubmitCompositorFrame(
        local_surface_id, std::move(frame), hit_test_region_list, submit_time);
}

void RenderCompositorFrameSinkImpl::SubmitCompositorFrameSync(
    const viz::LocalSurfaceId& local_surface_id,
    viz::CompositorFrame frame,
    base::Optional<viz::HitTestRegionList> hit_test_region_list,
    uint64_t submit_time,
    SubmitCompositorFrameSyncCallback callback)
{
    d_root_compositor_frame_sink->SubmitCompositorFrameSync(
        local_surface_id, std::move(frame), hit_test_region_list, submit_time,
        std::move(callback));
}

void RenderCompositorFrameSinkImpl::DidNotProduceFrame(const viz::BeginFrameAck& ack)
{
    d_root_compositor_frame_sink->DidNotProduceFrame(ack);
}

void RenderCompositorFrameSinkImpl::DidAllocateSharedBitmap(
    base::ReadOnlySharedMemoryRegion region,
    const gpu::Mailbox& id)
{
    d_root_compositor_frame_sink->DidAllocateSharedBitmap(std::move(region), id);
}

void RenderCompositorFrameSinkImpl::DidDeleteSharedBitmap(const viz::SharedBitmapId& id)
{
    d_root_compositor_frame_sink->DidDeleteSharedBitmap(id);
}

void RenderCompositorFrameSinkImpl::InitializeCompositorFrameSinkType(
    viz::mojom::CompositorFrameSinkType type)
{
    d_root_compositor_frame_sink->InitializeCompositorFrameSinkType(type);
}

} // close namespace blpwtk2
