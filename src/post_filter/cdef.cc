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
#include "src/utils/compiler_attributes.h"
#include "src/utils/constants.h"

namespace libgav1 {
namespace {

constexpr int kStep64x64 = 16;  // =64/4.

constexpr uint8_t kCdefUvDirection[2][2][8] = {
    {{0, 1, 2, 3, 4, 5, 6, 7}, {1, 2, 2, 2, 3, 4, 6, 0}},
    {{7, 0, 2, 4, 5, 6, 6, 6}, {0, 1, 2, 3, 4, 5, 6, 7}}};

template <typename Pixel>
void CopyRowForCdef(const Pixel* src, int block_width, int unit_width,
                    bool is_frame_left, bool is_frame_right,
                    uint16_t* const dst) {
  if (sizeof(src[0]) == sizeof(dst[0])) {
    if (is_frame_left) {
      Memset(dst - kCdefBorder, kCdefLargeValue, kCdefBorder);
    } else {
      memcpy(dst - kCdefBorder, src - kCdefBorder,
             kCdefBorder * sizeof(dst[0]));
    }
    memcpy(dst, src, block_width * sizeof(dst[0]));
    if (is_frame_right) {
      Memset(dst + block_width, kCdefLargeValue,
             unit_width + kCdefBorder - block_width);
    } else {
      memcpy(dst + block_width, src + block_width,
             (unit_width + kCdefBorder - block_width) * sizeof(dst[0]));
    }
    return;
  }
  for (int x = -kCdefBorder; x < 0; ++x) {
    dst[x] = is_frame_left ? static_cast<uint16_t>(kCdefLargeValue) : src[x];
  }
  for (int x = 0; x < block_width; ++x) {
    dst[x] = src[x];
  }
  for (int x = block_width; x < unit_width + kCdefBorder; ++x) {
    dst[x] = is_frame_right ? static_cast<uint16_t>(kCdefLargeValue) : src[x];
  }
}

}  // namespace

uint8_t* PostFilter::GetCdefBufferAndStride(const int start_x,
                                            const int start_y, const int plane,
                                            const int subsampling_x,
                                            const int subsampling_y,
                                            const int window_buffer_plane_size,
                                            int* cdef_stride) const {
  if (thread_pool_ != nullptr) {
    // write output to threaded_window_buffer.
    *cdef_stride = window_buffer_width_ * pixel_size_;
    const int column_window = start_x % (window_buffer_width_ >> subsampling_x);
    const int row_window = start_y % (window_buffer_height_ >> subsampling_y);
    return threaded_window_buffer_ + plane * window_buffer_plane_size +
           row_window * (*cdef_stride) + column_window * pixel_size_;
  }
  // write output to |cdef_buffer_|.
  *cdef_stride = frame_buffer_.stride(plane);
  return cdef_buffer_[plane] + start_y * (*cdef_stride) + start_x * pixel_size_;
}

template <typename Pixel>
void PostFilter::PrepareCdefBlock(int block_width4x4, int block_height4x4,
                                  int row_64x64, int column_64x64,
                                  uint16_t* cdef_source,
                                  ptrdiff_t cdef_stride) {
  for (int plane = kPlaneY; plane < planes_; ++plane) {
    uint16_t* cdef_src =
        cdef_source + plane * kRestorationProcessingUnitSizeWithBorders *
                          kRestorationProcessingUnitSizeWithBorders;
    const int8_t subsampling_x = subsampling_x_[plane];
    const int8_t subsampling_y = subsampling_y_[plane];
    const int start_x = MultiplyBy4(column_64x64) >> subsampling_x;
    const int start_y = MultiplyBy4(row_64x64) >> subsampling_y;
    const int plane_width = RightShiftWithRounding(width_, subsampling_x);
    const int plane_height = RightShiftWithRounding(height_, subsampling_y);
    const int block_width = MultiplyBy4(block_width4x4) >> subsampling_x;
    const int block_height = MultiplyBy4(block_height4x4) >> subsampling_y;
    // unit_width, unit_height are the same as block_width, block_height unless
    // it reaches the frame boundary, where block_width < 64 or
    // block_height < 64. unit_width, unit_height guarantee we build blocks on
    // a multiple of 8.
    const int unit_width = Align(block_width, (subsampling_x > 0) ? 4 : 8);
    const int unit_height = Align(block_height, (subsampling_y > 0) ? 4 : 8);
    const bool is_frame_left = column_64x64 == 0;
    const bool is_frame_right = start_x + block_width >= plane_width;
    const bool is_frame_top = row_64x64 == 0;
    const bool is_frame_bottom = start_y + block_height >= plane_height;
    const int src_stride = frame_buffer_.stride(plane) / sizeof(Pixel);
    const Pixel* src_buffer =
        reinterpret_cast<const Pixel*>(source_buffer_[plane]) +
        (start_y - (is_frame_top ? 0 : kCdefBorder)) * src_stride + start_x;

    // All the copying code will use negative indices for populating the left
    // border. So the starting point is set to kCdefBorder.
    cdef_src += kCdefBorder;

    // Copy the top 2 rows.
    for (int y = 0; y < kCdefBorder; ++y) {
      if (is_frame_top) {
        Memset(cdef_src - kCdefBorder, kCdefLargeValue,
               unit_width + 2 * kCdefBorder);
      } else {
        CopyRowForCdef(src_buffer, block_width, unit_width, is_frame_left,
                       is_frame_right, cdef_src);
        src_buffer += src_stride;
      }
      cdef_src += cdef_stride;
    }

    // Copy the body.
    for (int y = 0; y < block_height; ++y) {
      CopyRowForCdef(src_buffer, block_width, unit_width, is_frame_left,
                     is_frame_right, cdef_src);
      cdef_src += cdef_stride;
      src_buffer += src_stride;
    }

    // Copy the bottom 2 rows.
    for (int y = 0; y < kCdefBorder + unit_height - block_height; ++y) {
      if (is_frame_bottom) {
        Memset(cdef_src - kCdefBorder, kCdefLargeValue,
               unit_width + 2 * kCdefBorder);
      } else {
        CopyRowForCdef(src_buffer, block_width, unit_width, is_frame_left,
                       is_frame_right, cdef_src);
        src_buffer += src_stride;
      }
      cdef_src += cdef_stride;
    }
  }
}

template <typename Pixel>
void PostFilter::ApplyCdefForOneUnit(uint16_t* cdef_block, const int index,
                                     const int block_width4x4,
                                     const int block_height4x4,
                                     const int row4x4_start,
                                     const int column4x4_start) {
  const int coeff_shift = bitdepth_ - 8;
  const int step = kNum4x4BlocksWide[kBlock8x8];
  const int window_buffer_plane_size =
      window_buffer_width_ * window_buffer_height_ * pixel_size_;

  if (index == -1) {
    for (int plane = kPlaneY; plane < planes_; ++plane) {
      const int start_x = MultiplyBy4(column4x4_start) >> subsampling_x_[plane];
      const int start_y = MultiplyBy4(row4x4_start) >> subsampling_y_[plane];
      int cdef_stride;
      uint8_t* const cdef_buffer = GetCdefBufferAndStride(
          start_x, start_y, plane, subsampling_x_[plane], subsampling_y_[plane],
          window_buffer_plane_size, &cdef_stride);
      const int src_stride = frame_buffer_.stride(plane);
      uint8_t* const src_buffer =
          source_buffer_[plane] + start_y * src_stride + start_x * pixel_size_;
      const int block_width =
          MultiplyBy4(block_width4x4) >> subsampling_x_[plane];
      const int block_height =
          MultiplyBy4(block_height4x4) >> subsampling_y_[plane];
      for (int y = 0; y < block_height; ++y) {
        memcpy(cdef_buffer + y * cdef_stride, src_buffer + y * src_stride,
               block_width * pixel_size_);
      }
    }
    return;
  }

  PrepareCdefBlock<Pixel>(block_width4x4, block_height4x4, row4x4_start,
                          column4x4_start, cdef_block,
                          kRestorationProcessingUnitSizeWithBorders);

  for (int row4x4 = row4x4_start; row4x4 < row4x4_start + block_height4x4;
       row4x4 += step) {
    for (int column4x4 = column4x4_start;
         column4x4 < column4x4_start + block_width4x4; column4x4 += step) {
      const bool skip =
          block_parameters_.Find(row4x4, column4x4) != nullptr &&
          block_parameters_.Find(row4x4 + 1, column4x4) != nullptr &&
          block_parameters_.Find(row4x4, column4x4 + 1) != nullptr &&
          block_parameters_.Find(row4x4 + 1, column4x4 + 1) != nullptr &&
          block_parameters_.Find(row4x4, column4x4)->skip &&
          block_parameters_.Find(row4x4 + 1, column4x4)->skip &&
          block_parameters_.Find(row4x4, column4x4 + 1)->skip &&
          block_parameters_.Find(row4x4 + 1, column4x4 + 1)->skip;
      int damping = frame_header_.cdef.damping + coeff_shift;
      int direction_y;
      int direction;
      int variance;
      uint8_t primary_strength;
      uint8_t secondary_strength;

      for (int plane = kPlaneY; plane < planes_; ++plane) {
        const int8_t subsampling_x = subsampling_x_[plane];
        const int8_t subsampling_y = subsampling_y_[plane];
        const int start_x = MultiplyBy4(column4x4) >> subsampling_x;
        const int start_y = MultiplyBy4(row4x4) >> subsampling_y;
        const int block_width = 8 >> subsampling_x;
        const int block_height = 8 >> subsampling_y;
        int cdef_stride;
        uint8_t* const cdef_buffer = GetCdefBufferAndStride(
            start_x, start_y, plane, subsampling_x, subsampling_y,
            window_buffer_plane_size, &cdef_stride);
        const int src_stride = frame_buffer_.stride(plane);
        uint8_t* const src_buffer = source_buffer_[plane] +
                                    start_y * src_stride +
                                    start_x * pixel_size_;

        if (skip) {  // No cdef filtering.
          for (int y = 0; y < block_height; ++y) {
            memcpy(cdef_buffer + y * cdef_stride, src_buffer + y * src_stride,
                   block_width * pixel_size_);
          }
          continue;
        }

        if (plane == kPlaneY) {
          dsp_.cdef_direction(src_buffer, src_stride, &direction_y, &variance);
          primary_strength = frame_header_.cdef.y_primary_strength[index]
                             << coeff_shift;
          secondary_strength = frame_header_.cdef.y_secondary_strength[index]
                               << coeff_shift;
          direction = (primary_strength == 0) ? 0 : direction_y;
          const int variance_strength =
              ((variance >> 6) != 0) ? std::min(FloorLog2(variance >> 6), 12)
                                     : 0;
          primary_strength =
              (variance != 0)
                  ? (primary_strength * (4 + variance_strength) + 8) >> 4
                  : 0;
        } else {
          primary_strength = frame_header_.cdef.uv_primary_strength[index]
                             << coeff_shift;
          secondary_strength = frame_header_.cdef.uv_secondary_strength[index]
                               << coeff_shift;
          direction =
              (primary_strength == 0)
                  ? 0
                  : kCdefUvDirection[subsampling_x][subsampling_y][direction_y];
          damping = frame_header_.cdef.damping + coeff_shift - 1;
        }

        if ((primary_strength | secondary_strength) == 0) {
          for (int y = 0; y < block_height; ++y) {
            memcpy(cdef_buffer + y * cdef_stride, src_buffer + y * src_stride,
                   block_width * pixel_size_);
          }
          continue;
        }
        uint16_t* cdef_src =
            cdef_block + plane * kRestorationProcessingUnitSizeWithBorders *
                             kRestorationProcessingUnitSizeWithBorders;
        cdef_src += kCdefBorder * kRestorationProcessingUnitSizeWithBorders +
                    kCdefBorder;
        cdef_src += (MultiplyBy4(row4x4 - row4x4_start) >> subsampling_y) *
                        kRestorationProcessingUnitSizeWithBorders +
                    (MultiplyBy4(column4x4 - column4x4_start) >> subsampling_x);
        dsp_.cdef_filter(cdef_src, kRestorationProcessingUnitSizeWithBorders,
                         frame_header_.rows4x4, frame_header_.columns4x4,
                         start_x, start_y, subsampling_x, subsampling_y,
                         primary_strength, secondary_strength, damping,
                         direction, cdef_buffer, cdef_stride);
      }
    }
  }
}

void PostFilter::ApplyCdefForOneSuperBlockRowHelper(int row4x4,
                                                    int block_height4x4) {
  for (int column4x4 = 0; column4x4 < frame_header_.columns4x4;
       column4x4 += kStep64x64) {
    const int index = cdef_index_[DivideBy16(row4x4)][DivideBy16(column4x4)];
    const int block_width4x4 =
        std::min(kStep64x64, frame_header_.columns4x4 - column4x4);

#if LIBGAV1_MAX_BITDEPTH >= 10
    if (bitdepth_ >= 10) {
      ApplyCdefForOneUnit<uint16_t>(cdef_block_, index, block_width4x4,
                                    block_height4x4, row4x4, column4x4);
      continue;
    }
#endif  // LIBGAV1_MAX_BITDEPTH >= 10
    ApplyCdefForOneUnit<uint8_t>(cdef_block_, index, block_width4x4,
                                 block_height4x4, row4x4, column4x4);
  }
}

void PostFilter::ApplyCdefForOneSuperBlockRow(int row4x4_start, int sb4x4,
                                              bool is_last_row) {
  assert(row4x4_start >= 0);
  assert(DoCdef());
  for (int y = 0; y < sb4x4; y += kStep64x64) {
    const int row4x4 = row4x4_start + y;
    if (row4x4 >= frame_header_.rows4x4) return;

    // Apply cdef for the last 8 rows of the previous superblock row.
    // One exception: If the superblock size is 128x128 and is_last_row is true,
    // then we simply apply cdef for the entire superblock row without any lag.
    // In that case, apply cdef for the previous superblock row only during the
    // first iteration (y == 0).
    if (row4x4 > 0 && (!is_last_row || y == 0)) {
      assert(row4x4 >= 16);
      ApplyCdefForOneSuperBlockRowHelper(row4x4 - 2, 2);
    }

    // Apply cdef for the current superblock row. If this is the last superblock
    // row we apply cdef for all the rows, otherwise we leave out the last 8
    // rows.
    const int block_height4x4 =
        std::min(kStep64x64, frame_header_.rows4x4 - row4x4);
    const int height4x4 = block_height4x4 - (is_last_row ? 0 : 2);
    if (height4x4 > 0) {
      ApplyCdefForOneSuperBlockRowHelper(row4x4, height4x4);
    }
  }
}

template <typename Pixel>
void PostFilter::ApplyCdefForOneRowInWindow(const int row4x4,
                                            const int column4x4_start) {
  uint16_t cdef_block[kRestorationProcessingUnitSizeWithBorders *
                      kRestorationProcessingUnitSizeWithBorders * 3];

  for (int column4x4_64x64 = 0;
       column4x4_64x64 < std::min(DivideBy4(window_buffer_width_),
                                  frame_header_.columns4x4 - column4x4_start);
       column4x4_64x64 += kStep64x64) {
    const int column4x4 = column4x4_start + column4x4_64x64;
    const int index = cdef_index_[DivideBy16(row4x4)][DivideBy16(column4x4)];
    const int block_width4x4 =
        std::min(kStep64x64, frame_header_.columns4x4 - column4x4);
    const int block_height4x4 =
        std::min(kStep64x64, frame_header_.rows4x4 - row4x4);

    ApplyCdefForOneUnit<Pixel>(cdef_block, index, block_width4x4,
                               block_height4x4, row4x4, column4x4);
  }
}

// Each thread processes one row inside the window.
// Y, U, V planes are processed together inside one thread.
template <typename Pixel>
void PostFilter::ApplyCdefThreaded() {
  assert((window_buffer_height_ & 63) == 0);
  const int num_workers = thread_pool_->num_threads();
  const int window_buffer_plane_size =
      window_buffer_width_ * window_buffer_height_ * pixel_size_;
  const int window_buffer_height4x4 = DivideBy4(window_buffer_height_);
  for (int row4x4 = 0; row4x4 < frame_header_.rows4x4;
       row4x4 += window_buffer_height4x4) {
    const int actual_window_height4x4 =
        std::min(window_buffer_height4x4, frame_header_.rows4x4 - row4x4);
    const int vertical_units_per_window =
        DivideBy16(actual_window_height4x4 + 15);
    for (int column4x4 = 0; column4x4 < frame_header_.columns4x4;
         column4x4 += DivideBy4(window_buffer_width_)) {
      const int jobs_for_threadpool =
          vertical_units_per_window * num_workers / (num_workers + 1);
      BlockingCounter pending_jobs(jobs_for_threadpool);
      int job_count = 0;
      for (int row64x64 = 0; row64x64 < actual_window_height4x4;
           row64x64 += kStep64x64) {
        if (job_count < jobs_for_threadpool) {
          thread_pool_->Schedule(
              [this, row4x4, column4x4, row64x64, &pending_jobs]() {
                ApplyCdefForOneRowInWindow<Pixel>(row4x4 + row64x64, column4x4);
                pending_jobs.Decrement();
              });
        } else {
          ApplyCdefForOneRowInWindow<Pixel>(row4x4 + row64x64, column4x4);
        }
        ++job_count;
      }
      pending_jobs.Wait();

      // Copy |threaded_window_buffer_| to |cdef_buffer_|.
      for (int plane = kPlaneY; plane < planes_; ++plane) {
        const int src_stride = frame_buffer_.stride(plane);
        const int plane_row = MultiplyBy4(row4x4) >> subsampling_y_[plane];
        const int plane_column =
            MultiplyBy4(column4x4) >> subsampling_x_[plane];
        int copy_width = std::min(frame_header_.columns4x4 - column4x4,
                                  DivideBy4(window_buffer_width_));
        copy_width = MultiplyBy4(copy_width) >> subsampling_x_[plane];
        int copy_height =
            std::min(frame_header_.rows4x4 - row4x4, window_buffer_height4x4);
        copy_height = MultiplyBy4(copy_height) >> subsampling_y_[plane];
        CopyPlane<Pixel>(
            threaded_window_buffer_ + plane * window_buffer_plane_size,
            window_buffer_width_ * pixel_size_, copy_width, copy_height,
            cdef_buffer_[plane] + plane_row * src_stride +
                plane_column * pixel_size_,
            src_stride);
      }
    }
  }
}

void PostFilter::ApplyCdef() {
#if LIBGAV1_MAX_BITDEPTH >= 10
  if (bitdepth_ >= 10) {
    ApplyCdefThreaded<uint16_t>();
    return;
  }
#endif
  ApplyCdefThreaded<uint8_t>();
}

}  // namespace libgav1
