// Copyright 2019 The libgav1 Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/decoder_impl.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <iterator>
#include <new>
#include <utility>

#include "src/dsp/common.h"
#include "src/dsp/constants.h"
#include "src/dsp/dsp.h"
#include "src/loop_filter_mask.h"
#include "src/loop_restoration_info.h"
#include "src/post_filter.h"
#include "src/prediction_mask.h"
#include "src/quantizer.h"
#include "src/utils/blocking_counter.h"
#include "src/utils/common.h"
#include "src/utils/logging.h"
#include "src/utils/parameter_tree.h"
#include "src/utils/raw_bit_reader.h"
#include "src/utils/segmentation.h"
#include "src/utils/threadpool.h"
#include "src/yuv_buffer.h"

namespace libgav1 {
namespace {

constexpr int kMaxBlockWidth4x4 = 32;
constexpr int kMaxBlockHeight4x4 = 32;

// A cleanup helper class that releases the frame buffer reference held in
// |frame| in the destructor.
class RefCountedBufferPtrCleanup {
 public:
  explicit RefCountedBufferPtrCleanup(RefCountedBufferPtr* frame)
      : frame_(*frame) {}

  // Not copyable or movable.
  RefCountedBufferPtrCleanup(const RefCountedBufferPtrCleanup&) = delete;
  RefCountedBufferPtrCleanup& operator=(const RefCountedBufferPtrCleanup&) =
      delete;

  ~RefCountedBufferPtrCleanup() { frame_ = nullptr; }

 private:
  RefCountedBufferPtr& frame_;
};

}  // namespace

// static
StatusCode DecoderImpl::Create(const DecoderSettings* settings,
                               std::unique_ptr<DecoderImpl>* output) {
  if (settings->threads <= 0) {
    LIBGAV1_DLOG(ERROR, "Invalid settings->threads: %d.", settings->threads);
    return kLibgav1StatusInvalidArgument;
  }
  if (settings->frame_parallel) {
    LIBGAV1_DLOG(ERROR,
                 "Frame parallel decoding is not implemented. Don't set "
                 "frame_parallel to true in DecoderSettings.");
    return kLibgav1StatusUnimplemented;
  }
  std::unique_ptr<DecoderImpl> impl(new (std::nothrow) DecoderImpl(settings));
  if (impl == nullptr) {
    LIBGAV1_DLOG(ERROR, "Failed to allocate DecoderImpl.");
    return kLibgav1StatusOutOfMemory;
  }
  const StatusCode status = impl->Init();
  if (status != kLibgav1StatusOk) return status;
  *output = std::move(impl);
  return kLibgav1StatusOk;
}

DecoderImpl::DecoderImpl(const DecoderSettings* settings)
    : buffer_pool_(*settings), settings_(*settings) {
  dsp::DspInit();
}

DecoderImpl::~DecoderImpl() {
  // The frame buffer references need to be released before |buffer_pool_| is
  // destroyed.
  ReleaseOutputFrame();
  assert(state_.current_frame == nullptr);
  for (auto& reference_frame : state_.reference_frame) {
    reference_frame = nullptr;
  }
}

StatusCode DecoderImpl::Init() {
  const int max_allowed_frames = GetMaxAllowedFrames();
  assert(max_allowed_frames > 0);
  if (!encoded_frames_.Init(max_allowed_frames)) {
    LIBGAV1_DLOG(ERROR, "encoded_frames_.Init() failed.");
    return kLibgav1StatusOutOfMemory;
  }
  if (!GenerateWedgeMask(&state_.wedge_masks)) {
    LIBGAV1_DLOG(ERROR, "GenerateWedgeMask() failed.");
    return kLibgav1StatusOutOfMemory;
  }
  return kLibgav1StatusOk;
}

StatusCode DecoderImpl::EnqueueFrame(const uint8_t* data, size_t size,
                                     int64_t user_private_data) {
  if (data == nullptr) {
    // This has to actually flush the decoder.
    return kLibgav1StatusOk;
  }
  if (encoded_frames_.Full()) {
    return kLibgav1StatusResourceExhausted;
  }
  encoded_frames_.Push(EncodedFrame(data, size, user_private_data));
  return kLibgav1StatusOk;
}

// DequeueFrame() follows the following policy to avoid holding unnecessary
// frame buffer references in state_.current_frame and output_frame_.
//
// 1. state_.current_frame must be null when DequeueFrame() returns (success
// or failure).
//
// 2. output_frame_ must be null when DequeueFrame() returns false.
StatusCode DecoderImpl::DequeueFrame(const DecoderBuffer** out_ptr) {
  if (out_ptr == nullptr) {
    LIBGAV1_DLOG(ERROR, "Invalid argument: out_ptr == nullptr.");
    return kLibgav1StatusInvalidArgument;
  }
  assert(state_.current_frame == nullptr);
  // We assume a call to DequeueFrame() indicates that the caller is no longer
  // using the previous output frame, so we can release it.
  ReleaseOutputFrame();
  if (encoded_frames_.Empty()) {
    // No encoded frame to decode. Not an error.
    *out_ptr = nullptr;
    return kLibgav1StatusOk;
  }
  const EncodedFrame encoded_frame = encoded_frames_.Pop();
  std::unique_ptr<ObuParser> obu(new (std::nothrow) ObuParser(
      encoded_frame.data, encoded_frame.size, &state_));
  if (obu == nullptr) {
    LIBGAV1_DLOG(ERROR, "Failed to initialize OBU parser.");
    return kLibgav1StatusOutOfMemory;
  }
  if (state_.has_sequence_header) {
    obu->set_sequence_header(state_.sequence_header);
  }
  RefCountedBufferPtrCleanup current_frame_cleanup(&state_.current_frame);
  RefCountedBufferPtr displayable_frame;
  StatusCode status;
  while (obu->HasData()) {
    state_.current_frame = buffer_pool_.GetFreeBuffer();
    if (state_.current_frame == nullptr) {
      LIBGAV1_DLOG(ERROR, "Could not get current_frame from the buffer pool.");
      return kLibgav1StatusResourceExhausted;
    }

    status = obu->ParseOneFrame();
    if (status != kLibgav1StatusOk) {
      LIBGAV1_DLOG(ERROR, "Failed to parse OBU.");
      return status;
    }
    if (std::find_if(obu->obu_headers().begin(), obu->obu_headers().end(),
                     [](const ObuHeader& obu_header) {
                       return obu_header.type == kObuSequenceHeader;
                     }) != obu->obu_headers().end()) {
      state_.sequence_header = obu->sequence_header();
      state_.has_sequence_header = true;
      decoder_scratch_buffer_pool_.Reset(
          obu->sequence_header().color_config.bitdepth);
    }
    if (!obu->frame_header().show_existing_frame) {
      if (obu->tile_groups().empty()) {
        // This means that the last call to ParseOneFrame() did not actually
        // have any tile groups. This could happen in rare cases (for example,
        // if there is a Metadata OBU after the TileGroup OBU). We currently do
        // not have a reason to handle those cases, so we simply continue.
        continue;
      }
      status = DecodeTiles(obu.get());
      if (status != kLibgav1StatusOk) {
        return status;
      }
    }
    UpdateReferenceFrames(obu->frame_header().refresh_frame_flags);
    if (obu->frame_header().show_frame ||
        obu->frame_header().show_existing_frame) {
      if (displayable_frame != nullptr) {
        // This can happen if there are multiple spatial/temporal layers. We
        // don't care about it for now, so simply return the last displayable
        // frame.
        // TODO(b/129153372): Add support for outputting multiple
        // spatial/temporal layers.
        LIBGAV1_DLOG(
            WARNING,
            "More than one displayable frame found. Using the last one.");
      }
      displayable_frame = std::move(state_.current_frame);
      if (obu->sequence_header().film_grain_params_present &&
          displayable_frame->film_grain_params().apply_grain &&
          (settings_.post_filter_mask & 0x10) != 0) {
        RefCountedBufferPtr film_grain_frame;
        if (!obu->frame_header().show_existing_frame &&
            obu->frame_header().refresh_frame_flags == 0) {
          // If show_existing_frame is true, then the current frame is a
          // previously saved reference frame. If refresh_frame_flags is
          // nonzero, then the UpdateReferenceFrames() call above has saved the
          // current frame as a reference frame. Therefore, if both of these
          // conditions are false, then the current frame is not saved as a
          // reference frame. displayable_frame should hold the only reference
          // to the current frame.
          assert(displayable_frame.use_count() == 1);
          // Add film grain noise in place.
          film_grain_frame = displayable_frame;
        } else {
          film_grain_frame = buffer_pool_.GetFreeBuffer();
          if (film_grain_frame == nullptr) {
            LIBGAV1_DLOG(
                ERROR, "Could not get film_grain_frame from the buffer pool.");
            return kLibgav1StatusResourceExhausted;
          }
          if (!film_grain_frame->Realloc(
                  displayable_frame->buffer()->bitdepth(),
                  displayable_frame->buffer()->is_monochrome(),
                  displayable_frame->upscaled_width(),
                  displayable_frame->frame_height(),
                  displayable_frame->buffer()->subsampling_x(),
                  displayable_frame->buffer()->subsampling_y(),
                  /*border=*/0,
                  /*byte_alignment=*/0)) {
            LIBGAV1_DLOG(ERROR, "film_grain_frame->Realloc() failed.");
            return kLibgav1StatusOutOfMemory;
          }
          film_grain_frame->set_chroma_sample_position(
              displayable_frame->chroma_sample_position());
        }
        const dsp::Dsp* const dsp =
            dsp::GetDspTable(displayable_frame->buffer()->bitdepth());
        if (!dsp->film_grain_synthesis(
                displayable_frame->buffer()->data(kPlaneY),
                displayable_frame->buffer()->stride(kPlaneY),
                displayable_frame->buffer()->data(kPlaneU),
                displayable_frame->buffer()->stride(kPlaneU),
                displayable_frame->buffer()->data(kPlaneV),
                displayable_frame->buffer()->stride(kPlaneV),
                displayable_frame->film_grain_params(),
                displayable_frame->buffer()->is_monochrome(),
                obu->sequence_header().color_config.matrix_coefficients ==
                    kMatrixCoefficientIdentity,
                displayable_frame->upscaled_width(),
                displayable_frame->frame_height(),
                displayable_frame->buffer()->subsampling_x(),
                displayable_frame->buffer()->subsampling_y(),
                film_grain_frame->buffer()->data(kPlaneY),
                film_grain_frame->buffer()->stride(kPlaneY),
                film_grain_frame->buffer()->data(kPlaneU),
                film_grain_frame->buffer()->stride(kPlaneU),
                film_grain_frame->buffer()->data(kPlaneV),
                film_grain_frame->buffer()->stride(kPlaneV))) {
          LIBGAV1_DLOG(ERROR, "dsp->film_grain_synthesis() failed.");
          return kLibgav1StatusOutOfMemory;
        }
        displayable_frame = std::move(film_grain_frame);
      }
    }
  }
  if (displayable_frame == nullptr) {
    // No displayable frame in the encoded frame. Not an error.
    *out_ptr = nullptr;
    return kLibgav1StatusOk;
  }
  status = CopyFrameToOutputBuffer(displayable_frame);
  if (status != kLibgav1StatusOk) {
    return status;
  }
  buffer_.user_private_data = encoded_frame.user_private_data;
  *out_ptr = &buffer_;
  return kLibgav1StatusOk;
}

bool DecoderImpl::AllocateCurrentFrame(const ObuFrameHeader& frame_header) {
  const ColorConfig& color_config = state_.sequence_header.color_config;
  state_.current_frame->set_chroma_sample_position(
      color_config.chroma_sample_position);
  return state_.current_frame->Realloc(
      color_config.bitdepth, color_config.is_monochrome,
      frame_header.upscaled_width, frame_header.height,
      color_config.subsampling_x, color_config.subsampling_y, kBorderPixels,
      /*byte_alignment=*/0);
}

StatusCode DecoderImpl::CopyFrameToOutputBuffer(
    const RefCountedBufferPtr& frame) {
  YuvBuffer* yuv_buffer = frame->buffer();

  buffer_.chroma_sample_position = frame->chroma_sample_position();

  if (yuv_buffer->is_monochrome()) {
    buffer_.image_format = kImageFormatMonochrome400;
  } else {
    if (yuv_buffer->subsampling_x() == 0 && yuv_buffer->subsampling_y() == 0) {
      buffer_.image_format = kImageFormatYuv444;
    } else if (yuv_buffer->subsampling_x() == 1 &&
               yuv_buffer->subsampling_y() == 0) {
      buffer_.image_format = kImageFormatYuv422;
    } else if (yuv_buffer->subsampling_x() == 1 &&
               yuv_buffer->subsampling_y() == 1) {
      buffer_.image_format = kImageFormatYuv420;
    } else {
      LIBGAV1_DLOG(ERROR,
                   "Invalid chroma subsampling values: cannot determine buffer "
                   "image format.");
      return kLibgav1StatusInvalidArgument;
    }
  }

  buffer_.bitdepth = yuv_buffer->bitdepth();
  const int num_planes =
      yuv_buffer->is_monochrome() ? kMaxPlanesMonochrome : kMaxPlanes;
  int plane = 0;
  for (; plane < num_planes; ++plane) {
    buffer_.stride[plane] = yuv_buffer->stride(plane);
    buffer_.plane[plane] = yuv_buffer->data(plane);
    buffer_.displayed_width[plane] = yuv_buffer->displayed_width(plane);
    buffer_.displayed_height[plane] = yuv_buffer->displayed_height(plane);
  }
  for (; plane < kMaxPlanes; ++plane) {
    buffer_.stride[plane] = 0;
    buffer_.plane[plane] = nullptr;
    buffer_.displayed_width[plane] = 0;
    buffer_.displayed_height[plane] = 0;
  }
  buffer_.buffer_private_data = frame->buffer_private_data();
  output_frame_ = frame;
  return kLibgav1StatusOk;
}

void DecoderImpl::ReleaseOutputFrame() {
  for (auto& plane : buffer_.plane) {
    plane = nullptr;
  }
  output_frame_ = nullptr;
}

StatusCode DecoderImpl::DecodeTiles(const ObuParser* obu) {
  if (PostFilter::DoDeblock(obu->frame_header(), settings_.post_filter_mask) &&
      !loop_filter_mask_.Reset(obu->frame_header().width,
                               obu->frame_header().height)) {
    LIBGAV1_DLOG(ERROR, "Failed to allocate memory for loop filter masks.");
    return kLibgav1StatusOutOfMemory;
  }
  LoopRestorationInfo loop_restoration_info(
      obu->frame_header().loop_restoration, obu->frame_header().upscaled_width,
      obu->frame_header().height,
      obu->sequence_header().color_config.subsampling_x,
      obu->sequence_header().color_config.subsampling_y,
      obu->sequence_header().color_config.is_monochrome);
  if (!loop_restoration_info.Allocate()) {
    LIBGAV1_DLOG(ERROR,
                 "Failed to allocate memory for loop restoration info units.");
    return kLibgav1StatusOutOfMemory;
  }
  if (!AllocateCurrentFrame(obu->frame_header())) {
    LIBGAV1_DLOG(ERROR, "Failed to allocate memory for the decoder buffer.");
    return kLibgav1StatusOutOfMemory;
  }
  if (obu->sequence_header().enable_cdef) {
    if (!cdef_index_.Reset(
            DivideBy16(obu->frame_header().rows4x4 + kMaxBlockHeight4x4),
            DivideBy16(obu->frame_header().columns4x4 + kMaxBlockWidth4x4),
            /*zero_initialize=*/false)) {
      LIBGAV1_DLOG(ERROR, "Failed to allocate memory for cdef index.");
      return kLibgav1StatusOutOfMemory;
    }
  }
  if (!inter_transform_sizes_.Reset(
          obu->frame_header().rows4x4 + kMaxBlockHeight4x4,
          obu->frame_header().columns4x4 + kMaxBlockWidth4x4,
          /*zero_initialize=*/false)) {
    LIBGAV1_DLOG(ERROR, "Failed to allocate memory for inter_transform_sizes.");
    return kLibgav1StatusOutOfMemory;
  }
  if (obu->frame_header().use_ref_frame_mvs &&
      !state_.motion_field_mv.Reset(DivideBy2(obu->frame_header().rows4x4),
                                    DivideBy2(obu->frame_header().columns4x4),
                                    /*zero_initialize=*/false)) {
    LIBGAV1_DLOG(ERROR,
                 "Failed to allocate memory for temporal motion vectors.");
    return kLibgav1StatusOutOfMemory;
  }

  // The addition of kMaxBlockHeight4x4 and kMaxBlockWidth4x4 is necessary so
  // that the block parameters cache can be filled in for the last row/column
  // without having to check for boundary conditions.
  BlockParametersHolder block_parameters_holder(
      obu->frame_header().rows4x4 + kMaxBlockHeight4x4,
      obu->frame_header().columns4x4 + kMaxBlockWidth4x4,
      obu->sequence_header().use_128x128_superblock);
  if (!block_parameters_holder.Init()) {
    return kLibgav1StatusOutOfMemory;
  }
  const dsp::Dsp* const dsp =
      dsp::GetDspTable(obu->sequence_header().color_config.bitdepth);
  if (dsp == nullptr) {
    LIBGAV1_DLOG(ERROR, "Failed to get the dsp table for bitdepth %d.",
                 obu->sequence_header().color_config.bitdepth);
    return kLibgav1StatusInternalError;
  }
  // If prev_segment_ids is a null pointer, it is treated as if it pointed to
  // a segmentation map containing all 0s.
  const SegmentationMap* prev_segment_ids = nullptr;
  if (obu->frame_header().primary_reference_frame == kPrimaryReferenceNone) {
    symbol_decoder_context_.Initialize(
        obu->frame_header().quantizer.base_index);
  } else {
    const int index =
        obu->frame_header()
            .reference_frame_index[obu->frame_header().primary_reference_frame];
    const RefCountedBuffer* prev_frame = state_.reference_frame[index].get();
    symbol_decoder_context_ = prev_frame->FrameContext();
    if (obu->frame_header().segmentation.enabled &&
        prev_frame->columns4x4() == obu->frame_header().columns4x4 &&
        prev_frame->rows4x4() == obu->frame_header().rows4x4) {
      prev_segment_ids = prev_frame->segmentation_map();
    }
  }

  const uint8_t tile_size_bytes = obu->frame_header().tile_info.tile_size_bytes;
  const int tile_count = obu->tile_groups().back().end + 1;
  assert(tile_count >= 1);
  Vector<std::unique_ptr<Tile>> tiles;
  if (!tiles.reserve(tile_count)) {
    LIBGAV1_DLOG(ERROR, "tiles.reserve(%d) failed.\n", tile_count);
    return kLibgav1StatusOutOfMemory;
  }
  if (!threading_strategy_.Reset(obu->frame_header(), settings_.threads)) {
    return kLibgav1StatusOutOfMemory;
  }

  if (threading_strategy_.row_thread_pool(0) != nullptr) {
    if (residual_buffer_pool_ == nullptr) {
      residual_buffer_pool_.reset(new (std::nothrow) ResidualBufferPool(
          obu->sequence_header().use_128x128_superblock,
          obu->sequence_header().color_config.subsampling_x,
          obu->sequence_header().color_config.subsampling_y,
          obu->sequence_header().color_config.bitdepth == 8 ? sizeof(int16_t)
                                                            : sizeof(int32_t)));
      if (residual_buffer_pool_ == nullptr) {
        LIBGAV1_DLOG(ERROR, "Failed to allocate residual buffer.\n");
        return kLibgav1StatusOutOfMemory;
      }
    } else {
      residual_buffer_pool_->Reset(
          obu->sequence_header().use_128x128_superblock,
          obu->sequence_header().color_config.subsampling_x,
          obu->sequence_header().color_config.subsampling_y,
          obu->sequence_header().color_config.bitdepth == 8 ? sizeof(int16_t)
                                                            : sizeof(int32_t));
    }
  }

  const bool do_cdef =
      PostFilter::DoCdef(obu->frame_header(), settings_.post_filter_mask);
  const int num_planes = obu->sequence_header().color_config.is_monochrome
                             ? kMaxPlanesMonochrome
                             : kMaxPlanes;
  const bool do_restoration =
      PostFilter::DoRestoration(obu->frame_header().loop_restoration,
                                settings_.post_filter_mask, num_planes);
  if (threading_strategy_.post_filter_thread_pool() != nullptr &&
      (do_cdef || do_restoration)) {
    const int window_buffer_width = PostFilter::GetWindowBufferWidth(
        threading_strategy_.post_filter_thread_pool(), obu->frame_header());
    size_t threaded_window_buffer_size =
        window_buffer_width *
        PostFilter::GetWindowBufferHeight(
            threading_strategy_.post_filter_thread_pool(),
            obu->frame_header()) *
        (obu->sequence_header().color_config.bitdepth == 8 ? sizeof(uint8_t)
                                                           : sizeof(uint16_t));
    if (do_cdef && !do_restoration) {
      // TODO(chengchen): for cdef U, V planes, if there's subsampling, we can
      // use smaller buffer.
      threaded_window_buffer_size *= num_planes;
    }
    if (threaded_window_buffer_size_ < threaded_window_buffer_size) {
      // threaded_window_buffer_ will be subdivided by PostFilter into windows
      // of width 512 pixels. Each row in the window is filtered by a worker
      // thread. To avoid false sharing, each 512-pixel row processed by one
      // thread should not share a cache line with a row processed by another
      // thread. So we align threaded_window_buffer_ to the cache line size.
      // In addition, it is faster to memcpy from an aligned buffer.
      //
      // On Linux, the cache line size can be looked up with the command:
      //   getconf LEVEL1_DCACHE_LINESIZE
      //
      // The cache line size should ideally be queried at run time. 64 is a
      // common cache line size of x86 CPUs. Web searches showed the cache line
      // size of ARM CPUs is 32 or 64 bytes. So aligning to 64-byte boundary
      // will work for all CPUs that we care about, even though it is excessive
      // for some ARM CPUs.
      constexpr size_t kCacheLineSize = 64;
      // To avoid false sharing, PostFilter's window width in bytes should also
      // be a multiple of the cache line size. For simplicity, we check the
      // window width in pixels.
      assert(window_buffer_width % kCacheLineSize == 0);
      threaded_window_buffer_ = MakeAlignedUniquePtr<uint8_t>(
          kCacheLineSize, threaded_window_buffer_size);
      if (threaded_window_buffer_ == nullptr) {
        LIBGAV1_DLOG(ERROR,
                     "Failed to allocate threaded loop restoration buffer.\n");
        threaded_window_buffer_size_ = 0;
        return kLibgav1StatusOutOfMemory;
      }
      threaded_window_buffer_size_ = threaded_window_buffer_size;
    }
  }

  PostFilter post_filter(
      obu->frame_header(), obu->sequence_header(), &loop_filter_mask_,
      cdef_index_, &loop_restoration_info, &block_parameters_holder,
      state_.current_frame->buffer(), dsp,
      threading_strategy_.post_filter_thread_pool(),
      threaded_window_buffer_.get(), settings_.post_filter_mask);
  SymbolDecoderContext saved_symbol_decoder_context;
  int tile_index = 0;
  BlockingCounterWithStatus pending_tiles(tile_count);
  for (const auto& tile_group : obu->tile_groups()) {
    size_t bytes_left = tile_group.data_size;
    size_t byte_offset = 0;
    // The for loop in 5.11.1.
    for (int tile_number = tile_group.start; tile_number <= tile_group.end;
         ++tile_number) {
      size_t tile_size = 0;
      if (tile_number != tile_group.end) {
        RawBitReader bit_reader(tile_group.data + byte_offset, bytes_left);
        if (!bit_reader.ReadLittleEndian(tile_size_bytes, &tile_size)) {
          LIBGAV1_DLOG(ERROR, "Could not read tile size for tile #%d",
                       tile_number);
          return kLibgav1StatusBitstreamError;
        }
        ++tile_size;
        byte_offset += tile_size_bytes;
        bytes_left -= tile_size_bytes;
        if (tile_size > bytes_left) {
          LIBGAV1_DLOG(ERROR, "Invalid tile size %zu for tile #%d", tile_size,
                       tile_number);
          return kLibgav1StatusBitstreamError;
        }
      } else {
        tile_size = bytes_left;
      }

      std::unique_ptr<Tile> tile(new (std::nothrow) Tile(
          tile_number, tile_group.data + byte_offset, tile_size,
          obu->sequence_header(), obu->frame_header(),
          state_.current_frame.get(), state_.reference_frame_sign_bias,
          state_.reference_frame, &state_.motion_field_mv,
          state_.reference_order_hint, state_.wedge_masks,
          symbol_decoder_context_, &saved_symbol_decoder_context,
          prev_segment_ids, &post_filter, &block_parameters_holder,
          &cdef_index_, &inter_transform_sizes_, dsp,
          threading_strategy_.row_thread_pool(tile_index++),
          residual_buffer_pool_.get(), &decoder_scratch_buffer_pool_,
          &pending_tiles));
      if (tile == nullptr) {
        LIBGAV1_DLOG(ERROR, "Failed to allocate tile.");
        return kLibgav1StatusOutOfMemory;
      }
      tiles.push_back_unchecked(std::move(tile));

      byte_offset += tile_size;
      bytes_left -= tile_size;
    }
  }
  assert(tiles.size() == static_cast<size_t>(tile_count));
  bool tile_decoding_failed = false;
  if (threading_strategy_.tile_thread_pool() == nullptr) {
    for (const auto& tile_ptr : tiles) {
      if (!tile_decoding_failed) {
        if (!tile_ptr->Decode(/*is_main_thread=*/true)) {
          LIBGAV1_DLOG(ERROR, "Error decoding tile #%d", tile_ptr->number());
          tile_decoding_failed = true;
        }
      } else {
        pending_tiles.Decrement(false);
      }
    }
  } else {
    const int num_workers = threading_strategy_.tile_thread_count();
    BlockingCounterWithStatus pending_workers(num_workers);
    std::atomic<int> tile_counter(0);
    // Submit tile decoding jobs to the thread pool.
    for (int i = 0; i < num_workers; ++i) {
      threading_strategy_.tile_thread_pool()->Schedule(
          [&tiles, tile_count, &tile_counter, &pending_workers,
           &pending_tiles]() {
            bool failed = false;
            int index;
            while ((index = tile_counter.fetch_add(
                        1, std::memory_order_relaxed)) < tile_count) {
              if (!failed) {
                const auto& tile_ptr = tiles[index];
                if (!tile_ptr->Decode(/*is_main_thread=*/false)) {
                  LIBGAV1_DLOG(ERROR, "Error decoding tile #%d",
                               tile_ptr->number());
                  failed = true;
                }
              } else {
                pending_tiles.Decrement(false);
              }
            }
            pending_workers.Decrement(!failed);
          });
    }
    // Have the current thread partake in tile decoding.
    int index;
    while ((index = tile_counter.fetch_add(1, std::memory_order_relaxed)) <
           tile_count) {
      if (!tile_decoding_failed) {
        const auto& tile_ptr = tiles[index];
        if (!tile_ptr->Decode(/*is_main_thread=*/true)) {
          LIBGAV1_DLOG(ERROR, "Error decoding tile #%d", tile_ptr->number());
          tile_decoding_failed = true;
        }
      } else {
        pending_tiles.Decrement(false);
      }
    }
    // Wait until all the workers are done. This ensures that all the tiles have
    // been parsed.
    tile_decoding_failed |= !pending_workers.Wait();
  }
  // Wait until all the tiles have been decoded.
  tile_decoding_failed |= !pending_tiles.Wait();

  // At this point, all the tiles have been parsed and decoded and the
  // threadpool will be empty.
  if (tile_decoding_failed) return kLibgav1StatusUnknownError;

  if (obu->frame_header().enable_frame_end_update_cdf) {
    symbol_decoder_context_ = saved_symbol_decoder_context;
  }
  state_.current_frame->SetFrameContext(symbol_decoder_context_);
  if (post_filter.DoDeblock()) {
    loop_filter_mask_.Build(obu->sequence_header(), obu->frame_header(),
                            obu->tile_groups().front().start,
                            obu->tile_groups().back().end,
                            block_parameters_holder, inter_transform_sizes_);
  }
  if (!post_filter.ApplyFiltering()) {
    LIBGAV1_DLOG(ERROR, "Error applying in-loop filtering.");
    return kLibgav1StatusUnknownError;
  }
  SetCurrentFrameSegmentationMap(obu->frame_header(), prev_segment_ids);
  return kLibgav1StatusOk;
}

void DecoderImpl::SetCurrentFrameSegmentationMap(
    const ObuFrameHeader& frame_header,
    const SegmentationMap* prev_segment_ids) {
  if (!frame_header.segmentation.enabled) {
    // All segment_id's are 0.
    state_.current_frame->segmentation_map()->Clear();
  } else if (!frame_header.segmentation.update_map) {
    // Copy from prev_segment_ids.
    if (prev_segment_ids == nullptr) {
      // Treat a null prev_segment_ids pointer as if it pointed to a
      // segmentation map containing all 0s.
      state_.current_frame->segmentation_map()->Clear();
    } else {
      state_.current_frame->segmentation_map()->CopyFrom(*prev_segment_ids);
    }
  }
}

void DecoderImpl::UpdateReferenceFrames(int refresh_frame_flags) {
  for (int ref_index = 0, mask = refresh_frame_flags; mask != 0;
       ++ref_index, mask >>= 1) {
    if ((mask & 1) != 0) {
      state_.reference_valid[ref_index] = true;
      state_.reference_frame_id[ref_index] = state_.current_frame_id;
      state_.reference_frame[ref_index] = state_.current_frame;
      state_.reference_order_hint[ref_index] = state_.order_hint;
    }
  }
}

}  // namespace libgav1
