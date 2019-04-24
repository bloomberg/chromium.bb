// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/apps/apk_web_app_installer.h"

#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/bookmark_app_helper.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_ids_map.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {
constexpr int64_t kInvalidColor =
    static_cast<int64_t>(std::numeric_limits<int32_t>::max()) + 1;
}

namespace chromeos {

// static
void ApkWebAppInstaller::Install(Profile* profile,
                                 arc::mojom::WebAppInfoPtr web_app_info,
                                 const std::vector<uint8_t>& icon_png_data,
                                 InstallFinishCallback callback,
                                 base::WeakPtr<Owner> weak_owner) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(weak_owner.get());

  // If |weak_owner| is invalidated, installation will be stopped.
  // ApkWebAppInstaller owns itself and deletes itself when finished in
  // CompleteInstallation().
  auto* installer =
      new ApkWebAppInstaller(profile, std::move(callback), weak_owner);
  installer->Start(
      content::ServiceManagerConnection::GetForProcess()->GetConnector(),
      std::move(web_app_info), icon_png_data);
}

ApkWebAppInstaller::ApkWebAppInstaller(Profile* profile,
                                       InstallFinishCallback callback,
                                       base::WeakPtr<Owner> weak_owner)
    : profile_(profile),
      callback_(std::move(callback)),
      weak_owner_(weak_owner) {}

ApkWebAppInstaller::~ApkWebAppInstaller() = default;

void ApkWebAppInstaller::Start(service_manager::Connector* connector,
                               arc::mojom::WebAppInfoPtr web_app_info,
                               const std::vector<uint8_t>& icon_png_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!weak_owner_.get()) {
    CompleteInstallation(extensions::ExtensionId());
    return;
  }

  // We can't install without |web_app_info| or |icon_png_data|. They may be
  // null if there was an error generating the data.
  if (web_app_info.is_null() || icon_png_data.empty()) {
    LOG(ERROR) << "Insufficient data to install a web app";
    CompleteInstallation(extensions::ExtensionId());
    return;
  }

  web_app_info_.title = base::UTF8ToUTF16(web_app_info->title);

  web_app_info_.app_url = GURL(web_app_info->start_url);
  DCHECK(web_app_info_.app_url.is_valid());

  web_app_info_.scope = GURL(web_app_info->scope_url);
  DCHECK(web_app_info_.scope.is_valid());

  if (web_app_info->theme_color != kInvalidColor)
    web_app_info_.theme_color = static_cast<SkColor>(web_app_info->theme_color);
  web_app_info_.open_as_window = true;

  // Set up the connection to the service manager to decode the raw PNG icon
  // bytes into SkBitmaps.
  service_manager::mojom::ConnectorRequest connector_request;
  connector_ = service_manager::Connector::Create(&connector_request);
  connector->BindConnectorRequest(std::move(connector_request));

  // Decode the image in a sandboxed process off the main thread.
  // base::Unretained is safe because this object owns itself.
  data_decoder::DecodeImage(
      connector_.get(), icon_png_data, data_decoder::mojom::ImageCodec::DEFAULT,
      /*shrink_to_fit=*/false, data_decoder::kDefaultMaxSizeInBytes,
      /*desired_image_frame_size=*/gfx::Size(),
      base::BindOnce(&ApkWebAppInstaller::OnImageDecoded,
                     base::Unretained(this)));
}

void ApkWebAppInstaller::CompleteInstallation(
    const extensions::ExtensionId& id) {
  std::move(callback_).Run(id);
  delete this;
}

void ApkWebAppInstaller::OnBookmarkAppCreated(
    const extensions::Extension* extension,
    const WebApplicationInfo& web_app_info) {
  if (weak_owner_.get() && extension) {
    // It is assumed that if |weak_owner_| is gone, |profile_| is gone too. The
    // extension will be automatically cleaned up by
    // extensions::ExtensionSystem. Otherwise, insert this web app into the
    // extensions ID map so it is not removed automatically.
    // TODO(crbug.com/910008): have a less bad way of doing this.
    web_app::ExtensionIdsMap(profile_->GetPrefs())
        .Insert(web_app_info.app_url, extension->id(),
                web_app::InstallSource::kArc);
  }
  CompleteInstallation(extension ? extension->id() : extensions::ExtensionId());
}

void ApkWebAppInstaller::OnImageDecoded(const SkBitmap& decoded_image) {
  WebApplicationInfo::IconInfo icon_info;
  icon_info.data = decoded_image;
  icon_info.width = decoded_image.width();
  icon_info.height = decoded_image.height();

  web_app_info_.icons.push_back(icon_info);

  if (!weak_owner_.get()) {
    // Assume |profile_| is no longer valid - destroy this object and
    // terminate.
    CompleteInstallation(extensions::ExtensionId());
    return;
  }
  DoInstall();
}

void ApkWebAppInstaller::DoInstall() {
  DCHECK(!helper_);

  helper_ = std::make_unique<extensions::BookmarkAppHelper>(
      profile_, web_app_info_, /*web_contents=*/nullptr,
      WebappInstallSource::ARC);

  // We should only install windowed apps via this method.
  helper_->set_forced_launch_type(extensions::LAUNCH_TYPE_WINDOW);
  helper_->set_is_no_network_install();
  helper_->Create(base::BindRepeating(&ApkWebAppInstaller::OnBookmarkAppCreated,
                                      base::Unretained(this)));
}

}  // namespace chromeos
