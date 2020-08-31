// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/certificate_viewer_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/certificate_viewer_webui.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace {

content::WebUIDataSource* GetWebUIDataSource(const std::string& host) {
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(host);

  static constexpr webui::LocalizedString kStrings[] = {
      {"general", IDS_CERT_INFO_GENERAL_TAB_LABEL},
      {"details", IDS_CERT_INFO_DETAILS_TAB_LABEL},
      {"close", IDS_CLOSE},
      {"export", IDS_CERT_DETAILS_EXPORT_CERTIFICATE},
      {"usages", IDS_CERT_INFO_VERIFIED_USAGES_GROUP},
      {"issuedTo", IDS_CERT_INFO_SUBJECT_GROUP},
      {"issuedBy", IDS_CERT_INFO_ISSUER_GROUP},
      {"cn", IDS_CERT_INFO_COMMON_NAME_LABEL},
      {"o", IDS_CERT_INFO_ORGANIZATION_LABEL},
      {"ou", IDS_CERT_INFO_ORGANIZATIONAL_UNIT_LABEL},
      {"validity", IDS_CERT_INFO_VALIDITY_GROUP},
      {"issuedOn", IDS_CERT_INFO_ISSUED_ON_LABEL},
      {"expiresOn", IDS_CERT_INFO_EXPIRES_ON_LABEL},
      {"fingerprints", IDS_CERT_INFO_FINGERPRINTS_GROUP},
      {"sha256", IDS_CERT_INFO_SHA256_FINGERPRINT_LABEL},
      {"sha1", IDS_CERT_INFO_SHA1_FINGERPRINT_LABEL},
      {"hierarchy", IDS_CERT_DETAILS_CERTIFICATE_HIERARCHY_LABEL},
      {"certFields", IDS_CERT_DETAILS_CERTIFICATE_FIELDS_LABEL},
      {"certFieldVal", IDS_CERT_DETAILS_CERTIFICATE_FIELD_VALUE_LABEL},
  };
  AddLocalizedStringsBulk(html_source, kStrings);

  html_source->UseStringsJs();

  // Add required resources.
  html_source->AddResourcePath("certificate_viewer.js",
      IDR_CERTIFICATE_VIEWER_JS);
  html_source->AddResourcePath("certificate_viewer.css",
      IDR_CERTIFICATE_VIEWER_CSS);
  html_source->SetDefaultResource(IDR_CERTIFICATE_VIEWER_HTML);
  return html_source;
}

}  // namespace

CertificateViewerUI::CertificateViewerUI(content::WebUI* web_ui)
    : ConstrainedWebDialogUI(web_ui) {
  // Set up the chrome://view-cert source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(
      profile,
      GetWebUIDataSource(chrome::kChromeUICertificateViewerHost));
}

CertificateViewerUI::~CertificateViewerUI() {
}
