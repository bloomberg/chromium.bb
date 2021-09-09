// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_H_

#include <stdint.h>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_region.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_image_source.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap_source.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_image_source_util.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webcodecs/plane.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame_handle.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

// Note: Don't include "media/base/video_frame.h" here without good reason,
// since it includes a lot of non-blink types which can pollute the namespace.

namespace media {
class VideoFrame;
}

namespace blink {

class ArrayBufferOrArrayBufferView;
class CanvasImageSource;
class ExceptionState;
class ExecutionContext;
class PlaneInit;
class ScriptPromise;
class ScriptState;
class VideoFrameInit;
class VideoFramePlaneInit;
class VideoFrameReadIntoOptions;

class MODULES_EXPORT VideoFrame final : public ScriptWrappable,
                                        public CanvasImageSource,
                                        public ImageBitmapSource {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Creates a VideoFrame with a new VideoFrameHandle wrapping |frame|.
  VideoFrame(scoped_refptr<media::VideoFrame> frame, ExecutionContext*);

  // Creates a VideoFrame from an existing handle.
  // All frames sharing |handle| will have their |handle_| invalidated if any of
  // the frames receives a call to close().
  explicit VideoFrame(scoped_refptr<VideoFrameHandle> handle);

  // video_frame.idl implementation.
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  static VideoFrame* Create(ScriptState* script_state,
                            const V8CanvasImageSource* source,
                            const VideoFrameInit* init,
                            ExceptionState& exception_state);
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  static VideoFrame* Create(ScriptState*,
                            const CanvasImageSourceUnion&,
                            const VideoFrameInit*,
                            ExceptionState&);
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  static VideoFrame* Create(ScriptState*,
                            const HeapVector<Member<PlaneInit>>&,
                            const VideoFramePlaneInit*,
                            ExceptionState&);

  String format() const;
  absl::optional<HeapVector<Member<Plane>>> planes();

  uint32_t codedWidth() const;
  uint32_t codedHeight() const;

  VideoFrameRegion* codedRegion() const;
  VideoFrameRegion* visibleRegion() const;

  uint32_t cropLeft(ExecutionContext*) const;
  uint32_t cropTop(ExecutionContext*) const;
  uint32_t cropWidth(ExecutionContext*) const;
  uint32_t cropHeight(ExecutionContext*) const;

  uint32_t displayWidth() const;
  uint32_t displayHeight() const;

  absl::optional<int64_t> timestamp() const;
  absl::optional<uint64_t> duration() const;

  uint32_t allocationSize(VideoFrameReadIntoOptions* options, ExceptionState&);

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  ScriptPromise readInto(ScriptState* script_state,
                         const V8BufferSource* destination,
                         VideoFrameReadIntoOptions* options,
                         ExceptionState& exception_state);
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  ScriptPromise readInto(ScriptState*,
                         const ArrayBufferOrArrayBufferView& destination,
                         VideoFrameReadIntoOptions* options,
                         ExceptionState&);
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

  // Invalidates |handle_|, releasing underlying media::VideoFrame references.
  // This effectively "destroys" all frames sharing the same Handle.
  void close();

  // Creates a clone of |this|, with a new Handle, referencing the same
  // media::VideoFrame. The cloned frame will not be closed when |this| is,
  // and its lifetime should be independently managed.
  VideoFrame* clone(ExceptionState&);

  // Convenience functions
  scoped_refptr<VideoFrameHandle> handle() const { return handle_; }
  scoped_refptr<media::VideoFrame> frame() const { return handle_->frame(); }

  // GarbageCollected override
  void Trace(Visitor*) const override;

 private:
  // CanvasImageSource implementation
  scoped_refptr<Image> GetSourceImageForCanvas(SourceImageStatus*,
                                               const FloatSize&) override;
  bool WouldTaintOrigin() const override;
  FloatSize ElementSize(const FloatSize&,
                        const RespectImageOrientationEnum) const override;
  bool IsVideoFrame() const override;
  bool IsOpaque() const override;
  bool IsAccelerated() const override;

  // ImageBitmapSource implementation
  static constexpr uint64_t kCpuEfficientFrameSize = 320u * 240u;
  IntSize BitmapSourceSize() const override;
  ScriptPromise CreateImageBitmap(ScriptState*,
                                  absl::optional<IntRect> crop_rect,
                                  const ImageBitmapOptions*,
                                  ExceptionState&) override;

  scoped_refptr<VideoFrameHandle> handle_;
  HeapVector<Member<Plane>> planes_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_H_
