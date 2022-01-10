// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/file_analyzer.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/file_util_service.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/common/safe_browsing/document_analyzer_results.h"
#include "chrome/common/safe_browsing/download_type_util.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_LINUX) || defined(OS_WIN)
#include "chrome/browser/safe_browsing/download_protection/document_analysis_service.h"
#endif

namespace safe_browsing {

namespace {

using content::BrowserThread;

void CopyArchivedBinaries(
    const google::protobuf::RepeatedPtrField<
        ClientDownloadRequest::ArchivedBinary>& src_binaries,
    google::protobuf::RepeatedPtrField<ClientDownloadRequest::ArchivedBinary>*
        dest_binaries) {
  // Limit the number of entries so we don't clog the backend.
  // We can expand this limit by pushing a new download_file_types update.
  int limit = FileTypePolicies::GetInstance()->GetMaxArchivedBinariesToReport();

  dest_binaries->Clear();
  for (int i = 0; dest_binaries->size() < limit && i < src_binaries.size();
       i++) {
    if (src_binaries[i].is_executable() || src_binaries[i].is_archive()) {
      *dest_binaries->Add() = src_binaries[i];
    }
  }
}

FileAnalyzer::Results ExtractFileFeatures(
    scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor,
    base::FilePath file_path) {
  FileAnalyzer::Results results;
  binary_feature_extractor->CheckSignature(file_path, &results.signature_info);

  if (!binary_feature_extractor->ExtractImageFeatures(
          file_path, BinaryFeatureExtractor::kDefaultOptions,
          &results.image_headers, nullptr)) {
    results.image_headers = ClientDownloadRequest::ImageHeaders();
  }

  return results;
}

}  // namespace

FileAnalyzer::Results::Results() = default;
FileAnalyzer::Results::~Results() {}
FileAnalyzer::Results::Results(const FileAnalyzer::Results& other) = default;

FileAnalyzer::FileAnalyzer(
    scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor)
    : binary_feature_extractor_(binary_feature_extractor) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

FileAnalyzer::~FileAnalyzer() {}

void FileAnalyzer::Start(const base::FilePath& target_path,
                         const base::FilePath& tmp_path,
                         base::OnceCallback<void(Results)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  target_path_ = target_path;
  tmp_path_ = tmp_path;
  callback_ = std::move(callback);

  results_.type = download_type_util::GetDownloadType(target_path_);

  DownloadFileType::InspectionType inspection_type =
      FileTypePolicies::GetInstance()
          ->PolicyForFile(target_path_)
          .inspection_type();

  if (inspection_type == DownloadFileType::ZIP) {
    StartExtractZipFeatures();
  } else if (inspection_type == DownloadFileType::RAR) {
    StartExtractRarFeatures();
#if defined(OS_MAC)
  } else if (inspection_type == DownloadFileType::DMG) {
    StartExtractDmgFeatures();
#endif
#if defined(OS_LINUX) || defined(OS_WIN)
  } else if (base::FeatureList::IsEnabled(
                 safe_browsing::kClientSideDetectionDocumentScanning) &&
             inspection_type == DownloadFileType::OFFICE_DOCUMENT) {
    StartExtractDocumentFeatures();
#endif
  } else {
#if defined(OS_MAC)
    // Checks for existence of "koly" signature even if file doesn't have
    // archive-type extension, then calls ExtractFileOrDmgFeatures() with
    // result.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(DiskImageTypeSnifferMac::IsAppleDiskImage, tmp_path_),
        base::BindOnce(&FileAnalyzer::ExtractFileOrDmgFeatures,
                       weakptr_factory_.GetWeakPtr()));
#else
    StartExtractFileFeatures();
#endif
  }
}

void FileAnalyzer::StartExtractFileFeatures() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ExtractFileFeatures, binary_feature_extractor_,
                     tmp_path_),
      base::BindOnce(&FileAnalyzer::OnFileAnalysisFinished,
                     weakptr_factory_.GetWeakPtr()));
}

void FileAnalyzer::OnFileAnalysisFinished(FileAnalyzer::Results results) {
  results.type = download_type_util::GetDownloadType(target_path_);
  std::move(callback_).Run(results);
}

void FileAnalyzer::StartExtractZipFeatures() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // We give the zip analyzer a weak pointer to this object.  Since the
  // analyzer is refcounted, it might outlive the request.
  zip_analyzer_ = new SandboxedZipAnalyzer(
      tmp_path_,
      base::BindOnce(&FileAnalyzer::OnZipAnalysisFinished,
                     weakptr_factory_.GetWeakPtr()),
      LaunchFileUtilService());
  zip_analyzer_->Start();
}

void FileAnalyzer::OnZipAnalysisFinished(
    const ArchiveAnalyzerResults& archive_results) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Even if !results.success, some of the zip may have been parsed.
  // Some unzippers will successfully unpack archives that we cannot,
  // so we're lenient here.
  results_.archive_is_valid =
      (archive_results.success ? ArchiveValid::VALID : ArchiveValid::INVALID);
  results_.archived_executable = archive_results.has_executable;
  results_.archived_archive = archive_results.has_archive;
  CopyArchivedBinaries(archive_results.archived_binary,
                       &results_.archived_binaries);

  if (!results_.archived_executable) {
    if (archive_results.has_archive) {
      results_.type = ClientDownloadRequest::ZIPPED_ARCHIVE;
    } else if (!archive_results.success) {
      // .zip files that look invalid to Chrome can often be successfully
      // unpacked by other archive tools, so they may be a real threat.
      results_.type = ClientDownloadRequest::INVALID_ZIP;
    }
  } else {
    results_.type = ClientDownloadRequest::ZIPPED_EXECUTABLE;
  }

  results_.file_count = archive_results.file_count;
  results_.directory_count = archive_results.directory_count;

  std::move(callback_).Run(std::move(results_));
}

void FileAnalyzer::StartExtractRarFeatures() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // We give the rar analyzer a weak pointer to this object.  Since the
  // analyzer is refcounted, it might outlive the request.
  rar_analyzer_ = new SandboxedRarAnalyzer(
      tmp_path_,
      base::BindOnce(&FileAnalyzer::OnRarAnalysisFinished,
                     weakptr_factory_.GetWeakPtr()),
      LaunchFileUtilService());
  rar_analyzer_->Start();
}

void FileAnalyzer::OnRarAnalysisFinished(
    const ArchiveAnalyzerResults& archive_results) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  results_.archive_is_valid =
      (archive_results.success ? ArchiveValid::VALID : ArchiveValid::INVALID);
  results_.archived_executable = archive_results.has_executable;
  results_.archived_archive = archive_results.has_archive;
  CopyArchivedBinaries(archive_results.archived_binary,
                       &results_.archived_binaries);

  if (!results_.archived_executable) {
    if (archive_results.has_archive) {
      results_.type = ClientDownloadRequest::RAR_COMPRESSED_ARCHIVE;
    } else if (!archive_results.success) {
      // .rar files that look invalid to Chrome may be successfully unpacked by
      // other archive tools, so they may be a real threat.
      results_.type = ClientDownloadRequest::INVALID_RAR;
    }
  } else {
    results_.type = ClientDownloadRequest::RAR_COMPRESSED_EXECUTABLE;
  }

  results_.file_count = archive_results.file_count;
  results_.directory_count = archive_results.directory_count;

  std::move(callback_).Run(std::move(results_));
}

#if defined(OS_MAC)
// This is called for .DMGs and other files that can be parsed by
// SandboxedDMGAnalyzer.
void FileAnalyzer::StartExtractDmgFeatures() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Directly use 'dmg' extension since download file may not have any
  // extension, but has still been deemed a DMG through file type sniffing.
  dmg_analyzer_ = new SandboxedDMGAnalyzer(
      tmp_path_,
      FileTypePolicies::GetInstance()->GetMaxFileSizeToAnalyze("dmg"),
      base::BindRepeating(&FileAnalyzer::OnDmgAnalysisFinished,
                          weakptr_factory_.GetWeakPtr()),
      LaunchFileUtilService());
  dmg_analyzer_->Start();
}

void FileAnalyzer::ExtractFileOrDmgFeatures(
    bool download_file_has_koly_signature) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (download_file_has_koly_signature)
    StartExtractDmgFeatures();
  else
    StartExtractFileFeatures();
}

void FileAnalyzer::OnDmgAnalysisFinished(
    const safe_browsing::ArchiveAnalyzerResults& archive_results) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (archive_results.signature_blob.size() > 0) {
    results_.disk_image_signature =
        std::vector<uint8_t>(archive_results.signature_blob);
  }

  results_.detached_code_signatures.CopyFrom(
      archive_results.detached_code_signatures);

  // Even if !results.success, some of the DMG may have been parsed.
  results_.archive_is_valid =
      (archive_results.success ? ArchiveValid::VALID : ArchiveValid::INVALID);
  results_.archived_executable = archive_results.has_executable;
  results_.archived_archive = archive_results.has_archive;
  CopyArchivedBinaries(archive_results.archived_binary,
                       &results_.archived_binaries);

  if (archive_results.success) {
    results_.type = ClientDownloadRequest::MAC_EXECUTABLE;
  } else {
    results_.type = ClientDownloadRequest::MAC_ARCHIVE_FAILED_PARSING;
  }

  std::move(callback_).Run(std::move(results_));
}
#endif  // defined(OS_MAC)

#if defined(OS_LINUX) || defined(OS_WIN)
void FileAnalyzer::StartExtractDocumentFeatures() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  document_analysis_start_time_ = base::TimeTicks::Now();

  document_analyzer_ = new SandboxedDocumentAnalyzer(
      target_path_, tmp_path_,
      base::BindOnce(&FileAnalyzer::OnDocumentAnalysisFinished,
                     weakptr_factory_.GetWeakPtr()),
      LaunchDocumentAnalysisService());

  document_analyzer_->Start();
}

void FileAnalyzer::OnDocumentAnalysisFinished(
    const DocumentAnalyzerResults& document_results) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Log metrics for Document Analysis.
  UMA_HISTOGRAM_BOOLEAN("SBClientDownload.DocumentAnalysisSuccess",
                        document_results.success);
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "SBClientDownload.ExtractDocumentFeaturesTimeMedium",
      base::TimeTicks::Now() - document_analysis_start_time_);

  ClientDownloadRequest::DocumentSummary document_summary;
  ClientDownloadRequest::DocumentInfo* document_info =
      document_summary.mutable_metadata();
  document_info->set_contains_macros(document_results.has_macros);

  ClientDownloadRequest::DocumentProcessingInfo* processing_info =
      document_summary.mutable_processing_info();
  processing_info->set_processing_successful(document_results.success);

  processing_info->set_maldoca_error_type(document_results.error_code);
  if (!document_results.error_message.empty()) {
    processing_info->set_maldoca_error_message(document_results.error_message);
  }
  results_.document_summary.CopyFrom(document_summary);

  results_.type = ClientDownloadRequest::DOCUMENT;

  std::move(callback_).Run(std::move(results_));
}
#endif
}  // namespace safe_browsing
