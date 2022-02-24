// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_user_provider_impl.h"

#include "ash/public/cpp/personalization_app/user_display_info.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "ash/webui/personalization_app/mojom/personalization_app_mojom_traits.h"
#include "base/bind.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/camera_presence_notifier.h"
#include "chrome/browser/ash/login/users/avatar/user_image_file_selector.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager.h"
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "chrome/browser/ash/login/users/default_user_image/default_user_images.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_manager/user_info.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/message.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace {

using ash::personalization_app::GetAccountId;
using ash::personalization_app::GetUser;

GURL GetUserImageDataUrl(const user_manager::User& user) {
  if (user.GetImage().isNull())
    return GURL();
  return GURL(webui::GetBitmapDataUrl(*user.GetImage().bitmap()));
}

}  // namespace

PersonalizationAppUserProviderImpl::PersonalizationAppUserProviderImpl(
    content::WebUI* web_ui)
    : profile_(Profile::FromWebUI(web_ui)) {
  ash::UserImageManager* user_image_manager =
      ash::ChromeUserManager::Get()->GetUserImageManager(
          GetAccountId(profile_));
  user_image_manager->DownloadProfileImage();
  user_image_file_selector_ =
      std::make_unique<ash::UserImageFileSelector>(web_ui);
}

PersonalizationAppUserProviderImpl::~PersonalizationAppUserProviderImpl() =
    default;

void PersonalizationAppUserProviderImpl::BindInterface(
    mojo::PendingReceiver<ash::personalization_app::mojom::UserProvider>
        receiver) {
  user_receiver_.reset();
  user_receiver_.Bind(std::move(receiver));
}

void PersonalizationAppUserProviderImpl::SetUserImageObserver(
    mojo::PendingRemote<ash::personalization_app::mojom::UserImageObserver>
        observer) {
  // May already be bound if user refreshes page.
  user_image_observer_remote_.reset();
  user_image_observer_remote_.Bind(std::move(observer));
  DCHECK(user_manager::UserManager::IsInitialized());
  auto* user_manager = user_manager::UserManager::Get();
  if (!user_manager_observer_.IsObserving())
    user_manager_observer_.Observe(user_manager);

  // Call it manually the first time.
  OnUserImageChanged(*GetUser(profile_));

  ash::UserImageManager* user_image_manager =
      ash::ChromeUserManager::Get()->GetUserImageManager(
          GetAccountId(profile_));
  const gfx::ImageSkia& profile_image =
      user_image_manager->DownloadedProfileImage();
  OnUserProfileImageUpdated(*GetUser(profile_), profile_image);

  // Always unbind and rebind the camera check observer to trigger an immediate
  // |OnCameraPresenceCheckDone|.
  camera_observer_.Reset();
  camera_observer_.Observe(ash::CameraPresenceNotifier::GetInstance());
}

void PersonalizationAppUserProviderImpl::GetUserInfo(
    GetUserInfoCallback callback) {
  const user_manager::User* user = GetUser(profile_);
  DCHECK(user);
  std::move(callback).Run(ash::personalization_app::UserDisplayInfo(*user));
}

void PersonalizationAppUserProviderImpl::GetDefaultUserImages(
    GetDefaultUserImagesCallback callback) {
  std::vector<ash::default_user_image::DefaultUserImage> images =
      ash::default_user_image::GetCurrentImageSet();
  std::move(callback).Run(std::move(images));
}

void PersonalizationAppUserProviderImpl::SelectImageFromDisk() {
  user_image_file_selector_->SelectFile(
      base::BindOnce(&PersonalizationAppUserProviderImpl::OnFileSelected,
                     weak_ptr_factory_.GetWeakPtr()),
      base::DoNothing());
}

void PersonalizationAppUserProviderImpl::SelectDefaultImage(int index) {
  if (!ash::default_user_image::IsInCurrentImageSet(index)) {
    mojo::ReportBadMessage("Invalid user image selected");
    return;
  }

  auto* user_image_manager = ash::ChromeUserManager::Get()->GetUserImageManager(
      GetAccountId(profile_));

  user_image_manager->SaveUserDefaultImageIndex(index);
}

void PersonalizationAppUserProviderImpl::SelectProfileImage() {
  ash::UserImageManager* user_image_manager =
      ash::ChromeUserManager::Get()->GetUserImageManager(
          GetAccountId(profile_));

  user_image_manager->SaveUserImageFromProfileImage();
}

void PersonalizationAppUserProviderImpl::SelectCameraImage(
    ::mojo_base::BigBuffer data) {
  // Make a copy of the data.
  auto ref_counted =
      base::MakeRefCounted<base::RefCountedBytes>(data.data(), data.size());
  // Get a view of the same data copied above.
  auto as_span = base::make_span(ref_counted->front(), ref_counted->size());

  data_decoder::DecodeImageIsolated(
      as_span, data_decoder::mojom::ImageCodec::kPng,
      /*shrink_to_fit=*/true, data_decoder::kDefaultMaxSizeInBytes,
      /*desired_image_frame_size=*/gfx::Size(),
      base::BindOnce(&PersonalizationAppUserProviderImpl::OnCameraImageDecoded,
                     image_decode_weak_ptr_factory_.GetWeakPtr(),
                     std::move(ref_counted)));
}

void PersonalizationAppUserProviderImpl::OnFileSelected(
    const base::FilePath& path) {
  ash::UserImageManager* user_image_manager =
      ash::ChromeUserManager::Get()->GetUserImageManager(
          GetAccountId(profile_));

  user_image_manager->SaveUserImageFromFile(path);
}

void PersonalizationAppUserProviderImpl::OnUserImageChanged(
    const user_manager::User& user) {
  const user_manager::User* desired_user = GetUser(profile_);
  DCHECK(desired_user);

  if (user.GetAccountId() != desired_user->GetAccountId())
    return;

  int image_index = user.image_index();
  // Image is a valid default image and has an internal chrome://theme url.
  if (ash::default_user_image::IsInCurrentImageSet(image_index)) {
    user_image_observer_remote_->OnUserImageChanged(
        ash::default_user_image::GetDefaultImageUrl(image_index));
    return;
  }
  // All other cases.
  user_image_observer_remote_->OnUserImageChanged(GetUserImageDataUrl(user));
}

void PersonalizationAppUserProviderImpl::OnUserProfileImageUpdated(
    const user_manager::User& user,
    const gfx::ImageSkia& profile_image) {
  const user_manager::User* desired_user = GetUser(profile_);
  DCHECK(desired_user);

  if (user.GetAccountId() != desired_user->GetAccountId())
    return;

  user_image_observer_remote_->OnUserProfileImageUpdated(
      GURL(webui::GetBitmapDataUrl(*profile_image.bitmap())));
}

void PersonalizationAppUserProviderImpl::OnCameraPresenceCheckDone(
    bool is_camera_present) {
  user_image_observer_remote_->OnCameraPresenceCheckDone(is_camera_present);
}

void PersonalizationAppUserProviderImpl::OnCameraImageDecoded(
    scoped_refptr<base::RefCountedBytes> bytes,
    const SkBitmap& decoded_bitmap) {
  if (decoded_bitmap.isNull()) {
    LOG(WARNING) << "Camera image failed decoding";
    return;
  }

  auto user_image = std::make_unique<user_manager::UserImage>(
      gfx::ImageSkia::CreateFrom1xBitmap(decoded_bitmap), std::move(bytes),
      user_manager::UserImage::ImageFormat::FORMAT_PNG);
  // Image was successfully decoded so it is valid png data.
  user_image->MarkAsSafe();

  auto* user_image_manager = ash::ChromeUserManager::Get()->GetUserImageManager(
      GetAccountId(profile_));

  user_image_manager->SaveUserImage(std::move(user_image));
}

void PersonalizationAppUserProviderImpl::SetUserImageFileSelectorForTesting(
    std::unique_ptr<ash::UserImageFileSelector> file_selector) {
  user_image_file_selector_ = std::move(file_selector);
}
