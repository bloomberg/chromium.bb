// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERS_AVATAR_USER_IMAGE_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERS_AVATAR_USER_IMAGE_LOADER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/image_decoder/image_decoder.h"

namespace base {
class SequencedTaskRunner;
}

namespace user_manager {
class UserImage;
}

// Helper functions that read, decode and optionally resize an image on a
// background thread. The image is returned in the form of a UserImage.
namespace chromeos {
namespace user_image_loader {

using LoadedCallback =
    base::OnceCallback<void(std::unique_ptr<user_manager::UserImage>)>;

// Loads an image with |image_codec| in the background and calls |loaded_cb|
// with the resulting UserImage (which may be empty in case of error). If
// |pixels_per_side| is positive, the image is cropped to a square and shrunk
// so that it does not exceed |pixels_per_side|x|pixels_per_side|. The first
// variant of this function reads the image from |file_path| on disk, the
// second processes |data| read into memory already. Decoding is done in a
// separate sandboxed process via ImageDecoder, and file I/O and resizing are
// done via |background_task_runner|.
void StartWithFilePath(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const base::FilePath& file_path,
    ImageDecoder::ImageCodec image_codec,
    int pixels_per_side,
    LoadedCallback loaded_cb);
void StartWithData(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    std::unique_ptr<std::string> data,
    ImageDecoder::ImageCodec image_codec,
    int pixels_per_side,
    LoadedCallback loaded_cb);

}  // namespace user_image_loader
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_AVATAR_USER_IMAGE_LOADER_H_
