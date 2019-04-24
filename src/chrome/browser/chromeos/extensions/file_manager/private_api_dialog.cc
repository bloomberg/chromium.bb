// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_dialog.h"

#include <stddef.h>

#include "base/bind.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/select_file_dialog_extension.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "ui/shell_dialogs/selected_file_info.h"

using content::BrowserThread;

namespace extensions {

namespace {

// TODO(https://crbug.com/844654): This should be using something more
// deterministic.
content::WebContents* GetAssociatedWebContentsDeprecated(
    UIThreadExtensionFunction* function) {
  if (function->dispatcher()) {
    content::WebContents* web_contents =
        function->dispatcher()->GetAssociatedWebContents();
    if (web_contents)
      return web_contents;
  }

  Browser* browser =
      ChromeExtensionFunctionDetails(function).GetCurrentBrowser();
  if (!browser)
    return nullptr;
  return browser->tab_strip_model()->GetActiveWebContents();
}

// Computes the routing ID for SelectFileDialogExtension from the |function|.
SelectFileDialogExtension::RoutingID GetFileDialogRoutingID(
    UIThreadExtensionFunction* function) {
  return SelectFileDialogExtension::GetRoutingIDFromWebContents(
      GetAssociatedWebContentsDeprecated(function));
}

}  // namespace

ExtensionFunction::ResponseAction
FileManagerPrivateCancelDialogFunction::Run() {
  SelectFileDialogExtension::OnFileSelectionCanceled(
      GetFileDialogRoutingID(this));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction FileManagerPrivateSelectFileFunction::Run() {
  using extensions::api::file_manager_private::SelectFile::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<GURL> file_paths;
  file_paths.emplace_back(params->selected_path);

  file_manager::util::GetSelectedFileInfoLocalPathOption option =
      file_manager::util::NO_LOCAL_PATH_RESOLUTION;
  if (params->should_return_local_path) {
    option = params->for_opening ?
        file_manager::util::NEED_LOCAL_PATH_FOR_OPENING :
        file_manager::util::NEED_LOCAL_PATH_FOR_SAVING;
  }

  ChromeExtensionFunctionDetails chrome_details(this);
  file_manager::util::GetSelectedFileInfo(
      render_frame_host(), chrome_details.GetProfile(), file_paths, option,
      base::BindOnce(
          &FileManagerPrivateSelectFileFunction::GetSelectedFileInfoResponse,
          this, params->index));
  return RespondLater();
}

void FileManagerPrivateSelectFileFunction::GetSelectedFileInfoResponse(
    int index,
    const std::vector<ui::SelectedFileInfo>& files) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (files.size() != 1) {
    Respond(Error("No file selected"));
    return;
  }
  SelectFileDialogExtension::OnFileSelected(GetFileDialogRoutingID(this),
                                            files[0], index);
  Respond(NoArguments());
}

ExtensionFunction::ResponseAction FileManagerPrivateSelectFilesFunction::Run() {
  using extensions::api::file_manager_private::SelectFiles::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<GURL> file_urls;
  for (size_t i = 0; i < params->selected_paths.size(); ++i)
    file_urls.emplace_back(params->selected_paths[i]);

  ChromeExtensionFunctionDetails chrome_details(this);
  file_manager::util::GetSelectedFileInfo(
      render_frame_host(), chrome_details.GetProfile(), file_urls,
      params->should_return_local_path
          ? file_manager::util::NEED_LOCAL_PATH_FOR_OPENING
          : file_manager::util::NO_LOCAL_PATH_RESOLUTION,
      base::BindOnce(
          &FileManagerPrivateSelectFilesFunction::GetSelectedFileInfoResponse,
          this));
  return RespondLater();
}

void FileManagerPrivateSelectFilesFunction::GetSelectedFileInfoResponse(
    const std::vector<ui::SelectedFileInfo>& files) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (files.empty()) {
    Respond(Error("No files selected"));
    return;
  }

  SelectFileDialogExtension::OnMultiFilesSelected(GetFileDialogRoutingID(this),
                                                  files);
  Respond(NoArguments());
}

}  // namespace extensions
