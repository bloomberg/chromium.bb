// Copyright 2020 The libgav1 Authors
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
#include "src/post_filter.h"
#include "src/utils/blocking_counter.h"

namespace libgav1 {
namespace {

template <typename Pixel>
void ExtendLine(uint8_t* const line_start, const int width, const int left,
                const int right) {
  auto* const start = reinterpret_cast<Pixel*>(line_start);
  const Pixel* src = start;
  Pixel* dst = start - left;
  // Copy to left and right borders.
  Memset(dst, src[0], left);
  Memset(dst + (left + width), src[width - 1], right);
}

}  // namespace

template <bool in_place>
void PostFilter::ApplySuperRes(const std::array<uint8_t*, kMaxPlanes>& buffers,
                               const std::array<int, kMaxPlanes>& strides,
                               const std::array<int, kMaxPlanes>& rows,
                               size_t line_buffer_offset) {
  uint8_t* const line_buffer_start =
      in_place ? nullptr
               : superres_line_buffer_ + line_buffer_offset +
                     kSuperResHorizontalBorder * pixel_size_;
  for (int plane = kPlaneY; plane < planes_; ++plane) {
    const int8_t subsampling_x = subsampling_x_[plane];
    const int plane_width =
        MultiplyBy4(frame_header_.columns4x4) >> subsampling_x;
    uint8_t* input = buffers[plane];
    const uint32_t input_stride = strides[plane];
#if LIBGAV1_MAX_BITDEPTH >= 10
    if (bitdepth_ >= 10) {
      for (int y = 0; y < rows[plane]; ++y, input += input_stride) {
        if (!in_place) {
          memcpy(line_buffer_start, input, plane_width * sizeof(uint16_t));
        }
        ExtendLine<uint16_t>(in_place ? input : line_buffer_start, plane_width,
                             kSuperResHorizontalBorder,
                             kSuperResHorizontalBorder);
        dsp_.super_res_row(in_place ? input : line_buffer_start,
                           super_res_info_[plane].upscaled_width,
                           super_res_info_[plane].initial_subpixel_x,
                           super_res_info_[plane].step,
                           input - (in_place ? input_stride : 0));
      }
      continue;
    }
#endif  // LIBGAV1_MAX_BITDEPTH >= 10
    for (int y = 0; y < rows[plane]; ++y, input += input_stride) {
      if (!in_place) {
        memcpy(line_buffer_start, input, plane_width);
      }
      ExtendLine<uint8_t>(in_place ? input : line_buffer_start, plane_width,
                          kSuperResHorizontalBorder, kSuperResHorizontalBorder);
      dsp_.super_res_row(in_place ? input : line_buffer_start,
                         super_res_info_[plane].upscaled_width,
                         super_res_info_[plane].initial_subpixel_x,
                         super_res_info_[plane].step,
                         input - (in_place ? input_stride : 0));
    }
  }
}

// Used by post_filter_test.cc.
template void PostFilter::ApplySuperRes<false>(
    const std::array<uint8_t*, kMaxPlanes>& buffers,
    const std::array<int, kMaxPlanes>& strides,
    const std::array<int, kMaxPlanes>& rows, size_t line_buffer_offset);

void PostFilter::ApplySuperResForOneSuperBlockRow(int row4x4_start, int sb4x4,
                                                  bool is_last_row) {
  assert(row4x4_start >= 0);
  assert(DoSuperRes());
  // If not doing cdef, then LR needs two rows of border with superres applied.
  const int num_rows_extra = (DoCdef() || !DoRestoration()) ? 0 : 2;
  std::array<uint8_t*, kMaxPlanes> buffers;
  std::array<int, kMaxPlanes> strides;
  std::array<int, kMaxPlanes> rows;
  // Apply superres for the last 8-num_rows_extra rows of the previous
  // superblock.
  if (row4x4_start > 0) {
    const int row4x4 = row4x4_start - 2;
    for (int plane = 0; plane < planes_; ++plane) {
      const int row =
          (MultiplyBy4(row4x4) >> subsampling_y_[plane]) + num_rows_extra;
      const ptrdiff_t row_offset = row * frame_buffer_.stride(plane);
      buffers[plane] = cdef_buffer_[plane] + row_offset;
      strides[plane] = frame_buffer_.stride(plane);
      // Note that the |num_rows_extra| subtraction is done after the value is
      // subsampled since we always need to work on |num_rows_extra| extra rows
      // irrespective of the plane subsampling.
      rows[plane] = (8 >> subsampling_y_[plane]) - num_rows_extra;
    }
    ApplySuperRes<true>(buffers, strides, rows, /*line_buffer_offset=*/0);
  }
  // Apply superres for the current superblock row (except for the last
  // 8-num_rows_extra rows).
  const int num_rows4x4 =
      std::min(sb4x4, frame_header_.rows4x4 - row4x4_start) -
      (is_last_row ? 0 : 2);
  for (int plane = 0; plane < planes_; ++plane) {
    const ptrdiff_t row_offset =
        (MultiplyBy4(row4x4_start) >> subsampling_y_[plane]) *
        frame_buffer_.stride(plane);
    buffers[plane] = cdef_buffer_[plane] + row_offset;
    strides[plane] = frame_buffer_.stride(plane);
    // Note that the |num_rows_extra| subtraction is done after the value is
    // subsampled since we always need to work on |num_rows_extra| extra rows
    // irrespective of the plane subsampling.
    rows[plane] = (MultiplyBy4(num_rows4x4) >> subsampling_y_[plane]) +
                  (is_last_row ? 0 : num_rows_extra);
  }
  ApplySuperRes<true>(buffers, strides, rows, /*line_buffer_offset=*/0);
}

void PostFilter::ApplySuperResThreaded() {
  const int num_threads = thread_pool_->num_threads() + 1;
  // The number of rows4x4 that will be processed by each thread in the thread
  // pool (other than the current thread).
  const int thread_pool_rows4x4 = frame_header_.rows4x4 / num_threads;
  // For the current thread, we round up to process all the remaining rows so
  // that the current thread's job will potentially run the longest.
  const int current_thread_rows4x4 =
      frame_header_.rows4x4 - (thread_pool_rows4x4 * (num_threads - 1));
  // The size of the line buffer required by each thread. In the multi-threaded
  // case we are guaranteed to have a line buffer which can store |num_threads|
  // rows at the same time.
  const size_t line_buffer_size = (MultiplyBy4(frame_header_.columns4x4) +
                                   MultiplyBy2(kSuperResHorizontalBorder)) *
                                  pixel_size_;
  size_t line_buffer_offset = 0;
  BlockingCounter pending_workers(num_threads - 1);
  for (int i = 0, row4x4_start = 0; i < num_threads; ++i,
           row4x4_start += thread_pool_rows4x4,
           line_buffer_offset += line_buffer_size) {
    std::array<uint8_t*, kMaxPlanes> buffers;
    std::array<int, kMaxPlanes> strides;
    std::array<int, kMaxPlanes> rows;
    for (int plane = 0; plane < planes_; ++plane) {
      strides[plane] = frame_buffer_.stride(plane);
      buffers[plane] =
          GetBufferOffset(cdef_buffer_[plane], strides[plane],
                          static_cast<Plane>(plane), row4x4_start, 0);
      if (i < num_threads - 1) {
        rows[plane] = MultiplyBy4(thread_pool_rows4x4) >> subsampling_y_[plane];
      } else {
        rows[plane] =
            MultiplyBy4(current_thread_rows4x4) >> subsampling_y_[plane];
      }
    }
    if (i < num_threads - 1) {
      thread_pool_->Schedule([this, buffers, strides, rows, line_buffer_offset,
                              &pending_workers]() {
        ApplySuperRes<false>(buffers, strides, rows, line_buffer_offset);
        pending_workers.Decrement();
      });
    } else {
      ApplySuperRes<false>(buffers, strides, rows, line_buffer_offset);
    }
  }
  // Wait for the threadpool jobs to finish.
  pending_workers.Wait();
}

// This function lives in this file so that it has access to ExtendLine<>.
void PostFilter::SetupDeblockBuffer(int row4x4_start, int sb4x4) {
  assert(row4x4_start >= 0);
  assert(DoCdef());
  assert(DoRestoration());
  for (int sb_y = 0; sb_y < sb4x4; sb_y += 16) {
    const int row4x4 = row4x4_start + sb_y;
    for (int plane = 0; plane < planes_; ++plane) {
      CopyDeblockedPixels(static_cast<Plane>(plane), row4x4);
    }
    const int row_offset_start = DivideBy4(row4x4);
    if (DoSuperRes()) {
      std::array<uint8_t*, kMaxPlanes> buffers = {
          deblock_buffer_.data(kPlaneY) +
              row_offset_start * deblock_buffer_.stride(kPlaneY),
          deblock_buffer_.data(kPlaneU) +
              row_offset_start * deblock_buffer_.stride(kPlaneU),
          deblock_buffer_.data(kPlaneV) +
              row_offset_start * deblock_buffer_.stride(kPlaneV)};
      std::array<int, kMaxPlanes> strides = {deblock_buffer_.stride(kPlaneY),
                                             deblock_buffer_.stride(kPlaneU),
                                             deblock_buffer_.stride(kPlaneV)};
      std::array<int, kMaxPlanes> rows = {4, 4, 4};
      ApplySuperRes<false>(buffers, strides, rows,
                           /*line_buffer_offset=*/0);
    }
    // Extend the left and right boundaries needed for loop restoration.
    for (int plane = 0; plane < planes_; ++plane) {
      uint8_t* src = deblock_buffer_.data(plane) +
                     row_offset_start * deblock_buffer_.stride(plane);
      const int plane_width =
          RightShiftWithRounding(upscaled_width_, subsampling_x_[plane]);
      for (int i = 0; i < 4; ++i) {
#if LIBGAV1_MAX_BITDEPTH >= 10
        if (bitdepth_ >= 10) {
          ExtendLine<uint16_t>(src, plane_width, kRestorationBorder,
                               kRestorationBorder);
        } else  // NOLINT.
#endif
        {
          ExtendLine<uint8_t>(src, plane_width, kRestorationBorder,
                              kRestorationBorder);
        }
        src += deblock_buffer_.stride(plane);
      }
    }
  }
}

}  // namespace libgav1
