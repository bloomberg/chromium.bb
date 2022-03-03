// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DELEGATE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate_base.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_manager.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/file_analysis_request.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/file_opening_job.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "url/gurl.h"

class Profile;

namespace enterprise_connectors {

class ContentAnalysisDialog;

// A class that performs deep scans of data (for example malicious or sensitive
// content checks) before allowing a page to access it.
//
// If the UI is enabled, then a dialog is shown and blocks user interactions
// with the page until a scan verdict is obtained.
//
// If the UI is not enabled, then the dialog is not shown and the delegate
// proceeds as if all data (strings and files) have successfully passed the
// deep checks.  However the checks are still performed in the background and
// verdicts reported, implementing an audit-only mode.
//
// Users of this class normally call IsEnabled() first to determine if deep
// scanning is required for the given web page.  If so, the caller fills in
// the appropriate data members of |Data| and then call CreateForWebContents()
// to start the scan.
//
// Example:
//
//   contents::WebContent* contents = ...
//   Profile* profile = ...
//   safe_browsing::ContentAnalysisDelegate::Data data;
//   if (safe_browsing::ContentAnalysisDelegate::IsEnabled(
//           profile, contents->GetLastCommittedURL(), &data)) {
//     data.text.push_back(...);  // As needed.
//     data.paths.push_back(...);  // As needed.
//     safe_browsing::ContentAnalysisDelegate::CreateForWebContents(
//         contents, std::move(data), base::BindOnce(...));
//   }
class ContentAnalysisDelegate : public ContentAnalysisDelegateBase {
 public:
  // Used as an input to CreateForWebContents() to describe what data needs
  // deeper scanning.  Any members can be empty.
  struct Data {
    Data();
    Data(Data&& other);
    ~Data();

    // URL of the page that is to receive sensitive data.
    GURL url;

    // UTF-8 encoded text data to scan, such as plain text, URLs, HTML, etc.
    std::vector<std::string> text;

    // List of files to scan.
    std::vector<base::FilePath> paths;

    // The settings to use for the analysis of the data in this struct.
    enterprise_connectors::AnalysisSettings settings;
  };

  // Result of deep scanning.  Each Result contains the verdicts of deep scans
  // specified by one Data.
  struct Result {
    Result();
    Result(Result&& other);
    ~Result();

    // String data result.  Each element in this array is the result for the
    // corresponding Data::text element.  A value of true means the text
    // complies with all checks and is safe to be used.  A false means the
    // text does not comply with all checks and the caller should not use it.
    std::vector<bool> text_results;

    // File data result.  Each element in this array is the result for the
    // corresponding Data::paths element.  A value of true means the file
    // complies with all checks and is safe to be used.  A false means the
    // file does not comply with all checks and the caller should not use it.
    std::vector<bool> paths_results;
  };

  // File information used as an input to event report functions.
  struct FileInfo {
    FileInfo();
    FileInfo(FileInfo&& other);
    ~FileInfo();

    // Hex-encoded SHA256 hash for the given file.
    std::string sha256;

    // File size in bytes. -1 represents an unknown size.
    uint64_t size = 0;

    // File mime type.
    std::string mime_type;
  };

  // File contents used as input for |file_info_| and the BinaryUploadService.
  struct FileContents {
    FileContents();
    explicit FileContents(safe_browsing::BinaryUploadService::Result result);
    FileContents(FileContents&&);
    FileContents& operator=(FileContents&&);

    safe_browsing::BinaryUploadService::Result result =
        safe_browsing::BinaryUploadService::Result::UNKNOWN;
    safe_browsing::BinaryUploadService::Request::Data data;

    // Store the file size separately instead of using data.contents.size() to
    // keep track of size for large files.
    int64_t size = 0;
    std::string sha256;
  };

  // Callback used with CreateForWebContents() that informs caller of verdict
  // of deep scans.
  using CompletionCallback =
      base::OnceCallback<void(const Data& data, const Result& result)>;

  // A factory function used in tests to create fake ContentAnalysisDelegate
  // instances.
  using Factory =
      base::RepeatingCallback<std::unique_ptr<ContentAnalysisDelegate>(
          content::WebContents*,
          Data,
          CompletionCallback)>;

  ContentAnalysisDelegate(const ContentAnalysisDelegate&) = delete;
  ContentAnalysisDelegate& operator=(const ContentAnalysisDelegate&) = delete;
  ~ContentAnalysisDelegate() override;

  // ContentAnalysisDelegateBase:

  // Called when the user decides to bypass the verdict they obtained from DLP.
  // This will allow the upload of files marked as DLP warnings.
  void BypassWarnings() override;

  // Called when the user decides to cancel the file upload. This will stop the
  // upload to Chrome since the scan wasn't allowed to complete. If |warning| is
  // true, it means the user clicked Cancel after getting a warning, meaning the
  // "CancelledByUser" metrics should not be recorded.
  void Cancel(bool warning) override;

  absl::optional<std::u16string> GetCustomMessage() const override;

  absl::optional<GURL> GetCustomLearnMoreUrl() const override;

  absl::optional<std::u16string> OverrideCancelButtonText() const override;

  // Returns true if the deep scanning feature is enabled in the upload
  // direction via enterprise policies.  If the appropriate enterprise policies
  // are not set this feature is not enabled.
  //
  // The |do_dlp_scan| and |do_malware_scan| members of |data| are filled in
  // as needed.  If either is true, the function returns true, otherwise it
  // returns false.
  static bool IsEnabled(Profile* profile,
                        GURL url,
                        Data* data,
                        enterprise_connectors::AnalysisConnector connector);

  // Entry point for starting a deep scan, with the callback being called once
  // all results are available.  When the UI is enabled, a tab-modal dialog
  // is shown while the scans proceed in the background.  When the UI is
  // disabled, the callback will immedaitely inform the callers that all data
  // has successfully passed the checks, even though the checks will proceed
  // in the background.
  //
  // Whether the UI is enabled or not, verdicts of the scan will be reported.
  static void CreateForWebContents(
      content::WebContents* web_contents,
      Data data,
      CompletionCallback callback,
      safe_browsing::DeepScanAccessPoint access_point);

  // In tests, sets a factory function for creating fake
  // ContentAnalysisDelegates.
  static void SetFactoryForTesting(Factory factory);
  static void ResetFactoryForTesting();

  // Showing the UI is not possible in unit tests, call this to disable it.
  static void DisableUIForTesting();

  // Determines if a request result should be used to allow a data use or to
  // block it.
  static bool ResultShouldAllowDataUse(
      safe_browsing::BinaryUploadService::Result result,
      const enterprise_connectors::AnalysisSettings& settings);

 protected:
  ContentAnalysisDelegate(content::WebContents* web_contents,
                          Data data,
                          CompletionCallback callback,
                          safe_browsing::DeepScanAccessPoint access_point);

  // Callbacks from uploading data.  Protected so they can be called from
  // testing derived classes.
  void StringRequestCallback(
      safe_browsing::BinaryUploadService::Result result,
      enterprise_connectors::ContentAnalysisResponse response);
  void FileRequestCallback(
      base::FilePath path,
      safe_browsing::BinaryUploadService::Result result,
      enterprise_connectors::ContentAnalysisResponse response);

  base::WeakPtr<ContentAnalysisDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // Uploads data for deep scanning.  Returns true if uploading is occurring in
  // the background and false if there is nothing to do. Sets |data_uploaded_|
  // to true right before returning.
  bool UploadData();

  // Prepares an upload request for the text in |data_|. If |data_.text| is
  // empty, this method does nothing.
  void PrepareTextRequest();

  // Prepares an upload request for the file at |path|.  If the file
  // cannot be uploaded it will have a failure verdict added to |result_|.
  safe_browsing::FileAnalysisRequest* PrepareFileRequest(
      const base::FilePath& path);

  // Adds required fields to |request| before sending it to the binary upload
  // service.
  void PrepareRequest(enterprise_connectors::AnalysisConnector connector,
                      safe_browsing::BinaryUploadService::Request* request);

  // Fills the arrays in |result_| with the given boolean status.
  void FillAllResultsWith(bool status);

  // Upload the request for deep scanning using the binary upload service.
  // These methods exist so they can be overridden in tests as needed.
  // The |result| argument exists as an optimization to finish the request early
  // when the result is known in advance to avoid using the upload service.
  virtual void UploadTextForDeepScanning(
      std::unique_ptr<safe_browsing::BinaryUploadService::Request> request);
  virtual void UploadFileForDeepScanning(
      safe_browsing::BinaryUploadService::Result result,
      const base::FilePath& path,
      std::unique_ptr<safe_browsing::BinaryUploadService::Request> request);

  // Updates the tab modal dialog to show the scanning results. Returns false if
  // the UI was not enabled to indicate no action was taken. Virtual to override
  // in tests.
  virtual bool UpdateDialog();

  // Calls the CompletionCallback |callback_| if all requests associated with
  // scans of |data_| are finished.  This function may delete |this| so no
  // members should be accessed after this call.
  void MaybeCompleteScanRequest();

  // Runs |callback_| with the calculated results if it is non null.
  // |callback_| is cleared after being run.
  void RunCallback();

  // Called when the file info for |path| has been fetched. Also begins the
  // upload process.
  void OnGotFileInfo(
      std::unique_ptr<safe_browsing::BinaryUploadService::Request> request,
      const base::FilePath& path,
      safe_browsing::BinaryUploadService::Result result,
      safe_browsing::BinaryUploadService::Request::Data data);

  // Updates |final_result_| following the precedence established by the
  // FinalResult enum.
  void UpdateFinalResult(ContentAnalysisDelegateBase::FinalResult message,
                         const std::string& tag);

  // Returns the BinaryUploadService used to upload content for deep scanning.
  // Virtual to override in tests.
  virtual safe_browsing::BinaryUploadService* GetBinaryUploadService();

  // The Profile corresponding to the pending scan request(s).
  raw_ptr<Profile> profile_ = nullptr;

  // The GURL corresponding to the page where the scan triggered.
  GURL url_;

  // Description of the data being scanned and the results of the scan.
  // The elements of the vector |file_info_| hold the FileInfo of the file at
  // the same index in |data_.paths|.
  const Data data_;
  Result result_;
  std::vector<FileInfo> file_info_;

  // Set to true if the full text got a DLP warning verdict.
  bool text_warning_ = false;
  enterprise_connectors::ContentAnalysisResponse text_response_;

  // Scanning responses of files that got DLP warning verdicts.
  std::map<size_t, enterprise_connectors::ContentAnalysisResponse>
      file_warnings_;

  // Set to true once the scan of text has completed.  If the scan request has
  // no text requiring deep scanning, this is set to true immediately.
  bool text_request_complete_ = false;

  // The number of files scans that have completed.  If more than one file is
  // requested for scanning in |data_|, each is scanned in parallel with
  // separate requests.
  size_t file_result_count_ = 0;

  // Called once all text and files have completed deep scanning.
  CompletionCallback callback_;

  // Pointer to UI when enabled.
  raw_ptr<ContentAnalysisDialog> dialog_ = nullptr;

  // Access point to use to record UMA metrics.
  safe_browsing::DeepScanAccessPoint access_point_;

  // Scanning result to be shown to the user once every request is done.
  ContentAnalysisDelegateBase::FinalResult final_result_ =
      ContentAnalysisDelegateBase::FinalResult::SUCCESS;
  // The tag (dlp, malware, etc) of the result that triggered the verdict
  // represented by |final_result_|.
  std::string final_result_tag_;

  // Set to true at the end of UploadData to indicate requests have been made
  // for every file/text. This is read to ensure |this| isn't deleted too early.
  bool data_uploaded_ = false;

  // This is set to true as soon as a TOO_MANY_REQUESTS response is obtained. No
  // more data should be upload for |this| at that point.
  bool throttled_ = false;

  // Owner of the FileOpeningJob responsible for opening files on parallel
  // threads. Always nullptr for non-file content scanning.
  std::unique_ptr<safe_browsing::FileOpeningJob> file_opening_job_;

  base::TimeTicks upload_start_time_;

  base::WeakPtrFactory<ContentAnalysisDelegate> weak_ptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DELEGATE_H_
