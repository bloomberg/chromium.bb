// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/grpc_resource_data_source.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/base/cast_constants.h"
#include "net/base/mime_util.h"

namespace chromecast {

namespace {

// File extension types
constexpr const char kExtensionTypeCss[] = ".css";
constexpr const char kExtensionTypeJs[] = ".js";
constexpr const char kExtensionTypeJson[] = ".json";
constexpr const char kExtensionTypePdf[] = ".pdf";
constexpr const char kExtensionTypeSvg[] = ".svg";
constexpr const char kExtensionTypePng[] = ".png";
constexpr const char kExtensionTypeJpeg[] = ".jpeg";
constexpr const char kExtensionTypeHtml[] = ".html";

// Mime types of the resource requested by the Cast app's WebUI.
constexpr const char kMimeTypeHtml[] = "text/html";
constexpr const char kMimeTypeCss[] = "text/css";
constexpr const char kMimeTypeJavascript[] = "application/javascript";
constexpr const char kMimeTypeJson[] = "application/json";
constexpr const char kMimeTypePdf[] = "application/pdf";
constexpr const char kMimeTypeSvgXml[] = "image/svg+xml";
constexpr const char kMimeTypeJpeg[] = "image/jpeg";
constexpr const char kMimeTypePng[] = "image/png";
constexpr const char kAllowedOriginPrefix[] = "chrome://";

}  // namespace

GrpcResourceDataSource::GrpcResourceDataSource(
    const std::string host,
    bool for_webui,
    cast::v2::CoreApplicationService::Stub core_app_service_stub)
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      host_(host),
      for_webui_(for_webui),
      core_app_service_stub_(std::move(core_app_service_stub)) {
  DCHECK(!host.empty());
}

GrpcResourceDataSource::~GrpcResourceDataSource() = default;

std::string GrpcResourceDataSource::GetSource() {
  return host_;
}

void GrpcResourceDataSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  LOG(INFO) << "Starting Data request for " << url;
  std::string path = content::URLDataSource::URLToRequestPath(url);
  std::string contents;

  // TODO(b/195894244): Replace this with gRPC framework from
  // chromecast/cast_core.
  grpc::Status status;
  cast::v2::GetWebUIResourceResponse response;
  {
    grpc::ClientContext context;
    cast::v2::GetWebUIResourceRequest request;
    request.set_resource_id(path);
    status =
        core_app_service_stub_.GetWebUIResource(&context, request, &response);
    contents = response.resource_path();
  }

  if (!status.ok()) {
    LOG(ERROR) << "Failed to receive resource path response";
    std::move(callback).Run(nullptr);
    return;
  }

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GrpcResourceDataSource::ReadResourceFile,
                                weak_ptr_factory_.GetWeakPtr(), contents,
                                std::move(callback)));
}

void GrpcResourceDataSource::ReadResourceFile(
    base::StringPiece resource_file_path,
    content::URLDataSource::GotDataCallback callback) {
  base::FilePath path(resource_file_path);
  if (!base::PathExists(path)) {
    LOG(ERROR) << "Resource " << resource_file_path << " does not exist";
    std::move(callback).Run(nullptr);
    return;
  }

  std::string text;
  base::ReadFileToString(base::FilePath(resource_file_path), &text);
  std::move(callback).Run(base::RefCountedString::TakeString(&text));
}

// The Path can either be a filename or a remote url string starting with "?".
// Examples - "?remote_url=https://google.com", "fonts.css".
std::string GrpcResourceDataSource::GetMimeType(const std::string& path) {
  if (!for_webui_) {
    std::string mime_type;
    base::FilePath::StringType file_ext =
        base::FilePath().AppendASCII(path).Extension();
    if (!file_ext.empty())
      net::GetWellKnownMimeTypeFromExtension(file_ext.substr(1), &mime_type);
    return mime_type;
  }

  // If the path starts with ? or if the path does not contain an extension,
  // return default MimeType.
  auto extension_index = path.find_last_of(".");
  if (path.find("?") != std::string::npos ||
      extension_index == std::string::npos) {
    return kMimeTypeHtml;
  }

  base::FilePath file_path(path);
  auto extension = file_path.Extension();
  if (extension.empty()) {
    return kMimeTypeHtml;
  }

  if (base::EqualsCaseInsensitiveASCII(extension, kExtensionTypeCss)) {
    return kMimeTypeCss;
  }
  if (base::EqualsCaseInsensitiveASCII(extension, kExtensionTypeJs)) {
    return kMimeTypeJavascript;
  }
  if (base::EqualsCaseInsensitiveASCII(extension, kExtensionTypeJson)) {
    return kMimeTypeJson;
  }
  if (base::EqualsCaseInsensitiveASCII(extension, kExtensionTypePdf)) {
    return kMimeTypePdf;
  }
  if (base::EqualsCaseInsensitiveASCII(extension, kExtensionTypeSvg)) {
    return kMimeTypeSvgXml;
  }
  if (base::EqualsCaseInsensitiveASCII(extension, kExtensionTypeJpeg)) {
    return kMimeTypeJpeg;
  }
  if (base::EqualsCaseInsensitiveASCII(extension, kExtensionTypePng)) {
    return kMimeTypePng;
  }
  if (base::EqualsCaseInsensitiveASCII(extension, kExtensionTypeHtml)) {
    return kMimeTypeHtml;
  }

  NOTREACHED() << "Unknown Mime type of file " << path;
  return "";
}

bool GrpcResourceDataSource::ShouldServiceRequest(
    const GURL& url,
    content::BrowserContext* browser_context,
    int render_process_id) {
  if (url.SchemeIs(kChromeResourceScheme)) {
    return true;
  }
  return URLDataSource::ShouldServiceRequest(url, browser_context,
                                             render_process_id);
}

std::string GrpcResourceDataSource::GetAccessControlAllowOriginForOrigin(
    const std::string& origin) {
  if (!base::StartsWith(origin, kAllowedOriginPrefix,
                        base::CompareCase::SENSITIVE)) {
    return "";
  }
  return origin;
}

void GrpcResourceDataSource::OverrideContentSecurityPolicyChildSrc(
    const std::string& data) {
  frame_src_ = data;
}

void GrpcResourceDataSource::DisableDenyXFrameOptions() {
  deny_xframe_options_ = false;
}

bool GrpcResourceDataSource::ShouldDenyXFrameOptions() {
  return deny_xframe_options_;
}

}  // namespace chromecast
