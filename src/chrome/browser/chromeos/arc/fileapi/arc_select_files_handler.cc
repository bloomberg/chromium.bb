// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_select_files_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_url_util.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/views/select_file_dialog_extension.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/url_constants.h"
#include "net/base/filename_util.h"
#include "net/base/mime_util.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "url/gurl.h"

namespace arc {

// Script for clicking OK button on the selector.
const char kScriptClickOk[] =
    "(function() { document.querySelector('#ok-button').click(); })();";

// Script for clicking a directory element in the left pane of the selector.
// %s should be replaced by the target directory name wrapped by double-quotes.
const char kScriptClickDirectory[] =
    "(function() {"
    "  var dirs = document.querySelectorAll('#directory-tree .entry-name');"
    "  Array.from(dirs).filter(a => a.innerText === %s)[0].click();"
    "})();";

// Script for clicking a file element in the right pane of the selector.
// %s should be replaced by the target file name wrapped by double-quotes.
const char kScriptClickFile[] =
    "(function() {"
    "  var evt = document.createEvent('MouseEvents');"
    "  evt.initMouseEvent('mousedown', true, false);"
    "  var files = document.querySelectorAll('#file-list .file');"
    "  Array.from(files).filter(a => a.getAttribute('file-name') === %s)[0]"
    "      .dispatchEvent(evt);"
    "})();";

// Script for querying UI elements (directories and files) shown on the selector.
const char kScriptGetElements[] =
    "(function() {"
    "  var dirs = document.querySelectorAll('#directory-tree .entry-name');"
    "  var files = document.querySelectorAll('#file-list .file');"
    "  return {dirNames: Array.from(dirs, a => a.innerText),"
    "          fileNames: Array.from(files, a => a.getAttribute('file-name'))};"
    "})();";

namespace {

void ConvertToElementVector(
    const base::Value* list_value,
    std::vector<mojom::FileSelectorElementPtr>* elements) {
  if (!list_value || !list_value->is_list())
    return;

  for (const base::Value& value : list_value->GetList()) {
    mojom::FileSelectorElementPtr element = mojom::FileSelectorElement::New();
    element->name = value.GetString();
    elements->push_back(std::move(element));
  }
}

void OnGetElementsScriptResults(
    mojom::FileSystemHost::GetFileSelectorElementsCallback callback,
    base::Value value) {
  mojom::FileSelectorElementsPtr result = mojom::FileSelectorElements::New();
  if (value.is_dict()) {
    ConvertToElementVector(value.FindKey("dirNames"),
                           &result->directory_elements);
    ConvertToElementVector(value.FindKey("fileNames"), &result->file_elements);
  }
  std::move(callback).Run(std::move(result));
}

void ContentUrlsResolved(mojom::FileSystemHost::SelectFilesCallback callback,
                         const std::vector<GURL>& content_urls) {
  mojom::SelectFilesResultPtr result = mojom::SelectFilesResult::New();
  for (const GURL& content_url : content_urls) {
    result->urls.push_back(content_url);
  }
  std::move(callback).Run(std::move(result));
}

ui::SelectFileDialog::Type GetDialogType(
    const mojom::SelectFilesRequestPtr& request) {
  switch (request->action_type) {
    case mojom::SelectFilesActionType::GET_CONTENT:
    case mojom::SelectFilesActionType::OPEN_DOCUMENT:
      return request->allow_multiple
                 ? ui::SelectFileDialog::SELECT_OPEN_MULTI_FILE
                 : ui::SelectFileDialog::SELECT_OPEN_FILE;
    case mojom::SelectFilesActionType::OPEN_DOCUMENT_TREE:
      return ui::SelectFileDialog::SELECT_EXISTING_FOLDER;
    case mojom::SelectFilesActionType::CREATE_DOCUMENT:
      return ui::SelectFileDialog::SELECT_SAVEAS_FILE;
  }
  NOTREACHED();
}

base::FilePath GetInitialFilePath(const mojom::SelectFilesRequestPtr& request) {
  const mojom::DocumentPathPtr& document_path = request->initial_document_path;
  if (!document_path)
    return base::FilePath();

  if (document_path->path.empty()) {
    LOG(ERROR) << "path should at least contain root Document ID.";
    return base::FilePath();
  }

  const std::string& root_document_id = document_path->path[0];
  // TODO(niwa): Convert non-root document IDs to the relative path and append.
  return arc::GetDocumentsProviderMountPath(document_path->authority,
                                            root_document_id);
}

void BuildFileTypeInfo(const mojom::SelectFilesRequestPtr& request,
                       ui::SelectFileDialog::FileTypeInfo* file_type_info) {
  file_type_info->allowed_paths = ui::SelectFileDialog::FileTypeInfo::ANY_PATH;
  for (const std::string& mime_type : request->mime_types) {
    std::vector<base::FilePath::StringType> extensions;
    net::GetExtensionsForMimeType(mime_type, &extensions);
    file_type_info->extensions.push_back(extensions);
  }
}

}  // namespace

ArcSelectFilesHandler::ArcSelectFilesHandler(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {
  dialog_holder_ = std::make_unique<SelectFileDialogHolder>(this);
}

ArcSelectFilesHandler::~ArcSelectFilesHandler() = default;

void ArcSelectFilesHandler::SelectFiles(
    const mojom::SelectFilesRequestPtr& request,
    mojom::FileSystemHost::SelectFilesCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!callback_.is_null()) {
    LOG(ERROR)
        << "There is already a ui::SelectFileDialog being shown currently. "
        << "We can't open multiple ui::SelectFileDialogs at one time.";
    std::move(callback).Run(mojom::SelectFilesResult::New());
    return;
  }
  callback_ = std::move(callback);

  // TODO(niwa): Convert all request options.
  ui::SelectFileDialog::Type dialog_type = GetDialogType(request);
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  BuildFileTypeInfo(request, &file_type_info);
  base::FilePath default_path = GetInitialFilePath(request);

  dialog_holder_->SelectFile(dialog_type, default_path, &file_type_info);
}

void ArcSelectFilesHandler::FileSelected(const base::FilePath& path,
                                         int index,
                                         void* params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::vector<base::FilePath> files;
  files.push_back(path);
  FilesSelectedInternal(files, params);
}

void ArcSelectFilesHandler::MultiFilesSelected(
    const std::vector<base::FilePath>& files,
    void* params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  FilesSelectedInternal(files, params);
}

void ArcSelectFilesHandler::FileSelectionCanceled(void* params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(callback_);
  // Returns an empty result if the user cancels file selection.
  std::move(callback_).Run(mojom::SelectFilesResult::New());
}

void ArcSelectFilesHandler::FilesSelectedInternal(
    const std::vector<base::FilePath>& files,
    void* params) {
  DCHECK(callback_);

  storage::FileSystemContext* file_system_context =
      file_manager::util::GetFileSystemContextForExtensionId(
          profile_, file_manager::kFileManagerAppId);

  std::vector<storage::FileSystemURL> file_system_urls;
  for (const base::FilePath& file_path : files) {
    GURL gurl;
    file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
        profile_, file_path, file_manager::kFileManagerAppId, &gurl);
    file_system_urls.push_back(file_system_context->CrackURL(gurl));
  }

  file_manager::util::ConvertToContentUrls(
      file_system_urls,
      base::BindOnce(&ContentUrlsResolved, std::move(callback_)));
}

void ArcSelectFilesHandler::OnFileSelectorEvent(
    mojom::FileSelectorEventPtr event,
    mojom::FileSystemHost::OnFileSelectorEventCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string quotedClickTargetName =
      base::GetQuotedJSONString(event->click_target->name.c_str());
  std::string script;
  switch (event->type) {
    case mojom::FileSelectorEventType::CLICK_OK:
      script = kScriptClickOk;
      break;
    case mojom::FileSelectorEventType::CLICK_DIRECTORY:
      script = base::StringPrintf(kScriptClickDirectory,
                                  quotedClickTargetName.c_str());
      break;
    case mojom::FileSelectorEventType::CLICK_FILE:
      script =
          base::StringPrintf(kScriptClickFile, quotedClickTargetName.c_str());
      break;
  }
  dialog_holder_->ExecuteJavaScript(script, {});

  std::move(callback).Run();
}

void ArcSelectFilesHandler::GetFileSelectorElements(
    mojom::FileSystemHost::GetFileSelectorElementsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  dialog_holder_->ExecuteJavaScript(
      kScriptGetElements,
      base::BindOnce(&OnGetElementsScriptResults, std::move(callback)));
}

void ArcSelectFilesHandler::SetDialogHolderForTesting(
    std::unique_ptr<SelectFileDialogHolder> dialog_holder) {
  dialog_holder_ = std::move(dialog_holder);
}

SelectFileDialogHolder::SelectFileDialogHolder(
    ui::SelectFileDialog::Listener* listener) {
  select_file_dialog_ = static_cast<SelectFileDialogExtension*>(
      ui::SelectFileDialog::Create(listener, nullptr).get());
}

SelectFileDialogHolder::~SelectFileDialogHolder() {
  // select_file_dialog_ can be nullptr only in unit tests.
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();
}

void SelectFileDialogHolder::SelectFile(
    ui::SelectFileDialog::Type type,
    const base::FilePath& default_path,
    const ui::SelectFileDialog::FileTypeInfo* file_types) {
  select_file_dialog_->SelectFile(
      type,
      /*title=*/base::string16(), default_path, file_types,
      /*file_type_index=*/0,
      /*default_extension=*/base::FilePath::StringType(),
      /*owning_window=*/nullptr,
      /*params=*/nullptr);
}

void SelectFileDialogHolder::ExecuteJavaScript(
    const std::string& script,
    content::RenderFrameHost::JavaScriptResultCallback callback) {
  content::RenderViewHost* view_host = select_file_dialog_->GetRenderViewHost();
  content::RenderFrameHost* frame_host =
      view_host ? view_host->GetMainFrame() : nullptr;

  if (!frame_host) {
    LOG(ERROR) << "Can't execute a script. SelectFileDialog is not ready.";
    if (callback)
      std::move(callback).Run(base::Value());
    return;
  }

  frame_host->ExecuteJavaScriptInIsolatedWorld(
      base::UTF8ToUTF16(script), std::move(callback),
      ISOLATED_WORLD_ID_CHROME_INTERNAL);
}

}  // namespace arc
