// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/image_thumbnail_request.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_thread.h"
#include "skia/ext/image_operations.h"

namespace {

// Ignore image files that are too large to avoid long delays.
const int64_t kMaxImageSize = 10 * 1024 * 1024;  // 10 MB

std::string LoadImageData(const base::FilePath& path) {
  base::AssertBlockingAllowed();

  // Confirm that the file's size is within our threshold.
  int64_t file_size;
  if (!base::GetFileSize(path, &file_size) || file_size > kMaxImageSize) {
    LOG(ERROR) << "Unexpected file size: " << path.MaybeAsASCII() << ", "
               << file_size;
    return std::string();
  }

  std::string data;
  bool success = base::ReadFileToString(path, &data);

  // Make sure the file isn't empty.
  if (!success || data.empty()) {
    LOG(ERROR) << "Failed to read file: " << path.MaybeAsASCII();
    return std::string();
  }

  return data;
}

SkBitmap ScaleDownBitmap(int icon_size, const SkBitmap& decoded_image) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (decoded_image.drawsNothing())
    return decoded_image;

  // Shrink the image down so that its smallest dimension is equal to or
  // smaller than the requested size.
  int min_dimension = std::min(decoded_image.width(), decoded_image.height());

  if (min_dimension <= icon_size)
    return decoded_image;

  uint64_t width = static_cast<uint64_t>(decoded_image.width());
  uint64_t height = static_cast<uint64_t>(decoded_image.height());
  return skia::ImageOperations::Resize(
      decoded_image, skia::ImageOperations::RESIZE_BEST,
      width * icon_size / min_dimension, height * icon_size / min_dimension);
}

}  // namespace

ImageThumbnailRequest::ImageThumbnailRequest(
    int icon_size,
    base::OnceCallback<void(const SkBitmap&)> callback)
    : icon_size_(icon_size),
      callback_(std::move(callback)),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

ImageThumbnailRequest::~ImageThumbnailRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void ImageThumbnailRequest::Start(const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&LoadImageData, path),
      base::BindOnce(&ImageThumbnailRequest::OnLoadComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ImageThumbnailRequest::OnImageDecoded(const SkBitmap& decoded_image) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&ScaleDownBitmap, icon_size_, decoded_image),
      base::BindOnce(&ImageThumbnailRequest::FinishRequest,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ImageThumbnailRequest::OnDecodeImageFailed() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  LOG(ERROR) << "Failed to decode image.";
  FinishRequest(SkBitmap());
}

void ImageThumbnailRequest::OnLoadComplete(const std::string& data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (data.empty()) {
    FinishRequest(SkBitmap());
    return;
  }

  ImageDecoder::Start(this, data);
}

void ImageThumbnailRequest::FinishRequest(const SkBitmap& thumbnail) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(std::move(callback_), thumbnail));
  delete this;
}
