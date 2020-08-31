// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_select_helper.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/grit/generated_resources.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/base/filename_util.h"
#include "net/base/mime_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/selected_file_info.h"

#if defined(OS_ANDROID)
#include "chrome/browser/file_select_helper_contacts_android.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "content/public/browser/site_instance.h"
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#endif

using blink::mojom::FileChooserFileInfo;
using blink::mojom::FileChooserFileInfoPtr;
using blink::mojom::FileChooserParams;
using blink::mojom::FileChooserParamsPtr;
using content::BrowserThread;
using content::RenderViewHost;
using content::RenderWidgetHost;
using content::WebContents;

namespace {

#if defined(OS_ANDROID)
// The MIME type for selecting contacts.
constexpr char kContactsMimeType[] = "text/json+contacts";
#endif

void DeleteFiles(std::vector<base::FilePath> paths) {
  for (auto& file_path : paths)
    base::DeleteFile(file_path, false);
}

bool IsValidProfile(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // No profile manager in unit tests.
  if (!g_browser_process->profile_manager())
    return true;
  return g_browser_process->profile_manager()->IsValidProfile(profile);
}

#if BUILDFLAG(FULL_SAFE_BROWSING)

bool IsDownloadAllowedBySafeBrowsing(
    safe_browsing::DownloadCheckResult result) {
  using Result = safe_browsing::DownloadCheckResult;
  switch (result) {
    // Only allow downloads that are marked as SAFE or UNKNOWN by SafeBrowsing.
    // All other types are going to be blocked. UNKNOWN could be the result of a
    // failed safe browsing ping.
    case Result::UNKNOWN:
    case Result::SAFE:
    case Result::WHITELISTED_BY_POLICY:
      return true;

    case Result::DANGEROUS:
    case Result::UNCOMMON:
    case Result::DANGEROUS_HOST:
    case Result::POTENTIALLY_UNWANTED:
      return false;

    // Safe Browsing should only return these results for client downloads, not
    // for PPAPI downloads.
    case Result::ASYNC_SCANNING:
    case Result::BLOCKED_PASSWORD_PROTECTED:
    case Result::BLOCKED_TOO_LARGE:
    case Result::SENSITIVE_CONTENT_BLOCK:
    case Result::SENSITIVE_CONTENT_WARNING:
    case Result::DEEP_SCANNED_SAFE:
    case Result::PROMPT_FOR_SCANNING:
    case Result::BLOCKED_UNSUPPORTED_FILE_TYPE:
      NOTREACHED();
      return true;
  }
  NOTREACHED();
  return false;
}

void InterpretSafeBrowsingVerdict(const base::Callback<void(bool)>& recipient,
                                  safe_browsing::DownloadCheckResult result) {
  recipient.Run(IsDownloadAllowedBySafeBrowsing(result));
}

#endif

}  // namespace

struct FileSelectHelper::ActiveDirectoryEnumeration {
  explicit ActiveDirectoryEnumeration(const base::FilePath& path)
      : path_(path) {}

  std::unique_ptr<net::DirectoryLister> lister_;
  const base::FilePath path_;
  std::vector<base::FilePath> results_;
};

FileSelectHelper::FileSelectHelper(Profile* profile)
    : profile_(profile),
      render_frame_host_(nullptr),
      web_contents_(nullptr),
      select_file_dialog_(),
      select_file_types_(),
      dialog_type_(ui::SelectFileDialog::SELECT_OPEN_FILE),
      dialog_mode_(FileChooserParams::Mode::kOpen) {}

FileSelectHelper::~FileSelectHelper() {
  // There may be pending file dialogs, we need to tell them that we've gone
  // away so they don't try and call back to us.
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();
}

void FileSelectHelper::FileSelected(const base::FilePath& path,
                                    int index, void* params) {
  FileSelectedWithExtraInfo(ui::SelectedFileInfo(path, path), index, params);
}

void FileSelectHelper::FileSelectedWithExtraInfo(
    const ui::SelectedFileInfo& file,
    int index,
    void* params) {
  if (IsValidProfile(profile_)) {
    base::FilePath path = file.file_path;
    if (dialog_mode_ != FileChooserParams::Mode::kUploadFolder)
      path = path.DirName();
    profile_->set_last_selected_directory(path);
  }

  if (!render_frame_host_) {
    RunFileChooserEnd();
    return;
  }

  const base::FilePath& path = file.local_path;
  if (dialog_type_ == ui::SelectFileDialog::SELECT_UPLOAD_FOLDER) {
    StartNewEnumeration(path);
    return;
  }

  std::vector<ui::SelectedFileInfo> files;
  files.push_back(file);

#if defined(OS_MACOSX)
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&FileSelectHelper::ProcessSelectedFilesMac, this, files));
#else
  ConvertToFileChooserFileInfoList(files);
#endif  // defined(OS_MACOSX)
}

void FileSelectHelper::MultiFilesSelected(
    const std::vector<base::FilePath>& files,
    void* params) {
  std::vector<ui::SelectedFileInfo> selected_files =
      ui::FilePathListToSelectedFileInfoList(files);

  MultiFilesSelectedWithExtraInfo(selected_files, params);
}

void FileSelectHelper::MultiFilesSelectedWithExtraInfo(
    const std::vector<ui::SelectedFileInfo>& files,
    void* params) {
  if (!files.empty() && IsValidProfile(profile_)) {
    base::FilePath path = files[0].file_path;
    if (dialog_mode_ != FileChooserParams::Mode::kUploadFolder)
      path = path.DirName();
    profile_->set_last_selected_directory(path);
  }
#if defined(OS_MACOSX)
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&FileSelectHelper::ProcessSelectedFilesMac, this, files));
#else
  ConvertToFileChooserFileInfoList(files);
#endif  // defined(OS_MACOSX)
}

void FileSelectHelper::FileSelectionCanceled(void* params) {
  RunFileChooserEnd();
}

void FileSelectHelper::StartNewEnumeration(const base::FilePath& path) {
  base_dir_ = path;
  auto entry = std::make_unique<ActiveDirectoryEnumeration>(path);
  entry->lister_.reset(new net::DirectoryLister(
      path, net::DirectoryLister::NO_SORT_RECURSIVE, this));
  entry->lister_->Start();
  directory_enumeration_ = std::move(entry);
}

void FileSelectHelper::OnListFile(
    const net::DirectoryLister::DirectoryListerData& data) {
  // Directory upload only cares about files.
  if (data.info.IsDirectory())
    return;

  directory_enumeration_->results_.push_back(data.path);
}

void FileSelectHelper::LaunchConfirmationDialog(
    const base::FilePath& path,
    std::vector<ui::SelectedFileInfo> selected_files) {
  ShowFolderUploadConfirmationDialog(
      path,
      base::BindOnce(&FileSelectHelper::ConvertToFileChooserFileInfoList, this),
      std::move(selected_files), web_contents_);
}

void FileSelectHelper::OnListDone(int error) {
  if (!web_contents_) {
    // Web contents was destroyed under us (probably by closing the tab). We
    // must notify |listener_| and release our reference to
    // ourself. RunFileChooserEnd() performs this.
    RunFileChooserEnd();
    return;
  }

  // This entry needs to be cleaned up when this function is done.
  std::unique_ptr<ActiveDirectoryEnumeration> entry =
      std::move(directory_enumeration_);
  if (error) {
    FileSelectionCanceled(NULL);
    return;
  }

  std::vector<ui::SelectedFileInfo> selected_files =
      ui::FilePathListToSelectedFileInfoList(entry->results_);

  if (dialog_type_ == ui::SelectFileDialog::SELECT_UPLOAD_FOLDER) {
    LaunchConfirmationDialog(entry->path_, std::move(selected_files));
  } else {
    std::vector<FileChooserFileInfoPtr> chooser_files;
    for (const auto& file_path : entry->results_) {
      chooser_files.push_back(FileChooserFileInfo::NewNativeFile(
          blink::mojom::NativeFileInfo::New(file_path, base::string16())));
    }

    listener_->FileSelected(std::move(chooser_files), base_dir_,
                            FileChooserParams::Mode::kUploadFolder);
    listener_.reset();
    EnumerateDirectoryEnd();
  }
}

void FileSelectHelper::ConvertToFileChooserFileInfoList(
    const std::vector<ui::SelectedFileInfo>& files) {
  if (AbortIfWebContentsDestroyed())
    return;

#if defined(OS_CHROMEOS)
  if (!files.empty()) {
    if (!IsValidProfile(profile_)) {
      RunFileChooserEnd();
      return;
    }
    // Converts |files| into FileChooserFileInfo with handling of non-native
    // files.
    content::SiteInstance* site_instance =
        render_frame_host_->GetSiteInstance();
    storage::FileSystemContext* file_system_context =
        content::BrowserContext::GetStoragePartition(profile_, site_instance)
            ->GetFileSystemContext();
    file_manager::util::ConvertSelectedFileInfoListToFileChooserFileInfoList(
        file_system_context, site_instance->GetSiteURL(), files,
        base::BindOnce(&FileSelectHelper::PerformSafeBrowsingDeepScanIfNeeded,
                       this));
    return;
  }
#endif  // defined(OS_CHROMEOS)

  std::vector<FileChooserFileInfoPtr> chooser_files;
  for (const auto& file : files) {
    chooser_files.push_back(
        FileChooserFileInfo::NewNativeFile(blink::mojom::NativeFileInfo::New(
            file.local_path,
            base::FilePath(file.display_name).AsUTF16Unsafe())));
  }

  PerformSafeBrowsingDeepScanIfNeeded(std::move(chooser_files));
}

void FileSelectHelper::PerformSafeBrowsingDeepScanIfNeeded(
    std::vector<FileChooserFileInfoPtr> list) {
  if (AbortIfWebContentsDestroyed())
    return;

#if BUILDFLAG(FULL_SAFE_BROWSING)
  safe_browsing::DeepScanningDialogDelegate::Data data;
  if (safe_browsing::DeepScanningDialogDelegate::IsEnabled(
          profile_, render_frame_host_->GetLastCommittedURL(), &data,
          enterprise_connectors::AnalysisConnector::FILE_ATTACHED)) {
    data.paths.reserve(list.size());
    for (const auto& file : list)
      data.paths.push_back(file->get_native_file()->file_path);

    safe_browsing::DeepScanningDialogDelegate::ShowForWebContents(
        web_contents_, std::move(data),
        base::BindOnce(&FileSelectHelper::DeepScanCompletionCallback, this,
                       std::move(list)),
        safe_browsing::DeepScanAccessPoint::UPLOAD);
  } else {
    NotifyListenerAndEnd(std::move(list));
  }
#else
  NotifyListenerAndEnd(std::move(list));
#endif  // BUILDFLAG(FULL_SAFE_BROWSING)
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
void FileSelectHelper::DeepScanCompletionCallback(
    std::vector<blink::mojom::FileChooserFileInfoPtr> list,
    const safe_browsing::DeepScanningDialogDelegate::Data& data,
    const safe_browsing::DeepScanningDialogDelegate::Result& result) {
  if (AbortIfWebContentsDestroyed())
    return;

  DCHECK_EQ(data.text.size(), result.text_results.size());
  DCHECK_EQ(data.paths.size(), result.paths_results.size());
  DCHECK_EQ(list.size(), result.paths_results.size());

  // Remove any files that did not pass the deep scan.
  size_t i = 0;
  for (auto it = list.begin(); it != list.end(); ++i) {
    if (!result.paths_results[i]) {
      it = list.erase(it);
    } else {
      ++it;
    }
  }

  NotifyListenerAndEnd(std::move(list));
}
#endif  // BUILDFLAG(FULL_SAFE_BROWSING)

void FileSelectHelper::NotifyListenerAndEnd(
    std::vector<blink::mojom::FileChooserFileInfoPtr> list) {
  listener_->FileSelected(std::move(list), base_dir_, dialog_mode_);
  listener_.reset();

  // No members should be accessed from here on.
  RunFileChooserEnd();
}

void FileSelectHelper::DeleteTemporaryFiles() {
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&DeleteFiles, std::move(temporary_files_)));
}

void FileSelectHelper::CleanUp() {
  if (!temporary_files_.empty()) {
    DeleteTemporaryFiles();

    // Now that the temporary files have been scheduled for deletion, there
    // is no longer any reason to keep this instance around.
    Release();
  }
}

bool FileSelectHelper::AbortIfWebContentsDestroyed() {
  if (abort_on_missing_web_contents_in_tests_ &&
      (render_frame_host_ == nullptr || web_contents_ == nullptr)) {
    RunFileChooserEnd();
    return true;
  }

  return false;
}

void FileSelectHelper::SetFileSelectListenerForTesting(
    std::unique_ptr<content::FileSelectListener> listener) {
  DCHECK(listener);
  DCHECK(!listener_);
  listener_ = std::move(listener);
}

void FileSelectHelper::DontAbortOnMissingWebContentsForTesting() {
  abort_on_missing_web_contents_in_tests_ = false;
}

std::unique_ptr<ui::SelectFileDialog::FileTypeInfo>
FileSelectHelper::GetFileTypesFromAcceptType(
    const std::vector<base::string16>& accept_types) {
  std::unique_ptr<ui::SelectFileDialog::FileTypeInfo> base_file_type(
      new ui::SelectFileDialog::FileTypeInfo());
  if (accept_types.empty())
    return base_file_type;

  // Create FileTypeInfo and pre-allocate for the first extension list.
  std::unique_ptr<ui::SelectFileDialog::FileTypeInfo> file_type(
      new ui::SelectFileDialog::FileTypeInfo(*base_file_type));
  file_type->include_all_files = true;
  file_type->extensions.resize(1);
  std::vector<base::FilePath::StringType>* extensions =
      &file_type->extensions.back();

  // Find the corresponding extensions.
  int valid_type_count = 0;
  int description_id = 0;
  for (const auto& accept_type : accept_types) {
    size_t old_extension_size = extensions->size();
    if (accept_type[0] == '.') {
      // If the type starts with a period it is assumed to be a file extension
      // so we just have to add it to the list.
      base::FilePath::StringType ext =
          base::FilePath::FromUTF16Unsafe(accept_type).value();
      extensions->push_back(ext.substr(1));
    } else {
      if (!base::IsStringASCII(accept_type))
        continue;
      std::string ascii_type = base::UTF16ToASCII(accept_type);
      if (ascii_type == "image/*")
        description_id = IDS_IMAGE_FILES;
      else if (ascii_type == "audio/*")
        description_id = IDS_AUDIO_FILES;
      else if (ascii_type == "video/*")
        description_id = IDS_VIDEO_FILES;

      net::GetExtensionsForMimeType(ascii_type, extensions);
    }

    if (extensions->size() > old_extension_size)
      valid_type_count++;
  }

  // If no valid extension is added, bail out.
  if (valid_type_count == 0)
    return base_file_type;

  // Use a generic description "Custom Files" if either of the following is
  // true:
  // 1) There're multiple types specified, like "audio/*,video/*"
  // 2) There're multiple extensions for a MIME type without parameter, like
  //    "ehtml,shtml,htm,html" for "text/html". On Windows, the select file
  //    dialog uses the first extension in the list to form the description,
  //    like "EHTML Files". This is not what we want.
  if (valid_type_count > 1 ||
      (valid_type_count == 1 && description_id == 0 && extensions->size() > 1))
    description_id = IDS_CUSTOM_FILES;

  if (description_id) {
    file_type->extension_description_overrides.push_back(
        l10n_util::GetStringUTF16(description_id));
  }

  return file_type;
}

// static
void FileSelectHelper::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    std::unique_ptr<content::FileSelectListener> listener,
    const FileChooserParams& params) {
  Profile* profile = Profile::FromBrowserContext(
      render_frame_host->GetProcess()->GetBrowserContext());

#if defined(OS_ANDROID)
  if (params.accept_types.size() == 1 &&
      params.accept_types[0].compare(base::ASCIIToUTF16(kContactsMimeType)) ==
          0) {
    scoped_refptr<FileSelectHelperContactsAndroid> file_select_helper_android(
        new FileSelectHelperContactsAndroid(profile));
    file_select_helper_android->RunFileChooser(
        render_frame_host, std::move(listener), params.Clone());
    return;
  }
#endif

  // FileSelectHelper will keep itself alive until it sends the result
  // message.
  scoped_refptr<FileSelectHelper> file_select_helper(
      new FileSelectHelper(profile));
  file_select_helper->RunFileChooser(render_frame_host, std::move(listener),
                                     params.Clone());
}

// static
void FileSelectHelper::EnumerateDirectory(
    content::WebContents* tab,
    std::unique_ptr<content::FileSelectListener> listener,
    const base::FilePath& path) {
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  // FileSelectHelper will keep itself alive until it sends the result
  // message.
  scoped_refptr<FileSelectHelper> file_select_helper(
      new FileSelectHelper(profile));
  file_select_helper->EnumerateDirectoryImpl(tab, std::move(listener), path);
}

void FileSelectHelper::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    std::unique_ptr<content::FileSelectListener> listener,
    FileChooserParamsPtr params) {
  DCHECK(!render_frame_host_);
  DCHECK(!web_contents_);
  DCHECK(listener);
  DCHECK(!listener_);
  DCHECK(params->default_file_name.empty() ||
         params->mode == FileChooserParams::Mode::kSave)
      << "The default_file_name parameter should only be specified for Save "
         "file choosers";
  DCHECK(params->default_file_name == params->default_file_name.BaseName())
      << "The default_file_name parameter should not contain path separators";

  render_frame_host_ = render_frame_host;
  web_contents_ = WebContents::FromRenderFrameHost(render_frame_host);
  listener_ = std::move(listener);
  observer_.RemoveAll();
  content::WebContentsObserver::Observe(web_contents_);
  observer_.Add(render_frame_host_->GetRenderViewHost()->GetWidget());

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&FileSelectHelper::GetFileTypesInThreadPool, this,
                     std::move(params)));

  // Because this class returns notifications to the RenderViewHost, it is
  // difficult for callers to know how long to keep a reference to this
  // instance. We AddRef() here to keep the instance alive after we return
  // to the caller, until the last callback is received from the file dialog.
  // At that point, we must call RunFileChooserEnd().
  AddRef();
}

void FileSelectHelper::GetFileTypesInThreadPool(FileChooserParamsPtr params) {
  select_file_types_ = GetFileTypesFromAcceptType(params->accept_types);
  select_file_types_->allowed_paths =
      params->need_local_path ? ui::SelectFileDialog::FileTypeInfo::NATIVE_PATH
                              : ui::SelectFileDialog::FileTypeInfo::ANY_PATH;

  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&FileSelectHelper::GetSanitizedFilenameOnUIThread, this,
                     std::move(params)));
}

void FileSelectHelper::GetSanitizedFilenameOnUIThread(
    FileChooserParamsPtr params) {
  if (AbortIfWebContentsDestroyed())
    return;

  base::FilePath default_file_path = profile_->last_selected_directory().Append(
      GetSanitizedFileName(params->default_file_name));
#if BUILDFLAG(FULL_SAFE_BROWSING)
  if (params->mode == FileChooserParams::Mode::kSave) {
    CheckDownloadRequestWithSafeBrowsing(default_file_path, std::move(params));
    return;
  }
#endif
  RunFileChooserOnUIThread(default_file_path, std::move(params));
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
void FileSelectHelper::CheckDownloadRequestWithSafeBrowsing(
    const base::FilePath& default_file_path,
    FileChooserParamsPtr params) {
// Download Protection is not supported on Android.
#if BUILDFLAG(FULL_SAFE_BROWSING)
  safe_browsing::SafeBrowsingService* sb_service =
      g_browser_process->safe_browsing_service();

  if (!sb_service || !sb_service->download_protection_service() ||
      !sb_service->download_protection_service()->enabled()) {
    RunFileChooserOnUIThread(default_file_path, std::move(params));
    return;
  }

  std::vector<base::FilePath::StringType> alternate_extensions;
  if (select_file_types_) {
    for (const auto& extensions_list : select_file_types_->extensions) {
      for (const auto& extension_in_list : extensions_list) {
        base::FilePath::StringType extension =
            default_file_path.ReplaceExtension(extension_in_list)
                .FinalExtension();
        alternate_extensions.push_back(extension);
      }
    }
  }

  GURL requestor_url = params->requestor;
  sb_service->download_protection_service()->CheckPPAPIDownloadRequest(
      requestor_url,
      render_frame_host_ ? render_frame_host_->GetLastCommittedURL() : GURL(),
      WebContents::FromRenderFrameHost(render_frame_host_), default_file_path,
      alternate_extensions, profile_,
      base::BindOnce(
          &InterpretSafeBrowsingVerdict,
          base::Bind(&FileSelectHelper::ProceedWithSafeBrowsingVerdict, this,
                     default_file_path, base::Passed(&params))));
#endif
}

void FileSelectHelper::ProceedWithSafeBrowsingVerdict(
    const base::FilePath& default_file_path,
    FileChooserParamsPtr params,
    bool allowed_by_safe_browsing) {
  if (!allowed_by_safe_browsing) {
    RunFileChooserEnd();
    return;
  }
  RunFileChooserOnUIThread(default_file_path, std::move(params));
}
#endif

void FileSelectHelper::RunFileChooserOnUIThread(
    const base::FilePath& default_file_path,
    FileChooserParamsPtr params) {
  DCHECK(params);
  if (AbortIfWebContentsDestroyed())
    return;

  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents_));
  if (!select_file_dialog_.get())
    return;

  dialog_mode_ = params->mode;
  switch (params->mode) {
    case FileChooserParams::Mode::kOpen:
      dialog_type_ = ui::SelectFileDialog::SELECT_OPEN_FILE;
      break;
    case FileChooserParams::Mode::kOpenMultiple:
      dialog_type_ = ui::SelectFileDialog::SELECT_OPEN_MULTI_FILE;
      break;
    case FileChooserParams::Mode::kUploadFolder:
      dialog_type_ = ui::SelectFileDialog::SELECT_UPLOAD_FOLDER;
      break;
    case FileChooserParams::Mode::kSave:
      dialog_type_ = ui::SelectFileDialog::SELECT_SAVEAS_FILE;
      break;
    default:
      // Prevent warning.
      dialog_type_ = ui::SelectFileDialog::SELECT_OPEN_FILE;
      NOTREACHED();
  }

  gfx::NativeWindow owning_window =
      platform_util::GetTopLevel(web_contents_->GetNativeView());

#if defined(OS_ANDROID)
  // Android needs the original MIME types and an additional capture value.
  std::pair<std::vector<base::string16>, bool> accept_types =
      std::make_pair(params->accept_types, params->use_media_capture);
#endif

  select_file_dialog_->SelectFile(
      dialog_type_, params->title, default_file_path, select_file_types_.get(),
      select_file_types_.get() && !select_file_types_->extensions.empty()
          ? 1
          : 0,  // 1-based index of default extension to show.
      base::FilePath::StringType(),
      owning_window,
#if defined(OS_ANDROID)
      &accept_types);
#else
      NULL);
#endif

  select_file_types_.reset();
}

// This method is called when we receive the last callback from the file chooser
// dialog or if the renderer was destroyed. Perform any cleanup and release the
// reference we added in RunFileChooser().
void FileSelectHelper::RunFileChooserEnd() {
  // If there are temporary files, then this instance needs to stick around
  // until web_contents_ is destroyed, so that this instance can delete the
  // temporary files.
  if (!temporary_files_.empty())
    return;

  if (listener_)
    listener_->FileSelectionCanceled();
  render_frame_host_ = nullptr;
  web_contents_ = nullptr;
  Release();
}

void FileSelectHelper::EnumerateDirectoryImpl(
    content::WebContents* tab,
    std::unique_ptr<content::FileSelectListener> listener,
    const base::FilePath& path) {
  DCHECK(listener);
  DCHECK(!listener_);
  dialog_type_ = ui::SelectFileDialog::SELECT_NONE;
  web_contents_ = tab;
  listener_ = std::move(listener);
  // Because this class returns notifications to the RenderViewHost, it is
  // difficult for callers to know how long to keep a reference to this
  // instance. We AddRef() here to keep the instance alive after we return
  // to the caller, until the last callback is received from the enumeration
  // code. At that point, we must call EnumerateDirectoryEnd().
  AddRef();
  StartNewEnumeration(path);
}

// This method is called when we receive the last callback from the enumeration
// code. Perform any cleanup and release the reference we added in
// EnumerateDirectoryImpl().
void FileSelectHelper::EnumerateDirectoryEnd() {
  Release();
}

void FileSelectHelper::RenderWidgetHostDestroyed(
    content::RenderWidgetHost* widget_host) {
  render_frame_host_ = nullptr;
  observer_.Remove(widget_host);
}

void FileSelectHelper::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  if (!render_frame_host_)
    return;
  // The |old_host| and its children are now pending deletion. Do not give them
  // file access past this point.
  if (render_frame_host_ == old_host ||
      render_frame_host_->IsDescendantOf(old_host)) {
    render_frame_host_ = nullptr;
  }
}

void FileSelectHelper::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host == render_frame_host_)
    render_frame_host_ = nullptr;
}

void FileSelectHelper::WebContentsDestroyed() {
  render_frame_host_ = nullptr;
  web_contents_ = nullptr;
  CleanUp();
}

// static
bool FileSelectHelper::IsAcceptTypeValid(const std::string& accept_type) {
  // TODO(raymes): This only does some basic checks, extend to test more cases.
  // A 1 character accept type will always be invalid (either a "." in the case
  // of an extension or a "/" in the case of a MIME type).
  std::string unused;
  if (accept_type.length() <= 1 ||
      base::ToLowerASCII(accept_type) != accept_type ||
      base::TrimWhitespaceASCII(accept_type, base::TRIM_ALL, &unused) !=
          base::TRIM_NONE) {
    return false;
  }
  return true;
}

// static
base::FilePath FileSelectHelper::GetSanitizedFileName(
    const base::FilePath& suggested_filename) {
  if (suggested_filename.empty())
    return base::FilePath();
  return net::GenerateFileName(
      GURL(), std::string(), std::string(), suggested_filename.AsUTF8Unsafe(),
      std::string(), l10n_util::GetStringUTF8(IDS_DEFAULT_DOWNLOAD_FILENAME));
}
