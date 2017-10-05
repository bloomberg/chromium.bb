/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ImageBitmapFactories_h
#define ImageBitmapFactories_h

#include <memory>
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/image_bitmap_source.h"
#include "core/fileapi/FileReaderLoader.h"
#include "core/fileapi/FileReaderLoaderClient.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/imagebitmap/ImageBitmapOptions.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/Supplementable.h"
#include "platform/bindings/ScriptState.h"
#include "platform/geometry/IntRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkImage;

namespace blink {

class Blob;
class EventTarget;
class ExecutionContext;
class ImageBitmapSource;
class ImageBitmapOptions;
class WebTaskRunner;

typedef HTMLImageElementOrSVGImageElementOrHTMLVideoElementOrHTMLCanvasElementOrBlobOrImageDataOrImageBitmapOrOffscreenCanvas
    ImageBitmapSourceUnion;

class ImageBitmapFactories final
    : public GarbageCollectedFinalized<ImageBitmapFactories>,
      public Supplement<LocalDOMWindow>,
      public Supplement<WorkerGlobalScope>,
      public TraceWrapperBase {
  USING_GARBAGE_COLLECTED_MIXIN(ImageBitmapFactories);

 public:
  static ScriptPromise createImageBitmap(ScriptState*,
                                         EventTarget&,
                                         const ImageBitmapSourceUnion&,
                                         const ImageBitmapOptions&);
  static ScriptPromise createImageBitmap(ScriptState*,
                                         EventTarget&,
                                         const ImageBitmapSourceUnion&,
                                         int sx,
                                         int sy,
                                         int sw,
                                         int sh,
                                         const ImageBitmapOptions&);
  static ScriptPromise createImageBitmap(ScriptState*,
                                         EventTarget&,
                                         ImageBitmapSource*,
                                         Optional<IntRect> crop_rect,
                                         const ImageBitmapOptions&);
  static ScriptPromise CreateImageBitmapFromBlob(ScriptState*,
                                                 EventTarget&,
                                                 ImageBitmapSource*,
                                                 Optional<IntRect> crop_rect,
                                                 const ImageBitmapOptions&);

  virtual ~ImageBitmapFactories() {}

  DECLARE_TRACE();

 protected:
  static const char* SupplementName();

 private:
  class ImageBitmapLoader final
      : public GarbageCollectedFinalized<ImageBitmapLoader>,
        public FileReaderLoaderClient {
   public:
    static ImageBitmapLoader* Create(ImageBitmapFactories& factory,
                                     Optional<IntRect> crop_rect,
                                     const ImageBitmapOptions& options,
                                     ScriptState* script_state) {
      return new ImageBitmapLoader(factory, crop_rect, script_state, options);
    }

    void LoadBlobAsync(ExecutionContext*, Blob*);
    ScriptPromise Promise() { return resolver_->Promise(); }

    DECLARE_TRACE();

    ~ImageBitmapLoader() override {}

   private:
    ImageBitmapLoader(ImageBitmapFactories&,
                      Optional<IntRect> crop_rect,
                      ScriptState*,
                      const ImageBitmapOptions&);

    enum ImageBitmapRejectionReason {
      kUndecodableImageBitmapRejectionReason,
      kAllocationFailureImageBitmapRejectionReason,
    };

    void RejectPromise(ImageBitmapRejectionReason);

    void ScheduleAsyncImageBitmapDecoding(DOMArrayBuffer*);
    void DecodeImageOnDecoderThread(
        RefPtr<WebTaskRunner>,
        DOMArrayBuffer*,
        const String& premultiply_alpha_option,
        const String& color_space_conversion_option);
    void ResolvePromiseOnOriginalThread(sk_sp<SkImage>);

    // FileReaderLoaderClient
    void DidStartLoading() override {}
    void DidReceiveData() override {}
    void DidFinishLoading() override;
    void DidFail(FileError::ErrorCode) override;

    std::unique_ptr<FileReaderLoader> loader_;
    Member<ImageBitmapFactories> factory_;
    Member<ScriptPromiseResolver> resolver_;
    Optional<IntRect> crop_rect_;
    ImageBitmapOptions options_;
  };

  static ImageBitmapFactories& From(EventTarget&);

  template <class GlobalObject>
  static ImageBitmapFactories& FromInternal(GlobalObject&);

  void AddLoader(ImageBitmapLoader*);
  void DidFinishLoading(ImageBitmapLoader*);

  HeapHashSet<Member<ImageBitmapLoader>> pending_loaders_;
};

}  // namespace blink

#endif  // ImageBitmapFactories_h
