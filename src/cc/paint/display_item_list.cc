// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/display_item_list.h"

#include <stddef.h>

#include <string>

#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "cc/base/math_util.h"
#include "cc/debug/picture_debug_util.h"
#include "cc/paint/solid_color_analyzer.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"

namespace cc {

namespace {

bool GetCanvasClipBounds(SkCanvas* canvas, gfx::Rect* clip_bounds) {
  SkRect canvas_clip_bounds;
  if (!canvas->getLocalClipBounds(&canvas_clip_bounds))
    return false;
  *clip_bounds = ToEnclosingRect(gfx::SkRectToRectF(canvas_clip_bounds));
  return true;
}

template <typename Function>
void IterateTextContent(const PaintOpBuffer* buffer, const Function& yield) {
  for (auto* op : PaintOpBuffer::Iterator(buffer)) {
    if (op->GetType() == PaintOpType::DrawTextBlob) {
      yield(static_cast<DrawTextBlobOp*>(op));
    } else if (op->GetType() == PaintOpType::DrawRecord) {
      IterateTextContent(static_cast<DrawRecordOp*>(op)->record.get(), yield);
    }
  }
}

template <typename Function>
void IterateTextContentByOffsets(const PaintOpBuffer* buffer,
                                 const std::vector<size_t>& offsets,
                                 const Function& yield) {
  if (!buffer)
    return;
  for (auto* op : PaintOpBuffer::OffsetIterator(buffer, &offsets)) {
    if (op->GetType() == PaintOpType::DrawTextBlob) {
      yield(static_cast<DrawTextBlobOp*>(op));
    } else if (op->GetType() == PaintOpType::DrawRecord) {
      IterateTextContent(static_cast<DrawRecordOp*>(op)->record.get(), yield);
    }
  }
}

bool RotationEquivalentToAxisFlip(const SkMatrix& matrix) {
  float skew_x = matrix.getSkewX();
  float skew_y = matrix.getSkewY();
  return ((skew_x == 1.f || skew_x == -1.f) &&
          (skew_y == 1.f || skew_y == -1.f));
}

}  // namespace

DisplayItemList::DisplayItemList(UsageHint usage_hint)
    : usage_hint_(usage_hint) {
  if (usage_hint_ == kTopLevelDisplayItemList) {
    visual_rects_.reserve(1024);
    offsets_.reserve(1024);
    paired_begin_stack_.reserve(32);
  }
}

DisplayItemList::~DisplayItemList() = default;

void DisplayItemList::Raster(SkCanvas* canvas,
                             ImageProvider* image_provider) const {
  DCHECK(usage_hint_ == kTopLevelDisplayItemList);
  gfx::Rect canvas_playback_rect;
  if (!GetCanvasClipBounds(canvas, &canvas_playback_rect))
    return;

  std::vector<size_t> offsets;
  rtree_.Search(canvas_playback_rect, &offsets);
  paint_op_buffer_.Playback(canvas, PlaybackParams(image_provider), &offsets);
}

void DisplayItemList::CaptureContent(const gfx::Rect& rect,
                                     std::vector<NodeId>* content) const {
  std::vector<size_t> offsets;
  rtree_.Search(rect, &offsets);
  IterateTextContentByOffsets(
      &paint_op_buffer_, offsets,
      [content](const DrawTextBlobOp* op) { content->push_back(op->node_id); });
}

double DisplayItemList::AreaOfDrawText(const gfx::Rect& rect) const {
  std::vector<size_t> offsets;
  rtree_.Search(rect, &offsets);
  double area = 0;
  IterateTextContentByOffsets(
      &paint_op_buffer_, offsets, [&area](const DrawTextBlobOp* op) {
        // This is not fully accurate, e.g. when there is transform operations,
        // but is good for statistics purpose.
        SkRect bounds = op->blob->bounds();
        area += static_cast<double>(bounds.width()) * bounds.height();
      });
  return area;
}

void DisplayItemList::EndPaintOfPairedEnd() {
#if DCHECK_IS_ON()
  DCHECK(IsPainting());
  DCHECK_LT(current_range_start_, paint_op_buffer_.size());
  current_range_start_ = kNotPainting;
#endif
  if (usage_hint_ == kToBeReleasedAsPaintOpBuffer)
    return;

  DCHECK(paired_begin_stack_.size());
  size_t last_begin_index = paired_begin_stack_.back().first_index;
  size_t last_begin_count = paired_begin_stack_.back().count;
  DCHECK_GT(last_begin_count, 0u);

  // Copy the visual rect at |last_begin_index| to all indices that constitute
  // the begin item. Note that because we possibly reallocate the
  // |visual_rects_| buffer below, we need an actual copy instead of a const
  // reference which can become dangling.
  auto visual_rect = visual_rects_[last_begin_index];
  for (size_t i = 1; i < last_begin_count; ++i)
    visual_rects_[i + last_begin_index] = visual_rect;
  paired_begin_stack_.pop_back();

  // Copy the visual rect of the matching begin item to the end item(s).
  visual_rects_.resize(paint_op_buffer_.size(), visual_rect);

  // The block that ended needs to be included in the bounds of the enclosing
  // block.
  GrowCurrentBeginItemVisualRect(visual_rect);
}

void DisplayItemList::Finalize() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "DisplayItemList::Finalize");
#if DCHECK_IS_ON()
  // If this fails a call to StartPaint() was not ended.
  DCHECK(!IsPainting());
  // If this fails we had more calls to EndPaintOfPairedBegin() than
  // to EndPaintOfPairedEnd().
  DCHECK(paired_begin_stack_.empty());
  DCHECK_EQ(visual_rects_.size(), offsets_.size());
#endif

  if (usage_hint_ == kTopLevelDisplayItemList) {
    rtree_.Build(visual_rects_,
                 [](const std::vector<gfx::Rect>& rects, size_t index) {
                   return rects[index];
                 },
                 [this](const std::vector<gfx::Rect>& rects, size_t index) {
                   // Ignore the given rects, since the payload comes from
                   // offsets. However, the indices match, so we can just index
                   // into offsets.
                   return offsets_[index];
                 });
  }
  paint_op_buffer_.ShrinkToFit();
  visual_rects_.clear();
  visual_rects_.shrink_to_fit();
  offsets_.clear();
  offsets_.shrink_to_fit();
  paired_begin_stack_.shrink_to_fit();
}

size_t DisplayItemList::BytesUsed() const {
  // TODO(jbroman): Does anything else owned by this class substantially
  // contribute to memory usage?
  // TODO(vmpstr): Probably DiscardableImageMap is worth counting here.
  return sizeof(*this) + paint_op_buffer_.bytes_used();
}

void DisplayItemList::EmitTraceSnapshot() const {
  bool include_items;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.display_items"), &include_items);
  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.display_items") ","
      TRACE_DISABLED_BY_DEFAULT("cc.debug.picture") ","
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline.picture"),
      "cc::DisplayItemList", TRACE_ID_LOCAL(this),
      CreateTracedValue(include_items));
}

std::string DisplayItemList::ToString() const {
  base::trace_event::TracedValueJSON value;
  AddToValue(&value, true);
  return value.ToFormattedJSON();
}

std::unique_ptr<base::trace_event::TracedValue>
DisplayItemList::CreateTracedValue(bool include_items) const {
  auto state = std::make_unique<base::trace_event::TracedValue>();
  AddToValue(state.get(), include_items);
  return state;
}

void DisplayItemList::AddToValue(base::trace_event::TracedValue* state,
                                 bool include_items) const {
  state->BeginDictionary("params");

  gfx::Rect bounds;
  if (rtree_.has_valid_bounds()) {
    bounds = rtree_.GetBoundsOrDie();
  } else {
    // For tracing code, just use the entire positive quadrant if the |rtree_|
    // has invalid bounds.
    bounds = gfx::Rect(INT_MAX, INT_MAX);
  }

  if (include_items) {
    state->BeginArray("items");

    PlaybackParams params(nullptr, SkMatrix::I());
    std::map<size_t, gfx::Rect> visual_rects = rtree_.GetAllBoundsForTracing();
    for (const PaintOp* op : PaintOpBuffer::Iterator(&paint_op_buffer_)) {
      state->BeginDictionary();
      state->SetString("name", PaintOpTypeToString(op->GetType()));

      MathUtil::AddToTracedValue(
          "visual_rect",
          visual_rects[paint_op_buffer_.GetOpOffsetForTracing(op)], state);

      SkPictureRecorder recorder;
      SkCanvas* canvas = recorder.beginRecording(gfx::RectToSkRect(bounds));
      op->Raster(canvas, params);
      sk_sp<SkPicture> picture = recorder.finishRecordingAsPicture();

      if (picture->approximateOpCount()) {
        std::string b64_picture;
        PictureDebugUtil::SerializeAsBase64(picture.get(), &b64_picture);
        state->SetString("skp64", b64_picture);
      }

      state->EndDictionary();
    }

    state->EndArray();  // "items".
  }

  MathUtil::AddToTracedValue("layer_rect", bounds, state);
  state->EndDictionary();  // "params".

  {
    SkPictureRecorder recorder;
    SkCanvas* canvas = recorder.beginRecording(gfx::RectToSkRect(bounds));
    canvas->translate(-bounds.x(), -bounds.y());
    canvas->clipRect(gfx::RectToSkRect(bounds));
    Raster(canvas);
    sk_sp<SkPicture> picture = recorder.finishRecordingAsPicture();

    std::string b64_picture;
    PictureDebugUtil::SerializeAsBase64(picture.get(), &b64_picture);
    state->SetString("skp64", b64_picture);
  }
}

void DisplayItemList::GenerateDiscardableImagesMetadata() {
  DCHECK(usage_hint_ == kTopLevelDisplayItemList);

  gfx::Rect bounds;
  if (rtree_.has_valid_bounds()) {
    bounds = rtree_.GetBoundsOrDie();
  } else {
    // Bounds are only used to size an SkNoDrawCanvas, pass INT_MAX.
    bounds = gfx::Rect(INT_MAX, INT_MAX);
  }

  image_map_.Generate(&paint_op_buffer_, bounds);
}

void DisplayItemList::Reset() {
#if DCHECK_IS_ON()
  DCHECK(!IsPainting());
  DCHECK(paired_begin_stack_.empty());
#endif

  rtree_.Reset();
  image_map_.Reset();
  paint_op_buffer_.Reset();
  visual_rects_.clear();
  visual_rects_.shrink_to_fit();
  offsets_.clear();
  offsets_.shrink_to_fit();
  paired_begin_stack_.clear();
  paired_begin_stack_.shrink_to_fit();
  has_draw_ops_ = false;
}

sk_sp<PaintRecord> DisplayItemList::ReleaseAsRecord() {
  sk_sp<PaintRecord> record =
      sk_make_sp<PaintOpBuffer>(std::move(paint_op_buffer_));

  Reset();
  return record;
}

bool DisplayItemList::GetColorIfSolidInRect(const gfx::Rect& rect,
                                            SkColor* color,
                                            int max_ops_to_analyze) {
  DCHECK(usage_hint_ == kTopLevelDisplayItemList);
  std::vector<size_t>* offsets_to_use = nullptr;
  std::vector<size_t> offsets;
  if (rtree_.has_valid_bounds() && !rect.Contains(rtree_.GetBoundsOrDie())) {
    rtree_.Search(rect, &offsets);
    offsets_to_use = &offsets;
  }

  base::Optional<SkColor> solid_color =
      SolidColorAnalyzer::DetermineIfSolidColor(
          &paint_op_buffer_, rect, max_ops_to_analyze, offsets_to_use);
  if (solid_color) {
    *color = *solid_color;
    return true;
  }
  return false;
}

base::Optional<DisplayItemList::DirectlyCompositedImageResult>
DisplayItemList::GetDirectlyCompositedImageResult(
    gfx::Size containing_layer_bounds) const {
  const PaintOpBuffer* op_buffer = nullptr;
  if (paint_op_buffer_.size() == 1) {
    // The actual ops are wrapped in DrawRecord if they were previously
    // recorded.
    if (paint_op_buffer_.GetFirstOp()->GetType() == PaintOpType::DrawRecord) {
      const DrawRecordOp* draw_record =
          static_cast<const DrawRecordOp*>(paint_op_buffer_.GetFirstOp());
      op_buffer = draw_record->record.get();
    } else {
      op_buffer = &paint_op_buffer_;
    }
  } else {
    return base::nullopt;
  }

  const DrawImageRectOp* draw_image_rect_op = nullptr;
  bool transpose_image_size = false;
  constexpr size_t kNumDrawImageForOrientationOps = 10;
  if (op_buffer->size() == 1 &&
      op_buffer->GetFirstOp()->GetType() == PaintOpType::DrawImageRect) {
    draw_image_rect_op =
        static_cast<const DrawImageRectOp*>(op_buffer->GetFirstOp());
  } else if (op_buffer->size() < kNumDrawImageForOrientationOps) {
    // Images that respect orientation will have 5 paint operations:
    //  (1) Save
    //  (2) Translate
    //  (3) Concat (rotation matrix)
    //  (4) DrawImageRect
    //  (5) Restore
    // Detect these the paint op buffer and disqualify the layer as a directly
    // composited image if any other paint op is detected.
    for (auto* op : PaintOpBuffer::Iterator(op_buffer)) {
      switch (op->GetType()) {
        case PaintOpType::Save:
        case PaintOpType::Restore:
          break;
        case PaintOpType::Translate: {
          const TranslateOp* translate = static_cast<const TranslateOp*>(op);
          if (translate->dx != 0 || translate->dy != 0)
            return base::nullopt;
          break;
        }
        case PaintOpType::Concat: {
          // We only expect a single rotation. If we see another one, then this
          // image won't be eligible for directly compositing.
          if (transpose_image_size)
            return base::nullopt;

          const ConcatOp* concat_op = static_cast<const ConcatOp*>(op);
          if (concat_op->matrix.hasPerspective() ||
              !concat_op->matrix.preservesAxisAlignment())
            return base::nullopt;

          // If the rotation is not an axis flip, we'll need to transpose the
          // width and height dimensions to account for the same transform
          // applying when the layer bounds were calculated.
          transpose_image_size =
              RotationEquivalentToAxisFlip(concat_op->matrix);
          break;
        }
        case PaintOpType::DrawImageRect:
          if (draw_image_rect_op)
            return base::nullopt;
          draw_image_rect_op = static_cast<const DrawImageRectOp*>(op);
          break;
        default:
          return base::nullopt;
      }
    }
  }

  if (!draw_image_rect_op)
    return base::nullopt;

  // The src rect must match the image size exactly, i.e. the entire image
  // must be drawn.
  const SkRect& src = draw_image_rect_op->src;
  if (src.fLeft != 0 || src.fTop != 0 ||
      src.fRight != draw_image_rect_op->image.width() ||
      src.fBottom != draw_image_rect_op->image.height())
    return base::nullopt;

  // The DrawImageRect op's destination rect must match the layer bounds
  // exactly. Note that the layer bounds have already taken into account image
  // orientation so transpose the dst width/height before comparing, if
  // appropriate.
  const SkRect& dst = draw_image_rect_op->dst;
  int dst_width = transpose_image_size ? dst.fBottom : dst.fRight;
  int dst_height = transpose_image_size ? dst.fRight : dst.fBottom;
  if (dst.fLeft != 0 || dst.fTop != 0 ||
      dst_width != containing_layer_bounds.width() ||
      dst_height != containing_layer_bounds.height())
    return base::nullopt;

  int width = transpose_image_size ? draw_image_rect_op->image.height()
                                   : draw_image_rect_op->image.width();
  int height = transpose_image_size ? draw_image_rect_op->image.width()
                                    : draw_image_rect_op->image.height();
  DirectlyCompositedImageResult result;
  result.intrinsic_image_size = gfx::Size(width, height);
  // Ensure the layer will use nearest neighbor when drawn by the display
  // compositor, if required.
  result.nearest_neighbor =
      draw_image_rect_op->flags.getFilterQuality() == kNone_SkFilterQuality;
  return result;
}

}  // namespace cc
