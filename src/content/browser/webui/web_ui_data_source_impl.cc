// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/web_ui_data_source_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "content/grit/content_resources.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/template_expressions.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/resource_path.h"
#include "ui/base/webui/web_ui_util.h"

namespace content {

// static
WebUIDataSource* WebUIDataSource::Create(const std::string& source_name) {
  return new WebUIDataSourceImpl(source_name);
}

// static
void WebUIDataSource::Add(BrowserContext* browser_context,
                          WebUIDataSource* source) {
  URLDataManager::AddWebUIDataSource(browser_context, source);
}

// static
void WebUIDataSource::Update(BrowserContext* browser_context,
                             const std::string& source_name,
                             std::unique_ptr<base::DictionaryValue> update) {
  URLDataManager::UpdateWebUIDataSource(browser_context, source_name,
                                        std::move(update));
}

namespace {

void GetDataResourceBytesOnWorkerThread(
    int resource_id,
    URLDataSource::GotDataCallback callback) {
  base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN, base::MayBlock()})
      ->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](int resource_id, URLDataSource::GotDataCallback callback) {
                ContentClient* content_client = GetContentClient();
                DCHECK(content_client);
                std::move(callback).Run(
                    content_client->GetDataResourceBytes(resource_id));
              },
              resource_id, std::move(callback)));
}

std::string CleanUpPath(const std::string& path) {
  // Remove the query string for named resource lookups.
  std::string clean_path = path.substr(0, path.find_first_of('?'));
  // Remove a URL fragment (for example #foo) if it exists.
  clean_path = clean_path.substr(0, path.find_first_of('#'));

  return clean_path;
}

const int kNonExistentResource = -1;

}  // namespace

// Internal class to hide the fact that WebUIDataSourceImpl implements
// URLDataSource.
class WebUIDataSourceImpl::InternalDataSource : public URLDataSource {
 public:
  explicit InternalDataSource(WebUIDataSourceImpl* parent) : parent_(parent) {}

  ~InternalDataSource() override {}

  // URLDataSource implementation.
  std::string GetSource() override { return parent_->GetSource(); }
  std::string GetMimeType(const std::string& path) override {
    return parent_->GetMimeType(path);
  }
  void StartDataRequest(const GURL& url,
                        const WebContents::Getter& wc_getter,
                        URLDataSource::GotDataCallback callback) override {
    return parent_->StartDataRequest(url, wc_getter, std::move(callback));
  }
  bool ShouldReplaceExistingSource() override {
    return parent_->replace_existing_source_;
  }
  bool AllowCaching() override { return false; }
  bool ShouldAddContentSecurityPolicy() override { return parent_->add_csp_; }
  std::string GetContentSecurityPolicy(
      network::mojom::CSPDirectiveName directive) override {
    if (parent_->csp_overrides_.contains(directive)) {
      return parent_->csp_overrides_.at(directive);
    } else if (directive == network::mojom::CSPDirectiveName::FrameAncestors) {
      std::string frame_ancestors;
      if (parent_->frame_ancestors_.size() == 0)
        frame_ancestors += " 'none'";
      for (const GURL& frame_ancestor : parent_->frame_ancestors_) {
        frame_ancestors += " " + frame_ancestor.spec();
      }
      return "frame-ancestors" + frame_ancestors + ";";
    }

    return URLDataSource::GetContentSecurityPolicy(directive);
  }
  std::string GetCrossOriginOpenerPolicy() override {
    return parent_->coop_value_;
  }
  std::string GetCrossOriginEmbedderPolicy() override {
    return parent_->coep_value_;
  }
  std::string GetCrossOriginResourcePolicy() override {
    return parent_->corp_value_;
  }
  bool ShouldDenyXFrameOptions() override {
    return parent_->deny_xframe_options_;
  }
  bool ShouldServeMimeTypeAsContentTypeHeader() override { return true; }
  const ui::TemplateReplacements* GetReplacements() override {
    return &parent_->replacements_;
  }
  bool ShouldReplaceI18nInJS() override {
    return parent_->ShouldReplaceI18nInJS();
  }

 private:
  raw_ptr<WebUIDataSourceImpl> parent_;
};

WebUIDataSourceImpl::WebUIDataSourceImpl(const std::string& source_name)
    : URLDataSourceImpl(source_name,
                        std::make_unique<InternalDataSource>(this)),
      source_name_(source_name),
      default_resource_(kNonExistentResource) {}

WebUIDataSourceImpl::~WebUIDataSourceImpl() = default;

void WebUIDataSourceImpl::AddString(base::StringPiece name,
                                    const std::u16string& value) {
  // TODO(dschuyler): Share only one copy of these strings.
  localized_strings_.SetKey(name, base::Value(value));
  replacements_[std::string(name)] = base::UTF16ToUTF8(value);
}

void WebUIDataSourceImpl::AddString(base::StringPiece name,
                                    const std::string& value) {
  localized_strings_.SetKey(name, base::Value(value));
  replacements_[std::string(name)] = value;
}

void WebUIDataSourceImpl::AddLocalizedString(base::StringPiece name, int ids) {
  std::string utf8_str =
      base::UTF16ToUTF8(GetContentClient()->GetLocalizedString(ids));
  localized_strings_.SetKey(name, base::Value(utf8_str));
  replacements_[std::string(name)] = utf8_str;
}

void WebUIDataSourceImpl::AddLocalizedStrings(
    base::span<const webui::LocalizedString> strings) {
  for (const auto& str : strings)
    AddLocalizedString(str.name, str.id);
}

void WebUIDataSourceImpl::AddLocalizedStrings(
    const base::DictionaryValue& localized_strings) {
  localized_strings_.MergeDictionary(&localized_strings);
  ui::TemplateReplacementsFromDictionaryValue(localized_strings,
                                              &replacements_);
}

void WebUIDataSourceImpl::AddBoolean(base::StringPiece name, bool value) {
  localized_strings_.SetBoolean(name, value);
  // TODO(dschuyler): Change name of |localized_strings_| to |load_time_data_|
  // or similar. These values haven't been found as strings for
  // localization. The boolean values are not added to |replacements_|
  // for the same reason, that they are used as flags, rather than string
  // replacements.
}

void WebUIDataSourceImpl::AddInteger(base::StringPiece name, int32_t value) {
  localized_strings_.SetInteger(name, value);
}

void WebUIDataSourceImpl::AddDouble(base::StringPiece name, double value) {
  localized_strings_.SetDouble(name, value);
}

void WebUIDataSourceImpl::UseStringsJs() {
  use_strings_js_ = true;
}

void WebUIDataSourceImpl::AddResourcePath(base::StringPiece path,
                                          int resource_id) {
  path_to_idr_map_[std::string(path)] = resource_id;
}

void WebUIDataSourceImpl::AddResourcePaths(
    base::span<const webui::ResourcePath> paths) {
  for (const auto& path : paths)
    AddResourcePath(path.path, path.id);
}

void WebUIDataSourceImpl::SetDefaultResource(int resource_id) {
  default_resource_ = resource_id;
}

void WebUIDataSourceImpl::SetRequestFilter(
    const ShouldHandleRequestCallback& should_handle_request_callback,
    const HandleRequestCallback& handle_request_callback) {
  CHECK(!should_handle_request_callback_);
  CHECK(!filter_callback_);
  should_handle_request_callback_ = should_handle_request_callback;
  filter_callback_ = handle_request_callback;
}

void WebUIDataSourceImpl::DisableReplaceExistingSource() {
  replace_existing_source_ = false;
}

bool WebUIDataSourceImpl::IsWebUIDataSourceImpl() const {
  return true;
}

void WebUIDataSourceImpl::DisableContentSecurityPolicy() {
  add_csp_ = false;
}

void WebUIDataSourceImpl::OverrideContentSecurityPolicy(
    network::mojom::CSPDirectiveName directive,
    const std::string& value) {
  csp_overrides_.insert_or_assign(directive, value);
}

void WebUIDataSourceImpl::OverrideCrossOriginOpenerPolicy(
    const std::string& value) {
  coop_value_ = value;
}

void WebUIDataSourceImpl::OverrideCrossOriginEmbedderPolicy(
    const std::string& value) {
  coep_value_ = value;
}

void WebUIDataSourceImpl::OverrideCrossOriginResourcePolicy(
    const std::string& value) {
  corp_value_ = value;
}

void WebUIDataSourceImpl::DisableTrustedTypesCSP() {
  // TODO(crbug.com/1098685): Trusted Type remaining WebUI
  // This removes require-trusted-types-for and trusted-types directives
  // from the CSP header.
  OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::RequireTrustedTypesFor, std::string());
  OverrideContentSecurityPolicy(network::mojom::CSPDirectiveName::TrustedTypes,
                                std::string());
}

void WebUIDataSourceImpl::AddFrameAncestor(const GURL& frame_ancestor) {
  // Do not allow a wildcard to be a frame ancestor or it will allow any website
  // to embed the WebUI.
  CHECK(frame_ancestor.SchemeIs(kChromeUIScheme) ||
        frame_ancestor.SchemeIs(kChromeUIUntrustedScheme));
  frame_ancestors_.insert(frame_ancestor);
}

void WebUIDataSourceImpl::DisableDenyXFrameOptions() {
  deny_xframe_options_ = false;
}

void WebUIDataSourceImpl::EnableReplaceI18nInJS() {
  should_replace_i18n_in_js_ = true;
}

void WebUIDataSourceImpl::EnsureLoadTimeDataDefaultsAdded() {
  if (!add_load_time_data_defaults_)
    return;

  add_load_time_data_defaults_ = false;
  std::string locale = GetContentClient()->browser()->GetApplicationLocale();
  base::DictionaryValue defaults;
  webui::SetLoadTimeDataDefaults(locale, &defaults);
  AddLocalizedStrings(defaults);
}

std::string WebUIDataSourceImpl::GetSource() {
  return source_name_;
}

std::string WebUIDataSourceImpl::GetMimeType(const std::string& path) const {
  std::string file_path = CleanUpPath(path);

  if (base::EndsWith(file_path, ".css", base::CompareCase::INSENSITIVE_ASCII))
    return "text/css";

  if (base::EndsWith(file_path, ".js", base::CompareCase::INSENSITIVE_ASCII))
    return "application/javascript";

  if (base::EndsWith(file_path, ".json", base::CompareCase::INSENSITIVE_ASCII))
    return "application/json";

  if (base::EndsWith(file_path, ".pdf", base::CompareCase::INSENSITIVE_ASCII))
    return "application/pdf";

  if (base::EndsWith(file_path, ".svg", base::CompareCase::INSENSITIVE_ASCII))
    return "image/svg+xml";

  if (base::EndsWith(file_path, ".jpg", base::CompareCase::INSENSITIVE_ASCII))
    return "image/jpeg";

  if (base::EndsWith(file_path, ".png", base::CompareCase::INSENSITIVE_ASCII))
    return "image/png";

  if (base::EndsWith(file_path, ".mp4", base::CompareCase::INSENSITIVE_ASCII))
    return "video/mp4";

  if (base::EndsWith(file_path, ".wasm", base::CompareCase::INSENSITIVE_ASCII))
    return "application/wasm";

  if (base::EndsWith(file_path, ".woff2", base::CompareCase::INSENSITIVE_ASCII))
    return "application/font-woff2";

  return "text/html";
}

void WebUIDataSourceImpl::StartDataRequest(
    const GURL& url,
    const WebContents::Getter& wc_getter,
    URLDataSource::GotDataCallback callback) {
  const std::string path = URLDataSource::URLToRequestPath(url);
  TRACE_EVENT1("ui", "WebUIDataSourceImpl::StartDataRequest", "path", path);

  if (!should_handle_request_callback_.is_null() &&
      should_handle_request_callback_.Run(path)) {
    filter_callback_.Run(path, std::move(callback));
    return;
  }

  EnsureLoadTimeDataDefaultsAdded();

  if (use_strings_js_) {
    bool from_js_module = path == "strings.m.js";
    if (from_js_module || path == "strings.js") {
      SendLocalizedStringsAsJSON(std::move(callback), from_js_module);
      return;
    }
  }

  int resource_id = PathToIdrOrDefault(CleanUpPath(path));
  if (resource_id == kNonExistentResource) {
    std::move(callback).Run(nullptr);
  } else {
    GetDataResourceBytesOnWorkerThread(resource_id, std::move(callback));
  }
}

void WebUIDataSourceImpl::SendLocalizedStringsAsJSON(
    URLDataSource::GotDataCallback callback,
    bool from_js_module) {
  std::string template_data;
  webui::AppendJsonJS(&localized_strings_, &template_data, from_js_module);
  std::move(callback).Run(base::RefCountedString::TakeString(&template_data));
}

const base::DictionaryValue* WebUIDataSourceImpl::GetLocalizedStrings() const {
  return &localized_strings_;
}

bool WebUIDataSourceImpl::ShouldReplaceI18nInJS() const {
  return should_replace_i18n_in_js_;
}

int WebUIDataSourceImpl::PathToIdrOrDefault(const std::string& path) const {
  auto it = path_to_idr_map_.find(path);
  if (it != path_to_idr_map_.end())
    return it->second;

  if (default_resource_ != kNonExistentResource)
    return default_resource_;

  // Use GetMimeType() to check for most file requests. It returns text/html by
  // default regardless of the extension if it does not match a different file
  // type, so check for HTML file requests separately.
  if (GetMimeType(path) != "text/html" ||
      base::EndsWith(path, ".html", base::CompareCase::INSENSITIVE_ASCII)) {
    return kNonExistentResource;
  }

  // If not a file request, try to get the resource for the empty key.
  auto it_empty = path_to_idr_map_.find("");
  return (it_empty != path_to_idr_map_.end()) ? it_empty->second
                                              : kNonExistentResource;
}

}  // namespace content
