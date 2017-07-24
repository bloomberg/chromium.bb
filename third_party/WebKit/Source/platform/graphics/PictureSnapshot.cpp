/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "platform/graphics/PictureSnapshot.h"

#include <memory>
#include "platform/geometry/IntSize.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/LoggingCanvas.h"
#include "platform/graphics/ProfilingCanvas.h"
#include "platform/graphics/ReplayingCanvas.h"
#include "platform/graphics/skia/ImagePixelLocker.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "platform/image-decoders/ImageFrame.h"
#include "platform/image-decoders/SegmentReader.h"
#include "platform/image-encoders/ImageEncoder.h"
#include "platform/wtf/CurrentTime.h"
#include "platform/wtf/HexNumber.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/text/Base64.h"
#include "platform/wtf/text/TextEncoding.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageDeserializer.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkStream.h"

namespace blink {

PictureSnapshot::PictureSnapshot(sk_sp<const SkPicture> picture)
    : picture_(std::move(picture)) {}

class SkiaImageDecoder final : public SkImageDeserializer {
 public:
  sk_sp<SkImage> makeFromMemory(const void* data,
                                size_t length,
                                const SkIRect* subset) override {
    // No need to copy the data; this decodes immediately.
    RefPtr<SegmentReader> segment_reader =
        SegmentReader::CreateFromSkData(SkData::MakeWithoutCopy(data, length));
    std::unique_ptr<ImageDecoder> image_decoder = ImageDecoder::Create(
        std::move(segment_reader), true, ImageDecoder::kAlphaPremultiplied,
        ColorBehavior::Ignore());
    if (!image_decoder)
      return nullptr;

    ImageFrame* frame = image_decoder->DecodeFrameBufferAtIndex(0);
    return (frame && !image_decoder->Failed())
               ? frame->FinalizePixelsAndGetImage()
               : nullptr;
  }
  sk_sp<SkImage> makeFromData(SkData* data, const SkIRect* subset) override {
    return this->makeFromMemory(data->data(), data->size(), subset);
  }
};

PassRefPtr<PictureSnapshot> PictureSnapshot::Load(
    const Vector<RefPtr<TilePictureStream>>& tiles) {
  DCHECK(!tiles.IsEmpty());
  Vector<sk_sp<SkPicture>> pictures;
  pictures.ReserveCapacity(tiles.size());
  FloatRect union_rect;
  for (const auto& tile_stream : tiles) {
    SkMemoryStream stream(tile_stream->data.begin(), tile_stream->data.size());
    SkiaImageDecoder factory;
    sk_sp<SkPicture> picture = SkPicture::MakeFromStream(&stream, &factory);
    if (!picture)
      return nullptr;
    FloatRect cull_rect(picture->cullRect());
    cull_rect.MoveBy(tile_stream->layer_offset);
    union_rect.Unite(cull_rect);
    pictures.push_back(std::move(picture));
  }
  if (tiles.size() == 1)
    return AdoptRef(new PictureSnapshot(std::move(pictures[0])));
  SkPictureRecorder recorder;
  SkCanvas* canvas =
      recorder.beginRecording(union_rect.Width(), union_rect.Height(), 0, 0);
  for (size_t i = 0; i < pictures.size(); ++i) {
    canvas->save();
    canvas->translate(tiles[i]->layer_offset.X() - union_rect.X(),
                      tiles[i]->layer_offset.Y() - union_rect.Y());
    pictures[i]->playback(canvas, 0);
    canvas->restore();
  }
  return AdoptRef(new PictureSnapshot(recorder.finishRecordingAsPicture()));
}

bool PictureSnapshot::IsEmpty() const {
  return picture_->cullRect().isEmpty();
}

std::unique_ptr<Vector<char>> PictureSnapshot::Replay(unsigned from_step,
                                                      unsigned to_step,
                                                      double scale) const {
  const SkIRect bounds = picture_->cullRect().roundOut();
  int width = ceil(scale * bounds.width());
  int height = ceil(scale * bounds.height());

  // TODO(fmalita): convert this to SkSurface/SkImage, drop the intermediate
  // SkBitmap.
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(width, height));
  bitmap.eraseARGB(0, 0, 0, 0);
  {
    ReplayingCanvas canvas(bitmap, from_step, to_step);
    // Disable LCD text preemptively, because the picture opacity is unknown.
    // The canonical API involves SkSurface props, but since we're not
    // SkSurface-based at this point (see TODO above) we (ab)use saveLayer for
    // this purpose.
    SkAutoCanvasRestore auto_restore(&canvas, false);
    canvas.saveLayer(nullptr, nullptr);

    canvas.scale(scale, scale);
    canvas.ResetStepCount();
    picture_->playback(&canvas, &canvas);
  }
  std::unique_ptr<Vector<char>> base64_data = WTF::MakeUnique<Vector<char>>();
  Vector<char> encoded_image;

  SkPixmap src;
  bool peekResult = bitmap.peekPixels(&src);
  DCHECK(peekResult);

  SkPngEncoder::Options options;
  options.fFilterFlags = SkPngEncoder::FilterFlag::kSub;
  options.fZLibLevel = 3;
  options.fUnpremulBehavior = SkTransferFunctionBehavior::kIgnore;
  if (!ImageEncoder::Encode(
          reinterpret_cast<Vector<unsigned char>*>(&encoded_image), src,
          options)) {
    return nullptr;
  }

  Base64Encode(encoded_image, *base64_data);
  return base64_data;
}

std::unique_ptr<PictureSnapshot::Timings> PictureSnapshot::Profile(
    unsigned min_repeat_count,
    double min_duration,
    const FloatRect* clip_rect) const {
  std::unique_ptr<PictureSnapshot::Timings> timings =
      WTF::MakeUnique<PictureSnapshot::Timings>();
  timings->ReserveCapacity(min_repeat_count);
  const SkIRect bounds = picture_->cullRect().roundOut();
  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(bounds.width(), bounds.height()));
  bitmap.eraseARGB(0, 0, 0, 0);

  double now = WTF::MonotonicallyIncreasingTime();
  double stop_time = now + min_duration;
  for (unsigned step = 0; step < min_repeat_count || now < stop_time; ++step) {
    timings->push_back(Vector<double>());
    Vector<double>* current_timings = &timings->back();
    if (timings->size() > 1)
      current_timings->ReserveCapacity(timings->begin()->size());
    ProfilingCanvas canvas(bitmap);
    if (clip_rect) {
      canvas.clipRect(SkRect::MakeXYWH(clip_rect->X(), clip_rect->Y(),
                                       clip_rect->Width(),
                                       clip_rect->Height()));
      canvas.ResetStepCount();
    }
    canvas.SetTimings(current_timings);
    picture_->playback(&canvas);
    now = WTF::MonotonicallyIncreasingTime();
  }
  return timings;
}

std::unique_ptr<JSONArray> PictureSnapshot::SnapshotCommandLog() const {
  const SkIRect bounds = picture_->cullRect().roundOut();
  LoggingCanvas canvas(bounds.width(), bounds.height());
  picture_->playback(&canvas);
  return canvas.Log();
}

}  // namespace blink
