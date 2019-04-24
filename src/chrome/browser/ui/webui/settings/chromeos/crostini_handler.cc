// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/crostini_handler.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/chromeos/crostini/crostini_export_import.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_share_path.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {
namespace settings {

CrostiniHandler::CrostiniHandler(Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {}

CrostiniHandler::~CrostiniHandler() {
  if (crostini::CrostiniManager::GetForProfile(profile_)
          ->HasInstallerViewStatusObserver(this)) {
    crostini::CrostiniManager::GetForProfile(profile_)
        ->RemoveInstallerViewStatusObserver(this);
  }
}

void CrostiniHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestCrostiniInstallerView",
      base::BindRepeating(&CrostiniHandler::HandleRequestCrostiniInstallerView,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "requestRemoveCrostini",
      base::BindRepeating(&CrostiniHandler::HandleRequestRemoveCrostini,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getCrostiniSharedPathsDisplayText",
      base::BindRepeating(
          &CrostiniHandler::HandleGetCrostiniSharedPathsDisplayText,
          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "removeCrostiniSharedPath",
      base::BindRepeating(&CrostiniHandler::HandleRemoveCrostiniSharedPath,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "exportCrostiniContainer",
      base::BindRepeating(&CrostiniHandler::HandleExportCrostiniContainer,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "importCrostiniContainer",
      base::BindRepeating(&CrostiniHandler::HandleImportCrostiniContainer,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "requestCrostiniInstallerStatus",
      base::BindRepeating(
          &CrostiniHandler::HandleCrostiniInstallerStatusRequest,
          weak_ptr_factory_.GetWeakPtr()));
  crostini::CrostiniManager::GetForProfile(profile_)
      ->AddInstallerViewStatusObserver(this);
}

void CrostiniHandler::HandleRequestCrostiniInstallerView(
    const base::ListValue* args) {
  AllowJavascript();
  ShowCrostiniInstallerView(Profile::FromWebUI(web_ui()),
                            crostini::CrostiniUISurface::kSettings);
}

void CrostiniHandler::HandleRequestRemoveCrostini(const base::ListValue* args) {
  AllowJavascript();
  ShowCrostiniUninstallerView(Profile::FromWebUI(web_ui()),
                              crostini::CrostiniUISurface::kSettings);
}

void CrostiniHandler::HandleGetCrostiniSharedPathsDisplayText(
    const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(2U, args->GetSize());
  std::string callback_id;
  const base::ListValue* paths;
  CHECK(args->GetString(0, &callback_id));
  CHECK(args->GetList(1, &paths));

  base::ListValue texts;
  for (auto it = paths->begin(); it != paths->end(); ++it) {
    texts.AppendString(file_manager::util::GetPathDisplayTextForSettings(
        profile_, it->GetString()));
  }
  ResolveJavascriptCallback(base::Value(callback_id), texts);
}

void CrostiniHandler::HandleRemoveCrostiniSharedPath(
    const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  std::string path;
  CHECK(args->GetString(0, &path));

  crostini::CrostiniSharePath::GetForProfile(profile_)->UnsharePath(
      crostini::kCrostiniDefaultVmName, base::FilePath(path),
      base::BindOnce(
          [](const std::string& path, bool result, std::string failure_reason) {
            if (!result) {
              LOG(ERROR) << "Error unsharing " << path << ": "
                         << failure_reason;
            }
          },
          path));
}

void CrostiniHandler::HandleExportCrostiniContainer(
    const base::ListValue* args) {
  CHECK_EQ(0U, args->GetSize());
  crostini::CrostiniExportImport::GetForProfile(profile_)->ExportContainer(
      web_ui()->GetWebContents());
}

void CrostiniHandler::HandleImportCrostiniContainer(
    const base::ListValue* args) {
  CHECK_EQ(0U, args->GetSize());
  crostini::CrostiniExportImport::GetForProfile(profile_)->ImportContainer(
      web_ui()->GetWebContents());
}

void CrostiniHandler::HandleCrostiniInstallerStatusRequest(
    const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(0U, args->GetSize());
  bool status = crostini::CrostiniManager::GetForProfile(profile_)
                    ->GetInstallerViewStatus();
  OnCrostiniInstallerViewStatusChanged(status);
}

void CrostiniHandler::OnCrostiniInstallerViewStatusChanged(bool status) {
  // It's technically possible for this to be called before Javascript is
  // enabled, in which case we must not call FireWebUIListener
  if (IsJavascriptAllowed()) {
    // Other side listens with cr.addWebUIListener
    FireWebUIListener("crostini-installer-status-changed", base::Value(status));
  }
}

}  // namespace settings
}  // namespace chromeos
