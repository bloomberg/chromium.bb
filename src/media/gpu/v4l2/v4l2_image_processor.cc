// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/scopedfd_helper.h"
#include "media/base/video_types.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_image_processor.h"

#define IOCTL_OR_ERROR_RETURN_VALUE(type, arg, value, type_str) \
  do {                                                          \
    if (device_->Ioctl(type, arg) != 0) {                       \
      VPLOGF(1) << "ioctl() failed: " << type_str;              \
      return value;                                             \
    }                                                           \
  } while (0)

#define IOCTL_OR_ERROR_RETURN(type, arg) \
  IOCTL_OR_ERROR_RETURN_VALUE(type, arg, ((void)0), #type)

#define IOCTL_OR_ERROR_RETURN_FALSE(type, arg) \
  IOCTL_OR_ERROR_RETURN_VALUE(type, arg, false, #type)

#define IOCTL_OR_LOG_ERROR(type, arg)           \
  do {                                          \
    if (device_->Ioctl(type, arg) != 0)         \
      VPLOGF(1) << "ioctl() failed: " << #type; \
  } while (0)

namespace media {

V4L2ImageProcessor::InputRecord::InputRecord() : at_device(false) {}

V4L2ImageProcessor::InputRecord::InputRecord(
    const V4L2ImageProcessor::InputRecord&) = default;

V4L2ImageProcessor::InputRecord::~InputRecord() {}

V4L2ImageProcessor::OutputRecord::OutputRecord() : at_device(false) {}

V4L2ImageProcessor::OutputRecord::OutputRecord(OutputRecord&&) = default;

V4L2ImageProcessor::OutputRecord::~OutputRecord() {}

V4L2ImageProcessor::JobRecord::JobRecord() : output_buffer_index(-1) {}

V4L2ImageProcessor::JobRecord::~JobRecord() {}

V4L2ImageProcessor::V4L2ImageProcessor(
    scoped_refptr<V4L2Device> device,
    VideoFrame::StorageType input_storage_type,
    VideoFrame::StorageType output_storage_type,
    v4l2_memory input_memory_type,
    v4l2_memory output_memory_type,
    OutputMode output_mode,
    const VideoFrameLayout& input_layout,
    const VideoFrameLayout& output_layout,
    gfx::Size input_visible_size,
    gfx::Size output_visible_size,
    size_t num_buffers,
    const base::Closure& error_cb)
    : input_layout_(input_layout),
      input_visible_size_(input_visible_size),
      input_memory_type_(input_memory_type),
      input_storage_type_(input_storage_type),
      output_layout_(output_layout),
      output_visible_size_(output_visible_size),
      output_memory_type_(output_memory_type),
      output_storage_type_(output_storage_type),
      output_mode_(output_mode),
      child_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      device_(device),
      device_thread_("V4L2ImageProcessorThread"),
      device_poll_thread_("V4L2ImageProcessorDevicePollThread"),
      input_streamon_(false),
      input_buffer_queued_count_(0),
      output_streamon_(false),
      output_buffer_queued_count_(0),
      num_buffers_(num_buffers),
      error_cb_(error_cb),
      weak_this_factory_(this) {
  weak_this_ = weak_this_factory_.GetWeakPtr();
}

V4L2ImageProcessor::~V4L2ImageProcessor() {
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  Destroy();

  DCHECK(!device_thread_.IsRunning());
  DCHECK(!device_poll_thread_.IsRunning());

  DestroyInputBuffers();
  DestroyOutputBuffers();
}

void V4L2ImageProcessor::NotifyError() {
  VLOGF(1);
  DCHECK(!child_task_runner_->BelongsToCurrentThread());
  child_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&V4L2ImageProcessor::NotifyErrorOnChildThread,
                                weak_this_, error_cb_));
}

void V4L2ImageProcessor::NotifyErrorOnChildThread(
    const base::Closure& error_cb) {
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  error_cb_.Run();
}

namespace {

v4l2_memory InputStorageTypeToV4L2Memory(VideoFrame::StorageType storage_type) {
  switch (storage_type) {
    case VideoFrame::STORAGE_OWNED_MEMORY:
    case VideoFrame::STORAGE_UNOWNED_MEMORY:
    case VideoFrame::STORAGE_SHMEM:
    case VideoFrame::STORAGE_MOJO_SHARED_BUFFER:
      return V4L2_MEMORY_USERPTR;
    case VideoFrame::STORAGE_DMABUFS:
      return V4L2_MEMORY_DMABUF;
    default:
      return static_cast<v4l2_memory>(0);
  }
}

}  // namespace

// static
std::unique_ptr<V4L2ImageProcessor> V4L2ImageProcessor::Create(
      scoped_refptr<V4L2Device> device,
      VideoFrame::StorageType input_storage_type,
      VideoFrame::StorageType output_storage_type,
      OutputMode output_mode,
      const VideoFrameLayout& input_layout,
      const VideoFrameLayout& output_layout,
      gfx::Size input_visible_size,
      gfx::Size output_visible_size,
      size_t num_buffers,
      const base::Closure& error_cb) {
  VLOGF(2);
  DCHECK_GT(num_buffers, 0u);
  if (!device) {
    VLOGF(1) << "Failed creating V4L2Device";
    return nullptr;
  }

  const v4l2_memory input_memory_type = InputStorageTypeToV4L2Memory(
      input_storage_type);
  if (input_memory_type == 0) {
    VLOGF(1) << "Unsupported input storage type: " << input_storage_type;
    return nullptr;
  }

  // Note that for v4l2 IP, output storage type must be STORAGE_DMABUFS.
  // And output_memory_type depends on its output mode.
  if (output_storage_type != VideoFrame::STORAGE_DMABUFS) {
    VLOGF(1) << "Unsupported output storage type: " << output_storage_type;
    return nullptr;
  }
  const v4l2_memory output_memory_type =
      output_mode == ImageProcessor::OutputMode::ALLOCATE ? V4L2_MEMORY_MMAP
                                                          : V4L2_MEMORY_DMABUF;

  if (!device->IsImageProcessingSupported()) {
    VLOGF(1) << "V4L2ImageProcessor not supported in this platform";
    return nullptr;
  }
  const uint32_t input_format_fourcc =
      V4L2Device::VideoFrameLayoutToV4L2PixFmt(input_layout);
  if (!input_format_fourcc) {
    VLOGF(1) << "Invalid VideoFrameLayout: " << input_layout;
    return nullptr;
  }
  if (!device->Open(V4L2Device::Type::kImageProcessor, input_format_fourcc)) {
    VLOGF(1) << "Failed to open device for input format: "
             << VideoPixelFormatToString(input_layout.format())
             << " fourcc: " << FourccToString(input_format_fourcc);
    return nullptr;
  }

  // Try to set input format.
  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  format.fmt.pix_mp.width = input_layout.coded_size().width();
  format.fmt.pix_mp.height = input_layout.coded_size().height();
  format.fmt.pix_mp.pixelformat = input_format_fourcc;
  if (device->Ioctl(VIDIOC_S_FMT, &format) != 0 ||
      format.fmt.pix_mp.pixelformat != input_format_fourcc) {
    VLOGF(1) << "Failed to negotiate input format";
    return nullptr;
  }

  base::Optional<VideoFrameLayout> negotiated_input_layout =
      V4L2Device::V4L2FormatToVideoFrameLayout(format);
  if (!negotiated_input_layout) {
    VLOGF(1) << "Failed to negotiate input VideoFrameLayout";
    return nullptr;
  }
  DCHECK_LE(negotiated_input_layout->num_buffers(),
            static_cast<size_t>(VIDEO_MAX_PLANES));
  if (!gfx::Rect(negotiated_input_layout->coded_size())
      .Contains(gfx::Rect(input_visible_size))) {
    VLOGF(1) << "Negotiated input allocated size: "
             << negotiated_input_layout->coded_size().ToString()
             << " should contain visible size: "
             << input_visible_size.ToString();
    return nullptr;
  }

  const uint32_t output_format_fourcc =
      V4L2Device::VideoFrameLayoutToV4L2PixFmt(output_layout);
  if (!output_format_fourcc) {
    VLOGF(1) << "Invalid VideoFrameLayout: " << output_layout;
    return nullptr;
  }

  // Try to set output format.
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  format.fmt.pix_mp.width = output_layout.coded_size().width();
  format.fmt.pix_mp.height = output_layout.coded_size().height();
  format.fmt.pix_mp.pixelformat = output_format_fourcc;
  if (device->Ioctl(VIDIOC_S_FMT, &format) != 0 ||
      format.fmt.pix_mp.pixelformat != output_format_fourcc) {
    VLOGF(1) << "Failed to negotiate output format";
    return nullptr;
  }
  base::Optional<VideoFrameLayout> negotiated_output_layout =
      V4L2Device::V4L2FormatToVideoFrameLayout(format);
  if (!negotiated_output_layout) {
    VLOGF(1) << "Failed to negotiate output VideoFrameLayout";
    return nullptr;
  }
  DCHECK_LE(negotiated_output_layout->num_buffers(),
            static_cast<size_t>(VIDEO_MAX_PLANES));
  if (!gfx::Rect(negotiated_output_layout->coded_size())
      .Contains(gfx::Rect(output_layout.coded_size()))) {
    VLOGF(1) << "Negotiated output allocated size: "
             << negotiated_output_layout->coded_size().ToString()
             << " should contain original output allocated size: "
             << output_layout.coded_size().ToString();
    return nullptr;

  }

  auto processor = base::WrapUnique(new V4L2ImageProcessor(
      std::move(device), input_storage_type, output_storage_type,
      input_memory_type, output_memory_type, output_mode,
      *negotiated_input_layout, *negotiated_output_layout, input_visible_size,
      output_visible_size, num_buffers, std::move(error_cb)));
  if (!processor->Initialize()) {
    VLOGF(1) << "Failed to initialize V4L2ImageProcessor";
    return nullptr;
  }
  return processor;
}

bool V4L2ImageProcessor::Initialize() {
  // Capabilities check.
  struct v4l2_capability caps;
  memset(&caps, 0, sizeof(caps));
  const __u32 kCapsRequired = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QUERYCAP, &caps);
  if ((caps.capabilities & kCapsRequired) != kCapsRequired) {
    VLOGF(1) << "Initialize(): ioctl() failed: VIDIOC_QUERYCAP: "
             << "caps check failed: 0x" << std::hex << caps.capabilities;
    return false;
  }

  if (!CreateInputBuffers() || !CreateOutputBuffers())
    return false;

  if (!device_thread_.Start()) {
    VLOGF(1) << "Initialize(): device thread failed to start";
    return false;
  }

  // StartDevicePoll will NotifyError on failure.
  device_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&V4L2ImageProcessor::StartDevicePoll, base::Unretained(this)));

  VLOGF(2) << "V4L2ImageProcessor initialized for "
           << "input_layout: " << input_layout_
           << ", output_layout: " << output_layout_
           << ", input_visible_size: " << input_visible_size_.ToString()
           << ", output_visible_size: " << output_visible_size_.ToString();

  return true;
}

// static
bool V4L2ImageProcessor::IsSupported() {
  scoped_refptr<V4L2Device> device = V4L2Device::Create();
  if (!device)
    return false;

  return device->IsImageProcessingSupported();
}

// static
std::vector<uint32_t> V4L2ImageProcessor::GetSupportedInputFormats() {
  scoped_refptr<V4L2Device> device = V4L2Device::Create();
  if (!device)
    return std::vector<uint32_t>();

  return device->GetSupportedImageProcessorPixelformats(
      V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
}

// static
std::vector<uint32_t> V4L2ImageProcessor::GetSupportedOutputFormats() {
  scoped_refptr<V4L2Device> device = V4L2Device::Create();
  if (!device)
    return std::vector<uint32_t>();

  return device->GetSupportedImageProcessorPixelformats(
      V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
}

// static
bool V4L2ImageProcessor::TryOutputFormat(uint32_t input_pixelformat,
                                         uint32_t output_pixelformat,
                                         gfx::Size* size,
                                         size_t* num_planes) {
  VLOGF(2) << "size=" << size->ToString();
  scoped_refptr<V4L2Device> device = V4L2Device::Create();
  if (!device ||
      !device->Open(V4L2Device::Type::kImageProcessor, input_pixelformat))
    return false;

  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  format.fmt.pix_mp.width = size->width();
  format.fmt.pix_mp.height = size->height();
  format.fmt.pix_mp.pixelformat = output_pixelformat;
  if (device->Ioctl(VIDIOC_TRY_FMT, &format) != 0)
    return false;

  *num_planes = format.fmt.pix_mp.num_planes;
  *size = V4L2Device::CodedSizeFromV4L2Format(format);
  VLOGF(2) << "adjusted output coded size=" << size->ToString()
           << ", num_planes=" << *num_planes;
  return true;
}

gfx::Size V4L2ImageProcessor::input_allocated_size() const {
  return input_layout_.coded_size();
}

gfx::Size V4L2ImageProcessor::output_allocated_size() const {
  return output_layout_.coded_size();
}

VideoFrame::StorageType V4L2ImageProcessor::input_storage_type() const {
  return input_storage_type_;
}

VideoFrame::StorageType V4L2ImageProcessor::output_storage_type() const {
  return output_storage_type_;
}

ImageProcessor::OutputMode V4L2ImageProcessor::output_mode() const {
  return output_mode_;
}

bool V4L2ImageProcessor::Process(scoped_refptr<VideoFrame> frame,
                                 int output_buffer_index,
                                 std::vector<base::ScopedFD> output_dmabuf_fds,
                                 FrameReadyCB cb) {
  DVLOGF(4) << "ts=" << frame->timestamp().InMilliseconds();

  switch (output_memory_type_) {
    case V4L2_MEMORY_MMAP:
      if (!output_dmabuf_fds.empty()) {
        VLOGF(1) << "output_dmabuf_fds must be empty for MMAP output mode";
        return false;
      }
      output_dmabuf_fds =
          DuplicateFDs(output_buffer_map_[output_buffer_index].dmabuf_fds);
      break;

    case V4L2_MEMORY_DMABUF:
      break;

    default:
      NOTREACHED();
      return false;
  }

  if (output_dmabuf_fds.size() != output_layout_.num_buffers()) {
    VLOGF(1) << "wrong number of output fds. Expected "
             << output_layout_.num_buffers() << ", actual "
             << output_dmabuf_fds.size();
    return false;
  }

  std::unique_ptr<JobRecord> job_record(new JobRecord());
  job_record->input_frame = frame;
  job_record->output_buffer_index = output_buffer_index;
  job_record->ready_cb = std::move(cb);

  // Create the output frame
  job_record->output_frame = VideoFrame::WrapExternalDmabufs(
      output_layout_, gfx::Rect(output_visible_size_), output_visible_size_,
      std::move(output_dmabuf_fds), job_record->input_frame->timestamp());

  if (!job_record->output_frame)
    return false;

  device_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V4L2ImageProcessor::ProcessTask,
                                base::Unretained(this), std::move(job_record)));
  return true;
}

void V4L2ImageProcessor::ProcessTask(std::unique_ptr<JobRecord> job_record) {
  DVLOGF(4) << "Reusing output buffer, index="
            << job_record->output_buffer_index;
  DCHECK(device_thread_.task_runner()->BelongsToCurrentThread());

  EnqueueOutput(job_record.get());
  input_queue_.emplace(std::move(job_record));
  EnqueueInput();
}

bool V4L2ImageProcessor::Reset() {
  VLOGF(2);
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  DCHECK(device_thread_.IsRunning());

  weak_this_factory_.InvalidateWeakPtrs();
  device_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&V4L2ImageProcessor::StopDevicePoll, base::Unretained(this)));
  device_thread_.Stop();

  weak_this_ = weak_this_factory_.GetWeakPtr();
  if (!device_thread_.Start()) {
    VLOGF(1) << "device thread failed to start";
    return false;
  }
  device_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&V4L2ImageProcessor::StartDevicePoll, base::Unretained(this)));
  return true;
}

void V4L2ImageProcessor::Destroy() {
  VLOGF(2);
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  weak_this_factory_.InvalidateWeakPtrs();

  // If the device thread is running, destroy using posted task.
  if (device_thread_.IsRunning()) {
    device_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&V4L2ImageProcessor::StopDevicePoll,
                                  base::Unretained(this)));
    // Wait for tasks to finish/early-exit.
    device_thread_.Stop();
  } else {
    // Otherwise DestroyTask() is not needed.
    DCHECK(!device_poll_thread_.IsRunning());
  }
}

bool V4L2ImageProcessor::CreateInputBuffers() {
  VLOGF(2);
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  DCHECK(!input_streamon_);

  struct v4l2_control control;
  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_ROTATE;
  control.value = 0;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_CTRL, &control);

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_HFLIP;
  control.value = 0;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_CTRL, &control);

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_VFLIP;
  control.value = 0;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_CTRL, &control);

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_ALPHA_COMPONENT;
  control.value = 255;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_CTRL, &control);

  struct v4l2_rect visible_rect;
  visible_rect.left = 0;
  visible_rect.top = 0;
  visible_rect.width = base::checked_cast<__u32>(input_visible_size_.width());
  visible_rect.height = base::checked_cast<__u32>(input_visible_size_.height());

  struct v4l2_selection selection_arg;
  memset(&selection_arg, 0, sizeof(selection_arg));
  selection_arg.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  selection_arg.target = V4L2_SEL_TGT_CROP;
  selection_arg.r = visible_rect;
  if (device_->Ioctl(VIDIOC_S_SELECTION, &selection_arg) != 0) {
    VLOGF(2) << "Fallback to VIDIOC_S_CROP for input buffers.";
    struct v4l2_crop crop;
    memset(&crop, 0, sizeof(crop));
    crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    crop.c = visible_rect;
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_CROP, &crop);
  }

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = num_buffers_;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  reqbufs.memory = input_memory_type_;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_REQBUFS, &reqbufs);
  if (reqbufs.count != num_buffers_) {
    VLOGF(1) << "Failed to allocate input buffers. reqbufs.count="
             << reqbufs.count << ", num_buffers=" << num_buffers_;
    return false;
  }

  DCHECK(input_buffer_map_.empty());
  input_buffer_map_.resize(reqbufs.count);

  for (size_t i = 0; i < input_buffer_map_.size(); ++i)
    free_input_buffers_.push_back(i);

  return true;
}

bool V4L2ImageProcessor::CreateOutputBuffers() {
  VLOGF(2);
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  DCHECK(!output_streamon_);

  struct v4l2_rect visible_rect;
  visible_rect.left = 0;
  visible_rect.top = 0;
  visible_rect.width = base::checked_cast<__u32>(output_visible_size_.width());
  visible_rect.height =
    base::checked_cast<__u32>(output_visible_size_.height());

  struct v4l2_selection selection_arg;
  memset(&selection_arg, 0, sizeof(selection_arg));
  selection_arg.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  selection_arg.target = V4L2_SEL_TGT_COMPOSE;
  selection_arg.r = visible_rect;
  if (device_->Ioctl(VIDIOC_S_SELECTION, &selection_arg) != 0) {
    VLOGF(2) << "Fallback to VIDIOC_S_CROP for output buffers.";
    struct v4l2_crop crop;
    memset(&crop, 0, sizeof(crop));
    crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    crop.c = visible_rect;
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_CROP, &crop);
  }

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = num_buffers_;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  reqbufs.memory = output_memory_type_;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_REQBUFS, &reqbufs);
  if (reqbufs.count != num_buffers_) {
    VLOGF(1) << "Failed to allocate output buffers. reqbufs.count="
             << reqbufs.count << ", num_buffers=" << num_buffers_;
    return false;
  }

  DCHECK(output_buffer_map_.empty());
  output_buffer_map_.resize(reqbufs.count);

  // Get the DMA-BUF FDs for MMAP buffers
  if (output_memory_type_ == V4L2_MEMORY_MMAP) {
    for (unsigned int i = 0; i < output_buffer_map_.size(); i++) {
      OutputRecord& output_record = output_buffer_map_[i];
      output_record.dmabuf_fds = device_->GetDmabufsForV4L2Buffer(
          i, output_layout_.num_buffers(), V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
      if (output_record.dmabuf_fds.empty()) {
        VLOGF(1) << "failed to get fds of output buffer";
        return false;
      }
    }
  }

  return true;
}

void V4L2ImageProcessor::DestroyInputBuffers() {
  VLOGF(2);
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  DCHECK(!input_streamon_);

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 0;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  reqbufs.memory = input_memory_type_;
  IOCTL_OR_LOG_ERROR(VIDIOC_REQBUFS, &reqbufs);

  input_buffer_map_.clear();
  free_input_buffers_.clear();
}

void V4L2ImageProcessor::DestroyOutputBuffers() {
  VLOGF(2);
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  DCHECK(!output_streamon_);

  output_buffer_map_.clear();

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 0;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  reqbufs.memory = output_memory_type_;
  IOCTL_OR_LOG_ERROR(VIDIOC_REQBUFS, &reqbufs);
}

void V4L2ImageProcessor::DevicePollTask(bool poll_device) {
  DVLOGF(4);
  DCHECK(device_poll_thread_.task_runner()->BelongsToCurrentThread());

  bool event_pending;
  if (!device_->Poll(poll_device, &event_pending)) {
    NotifyError();
    return;
  }

  // All processing should happen on ServiceDeviceTask(), since we shouldn't
  // touch processor state from this thread.
  device_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V4L2ImageProcessor::ServiceDeviceTask,
                                base::Unretained(this)));
}

void V4L2ImageProcessor::ServiceDeviceTask() {
  DVLOGF(4);
  DCHECK(device_thread_.task_runner()->BelongsToCurrentThread());
  // ServiceDeviceTask() should only ever be scheduled from DevicePollTask(),
  // so either:
  // * device_poll_thread_ is running normally
  // * device_poll_thread_ scheduled us, but then a DestroyTask() shut it down,
  //   in which case we should early-out.
  if (!device_poll_thread_.task_runner())
    return;

  Dequeue();
  EnqueueInput();

  if (!device_->ClearDevicePollInterrupt()) {
    NotifyError();
    return;
  }

  bool poll_device =
      (input_buffer_queued_count_ > 0 || output_buffer_queued_count_ > 0);

  device_poll_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V4L2ImageProcessor::DevicePollTask,
                                base::Unretained(this), poll_device));

  DVLOGF(3) << __func__ << ": buffer counts: INPUT[" << input_queue_.size()
            << "] => DEVICE[" << free_input_buffers_.size() << "+"
            << input_buffer_queued_count_ << "/" << input_buffer_map_.size()
            << "->" << output_buffer_map_.size() - output_buffer_queued_count_
            << "+" << output_buffer_queued_count_ << "/"
            << output_buffer_map_.size() << "]";
}

void V4L2ImageProcessor::EnqueueInput() {
  DVLOGF(4);
  DCHECK(device_thread_.task_runner()->BelongsToCurrentThread());

  const int old_inputs_queued = input_buffer_queued_count_;
  while (!input_queue_.empty() && !free_input_buffers_.empty()) {
    if (!EnqueueInputRecord())
      return;
  }
  if (old_inputs_queued == 0 && input_buffer_queued_count_ != 0) {
    // We started up a previously empty queue.
    // Queue state changed; signal interrupt.
    if (!device_->SetDevicePollInterrupt()) {
      NotifyError();
      return;
    }
    // VIDIOC_STREAMON if we haven't yet.
    if (!input_streamon_) {
      __u32 type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
      IOCTL_OR_ERROR_RETURN(VIDIOC_STREAMON, &type);
      input_streamon_ = true;
    }
  }
}

void V4L2ImageProcessor::EnqueueOutput(const JobRecord* job_record) {
  DVLOGF(4);
  DCHECK(device_thread_.task_runner()->BelongsToCurrentThread());

  const int old_outputs_queued = output_buffer_queued_count_;
  if (!EnqueueOutputRecord(job_record))
    return;

  if (old_outputs_queued == 0 && output_buffer_queued_count_ != 0) {
    // We just started up a previously empty queue.
    // Queue state changed; signal interrupt.
    if (!device_->SetDevicePollInterrupt()) {
      NotifyError();
      return;
    }
    // Start VIDIOC_STREAMON if we haven't yet.
    if (!output_streamon_) {
      __u32 type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
      IOCTL_OR_ERROR_RETURN(VIDIOC_STREAMON, &type);
      output_streamon_ = true;
    }
  }
}

void V4L2ImageProcessor::Dequeue() {
  DVLOGF(4);
  DCHECK(device_thread_.task_runner()->BelongsToCurrentThread());

  // Dequeue completed input (VIDEO_OUTPUT) buffers,
  // and recycle to the free list.
  struct v4l2_buffer dqbuf;
  struct v4l2_plane planes[VIDEO_MAX_PLANES];
  while (input_buffer_queued_count_ > 0) {
    DCHECK(input_streamon_);
    memset(&dqbuf, 0, sizeof(dqbuf));
    memset(&planes, 0, sizeof(planes));
    dqbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    dqbuf.memory = input_memory_type_;
    dqbuf.m.planes = planes;
    dqbuf.length = input_layout_.num_buffers();
    if (device_->Ioctl(VIDIOC_DQBUF, &dqbuf) != 0) {
      if (errno == EAGAIN) {
        // EAGAIN if we're just out of buffers to dequeue.
        break;
      }
      VPLOGF(1) << "ioctl() failed: VIDIOC_DQBUF";
      NotifyError();
      return;
    }
    InputRecord& input_record = input_buffer_map_[dqbuf.index];
    DCHECK(input_record.at_device);
    input_record.at_device = false;
    input_record.frame = NULL;
    free_input_buffers_.push_back(dqbuf.index);
    input_buffer_queued_count_--;
  }
  // Dequeue completed output (VIDEO_CAPTURE) buffers, recycle to the free list.
  // Return the finished buffer to the client via the job ready callback.
  while (output_buffer_queued_count_ > 0) {
    DCHECK(output_streamon_);
    memset(&dqbuf, 0, sizeof(dqbuf));
    memset(&planes, 0, sizeof(planes));
    dqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    dqbuf.memory = output_memory_type_;
    dqbuf.m.planes = planes;
    dqbuf.length = output_layout_.num_buffers();
    if (device_->Ioctl(VIDIOC_DQBUF, &dqbuf) != 0) {
      if (errno == EAGAIN) {
        // EAGAIN if we're just out of buffers to dequeue.
        break;
      }
      VPLOGF(1) << "ioctl() failed: VIDIOC_DQBUF";
      NotifyError();
      return;
    }
    OutputRecord& output_record = output_buffer_map_[dqbuf.index];
    DCHECK(output_record.at_device);
    output_record.at_device = false;
    output_buffer_queued_count_--;

    // Jobs are always processed in FIFO order.
    DCHECK(!running_jobs_.empty());
    std::unique_ptr<JobRecord> job_record = std::move(running_jobs_.front());
    running_jobs_.pop();

    DVLOGF(4) << "Processing finished, returning frame, index=" << dqbuf.index;

    child_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&V4L2ImageProcessor::FrameReady, weak_this_,
                                  std::move(job_record->ready_cb),
                                  job_record->output_frame));
  }
}

bool V4L2ImageProcessor::EnqueueInputRecord() {
  DVLOGF(4);
  DCHECK(!input_queue_.empty());
  DCHECK(!free_input_buffers_.empty());

  // Enqueue an input (VIDEO_OUTPUT) buffer for an input video frame.
  std::unique_ptr<JobRecord> job_record = std::move(input_queue_.front());
  input_queue_.pop();
  const int index = free_input_buffers_.back();
  InputRecord& input_record = input_buffer_map_[index];
  DCHECK(!input_record.at_device);
  input_record.frame = job_record->input_frame;
  struct v4l2_buffer qbuf;
  struct v4l2_plane qbuf_planes[VIDEO_MAX_PLANES];
  memset(&qbuf, 0, sizeof(qbuf));
  memset(qbuf_planes, 0, sizeof(qbuf_planes));
  qbuf.index = index;
  qbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  qbuf.memory = input_memory_type_;
  qbuf.m.planes = qbuf_planes;
  qbuf.length = input_layout_.num_buffers();

  std::vector<int> fds;
  if (input_memory_type_ == V4L2_MEMORY_DMABUF) {
    auto& scoped_fds = input_record.frame->DmabufFds();
    if (scoped_fds.size() != input_layout_.num_buffers()) {
      VLOGF(1) << "Invalid number of planes in the frame";
      return false;
    }
    for (auto& fd : scoped_fds)
      fds.push_back(fd.get());
  }
  for (size_t i = 0; i < input_layout_.num_buffers(); ++i) {
    qbuf.m.planes[i].bytesused =
        VideoFrame::PlaneSize(input_record.frame->format(), i,
                              input_allocated_size())
            .GetArea();
    qbuf.m.planes[i].length = qbuf.m.planes[i].bytesused;
    switch (input_memory_type_) {
      case V4L2_MEMORY_USERPTR:
        qbuf.m.planes[i].m.userptr =
            reinterpret_cast<unsigned long>(input_record.frame->data(i));
        break;
      case V4L2_MEMORY_DMABUF:
        qbuf.m.planes[i].m.fd = fds[i];
        break;
      default:
        NOTREACHED();
        return false;
    }
  }
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QBUF, &qbuf);
  input_record.at_device = true;

  DVLOGF(4) << "enqueued frame ts="
            << job_record->input_frame->timestamp().InMilliseconds()
            << " to device.";

  running_jobs_.emplace(std::move(job_record));
  free_input_buffers_.pop_back();
  input_buffer_queued_count_++;

  return true;
}

bool V4L2ImageProcessor::EnqueueOutputRecord(const JobRecord* job_record) {
  DVLOGF(4);
  int index = job_record->output_buffer_index;
  DCHECK_GE(index, 0);
  DCHECK_LT(static_cast<size_t>(index), output_buffer_map_.size());
  // Enqueue an output (VIDEO_CAPTURE) buffer.
  OutputRecord& output_record = output_buffer_map_[index];
  DCHECK(!output_record.at_device);
  struct v4l2_buffer qbuf;
  struct v4l2_plane qbuf_planes[VIDEO_MAX_PLANES];
  memset(&qbuf, 0, sizeof(qbuf));
  memset(qbuf_planes, 0, sizeof(qbuf_planes));
  qbuf.index = index;
  qbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  qbuf.memory = output_memory_type_;
  if (output_memory_type_ == V4L2_MEMORY_DMABUF) {
    auto& fds = job_record->output_frame->DmabufFds();
    if (fds.size() != output_layout_.num_buffers()) {
      VLOGF(1) << "Invalid number of FDs in output record";
      return false;
    }
    for (size_t i = 0; i < fds.size(); ++i)
      qbuf_planes[i].m.fd = fds[i].get();
  }
  qbuf.m.planes = qbuf_planes;
  qbuf.length = output_layout_.num_buffers();
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QBUF, &qbuf);
  output_record.at_device = true;
  output_buffer_queued_count_++;
  return true;
}

void V4L2ImageProcessor::StartDevicePoll() {
  DVLOGF(3) << "starting device poll";
  DCHECK(device_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(!device_poll_thread_.IsRunning());

  // Start up the device poll thread and schedule its first DevicePollTask().
  if (!device_poll_thread_.Start()) {
    VLOGF(1) << "StartDevicePoll(): Device thread failed to start";
    NotifyError();
    return;
  }
  // Enqueue a poll task with no devices to poll on - will wait only for the
  // poll interrupt
  device_poll_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V4L2ImageProcessor::DevicePollTask,
                                base::Unretained(this), false));
}

void V4L2ImageProcessor::StopDevicePoll() {
  DVLOGF(3) << "stopping device poll";
  DCHECK(device_thread_.task_runner()->BelongsToCurrentThread());

  // Signal the DevicePollTask() to stop, and stop the device poll thread.
  bool result = device_->SetDevicePollInterrupt();
  device_poll_thread_.Stop();
  if (!result) {
    NotifyError();
    return;
  }

  // Clear the interrupt now, to be sure.
  if (!device_->ClearDevicePollInterrupt()) {
    NotifyError();
    return;
  }

  if (input_streamon_) {
    __u32 type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    IOCTL_OR_ERROR_RETURN(VIDIOC_STREAMOFF, &type);
  }
  input_streamon_ = false;

  if (output_streamon_) {
    __u32 type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    IOCTL_OR_ERROR_RETURN(VIDIOC_STREAMOFF, &type);
  }
  output_streamon_ = false;

  // Reset all our accounting info.
  while (!input_queue_.empty())
    input_queue_.pop();

  while (!running_jobs_.empty())
    running_jobs_.pop();

  free_input_buffers_.clear();
  for (size_t i = 0; i < input_buffer_map_.size(); ++i) {
    InputRecord& input_record = input_buffer_map_[i];
    input_record.at_device = false;
    input_record.frame = NULL;
    free_input_buffers_.push_back(i);
  }
  input_buffer_queued_count_ = 0;

  for (auto& output_buffer : output_buffer_map_)
    output_buffer.at_device = false;
  output_buffer_queued_count_ = 0;
}

void V4L2ImageProcessor::FrameReady(FrameReadyCB cb,
                                    scoped_refptr<VideoFrame> frame) {
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  std::move(cb).Run(frame);
}

}  // namespace media
