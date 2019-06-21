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
#include <base/containers/flat_map.h>
#include <base/memory/ref_counted.h>
#include <base/message_loop/message_loop.h>
#include <cc/trees/layer_tree_frame_sink_client.h>
#include <components/viz/common/display/renderer_settings.h>
#include <components/viz/common/frame_sinks/begin_frame_source.h>
#include <components/viz/common/frame_sinks/delay_based_time_source.h>
#include <components/viz/common/surfaces/frame_sink_id.h>
#include <components/viz/common/surfaces/frame_sink_id_allocator.h>
#include <components/viz/common/surfaces/parent_local_surface_id_allocator.h>
#include <components/viz/host/host_frame_sink_client.h>
#include <components/viz/host/host_frame_sink_manager.h>
#include <components/viz/host/renderer_settings_creation.h>
#include <components/viz/service/display/display.h>
#include <components/viz/service/display_embedder/compositor_overlay_candidate_validator.h>
#include <components/viz/service/display_embedder/output_device_backing.h>
#include <components/viz/service/display_embedder/server_shared_bitmap_manager.h>
#include <components/viz/service/display_embedder/software_output_device_win.h>
#include <components/viz/service/display/display_scheduler.h>
#include <components/viz/service/frame_sinks/direct_layer_tree_frame_sink.h>
#include <components/viz/service/frame_sinks/frame_sink_manager_impl.h>
#include <content/browser/compositor/gpu_browser_compositor_output_surface.h>
#include <content/browser/compositor/software_browser_compositor_output_surface.h>
#include <content/common/render_frame_metadata.mojom.h>
#include <content/public/common/gpu_stream_constants.h>
#include <content/renderer/render_thread_impl.h>
#include <content/renderer/renderer_blink_platform_impl.h>
#include <services/service_manager/public/cpp/connector.h>
#include <services/viz/public/interfaces/compositing/compositor_frame_sink.mojom.h>
#include <services/ws/public/cpp/gpu/context_provider_command_buffer.h>
#include <ui/compositor/compositor_vsync_manager.h>

namespace blpwtk2 {

class RenderCompositorFrameSinkImpl;

class RenderFrameSinkProviderImpl : public content::mojom::FrameSinkProvider {

    mojo::Binding<content::mojom::FrameSinkProvider> d_binding;
    content::mojom::FrameSinkProviderPtr d_default_frame_sink_provider;

    scoped_refptr<base::SingleThreadTaskRunner> d_main_task_runner, d_compositor_task_runner;
    gpu::GpuMemoryBufferManager *d_gpu_memory_buffer_manager = nullptr;

    std::unique_ptr<viz::SharedBitmapManager> d_shared_bitmap_manager;
    std::unique_ptr<viz::FrameSinkManagerImpl> d_frame_sink_manager;
    std::unique_ptr<viz::HostFrameSinkManager> d_host_frame_sink_manager;
    std::unique_ptr<viz::FrameSinkIdAllocator> d_frame_sink_id_allocator;
    std::unique_ptr<viz::RendererSettings> d_renderer_settings;
    std::unique_ptr<viz::OutputDeviceBacking> d_software_output_device_backing;

    scoped_refptr<gpu::GpuChannelHost> d_gpu_channel;
    scoped_refptr<ws::ContextProviderCommandBuffer> d_worker_context_provider;

    struct CompositorData {
        CompositorData(gpu::SurfaceHandle gpu_surface_handle)
        : gpu_surface_handle(gpu_surface_handle) {}

        gpu::SurfaceHandle gpu_surface_handle;
        std::unique_ptr<RenderCompositorFrameSinkImpl> compositor_frame_sink_impl;
        bool visible = false;
        gfx::Size size;
    };

    std::map<int32_t, CompositorData> d_compositor_data;

    void EstablishGpuChannelSyncOnMain(gpu::GpuChannelEstablishedCallback callback);

    void CreateForWidgetOnCompositor(
        int32_t widget_id,
        viz::mojom::CompositorFrameSinkRequest compositor_frame_sink_request,
        viz::mojom::CompositorFrameSinkClientPtr compositor_frame_sink_client,
        scoped_refptr<gpu::GpuChannelHost> gpu_channel);

    void DelegateToDefaultFrameSinkProviderOnMain(
        int32_t widget_id,
        viz::mojom::CompositorFrameSinkRequest compositor_frame_sink_request,
        viz::mojom::CompositorFrameSinkClientPtr compositor_frame_sink_client);

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

    scoped_refptr<ws::ContextProviderCommandBuffer> worker_context_provider() {
        return d_worker_context_provider;
    }

    void Bind(content::mojom::FrameSinkProviderRequest request);
    void Unbind();

    void RegisterCompositor(int32_t widget_id, gpu::SurfaceHandle gpu_surface_handle);
    void UnregisterCompositor(int32_t widget_id);
    void SetCompositorVisible(int32_t widget_id, bool visible);
    void ResizeCompositor(int32_t widget_id, gfx::Size size, base::WaitableEvent *event);

    // content::mojom::FrameSinkProvider overrides:
    void CreateForWidget(
        int32_t widget_id,
        viz::mojom::CompositorFrameSinkRequest compositor_frame_sink_request,
        viz::mojom::CompositorFrameSinkClientPtr compositor_frame_sink_client) override;
    void RegisterRenderFrameMetadataObserver(
        int32_t widget_id,
        content::mojom::RenderFrameMetadataObserverClientRequest render_frame_metadata_observer_client_request,
        content::mojom::RenderFrameMetadataObserverPtr observer) override;
};

class RenderCompositorFrameSinkImpl : public viz::mojom::CompositorFrameSink,
                                      private viz::HostFrameSinkClient,
                                      private cc::LayerTreeFrameSinkClient,
                                      private viz::BeginFrameObserver {

    RenderFrameSinkProviderImpl& d_context;

    mojo::Binding<viz::mojom::CompositorFrameSink> d_binding;
    viz::mojom::CompositorFrameSinkClientPtr d_client;

    viz::FrameSinkId d_frame_sink_id;

    std::unique_ptr<viz::SyntheticBeginFrameSource> d_begin_frame_source;
    scoped_refptr<ui::CompositorVSyncManager> d_vsync_manager;
    std::unique_ptr<viz::Display> d_display;
    std::unique_ptr<cc::LayerTreeFrameSink> d_layer_tree_frame_sink;

    viz::BeginFrameSource *d_delegated_begin_frame_source = nullptr;
    viz::BeginFrameArgs d_last_begin_frame_args;
    bool d_client_needs_begin_frame = false;
    bool d_added_frame_observer = false;
    bool d_client_wants_animate_only_begin_frames = false;
    std::vector<viz::ReturnedResource> d_resources_to_reclaim;
    base::flat_map<uint32_t, gfx::PresentationFeedback> d_presentation_feedbacks;

    void UpdateNeedsBeginFrameSource();
    void UpdateVSyncParameters(base::TimeTicks timebase, base::TimeDelta interval);

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
        const SubmitCompositorFrameSyncCallback callback) override;
    void DidNotProduceFrame(const viz::BeginFrameAck& ack) override;
    void DidAllocateSharedBitmap(mojo::ScopedSharedBufferHandle buffer,
                                 const viz::SharedBitmapId& id) override;
    void DidDeleteSharedBitmap(const viz::SharedBitmapId& id) override;

    // viz::HostFrameSinkClient overrides:
    void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override {}
    void OnFrameTokenChanged(uint32_t frame_token) override {}

    // cc::LayerTreeFrameSinkClient overrides:
    void SetBeginFrameSource(viz::BeginFrameSource* source) override;
    base::Optional<viz::HitTestRegionList> BuildHitTestData() override { return base::Optional<viz::HitTestRegionList>(); }
    void ReclaimResources(const std::vector<viz::ReturnedResource>& resources) override;
    void SetTreeActivationCallback(base::RepeatingClosure callback) override {}
    void DidReceiveCompositorFrameAck() override;
    void DidPresentCompositorFrame(
        uint32_t presentation_token,
        const gfx::PresentationFeedback& feedback) override;
    void DidLoseLayerTreeFrameSink() override {}
    void DidNotNeedBeginFrame() override {}
    void OnDraw(const gfx::Transform& transform,
                const gfx::Rect& viewport,
                bool resourceless_software_draw,
                bool skip_draw) override {}
    void SetMemoryPolicy(const cc::ManagedMemoryPolicy& policy) override {}
    void SetExternalTilePriorityConstraints(
        const gfx::Rect& viewport_rect,
        const gfx::Transform& transform) override {}

    // viz::BeginFrameObserver implementation:
    void OnBeginFrame(const viz::BeginFrameArgs& args) override;
    const viz::BeginFrameArgs& LastUsedBeginFrameArgs() const override;
    void OnBeginFrameSourcePausedChanged(bool paused) override;
    bool WantsAnimateOnlyBeginFrames() const override;

public:

    RenderCompositorFrameSinkImpl(
        RenderFrameSinkProviderImpl& context,
        gpu::SurfaceHandle gpu_surface_handle,
        scoped_refptr<gpu::GpuChannelHost> gpu_channel,
        scoped_refptr<base::SingleThreadTaskRunner> task_runner,
        gfx::Size size,
        bool visible,
        viz::mojom::CompositorFrameSinkRequest compositor_frame_sink_request,
        viz::mojom::CompositorFrameSinkClientPtr compositor_frame_sink_client);
    ~RenderCompositorFrameSinkImpl() override;

    void SetVisible(bool visible);
    void Resize(gfx::Size size);
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

void RenderCompositorFactory::Bind(content::mojom::FrameSinkProviderRequest request)
{
    d_compositor_task_runner->
        PostTask(
            FROM_HERE,
            base::BindOnce(&RenderFrameSinkProviderImpl::Bind,
                base::Unretained(d_frame_sink_provider.get()),
                std::move(request)));
}

void RenderCompositorFactory::Unbind()
{
    d_compositor_task_runner->
        PostTask(
            FROM_HERE,
            base::BindOnce(&RenderFrameSinkProviderImpl::Unbind,
                base::Unretained(d_frame_sink_provider.get())));
}

std::unique_ptr<RenderCompositor> RenderCompositorFactory::CreateCompositor(
    int32_t widget_id,
    gpu::SurfaceHandle gpu_surface_handle,
    blpwtk2::ProfileImpl *profile)
{
    auto it = d_compositors_by_widget_id.find(widget_id);
    DCHECK(it == d_compositors_by_widget_id.end());

    std::unique_ptr<RenderCompositor> render_compositor(
        new RenderCompositor(
            *this, widget_id,
            gpu_surface_handle, profile));

    return render_compositor;
}

//
RenderCompositor::RenderCompositor(
    RenderCompositorFactory& factory,
    int32_t widget_id,
    gpu::SurfaceHandle gpu_surface_handle,
    blpwtk2::ProfileImpl *profile)
: d_factory(factory)
, d_widget_id(widget_id)
, d_gpu_surface_handle(gpu_surface_handle)
, d_profile(profile)
, d_compositor_task_runner(d_factory.d_compositor_task_runner)
, d_local_surface_id_allocator(new viz::ParentLocalSurfaceIdAllocator())
{
    d_factory.d_compositors_by_widget_id.insert(
        std::make_pair(d_widget_id, this));

    d_profile->registerNativeViewForComposition(d_gpu_surface_handle);

    d_compositor_task_runner->
        PostTask(
            FROM_HERE,
            base::BindOnce(&RenderFrameSinkProviderImpl::RegisterCompositor,
                base::Unretained(d_factory.d_frame_sink_provider.get()),
                d_widget_id,
                d_gpu_surface_handle));
}

RenderCompositor::~RenderCompositor()
{
    auto it = d_factory.d_compositors_by_widget_id.find(d_widget_id);
    DCHECK(it != d_factory.d_compositors_by_widget_id.end());

    d_factory.d_compositors_by_widget_id.erase(it);

    d_profile->unregisterNativeViewForComposition(d_gpu_surface_handle);

    d_compositor_task_runner->
        PostTask(
            FROM_HERE,
            base::BindOnce(&RenderFrameSinkProviderImpl::UnregisterCompositor,
                base::Unretained(d_factory.d_frame_sink_provider.get()),
                d_widget_id));
}

viz::LocalSurfaceIdAllocation RenderCompositor::GetLocalSurfaceIdAllocation()
{
    return d_local_surface_id_allocator->GetCurrentLocalSurfaceIdAllocation();
}

void RenderCompositor::SetVisible(bool visible)
{
    d_visible = visible;

    d_compositor_task_runner->
        PostTask(
            FROM_HERE,
            base::BindOnce(&RenderFrameSinkProviderImpl::SetCompositorVisible,
                base::Unretained(d_factory.d_frame_sink_provider.get()),
                d_widget_id,
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
                d_widget_id,
                d_size,
                &event));

    event.Wait();
}

//
RenderFrameSinkProviderImpl::RenderFrameSinkProviderImpl()
: d_binding(this)
{
    content::RenderThreadImpl *render_thread =
        content::RenderThreadImpl::current();

    d_main_task_runner = base::MessageLoopCurrent::Get()->task_runner();
    d_compositor_task_runner = render_thread->compositor_task_runner();

    render_thread->GetConnector()->
        BindInterface(
            content::RenderThreadImpl::current_blink_platform_impl()->GetBrowserServiceName(),
            mojo::MakeRequest(&d_default_frame_sink_provider));
}

RenderFrameSinkProviderImpl::~RenderFrameSinkProviderImpl()
{
}

void RenderFrameSinkProviderImpl::Init(
    gpu::GpuMemoryBufferManager *gpu_memory_buffer_manager)
{
    d_gpu_memory_buffer_manager = gpu_memory_buffer_manager;

    d_shared_bitmap_manager = std::make_unique<viz::ServerSharedBitmapManager>();

    d_frame_sink_manager = std::make_unique<viz::FrameSinkManagerImpl>(d_shared_bitmap_manager.get());
    d_host_frame_sink_manager = std::make_unique<viz::HostFrameSinkManager>();

    d_host_frame_sink_manager->SetLocalManager(d_frame_sink_manager.get());
    d_frame_sink_manager->SetLocalClient(d_host_frame_sink_manager.get());

    d_frame_sink_id_allocator = std::make_unique<viz::FrameSinkIdAllocator>(0);

    d_renderer_settings = std::make_unique<viz::RendererSettings>(viz::CreateRendererSettings());
    d_software_output_device_backing = std::make_unique<viz::OutputDeviceBacking>();
}

void RenderFrameSinkProviderImpl::Bind(content::mojom::FrameSinkProviderRequest request)
{
    d_binding.Bind(std::move(request), d_compositor_task_runner);
}

void RenderFrameSinkProviderImpl::Unbind()
{
    d_binding.Close();
}

void RenderFrameSinkProviderImpl::RegisterCompositor(
    int32_t widget_id, gpu::SurfaceHandle gpu_surface_handle)
{
    auto it = d_compositor_data.find(widget_id);
    DCHECK(it == d_compositor_data.end());

    CompositorData compositor_data(gpu_surface_handle);

    d_compositor_data.insert(
        std::make_pair(
            widget_id, std::move(compositor_data)));
}

void RenderFrameSinkProviderImpl::UnregisterCompositor(
    int32_t widget_id)
{
    auto it = d_compositor_data.find(widget_id);
    DCHECK(it != d_compositor_data.end());

    d_compositor_data.erase(widget_id);
}

void RenderFrameSinkProviderImpl::SetCompositorVisible(int32_t widget_id, bool visible)
{
    auto it = d_compositor_data.find(widget_id);
    DCHECK(it != d_compositor_data.end());

    it->second.visible = visible;

    if (it->second.compositor_frame_sink_impl) {
        it->second.compositor_frame_sink_impl->SetVisible(it->second.visible);
    }
}

void RenderFrameSinkProviderImpl::ResizeCompositor(int32_t widget_id, gfx::Size size, base::WaitableEvent *event)
{
    auto it = d_compositor_data.find(widget_id);
    DCHECK(it != d_compositor_data.end());

    it->second.size = size;

    if (it->second.compositor_frame_sink_impl) {
        it->second.compositor_frame_sink_impl->Resize(it->second.size);
    }

    event->Signal();
}

void RenderFrameSinkProviderImpl::CreateForWidget(
    int32_t widget_id,
    viz::mojom::CompositorFrameSinkRequest compositor_frame_sink_request,
    viz::mojom::CompositorFrameSinkClientPtr compositor_frame_sink_client)
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
                    widget_id,
                    std::move(compositor_frame_sink_request),
                    std::move(compositor_frame_sink_client))));
}

void RenderFrameSinkProviderImpl::RegisterRenderFrameMetadataObserver(
    int32_t widget_id,
    content::mojom::RenderFrameMetadataObserverClientRequest render_frame_metadata_observer_client_request,
    content::mojom::RenderFrameMetadataObserverPtr observer)
{
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
    int32_t widget_id,
    viz::mojom::CompositorFrameSinkRequest compositor_frame_sink_request,
    viz::mojom::CompositorFrameSinkClientPtr compositor_frame_sink_client,
    scoped_refptr<gpu::GpuChannelHost> gpu_channel)
{
    auto it = d_compositor_data.find(widget_id);
    if (it == d_compositor_data.end()) {
        d_main_task_runner->
            PostTask(
                FROM_HERE,
                base::BindOnce(
                    &RenderFrameSinkProviderImpl::DelegateToDefaultFrameSinkProviderOnMain,
                    base::Unretained(this),
                    widget_id,
                    std::move(compositor_frame_sink_request),
                    std::move(compositor_frame_sink_client)));
        return;
    }

    if (it->second.compositor_frame_sink_impl) {
        it->second.compositor_frame_sink_impl.reset();
    }

    if (d_gpu_channel != gpu_channel) {
        d_gpu_channel = gpu_channel;

        if (d_gpu_channel) {
            constexpr bool automatic_flushes = false;
            constexpr bool support_locking   = true;
            constexpr bool support_gles2_interface = true;
            constexpr bool support_raster_interface = true;
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

            d_worker_context_provider = new ws::ContextProviderCommandBuffer(
                d_gpu_channel,
                d_gpu_memory_buffer_manager,
                content::kGpuStreamIdDefault,
                content::kGpuStreamPriorityUI,
                gpu::kNullSurfaceHandle,
                GURL("chrome://gpu/RenderFrameSinkProviderImpl::Init"),
                automatic_flushes,
                support_locking,
                support_grcontext,
                gpu::SharedMemoryLimits(),
                attributes,
                ws::command_buffer_metrics::ContextType::RENDER_WORKER);

            if (d_worker_context_provider->BindToCurrentThread()
                    != gpu::ContextResult::kSuccess) {
                d_worker_context_provider = nullptr;
            }
        }
        else {
            d_worker_context_provider.reset();
        }
    }

    it->second.compositor_frame_sink_impl =
        std::make_unique<RenderCompositorFrameSinkImpl>(
            *this,
            it->second.gpu_surface_handle,
            d_gpu_channel,
            d_compositor_task_runner,
            it->second.size,
            it->second.visible,
            std::move(compositor_frame_sink_request),
            std::move(compositor_frame_sink_client));
}

void RenderFrameSinkProviderImpl::DelegateToDefaultFrameSinkProviderOnMain(
        int32_t widget_id,
        viz::mojom::CompositorFrameSinkRequest compositor_frame_sink_request,
        viz::mojom::CompositorFrameSinkClientPtr compositor_frame_sink_client)
{
    d_default_frame_sink_provider->CreateForWidget(
        widget_id,
        std::move(compositor_frame_sink_request),
        std::move(compositor_frame_sink_client));
}

//
RenderCompositorFrameSinkImpl::RenderCompositorFrameSinkImpl(
    RenderFrameSinkProviderImpl& context,
    gpu::SurfaceHandle gpu_surface_handle,
    scoped_refptr<gpu::GpuChannelHost> gpu_channel,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    gfx::Size size,
    bool visible,
    viz::mojom::CompositorFrameSinkRequest compositor_frame_sink_request,
    viz::mojom::CompositorFrameSinkClientPtr compositor_frame_sink_client)
: d_context(context)
, d_binding(this)
{
    d_binding.Bind(std::move(compositor_frame_sink_request), task_runner);
    d_client = std::move(compositor_frame_sink_client);

    d_frame_sink_id = d_context.frame_sink_id_allocator()->NextFrameSinkId();
    d_context.host_frame_sink_manager()->RegisterFrameSinkId(
        d_frame_sink_id, this, viz::ReportFirstSurfaceActivation::kNo);

    //
    d_begin_frame_source =
        std::make_unique<viz::DelayBasedBeginFrameSource>(
            std::make_unique<viz::DelayBasedTimeSource>(
                task_runner.get()),
                viz::BeginFrameSource::kNotRestartableId);

    d_context.frame_sink_manager()->RegisterBeginFrameSource(
        d_begin_frame_source.get(), d_frame_sink_id);

    //
    scoped_refptr<ws::ContextProviderCommandBuffer> worker_context_provider =
        gpu_channel                          ?
        d_context.worker_context_provider() :
        nullptr;

    scoped_refptr<ws::ContextProviderCommandBuffer> context_provider;

    if (worker_context_provider) {
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

        context_provider = new ws::ContextProviderCommandBuffer(
            gpu_channel,
            d_context.gpu_memory_buffer_manager(),
            content::kGpuStreamIdDefault,
            content::kGpuStreamPriorityUI,
            gpu_surface_handle,
            GURL("chrome://gpu/RenderCompositorFrameSinkImpl::CreateFrameSink"),
            automatic_flushes,
            support_locking,
            support_grcontext,
            gpu::SharedMemoryLimits(),
            attributes,
            ws::command_buffer_metrics::ContextType::BROWSER_COMPOSITOR);

        if (context_provider->BindToCurrentThread()
                != gpu::ContextResult::kSuccess) {
            context_provider = nullptr;
            worker_context_provider = nullptr;
        }
    }

    // The viz::OutputSurface for the viz::Display:
    d_vsync_manager = new ui::CompositorVSyncManager();

    content::BrowserCompositorOutputSurface::UpdateVSyncParametersCallback
        update_vsync_parameters_callback =
            base::Bind(
                &RenderCompositorFrameSinkImpl::UpdateVSyncParameters,
                base::Unretained(this));

    std::unique_ptr<viz::OutputSurface> display_output_surface;

    if (context_provider && worker_context_provider) {
        display_output_surface =
            std::make_unique<content::GpuBrowserCompositorOutputSurface>(
                context_provider,
                update_vsync_parameters_callback,
                nullptr);
    }
    else {
        display_output_surface =
            std::make_unique<content::SoftwareBrowserCompositorOutputSurface>(
                viz::CreateSoftwareOutputDeviceWinBrowser(
                    gpu_surface_handle,
                    d_context.software_output_device_backing()),
                update_vsync_parameters_callback);
    }

    // The viz::DisplayScheduler:
    constexpr bool wait_for_all_pipeline_stages_before_draw = false;

    auto display_scheduler =
        std::make_unique<viz::DisplayScheduler>(
            d_begin_frame_source.get(),
            task_runner.get(),
            display_output_surface->capabilities().max_frames_pending,
            wait_for_all_pipeline_stages_before_draw);

    // The viz::Display:
    d_display =
        std::make_unique<viz::Display>(
            d_context.shared_bitmap_manager(),
            *d_context.renderer_settings(),
            d_frame_sink_id,
            std::move(display_output_surface),
            std::move(display_scheduler),
            task_runner);

    d_display->Resize(size);
    d_display->SetOutputIsSecure(true);

    d_display->SetVisible(visible);

    // The cc::LayerTreeFrameSink:
    d_layer_tree_frame_sink =
        std::make_unique<viz::DirectLayerTreeFrameSink>(
            d_frame_sink_id,
            d_context.host_frame_sink_manager(),
            d_context.frame_sink_manager(),
            d_display.get(), nullptr,
            context_provider,
            worker_context_provider,
            task_runner,
            d_context.gpu_memory_buffer_manager(),
            false);

    d_layer_tree_frame_sink->BindToClient(this);
}

RenderCompositorFrameSinkImpl::~RenderCompositorFrameSinkImpl()
{
    d_context.host_frame_sink_manager()->InvalidateFrameSinkId(
        d_frame_sink_id);

    d_context.frame_sink_manager()->UnregisterBeginFrameSource(
        d_begin_frame_source.get());

    d_layer_tree_frame_sink->DetachFromClient();
}

void RenderCompositorFrameSinkImpl::SetVisible(bool visible)
{
    DCHECK(d_display);

    d_display->SetVisible(visible);
}

void RenderCompositorFrameSinkImpl::Resize(gfx::Size size)
{
    DCHECK(d_display);

    d_display->Resize(size);
}

void RenderCompositorFrameSinkImpl::UpdateNeedsBeginFrameSource()
{
    if (!d_delegated_begin_frame_source) {
        return;
    }

    // We require a begin frame if there's a callback pending, or if the client
    // requested it.
    bool needs_begin_frame =
        d_client_needs_begin_frame;

    if (needs_begin_frame == d_added_frame_observer) {
        return;
    }

    if (needs_begin_frame) {
        d_delegated_begin_frame_source->AddObserver(this);
        d_added_frame_observer = true;
    }
    else {
        d_delegated_begin_frame_source->RemoveObserver(this);
        d_added_frame_observer = false;
    }
}

void RenderCompositorFrameSinkImpl::UpdateVSyncParameters(base::TimeTicks timebase, base::TimeDelta interval)
{
    if (d_begin_frame_source) {
        d_begin_frame_source->OnUpdateVSyncParameters(timebase, interval);
    }

    d_vsync_manager->UpdateVSyncParameters(timebase, interval);
}

// viz::mojom::CompositorFrameSink overrides:
void RenderCompositorFrameSinkImpl::SetNeedsBeginFrame(bool needs_begin_frame)
{
    d_client_needs_begin_frame = needs_begin_frame;
    UpdateNeedsBeginFrameSource();
}

void RenderCompositorFrameSinkImpl::SetWantsAnimateOnlyBeginFrames()
{
    d_client_wants_animate_only_begin_frames = true;
}

void RenderCompositorFrameSinkImpl::SubmitCompositorFrame(
    const viz::LocalSurfaceId& local_surface_id,
    viz::CompositorFrame frame,
    base::Optional<viz::HitTestRegionList> hit_test_region_list,
    uint64_t submit_time)
{
    d_resources_to_reclaim = viz::TransferableResource::ReturnResources(frame.resource_list);

    d_layer_tree_frame_sink->SubmitCompositorFrame(std::move(frame), /*hit_test_data_changed*/false, false);
}

void RenderCompositorFrameSinkImpl::SubmitCompositorFrameSync(
    const viz::LocalSurfaceId& local_surface_id,
    viz::CompositorFrame frame,
    base::Optional<viz::HitTestRegionList> hit_test_region_list,
    uint64_t submit_time,
    const SubmitCompositorFrameSyncCallback callback)
{
    NOTIMPLEMENTED();
}

void RenderCompositorFrameSinkImpl::DidNotProduceFrame(const viz::BeginFrameAck& ack)
{
    d_layer_tree_frame_sink->DidNotProduceFrame(ack);
}

void RenderCompositorFrameSinkImpl::DidAllocateSharedBitmap(
    mojo::ScopedSharedBufferHandle buffer,
    const viz::SharedBitmapId& id)
{
    d_layer_tree_frame_sink->DidAllocateSharedBitmap(std::move(buffer), id);
}

void RenderCompositorFrameSinkImpl::DidDeleteSharedBitmap(const viz::SharedBitmapId& id)
{
    d_layer_tree_frame_sink->DidDeleteSharedBitmap(id);
}

// cc::LayerTreeFrameSinkClient overrides:
void RenderCompositorFrameSinkImpl::SetBeginFrameSource(viz::BeginFrameSource* source)
{
    if (d_delegated_begin_frame_source && d_added_frame_observer) {
        d_delegated_begin_frame_source->RemoveObserver(this);
        d_added_frame_observer = false;
    }

    d_delegated_begin_frame_source = source;

    UpdateNeedsBeginFrameSource();
}

void RenderCompositorFrameSinkImpl::ReclaimResources(const std::vector<viz::ReturnedResource>& resources)
{
}

void RenderCompositorFrameSinkImpl::DidReceiveCompositorFrameAck()
{
    if (d_client) {
        d_client->DidReceiveCompositorFrameAck(
            std::move(d_resources_to_reclaim));
    }
}

void RenderCompositorFrameSinkImpl::DidPresentCompositorFrame(
    uint32_t presentation_token,
    const gfx::PresentationFeedback& feedback)
{
    d_presentation_feedbacks.insert({ presentation_token, feedback });
}

void RenderCompositorFrameSinkImpl::OnBeginFrame(const viz::BeginFrameArgs& args)
{
    d_last_begin_frame_args = args;

    if (d_client && d_client_needs_begin_frame) {
        d_client->OnBeginFrame(args, d_presentation_feedbacks);
        d_presentation_feedbacks.clear();
    }
}

const viz::BeginFrameArgs& RenderCompositorFrameSinkImpl::LastUsedBeginFrameArgs() const
{
    return d_last_begin_frame_args;
}

void RenderCompositorFrameSinkImpl::OnBeginFrameSourcePausedChanged(bool paused)
{
    if (d_client) {
        d_client->OnBeginFramePausedChanged(paused);
    }
}

bool RenderCompositorFrameSinkImpl::WantsAnimateOnlyBeginFrames() const
{
    return d_client_wants_animate_only_begin_frames;
}

} // close namespace blpwtk2
