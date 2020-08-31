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

template <typename Pixel>
void PostFilter::ApplyLoopRestorationForOneUnit(
    uint8_t* const cdef_buffer, const ptrdiff_t cdef_buffer_stride,
    const Plane plane, const int plane_height, const int x, const int y,
    const int row, const int column, const int unit_row,
    const int current_process_unit_height, const int plane_process_unit_width,
    const int plane_unit_size, const int num_horizontal_units,
    const int plane_width, Array2DView<Pixel>* const loop_restored_window) {
  const int unit_x = x + column;
  const int unit_y = y + row;
  const int current_process_unit_width =
      (unit_x + plane_process_unit_width <= plane_width)
          ? plane_process_unit_width
          : plane_width - unit_x;
  uint8_t* cdef_unit_buffer =
      cdef_buffer + unit_y * cdef_buffer_stride + unit_x * pixel_size_;
  const int unit_column =
      std::min(unit_x / plane_unit_size, num_horizontal_units - 1);
  const int unit_id = unit_row * num_horizontal_units + unit_column;
  const LoopRestorationType type =
      restoration_info_
          ->loop_restoration_info(static_cast<Plane>(plane), unit_id)
          .type;
  if (type == kLoopRestorationTypeNone) {
    Pixel* dest = &(*loop_restored_window)[row][column];
    for (int k = 0; k < current_process_unit_height; ++k) {
      memcpy(dest, cdef_unit_buffer, current_process_unit_width * pixel_size_);
      dest += loop_restored_window->columns();
      cdef_unit_buffer += cdef_buffer_stride;
    }
    return;
  }

  // The SIMD implementation of wiener filter (currently WienerFilter_SSE4_1())
  // over-reads 6 bytes, so add 6 extra bytes at the end of block_buffer for 8
  // bit.
  alignas(alignof(uint16_t))
      uint8_t block_buffer[kRestorationProcessingUnitSizeWithBorders *
                               kRestorationProcessingUnitSizeWithBorders *
                               sizeof(Pixel) +
                           ((sizeof(Pixel) == 1) ? 6 : 0)];
  const ptrdiff_t block_buffer_stride =
      kRestorationProcessingUnitSizeWithBorders * pixel_size_;
  IntermediateBuffers intermediate_buffers;

  RestorationBuffer restoration_buffer = {
      {intermediate_buffers.box_filter.output[0],
       intermediate_buffers.box_filter.output[1]},
      plane_process_unit_width,
      {intermediate_buffers.box_filter.intermediate_a,
       intermediate_buffers.box_filter.intermediate_b},
      kRestorationProcessingUnitSizeWithBorders + kRestorationPadding,
      intermediate_buffers.wiener,
      kMaxSuperBlockSizeInPixels};
  const int deblock_buffer_units = 64 >> subsampling_y_[plane];
  uint8_t* const deblock_buffer = deblock_buffer_.data(plane);
  const int deblock_buffer_stride = deblock_buffer_.stride(plane);
  const int deblock_unit_y =
      std::max(MultiplyBy4(Ceil(unit_y, deblock_buffer_units)) - 4, 0);
  uint8_t* deblock_unit_buffer =
      (deblock_buffer != nullptr)
          ? deblock_buffer + deblock_unit_y * deblock_buffer_stride +
                unit_x * pixel_size_
          : nullptr;
  assert(type == kLoopRestorationTypeSgrProj ||
         type == kLoopRestorationTypeWiener);
  const dsp::LoopRestorationFunc restoration_func =
      dsp_.loop_restorations[type - 2];
  PrepareLoopRestorationBlock<Pixel>(
      DoCdef(), cdef_unit_buffer, cdef_buffer_stride, deblock_unit_buffer,
      deblock_buffer_stride, block_buffer, block_buffer_stride,
      current_process_unit_width, current_process_unit_height, unit_y == 0,
      unit_y + current_process_unit_height >= plane_height);
  restoration_func(reinterpret_cast<const uint8_t*>(
                       block_buffer + kRestorationBorder * block_buffer_stride +
                       kRestorationBorder * pixel_size_),
                   &(*loop_restored_window)[row][column],
                   restoration_info_->loop_restoration_info(
                       static_cast<Plane>(plane), unit_id),
                   block_buffer_stride,
                   loop_restored_window->columns() * pixel_size_,
                   current_process_unit_width, current_process_unit_height,
                   &restoration_buffer);
}

template <typename Pixel>
void PostFilter::ApplyLoopRestorationForSuperBlock(
    const Plane plane, const int x, const int y, const int unit_row,
    const int current_process_unit_height, const int process_unit_width) {
  const int stride = frame_buffer_.stride(plane);
  const int plane_unit_size = loop_restoration_.unit_size[plane];
  const int num_horizontal_units =
      restoration_info_->num_horizontal_units(static_cast<Plane>(plane));
  const int plane_width =
      RightShiftWithRounding(upscaled_width_, subsampling_x_[plane]);
  const int plane_height =
      RightShiftWithRounding(height_, subsampling_y_[plane]);
  Array2DView<Pixel> loop_restored_window(
      current_process_unit_height, stride / sizeof(Pixel),
      reinterpret_cast<Pixel*>(loop_restoration_buffer_[plane] + y * stride +
                               x * pixel_size_));
  ApplyLoopRestorationForOneUnit<Pixel>(
      superres_buffer_[plane], stride, plane, plane_height, x, y, 0, 0,
      unit_row, current_process_unit_height, process_unit_width,
      plane_unit_size, num_horizontal_units, plane_width,
      &loop_restored_window);
}

void PostFilter::ApplyLoopRestorationForOneSuperBlockRow(int row4x4_start,
                                                         int sb4x4) {
  assert(row4x4_start >= 0);
  assert(DoRestoration());
  const int plane_process_unit_width[kMaxPlanes] = {
      kRestorationProcessingUnitSize,
      kRestorationProcessingUnitSize >> subsampling_x_[kPlaneU],
      kRestorationProcessingUnitSize >> subsampling_x_[kPlaneV]};
  const int plane_process_unit_height[kMaxPlanes] = {
      kRestorationProcessingUnitSize,
      kRestorationProcessingUnitSize >> subsampling_y_[kPlaneU],
      kRestorationProcessingUnitSize >> subsampling_y_[kPlaneV]};
  for (int plane = 0; plane < planes_; ++plane) {
    if (frame_header_.loop_restoration.type[plane] ==
        kLoopRestorationTypeNone) {
      continue;
    }
    const int unit_height_offset =
        kRestorationUnitOffset >> subsampling_y_[plane];
    const int plane_height =
        RightShiftWithRounding(frame_header_.height, subsampling_y_[plane]);
    const int plane_width = RightShiftWithRounding(frame_header_.upscaled_width,
                                                   subsampling_x_[plane]);
    const int num_vertical_units =
        restoration_info_->num_vertical_units(static_cast<Plane>(plane));
    const int process_unit_width = plane_process_unit_width[plane];
    for (int sb_y = 0; sb_y < sb4x4; sb_y += 16) {
      const int row4x4 = row4x4_start + sb_y;
      const int y = (MultiplyBy4(row4x4) - (row4x4 == 0 ? 0 : 8)) >>
                    subsampling_y_[plane];
      if (y >= plane_height) break;
      const int plane_unit_size =
          frame_header_.loop_restoration.unit_size[plane];
      const int unit_row = std::min((y + unit_height_offset) / plane_unit_size,
                                    num_vertical_units - 1);
      const int expected_height = plane_process_unit_height[plane] +
                                  ((y == 0) ? -unit_height_offset : 0);
      const int current_process_unit_height =
          (y + expected_height <= plane_height) ? expected_height
                                                : plane_height - y;
      for (int column4x4 = 0;; column4x4 += 16) {
        const int x = MultiplyBy4(column4x4) >> subsampling_x_[plane];
        if (x >= plane_width) break;
#if LIBGAV1_MAX_BITDEPTH >= 10
        if (bitdepth_ >= 10) {
          ApplyLoopRestorationForSuperBlock<uint16_t>(
              static_cast<Plane>(plane), x, y, unit_row,
              current_process_unit_height, process_unit_width);
          continue;
        }
#endif
        ApplyLoopRestorationForSuperBlock<uint8_t>(
            static_cast<Plane>(plane), x, y, unit_row,
            current_process_unit_height, process_unit_width);
      }
    }
  }
}

template <typename Pixel>
void PostFilter::ApplyLoopRestorationForOneRowInWindow(
    uint8_t* const cdef_buffer, const ptrdiff_t cdef_buffer_stride,
    const Plane plane, const int plane_height, const int plane_width,
    const int x, const int y, const int row, const int unit_row,
    const int current_process_unit_height, const int process_unit_width,
    const int window_width, const int plane_unit_size,
    const int num_horizontal_units) {
  Array2DView<Pixel> loop_restored_window(
      window_buffer_height_, window_buffer_width_,
      reinterpret_cast<Pixel*>(threaded_window_buffer_));
  for (int column = 0; column < window_width; column += process_unit_width) {
    ApplyLoopRestorationForOneUnit<Pixel>(
        cdef_buffer, cdef_buffer_stride, plane, plane_height, x, y, row, column,
        unit_row, current_process_unit_height, process_unit_width,
        plane_unit_size, num_horizontal_units, plane_width,
        &loop_restored_window);
  }
}

// Multi-thread version of loop restoration, based on a moving window of size
// |window_buffer_width_|x|window_buffer_height_|. Inside the moving window, we
// create a filtering job for each row and each filtering job is submitted to
// the thread pool. Each free thread takes one job from the thread pool and
// completes filtering until all jobs are finished. This approach requires an
// extra buffer (|threaded_window_buffer_|) to hold the filtering output, whose
// size is the size of the window. It also needs block buffers (i.e.,
// |block_buffer| and |intermediate_buffers| in
// ApplyLoopRestorationForOneUnit()) to store intermediate results in loop
// restoration for each thread. After all units inside the window are filtered,
// the output is written to the frame buffer.
template <typename Pixel>
void PostFilter::ApplyLoopRestorationThreaded() {
  const int plane_process_unit_width[kMaxPlanes] = {
      kRestorationProcessingUnitSize,
      kRestorationProcessingUnitSize >> subsampling_x_[kPlaneU],
      kRestorationProcessingUnitSize >> subsampling_x_[kPlaneV]};
  const int plane_process_unit_height[kMaxPlanes] = {
      kRestorationProcessingUnitSize,
      kRestorationProcessingUnitSize >> subsampling_y_[kPlaneU],
      kRestorationProcessingUnitSize >> subsampling_y_[kPlaneV]};

  for (int plane = kPlaneY; plane < planes_; ++plane) {
    if (loop_restoration_.type[plane] == kLoopRestorationTypeNone) {
      continue;
    }

    const int unit_height_offset =
        kRestorationUnitOffset >> subsampling_y_[plane];
    uint8_t* const src_buffer = superres_buffer_[plane];
    const int src_stride = frame_buffer_.stride(plane);
    const int plane_unit_size = loop_restoration_.unit_size[plane];
    const int num_vertical_units =
        restoration_info_->num_vertical_units(static_cast<Plane>(plane));
    const int num_horizontal_units =
        restoration_info_->num_horizontal_units(static_cast<Plane>(plane));
    const int plane_width =
        RightShiftWithRounding(upscaled_width_, subsampling_x_[plane]);
    const int plane_height =
        RightShiftWithRounding(height_, subsampling_y_[plane]);
    ExtendFrameBoundary(src_buffer, plane_width, plane_height, src_stride,
                        kRestorationBorder, kRestorationBorder,
                        kRestorationBorder, kRestorationBorder);

    const int num_workers = thread_pool_->num_threads();
    for (int y = 0; y < plane_height; y += window_buffer_height_) {
      const int actual_window_height =
          std::min(window_buffer_height_ - ((y == 0) ? unit_height_offset : 0),
                   plane_height - y);
      int vertical_units_per_window =
          (actual_window_height + plane_process_unit_height[plane] - 1) /
          plane_process_unit_height[plane];
      if (y == 0) {
        // The first row of loop restoration processing units is not 64x64, but
        // 64x56 (|unit_height_offset| = 8 rows less than other restoration
        // processing units). For u/v with subsampling, the size is halved. To
        // compute the number of vertical units per window, we need to take a
        // special handling for it.
        const int height_without_first_unit =
            actual_window_height -
            std::min(actual_window_height,
                     plane_process_unit_height[plane] - unit_height_offset);
        vertical_units_per_window =
            (height_without_first_unit + plane_process_unit_height[plane] - 1) /
                plane_process_unit_height[plane] +
            1;
      }
      for (int x = 0; x < plane_width; x += window_buffer_width_) {
        const int actual_window_width =
            std::min(window_buffer_width_, plane_width - x);
        const int jobs_for_threadpool =
            vertical_units_per_window * num_workers / (num_workers + 1);
        assert(jobs_for_threadpool < vertical_units_per_window);
        BlockingCounter pending_jobs(jobs_for_threadpool);
        int job_count = 0;
        int current_process_unit_height;
        for (int row = 0; row < actual_window_height;
             row += current_process_unit_height) {
          const int unit_y = y + row;
          const int expected_height = plane_process_unit_height[plane] +
                                      ((unit_y == 0) ? -unit_height_offset : 0);
          current_process_unit_height =
              (unit_y + expected_height <= plane_height)
                  ? expected_height
                  : plane_height - unit_y;
          const int unit_row =
              std::min((unit_y + unit_height_offset) / plane_unit_size,
                       num_vertical_units - 1);
          const int process_unit_width = plane_process_unit_width[plane];

          if (job_count < jobs_for_threadpool) {
            thread_pool_->Schedule(
                [this, src_buffer, src_stride, process_unit_width,
                 current_process_unit_height, actual_window_width,
                 plane_unit_size, num_horizontal_units, x, y, row, unit_row,
                 plane_height, plane_width, plane, &pending_jobs]() {
                  ApplyLoopRestorationForOneRowInWindow<Pixel>(
                      src_buffer, src_stride, static_cast<Plane>(plane),
                      plane_height, plane_width, x, y, row, unit_row,
                      current_process_unit_height, process_unit_width,
                      actual_window_width, plane_unit_size,
                      num_horizontal_units);
                  pending_jobs.Decrement();
                });
          } else {
            ApplyLoopRestorationForOneRowInWindow<Pixel>(
                src_buffer, src_stride, static_cast<Plane>(plane), plane_height,
                plane_width, x, y, row, unit_row, current_process_unit_height,
                process_unit_width, actual_window_width, plane_unit_size,
                num_horizontal_units);
          }
          ++job_count;
        }
        // Wait for all jobs of current window to finish.
        pending_jobs.Wait();
        // Copy |threaded_window_buffer_| to output frame.
        CopyPlane<Pixel>(
            threaded_window_buffer_, window_buffer_width_ * pixel_size_,
            actual_window_width, actual_window_height,
            loop_restoration_buffer_[plane] + y * src_stride + x * pixel_size_,
            src_stride);
      }
      if (y == 0) y -= unit_height_offset;
    }
  }
}

void PostFilter::ApplyLoopRestoration() {
  assert(threaded_window_buffer_ != nullptr);
#if LIBGAV1_MAX_BITDEPTH >= 10
  if (bitdepth_ >= 10) {
    ApplyLoopRestorationThreaded<uint16_t>();
    return;
  }
#endif
  ApplyLoopRestorationThreaded<uint8_t>();
}

}  // namespace libgav1
