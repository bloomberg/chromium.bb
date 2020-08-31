// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_utils.h"

#include <unicode/ulocdata.h>

#include <algorithm>
#include <cmath>
#include <string>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "printing/units.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_elider.h"

namespace printing {

namespace {

constexpr size_t kMaxDocumentTitleLength = 80;
constexpr gfx::Size kIsoA4Microns = gfx::Size(210000, 297000);

constexpr int kMicronsPerMM = 1000;
constexpr double kMMPerInch = 25.4;
constexpr double kMicronsPerInch = kMMPerInch * kMicronsPerMM;

// Defines two prefixes of a special breed of media sizes not meant for
// users' eyes. CUPS incidentally returns these IPP values to us, but
// we have no use for them.
constexpr base::StringPiece kMediaCustomMinPrefix = "custom_min";
constexpr base::StringPiece kMediaCustomMaxPrefix = "custom_max";

enum Unit {
  INCHES,
  MILLIMETERS,
};

gfx::Size DimensionsToMicrons(base::StringPiece value) {
  Unit unit;
  base::StringPiece dims;
  size_t unit_position;
  if ((unit_position = value.find("mm")) != base::StringPiece::npos) {
    unit = MILLIMETERS;
    dims = value.substr(0, unit_position);
  } else if ((unit_position = value.find("in")) != base::StringPiece::npos) {
    unit = INCHES;
    dims = value.substr(0, unit_position);
  } else {
    LOG(WARNING) << "Could not parse paper dimensions";
    return {0, 0};
  }

  double width;
  double height;
  std::vector<std::string> pieces = base::SplitString(
      dims, "x", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (pieces.size() != 2 || !base::StringToDouble(pieces[0], &width) ||
      !base::StringToDouble(pieces[1], &height)) {
    return {0, 0};
  }

  int width_microns;
  int height_microns;
  switch (unit) {
    case MILLIMETERS:
      width_microns = width * kMicronsPerMM;
      height_microns = height * kMicronsPerMM;
      break;
    case INCHES:
      width_microns = width * kMicronsPerInch;
      height_microns = height * kMicronsPerInch;
      break;
    default:
      NOTREACHED();
      break;
  }

  return gfx::Size{width_microns, height_microns};
}

}  // namespace

base::string16 SimplifyDocumentTitleWithLength(const base::string16& title,
                                               size_t length) {
  base::string16 no_controls(title);
  no_controls.erase(
      std::remove_if(no_controls.begin(), no_controls.end(), &u_iscntrl),
      no_controls.end());

  static constexpr const char* kCharsToReplace[] = {
      "\\", "/", "<", ">", ":", "\"", "'", "|", "?", "*", "~",
  };
  for (const char* c : kCharsToReplace) {
    base::ReplaceChars(no_controls, base::ASCIIToUTF16(c),
                       base::ASCIIToUTF16("_"), &no_controls);
  }

  base::string16 result;
  gfx::ElideString(no_controls, length, &result);
  return result;
}

base::string16 FormatDocumentTitleWithOwnerAndLength(
    const base::string16& owner,
    const base::string16& title,
    size_t length) {
  const base::string16 separator = base::ASCIIToUTF16(": ");
  DCHECK_LT(separator.size(), length);

  base::string16 short_title =
      SimplifyDocumentTitleWithLength(owner, length - separator.size());
  short_title += separator;
  if (short_title.size() < length) {
    short_title +=
        SimplifyDocumentTitleWithLength(title, length - short_title.size());
  }

  return short_title;
}

base::string16 SimplifyDocumentTitle(const base::string16& title) {
  return SimplifyDocumentTitleWithLength(title, kMaxDocumentTitleLength);
}

base::string16 FormatDocumentTitleWithOwner(const base::string16& owner,
                                            const base::string16& title) {
  return FormatDocumentTitleWithOwnerAndLength(owner, title,
                                               kMaxDocumentTitleLength);
}

gfx::Size GetDefaultPaperSizeFromLocaleMicrons(base::StringPiece locale) {
  if (locale.empty())
    return kIsoA4Microns;

  int32_t width = 0;
  int32_t height = 0;
  UErrorCode error = U_ZERO_ERROR;
  ulocdata_getPaperSize(locale.as_string().c_str(), &height, &width, &error);
  if (error > U_ZERO_ERROR) {
    // If the call failed, assume Letter paper size.
    LOG(WARNING) << "ulocdata_getPaperSize failed, using ISO A4 Paper, error: "
                 << error;

    return kIsoA4Microns;
  }
  // Convert millis to microns
  return gfx::Size(width * 1000, height * 1000);
}

bool SizesEqualWithinEpsilon(const gfx::Size& lhs,
                             const gfx::Size& rhs,
                             int epsilon) {
  DCHECK_GE(epsilon, 0);

  if (lhs.IsEmpty() && rhs.IsEmpty())
    return true;

  return std::abs(lhs.width() - rhs.width()) <= epsilon &&
         std::abs(lhs.height() - rhs.height()) <= epsilon;
}

// We read the media name expressed by |value| and return a Paper
// with the vendor_id and size_um members populated.
// We don't handle l10n here. We do populate the display_name member
// with the prettified vendor ID, but fully expect the caller to clobber
// this if a better localization exists.
PrinterSemanticCapsAndDefaults::Paper ParsePaper(base::StringPiece value) {
  // <name>_<width>x<height>{in,mm}
  // e.g. na_letter_8.5x11in, iso_a4_210x297mm

  std::vector<base::StringPiece> pieces = base::SplitStringPiece(
      value, "_", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  // We expect at least a display string and a dimension string.
  // Additionally, we drop the "custom_min*" and "custom_max*" special
  // "sizes" (not for users' eyes).
  if (pieces.size() < 2 || value.starts_with(kMediaCustomMinPrefix) ||
      value.starts_with(kMediaCustomMaxPrefix))
    return PrinterSemanticCapsAndDefaults::Paper();

  base::StringPiece dimensions = pieces.back();

  PrinterSemanticCapsAndDefaults::Paper paper;
  paper.vendor_id = value.as_string();
  paper.size_um = DimensionsToMicrons(dimensions);
  // Omits the final token describing the media dimensions.
  pieces.pop_back();
  paper.display_name = base::JoinString(pieces, " ");

  return paper;
}

}  // namespace printing
