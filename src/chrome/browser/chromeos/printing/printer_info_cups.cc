// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <array>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task_runner_util.h"
#include "base/version.h"
#include "chrome/browser/chromeos/printing/printer_info.h"
#include "printing/backend/cups_jobs.h"

namespace {

const char kPdfMimeType[] = "application/pdf";
const char kPwgRasterMimeType[] = "image/pwg-raster";

const char kPwgRasterDocumentResolutionSupported[] =
    "Printing.CUPS.PwgRasterDocumentResolutionSupported";

// List of known multi-word printer manufacturers to help with make-and-model
// string parsing.  Keep in UPPER CASE as that's how matches are performed.
const std::array<const char* const, 4> kMultiWordManufacturers{
    {"FUJI XEROX", "KODAK FUNAI", "KONICA MINOLTA", "TEXAS INSTRUMENTS"}};

// Wraps a PrinterQueryResult and a PrinterInfo so that we can use
// PostTaskAndResplyWithResult.
struct QueryResult {
  ::printing::PrinterQueryResult result;
  ::printing::PrinterInfo printer_info;
};

// Enums for Printing.CUPS.HighestIppVersion.  Do not delete entries.  Keep
// synced with enums.xml.  Represents IPP versions 1.0, 1.1, 2.0, 2.1, and 2.2.
// Error is used if the version was unparsable.  OutOfRange is used for values
// that are not currently mapped.
enum class IppVersion {
  kError,
  kUnknown,
  k10,
  k11,
  k20,
  k21,
  k22,
  kMaxValue = k22
};

// Returns the length of the portion of |make_and_model| representing the
// manufacturer.  This is either a value from kMultiWordManufacaturers or the
// first token.  If there is only one token or less, we assume that it does not
// represent the manufacturer and return 0.
size_t ManufacturerLength(base::StringPiece make_and_model) {
  // TODO(crbug.com/729245): Update when better data is available.
  for (base::StringPiece multi_word_manufacturer : kMultiWordManufacturers) {
    if (base::StartsWith(make_and_model, multi_word_manufacturer,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      return multi_word_manufacturer.size();
    }
  }

  // The position of the first space is equal to the length of the first token.
  size_t first_space = make_and_model.find(" ");
  return first_space != base::StringPiece::npos ? first_space : 0;
}

using MajorMinor = std::pair<uint32_t, uint32_t>;
using VersionEntry = std::pair<MajorMinor, IppVersion>;

IppVersion ToIppVersion(const base::Version& version) {
  constexpr std::array<VersionEntry, 5> kVersions = {
      VersionEntry(MajorMinor((uint32_t)1, (uint32_t)0), IppVersion::k10),
      VersionEntry(MajorMinor((uint32_t)1, (uint32_t)1), IppVersion::k11),
      VersionEntry(MajorMinor((uint32_t)2, (uint32_t)0), IppVersion::k20),
      VersionEntry(MajorMinor((uint32_t)2, (uint32_t)1), IppVersion::k21),
      VersionEntry(MajorMinor((uint32_t)2, (uint32_t)2), IppVersion::k22)};

  if (!version.IsValid()) {
    return IppVersion::kError;
  }

  const auto& components = version.components();
  if (components.size() != 2) {
    return IppVersion::kError;
  }

  const auto target = MajorMinor(components[0], components[1]);
  const VersionEntry* iter = std::find_if(
      kVersions.cbegin(), kVersions.cend(),
      [target](const VersionEntry& entry) { return entry.first == target; });

  if (iter == kVersions.end()) {
    return IppVersion::kUnknown;
  }

  return iter->second;
}

// Returns true if any of the |ipp_versions| are greater than or equal to 2.0.
bool AllowedIpp(const std::vector<base::Version>& ipp_versions) {
  auto found =
      std::find_if(ipp_versions.begin(), ipp_versions.end(),
                   [](const base::Version& version) {
                     return version.IsValid() && version.components()[0] >= 2;
                   });

  return found != ipp_versions.end();
}

// Returns true if |mime_type| is one of the supported types.
bool SupportedMime(const std::string& mime_type) {
  return mime_type == kPwgRasterMimeType || mime_type == kPdfMimeType;
}

// Returns true if |formats| contains one of the supported printer description
// languages for an autoconf printer identified by MIME type.
bool SupportsRequiredPDLS(const std::vector<std::string>& formats) {
  auto found = std::find_if(formats.begin(), formats.end(), &SupportedMime);
  return found != formats.end();
}

// Returns true if |info| describes a printer for which we want to attempt
// automatic configuration.
bool IsAutoconf(const ::printing::PrinterInfo& info) {
  return info.ipp_everywhere || (AllowedIpp(info.ipp_versions) &&
                                 SupportsRequiredPDLS(info.document_formats));
}

// Dispatches an IPP request to |host| to retrieve printer information.  Returns
// a nullptr if the request fails.
QueryResult QueryPrinterImpl(const std::string& host,
                             const int port,
                             const std::string& path,
                             bool encrypted) {
  QueryResult result;
  result.result = ::printing::GetPrinterInfo(host, port, path, encrypted,
                                             &result.printer_info);
  if (result.result != ::printing::PrinterQueryResult::SUCCESS) {
    LOG(ERROR) << "Could not retrieve printer info";
  }

  return result;
}

// Handles the request for |info|.  Parses make and model information before
// calling |callback|.
void OnPrinterQueried(chromeos::PrinterInfoCallback callback,
                      const QueryResult& query_result) {
  const ::printing::PrinterQueryResult& result = query_result.result;
  const ::printing::PrinterInfo& printer_info = query_result.printer_info;
  if (result != ::printing::PrinterQueryResult::SUCCESS) {
    VLOG(1) << "Could not reach printer";
    std::move(callback).Run(result, std::string(), std::string(), std::string(),
                            {}, false);
    return;
  }

  base::StringPiece make_and_model(printer_info.make_and_model);
  base::StringPiece make;
  base::StringPiece model;

  size_t split = ManufacturerLength(make_and_model);
  if (split != 0) {
    make = make_and_model.substr(0, split);
    model = make_and_model.substr(split + 1);
  } else {
    // If there's only one word or an empty string, use it.
    model = make_and_model;
  }

  base::UmaHistogramBoolean(kPwgRasterDocumentResolutionSupported,
                            printer_info.supports_pwg_raster_resolution);
  DCHECK(!printer_info.ipp_versions.empty())
      << "Properly queried PrinterInfo always has at least one version";
  base::UmaHistogramEnumeration(
      "Printing.CUPS.HighestIppVersion",
      ToIppVersion(*std::max_element(printer_info.ipp_versions.begin(),
                                     printer_info.ipp_versions.end())));

  std::move(callback).Run(
      result, make.as_string(), model.as_string(), printer_info.make_and_model,
      printer_info.document_formats, IsAutoconf(printer_info));
}

}  // namespace

namespace chromeos {

void QueryIppPrinter(const std::string& host,
                     const int port,
                     const std::string& path,
                     bool encrypted,
                     PrinterInfoCallback callback) {
  DCHECK(!host.empty());

  // QueryPrinterImpl could block on a network call for a noticable amount of
  // time (100s of ms). Also the user is waiting on this result.  Thus, run at
  // USER_VISIBLE with MayBlock.
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      base::TaskTraits{base::ThreadPool(), base::TaskPriority::USER_VISIBLE,
                       base::MayBlock()},
      base::BindOnce(&QueryPrinterImpl, host, port, path, encrypted),
      base::BindOnce(&OnPrinterQueried, std::move(callback)));
}

}  // namespace chromeos
