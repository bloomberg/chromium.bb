// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_MOCK_COMPOSITOR_FRAME_SINK_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_MOCK_COMPOSITOR_FRAME_SINK_H_

#include "components/viz/common/quads/compositor_frame.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/platform/modules/frame_sinks/embedded_frame_sink.mojom-blink.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// A CompositorFrameSink to be offered by an EmbeddedFrameSinkProvider, for
// inspecting the viz::CompositorFrame sent to SubmitcompositorFrame()
// or SubmitcompositorFrameSync();
class MockCompositorFrameSink : public viz::mojom::blink::CompositorFrameSink {
 public:
  explicit MockCompositorFrameSink(
      viz::mojom::blink::CompositorFrameSinkRequest request)
      : binding_(this, std::move(request)) {
    EXPECT_CALL(*this, SetNeedsBeginFrame(true)).Times(::testing::AnyNumber());
  }

  // viz::mojom::blink::CompositorFrameSink implementation
  MOCK_METHOD1(SetNeedsBeginFrame, void(bool));
  MOCK_METHOD0(SetWantsAnimateOnlyBeginFrames, void(void));
  void SubmitCompositorFrame(const viz::LocalSurfaceId&,
                             viz::CompositorFrame frame,
                             viz::mojom::blink::HitTestRegionListPtr,
                             uint64_t) {
    DoSubmitCompositorFrame(&frame);
  }
  MOCK_METHOD1(DoSubmitCompositorFrame, void(viz::CompositorFrame*));
  void SubmitCompositorFrameSync(const viz::LocalSurfaceId&,
                                 viz::CompositorFrame frame,
                                 viz::mojom::blink::HitTestRegionListPtr,
                                 uint64_t,
                                 SubmitCompositorFrameSyncCallback cb) {
    DoSubmitCompositorFrameSync(&frame);
    std::move(cb).Run(WTF::Vector<viz::ReturnedResource>());
  }
  MOCK_METHOD1(DoSubmitCompositorFrameSync, void(viz::CompositorFrame*));
  MOCK_METHOD1(DidNotProduceFrame, void(const viz::BeginFrameAck&));
  MOCK_METHOD2(DidAllocateSharedBitmap,
               void(mojo::ScopedSharedBufferHandle,
                    gpu::mojom::blink::MailboxPtr));
  MOCK_METHOD1(DidDeleteSharedBitmap, void(gpu::mojom::blink::MailboxPtr));

 private:
  mojo::Binding<viz::mojom::blink::CompositorFrameSink> binding_;

  DISALLOW_COPY_AND_ASSIGN(MockCompositorFrameSink);
};

// A Provider that creates and binds a MockCompositorFrameSink when requested.
class MockEmbeddedFrameSinkProvider
    : public mojom::blink::EmbeddedFrameSinkProvider {
 public:
  // EmbeddedFrameSinkProvider implementation.
  MOCK_METHOD3(RegisterEmbeddedFrameSink,
               void(const viz::FrameSinkId&,
                    const viz::FrameSinkId&,
                    mojom::blink::EmbeddedFrameSinkClientPtr));
  void CreateCompositorFrameSink(
      const viz::FrameSinkId& frame_sink_id,
      viz::mojom::blink::CompositorFrameSinkClientPtr client,
      viz::mojom::blink::CompositorFrameSinkRequest sink) override {
    mock_compositor_frame_sink_ =
        std::make_unique<MockCompositorFrameSink>(std::move(sink));
    CreateCompositorFrameSink(frame_sink_id);
  }
  MOCK_METHOD1(CreateCompositorFrameSink, void(const viz::FrameSinkId&));
  MOCK_METHOD5(CreateSimpleCompositorFrameSink,
               void(const viz::FrameSinkId&,
                    const viz::FrameSinkId&,
                    mojom::blink::EmbeddedFrameSinkClientPtr,
                    viz::mojom::blink::CompositorFrameSinkClientPtr,
                    viz::mojom::blink::CompositorFrameSinkRequest));

  std::unique_ptr<MockCompositorFrameSink> mock_compositor_frame_sink_;

  // Utility method to create a scoped EmbeddedFrameSinkProvider override.
  std::unique_ptr<TestingPlatformSupport::ScopedOverrideMojoInterface>
  CreateScopedOverrideMojoInterface(
      mojo::Binding<mojom::blink::EmbeddedFrameSinkProvider>* binding) {
    using mojom::blink::EmbeddedFrameSinkProvider;
    using mojom::blink::EmbeddedFrameSinkProviderRequest;

    return std::make_unique<
        TestingPlatformSupport::ScopedOverrideMojoInterface>(WTF::BindRepeating(
        [](mojo::Binding<EmbeddedFrameSinkProvider>* binding,
           const char* interface_name, mojo::ScopedMessagePipeHandle pipe) {
          if (strcmp(interface_name, EmbeddedFrameSinkProvider::Name_))
            return;
          if (binding->is_bound())
            binding->Unbind();
          binding->Bind(EmbeddedFrameSinkProviderRequest(std::move(pipe)));
        },
        WTF::Unretained(binding)));
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_MOCK_COMPOSITOR_FRAME_SINK_H_
