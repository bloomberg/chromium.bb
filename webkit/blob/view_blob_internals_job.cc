// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/blob/view_blob_internals_job.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"
#include "webkit/blob/blob_data.h"
#include "webkit/blob/blob_storage_controller.h"

namespace {

const char kEmptyBlobStorageMessage[] = "No available blob data.";
const char kRemove[] = "Remove";
const char kContentType[] = "Content Type: ";
const char kContentDisposition[] = "Content Disposition: ";
const char kCount[] = "Count: ";
const char kIndex[] = "Index: ";
const char kType[] = "Type: ";
const char kPath[] = "Path: ";
const char kURL[] = "URL: ";
const char kModificationTime[] = "Modification Time: ";
const char kOffset[] = "Offset: ";
const char kLength[] = "Length: ";

void StartHTML(std::string* out) {
  out->append(
      "<!DOCTYPE HTML>"
      "<html><title>Blob Storage Internals</title>"
      "<meta http-equiv=\"Content-Security-Policy\""
      "  content=\"object-src 'none'; script-src 'none'\">\n"
      "<style>\n"
      "body { font-family: sans-serif; font-size: 0.8em; }\n"
      "tt, code, pre { font-family: WebKitHack, monospace; }\n"
      "form { display: inline }\n"
      ".subsection_body { margin: 10px 0 10px 2em; }\n"
      ".subsection_title { font-weight: bold; }\n"
      "</style>\n"
      "</head><body>\n");
}

void EndHTML(std::string* out) {
  out->append("</body></html>");
}

void AddHTMLBoldText(const std::string& text, std::string* out) {
  out->append("<b>");
  out->append(net::EscapeForHTML(text));
  out->append("</b>");
}

void StartHTMLList(std::string* out) {
  out->append("<ul>");
}

void EndHTMLList(std::string* out) {
  out->append("</ul>");
}

void AddHTMLListItem(const std::string& element_title,
                     const std::string& element_data,
                     std::string* out) {
  out->append("<li>");
  // No need to escape element_title since constant string is passed.
  out->append(element_title);
  out->append(net::EscapeForHTML(element_data));
  out->append("</li>");
}

void AddHTMLButton(const std::string& title,
                   const std::string& command,
                   std::string* out) {
  // No need to escape title since constant string is passed.
  std::string escaped_command = net::EscapeForHTML(command.c_str());
  base::StringAppendF(out,
                      "<form action=\"\" method=\"GET\">\n"
                      "<input type=\"hidden\" name=\"remove\" value=\"%s\">\n"
                      "<input type=\"submit\" value=\"%s\">\n"
                      "</form><br/>\n",
                      escaped_command.c_str(),
                      title.c_str());
}

}  // namespace

namespace webkit_blob {

ViewBlobInternalsJob::ViewBlobInternalsJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    BlobStorageController* blob_storage_controller)
    : net::URLRequestSimpleJob(request, network_delegate),
      blob_storage_controller_(blob_storage_controller),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

ViewBlobInternalsJob::~ViewBlobInternalsJob() {
}

void ViewBlobInternalsJob::Start() {
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&ViewBlobInternalsJob::DoWorkAsync,
                            weak_factory_.GetWeakPtr()));
}

bool ViewBlobInternalsJob::IsRedirectResponse(GURL* location,
                                              int* http_status_code) {
  if (request_->url().has_query()) {
    // Strip the query parameters.
    GURL::Replacements replacements;
    replacements.ClearQuery();
    *location = request_->url().ReplaceComponents(replacements);
    *http_status_code = 307;
    return true;
  }
  return false;
}

void ViewBlobInternalsJob::Kill() {
  net::URLRequestSimpleJob::Kill();
  weak_factory_.InvalidateWeakPtrs();
}

void ViewBlobInternalsJob::DoWorkAsync() {
  if (request_->url().has_query() &&
      StartsWithASCII(request_->url().query(), "remove=", true)) {
    std::string blob_url = request_->url().query().substr(strlen("remove="));
    blob_url = net::UnescapeURLComponent(blob_url,
        net::UnescapeRule::NORMAL | net::UnescapeRule::URL_SPECIAL_CHARS);
    blob_storage_controller_->RemoveBlob(GURL(blob_url));
  }

  StartAsync();
}

int ViewBlobInternalsJob::GetData(
    std::string* mime_type,
    std::string* charset,
    std::string* data,
    const net::CompletionCallback& callback) const {
  mime_type->assign("text/html");
  charset->assign("UTF-8");

  data->clear();
  StartHTML(data);
  if (blob_storage_controller_->blob_map_.empty())
    data->append(kEmptyBlobStorageMessage);
  else
    GenerateHTML(data);
  EndHTML(data);
  return net::OK;
}

void ViewBlobInternalsJob::GenerateHTML(std::string* out) const {
  for (BlobStorageController::BlobMap::const_iterator iter =
           blob_storage_controller_->blob_map_.begin();
       iter != blob_storage_controller_->blob_map_.end();
       ++iter) {
    AddHTMLBoldText(iter->first, out);
    AddHTMLButton(kRemove, iter->first, out);
    GenerateHTMLForBlobData(*iter->second, out);
  }
}

void ViewBlobInternalsJob::GenerateHTMLForBlobData(const BlobData& blob_data,
                                                   std::string* out) {
  StartHTMLList(out);

  if (!blob_data.content_type().empty())
    AddHTMLListItem(kContentType, blob_data.content_type(), out);
  if (!blob_data.content_disposition().empty())
    AddHTMLListItem(kContentDisposition, blob_data.content_disposition(), out);

  bool has_multi_items = blob_data.items().size() > 1;
  if (has_multi_items) {
    AddHTMLListItem(kCount,
        UTF16ToUTF8(base::FormatNumber(blob_data.items().size())), out);
  }

  for (size_t i = 0; i < blob_data.items().size(); ++i) {
    if (has_multi_items) {
      AddHTMLListItem(kIndex, UTF16ToUTF8(base::FormatNumber(i)), out);
      StartHTMLList(out);
    }
    const BlobData::Item& item = blob_data.items().at(i);

    switch (item.type()) {
      case BlobData::Item::TYPE_BYTES:
        AddHTMLListItem(kType, "data", out);
        break;
      case BlobData::Item::TYPE_FILE:
        AddHTMLListItem(kType, "file", out);
        AddHTMLListItem(kPath,
                 net::EscapeForHTML(item.path().AsUTF8Unsafe()),
                 out);
        if (!item.expected_modification_time().is_null()) {
          AddHTMLListItem(kModificationTime, UTF16ToUTF8(
              TimeFormatFriendlyDateAndTime(item.expected_modification_time())),
              out);
        }
        break;
      case BlobData::Item::TYPE_BLOB:
        AddHTMLListItem(kType, "blob", out);
        AddHTMLListItem(kURL, item.url().spec(), out);
        break;
      case BlobData::Item::TYPE_FILE_FILESYSTEM:
        AddHTMLListItem(kType, "filesystem", out);
        AddHTMLListItem(kURL, item.url().spec(), out);
        if (!item.expected_modification_time().is_null()) {
          AddHTMLListItem(kModificationTime, UTF16ToUTF8(
              TimeFormatFriendlyDateAndTime(item.expected_modification_time())),
              out);
        }
        break;
      case BlobData::Item::TYPE_UNKNOWN:
        NOTREACHED();
        break;
    }
    if (item.offset()) {
      AddHTMLListItem(kOffset, UTF16ToUTF8(base::FormatNumber(
          static_cast<int64>(item.offset()))), out);
    }
    if (static_cast<int64>(item.length()) != -1) {
      AddHTMLListItem(kLength, UTF16ToUTF8(base::FormatNumber(
          static_cast<int64>(item.length()))), out);
    }

    if (has_multi_items)
      EndHTMLList(out);
  }

  EndHTMLList(out);
}

}  // namespace webkit_blob
