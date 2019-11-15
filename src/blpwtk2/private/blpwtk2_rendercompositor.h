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

#ifndef INCLUDED_BLPWTK2_RENDERCOMPOSITOR2_H
#define INCLUDED_BLPWTK2_RENDERCOMPOSITOR2_H

#include <base/lazy_instance.h>
#include <base/memory/scoped_refptr.h>
#include <content/common/frame_sink_provider.mojom.h>
#include <gpu/ipc/common/surface_handle.h>
#include <mojo/public/cpp/bindings/binding.h>
#include <ui/gfx/geometry/size.h>

#include <map>
#include <memory>

namespace base {
class SingleThreadTaskRunner;
} // close namespace base

namespace viz {
class ParentLocalSurfaceIdAllocator;
} // close namespace viz

namespace blpwtk2 {

class ProfileImpl;
class RenderCompositor;
class RenderFrameSinkProviderImpl;

class RenderCompositorFactory {

    friend struct base::LazyInstanceTraitsBase<RenderCompositorFactory>;
    friend class RenderCompositor;

    std::unique_ptr<RenderFrameSinkProviderImpl> d_frame_sink_provider;
    scoped_refptr<base::SingleThreadTaskRunner> d_compositor_task_runner;

    std::map<int, RenderCompositor *> d_compositors_by_widget_id;

    RenderCompositorFactory();

  public:

    static RenderCompositorFactory* GetInstance();
    static void Terminate();

    ~RenderCompositorFactory();

    void Bind(content::mojom::FrameSinkProviderRequest request);
    void Unbind();

    std::unique_ptr<RenderCompositor> CreateCompositor(
        int32_t widget_id,
        gpu::SurfaceHandle gpu_surface_handle,
        blpwtk2::ProfileImpl *profile);
};

class RenderCompositor {

    friend class RenderCompositorFactory;

    RenderCompositorFactory &d_factory;

    int32_t d_widget_id;
    gpu::SurfaceHandle d_gpu_surface_handle;
    blpwtk2::ProfileImpl *d_profile;
    scoped_refptr<base::SingleThreadTaskRunner> d_compositor_task_runner;
    std::unique_ptr<viz::ParentLocalSurfaceIdAllocator> d_local_surface_id_allocator;

    bool d_visible = false;
    gfx::Size d_size;

    RenderCompositor(
        RenderCompositorFactory &,
        int32_t widget_id,
        gpu::SurfaceHandle gpu_surface_handle,
        blpwtk2::ProfileImpl *profile);

  public:

    ~RenderCompositor();

    viz::LocalSurfaceIdAllocation GetLocalSurfaceIdAllocation();

    void SetVisible(bool visible);
    void Resize(const gfx::Size& size);
};

} // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_RENDERCOMPOSITOR_H
