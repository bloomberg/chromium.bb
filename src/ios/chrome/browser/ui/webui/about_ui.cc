// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/webui/about_ui.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/format_macros.h"
#include "base/i18n/number_formatting.h"
#include "base/macros.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/string_number_conversions.h"
#include "components/grit/components_resources.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/web/public/url_data_source_ios.h"
#include "net/base/escape.h"
#include "third_party/brotli/include/brotli/decode.h"
#include "ui/base/device_form_factor.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace {

const char kCreditsJsPath[] = "credits.js";
const char kStringsJsPath[] = "strings.js";

class AboutUIHTMLSource : public web::URLDataSourceIOS {
 public:
  // Construct a data source for the specified |source_name|.
  explicit AboutUIHTMLSource(const std::string& source_name);

  // web::URLDataSourceIOS implementation.
  std::string GetSource() const override;
  void StartDataRequest(
      const std::string& path,
      const web::URLDataSourceIOS::GotDataCallback& callback) override;
  std::string GetMimeType(const std::string& path) const override;
  bool ShouldDenyXFrameOptions() const override;

  // Send the response data.
  void FinishDataRequest(
      const std::string& html,
      const web::URLDataSourceIOS::GotDataCallback& callback);

 private:
  ~AboutUIHTMLSource() override;

  std::string source_name_;

  DISALLOW_COPY_AND_ASSIGN(AboutUIHTMLSource);
};

void AppendHeader(std::string* output,
                  int refresh,
                  const std::string& unescaped_title) {
  output->append("<!DOCTYPE HTML>\n<html>\n<head>\n");
  if (!unescaped_title.empty()) {
    output->append("<title>");
    output->append(net::EscapeForHTML(unescaped_title));
    output->append("</title>\n");
  }
  output->append("<meta charset='utf-8'>\n");
  if (refresh > 0) {
    output->append("<meta http-equiv='refresh' content='");
    output->append(base::IntToString(refresh));
    output->append("'/>\n");
  }
}

void AppendBody(std::string* output) {
  output->append("</head>\n<body>\n");
}

void AppendFooter(std::string* output) {
  output->append("</body>\n</html>\n");
}

std::string ChromeURLs() {
  std::string html;
  AppendHeader(&html, 0, "Chrome URLs");
  AppendBody(&html);
  html += "<h2>List of Chrome URLs</h2>\n<ul>\n";
  std::vector<std::string> hosts(kChromeHostURLs,
                                 kChromeHostURLs + kNumberOfChromeHostURLs);
  std::sort(hosts.begin(), hosts.end());
  for (std::vector<std::string>::const_iterator i = hosts.begin();
       i != hosts.end(); ++i)
    html += "<li><a href='chrome://" + *i + "/' id='" + *i + "'>chrome://" +
            *i + "</a></li>\n";
  html += "</ul>\n";
  AppendFooter(&html);
  return html;
}

}  // namespace

// AboutUIHTMLSource ----------------------------------------------------------

AboutUIHTMLSource::AboutUIHTMLSource(const std::string& source_name)
    : source_name_(source_name) {}

AboutUIHTMLSource::~AboutUIHTMLSource() {}

std::string AboutUIHTMLSource::GetSource() const {
  return source_name_;
}

void AboutUIHTMLSource::StartDataRequest(
    const std::string& path,
    const web::URLDataSourceIOS::GotDataCallback& callback) {
  std::string response;
  // Add your data source here, in alphabetical order.
  if (source_name_ == kChromeUIChromeURLsHost) {
    response = ChromeURLs();
  } else if (source_name_ == kChromeUICreditsHost) {
    int idr = IDR_ABOUT_UI_CREDITS_HTML;
    if (path == kCreditsJsPath)
      idr = IDR_ABOUT_UI_CREDITS_JS;
    base::StringPiece raw_response =
        ui::ResourceBundle::GetSharedInstance().GetRawDataResource(idr);
    if (idr == IDR_ABOUT_UI_CREDITS_HTML) {
      const uint8_t* next_encoded_byte =
          reinterpret_cast<const uint8_t*>(raw_response.data());
      size_t input_size_remaining = raw_response.size();
      BrotliDecoderState* decoder =
          BrotliDecoderCreateInstance(nullptr /* no custom allocator */,
                                      nullptr /* no custom deallocator */,
                                      nullptr /* no custom memory handle */);
      CHECK(!!decoder);
      while (!BrotliDecoderIsFinished(decoder)) {
        size_t output_size_remaining = 0;
        CHECK(BrotliDecoderDecompressStream(
                  decoder, &input_size_remaining, &next_encoded_byte,
                  &output_size_remaining, nullptr,
                  nullptr) != BROTLI_DECODER_RESULT_ERROR);
        const uint8_t* output_buffer =
            BrotliDecoderTakeOutput(decoder, &output_size_remaining);
        response.insert(response.end(), output_buffer,
                        output_buffer + output_size_remaining);
      }
      BrotliDecoderDestroyInstance(decoder);
    } else {
      response = raw_response.as_string();
    }
  } else if (source_name_ == kChromeUIHistogramHost) {
    // Note: On other platforms, this is implemented in //content. If there is
    // ever a need for embedders other than //ios/chrome to use
    // chrome://histograms, this code could likely be moved to //io/web.
    base::StatisticsRecorder::WriteHTMLGraph("", &response);
  }

  FinishDataRequest(response, callback);
}

void AboutUIHTMLSource::FinishDataRequest(
    const std::string& html,
    const web::URLDataSourceIOS::GotDataCallback& callback) {
  std::string html_copy(html);
  callback.Run(base::RefCountedString::TakeString(&html_copy));
}

std::string AboutUIHTMLSource::GetMimeType(const std::string& path) const {
  if (path == kCreditsJsPath || path == kStringsJsPath) {
    return "application/javascript";
  }
  return "text/html";
}

bool AboutUIHTMLSource::ShouldDenyXFrameOptions() const {
  return web::URLDataSourceIOS::ShouldDenyXFrameOptions();
}

AboutUI::AboutUI(web::WebUIIOS* web_ui, const std::string& name)
    : web::WebUIIOSController(web_ui) {
  web::URLDataSourceIOS::Add(ios::ChromeBrowserState::FromWebUIIOS(web_ui),
                             new AboutUIHTMLSource(name));
}

AboutUI::~AboutUI() {}
