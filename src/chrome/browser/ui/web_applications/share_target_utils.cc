// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/share_target_utils.h"

#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_share_target/target_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "extensions/common/constants.h"
#include "net/base/mime_util.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/arc/nearby_share/share_info_file_handler.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/file_stream_data_pipe_getter.h"
#endif

namespace web_app {

bool SharedField::operator==(const SharedField& other) const {
  return name == other.name && value == other.value;
}

std::vector<SharedField> ExtractSharedFields(
    const apps::ShareTarget& share_target,
    const apps::mojom::Intent& intent) {
  std::vector<SharedField> result;

  if (!share_target.params.title.empty() && intent.share_title.has_value() &&
      !intent.share_title->empty()) {
    result.push_back(
        {.name = share_target.params.title, .value = *intent.share_title});
  }

  if (!intent.share_text.has_value())
    return result;

  apps_util::SharedText extracted_text =
      apps_util::ExtractSharedText(*intent.share_text);

  if (!share_target.params.text.empty() && !extracted_text.text.empty())
    result.push_back(
        {.name = share_target.params.text, .value = extracted_text.text});

  if (!share_target.params.url.empty() && !extracted_text.url.is_empty())
    result.push_back(
        {.name = share_target.params.url, .value = extracted_text.url.spec()});

  return result;
}

NavigateParams NavigateParamsForShareTarget(
    Browser* browser,
    const apps::ShareTarget& share_target,
    const apps::mojom::Intent& intent) {
  NavigateParams nav_params(browser, share_target.action,
                            ui::PAGE_TRANSITION_AUTO_TOPLEVEL);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  constexpr int kBufSize = 16 * 1024;
  std::vector<std::string> names;
  std::vector<std::string> values;
  std::vector<bool> is_value_file_uris;
  std::vector<std::string> filenames;
  std::vector<std::string> types;
  std::vector<mojo::PendingRemote<network::mojom::DataPipeGetter>>
      data_pipe_getters;

  if (intent.mime_type.has_value() && intent.files.has_value()) {
    // Files for Web Share intents are created by the browser in
    // a .WebShare directory, with generated file names and file urls - see
    // //chrome/browser/webshare/chromeos/sharesheet_client.cc
    for (const auto& file : intent.files.value()) {
      const std::string& mime_type = file->mime_type.has_value()
                                         ? file->mime_type.value()
                                         : intent.mime_type.value();
      std::string name;
      for (const apps::ShareTarget::Files& files : share_target.params.files) {
        // Filter on MIME types. Chrome OS does not filter on file extensions.
        // https://w3c.github.io/web-share-target/level-2/#dfn-accepted
        if (base::ranges::any_of(
                files.accept, [&mime_type](const auto& criteria) {
                  return !base::StartsWith(criteria, ".") &&
                         net::MatchesMimeType(criteria, mime_type);
                })) {
          name = files.name;
          break;
        }
      }
      if (name.empty())
        continue;

      storage::FileSystemContext* file_system_context =
          file_manager::util::GetFileManagerFileSystemContext(
              browser->profile());
      storage::FileSystemURL file_system_url =
          file_system_context->CrackURLInFirstPartyContext(file->url);

      mojo::PendingRemote<network::mojom::DataPipeGetter> data_pipe_getter;
      if (!file_system_url.is_valid()) {
        // TODO(crbug.com/1166982): We could be more intelligent here and
        // decide which cracking method to use based on the scheme.
        auto file_system_url_and_handle =
            arc::ShareInfoFileHandler::GetFileSystemURL(browser->profile(),
                                                        file->url);
        file_system_url = file_system_url_and_handle.url;
        if (!file_system_url.is_valid()) {
          LOG(WARNING) << "Received unexpected file URL: " << file->url.spec();
          continue;
        }

        FileStreamDataPipeGetter::Create(
            /*receiver=*/data_pipe_getter.InitWithNewPipeAndPassReceiver(),
            /*context=*/file_system_context,
            /*url=*/file_system_url,
            /*offset=*/0,
            /*file_size=*/file->file_size,
            /*buf_size=*/kBufSize);
      }

      const std::string filename =
          (file->file_name.has_value() && !file->file_name->path().empty())
              ? (file->file_name->path().AsUTF8Unsafe())
              : file_system_url.path().BaseName().AsUTF8Unsafe();

      names.push_back(name);
      values.push_back(file_system_url.path().AsUTF8Unsafe());
      is_value_file_uris.push_back(true);
      filenames.push_back(filename);
      types.push_back(mime_type);
      data_pipe_getters.push_back(std::move(data_pipe_getter));
    }
  }

  std::vector<SharedField> shared_fields =
      ExtractSharedFields(share_target, intent);
  for (const auto& shared_field : shared_fields) {
    DCHECK(!shared_field.value.empty());
    names.push_back(shared_field.name);
    values.push_back(shared_field.value);
    is_value_file_uris.push_back(false);
    filenames.push_back(std::string());
    types.push_back("text/plain");
    data_pipe_getters.push_back(
        mojo::PendingRemote<network::mojom::DataPipeGetter>());
  }

  if (share_target.enctype == apps::ShareTarget::Enctype::kMultipartFormData) {
    const std::string boundary = net::GenerateMimeMultipartBoundary();
    nav_params.extra_headers = base::StringPrintf(
        "Content-Type: multipart/form-data; boundary=%s\r\n", boundary.c_str());
    nav_params.post_data = web_share_target::ComputeMultipartBody(
        names, values, is_value_file_uris, filenames, types,
        std::move(data_pipe_getters), boundary);
  } else {
    const std::string serialization =
        web_share_target::ComputeUrlEncodedBody(names, values);
    if (share_target.method == apps::ShareTarget::Method::kPost) {
      nav_params.extra_headers =
          "Content-Type: application/x-www-form-urlencoded\r\n";
      nav_params.post_data = network::ResourceRequestBody::CreateFromBytes(
          serialization.c_str(), serialization.length());
    } else {
      url::Replacements<char> replacements;
      replacements.SetQuery(serialization.c_str(),
                            url::Component(0, serialization.length()));
      nav_params.url = nav_params.url.ReplaceComponents(replacements);
    }
  }
#else
  // TODO(crbug.com/1153194): Support Web Share Target on Windows.
  // TODO(crbug.com/1153195): Support Web Share Target on Mac.
  NOTIMPLEMENTED();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return nav_params;
}

}  // namespace web_app
