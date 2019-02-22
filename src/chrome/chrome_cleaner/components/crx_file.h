// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Stolen from extensions/common/crx_file.h

#ifndef CHROME_CHROME_CLEANER_COMPONENTS_CRX_FILE_H_
#define CHROME_CHROME_CLEANER_COMPONENTS_CRX_FILE_H_

#include <stdint.h>
#include <sys/types.h>

#include <memory>

namespace chrome_cleaner {

// CRX files have a header that includes a magic key, version number, and
// some signature sizing information. Use CrxFile object to validate whether
// the header is valid or not.
class CrxFile {
 public:
  // The size of the magic character sequence at the beginning of each crx
  // file, in bytes. This should be a multiple of 4.
  static const size_t kCrxFileHeaderMagicSize = 4;

  // This header is the first data at the beginning of an extension. Its
  // contents are purposely 32-bit aligned so that it can just be slurped into
  // a struct without manual parsing.
  struct Header {
    char magic[kCrxFileHeaderMagicSize];
    uint32_t version;
    uint32_t key_size;        // The size of the public key, in bytes.
    uint32_t signature_size;  // The size of the signature, in bytes.
    // An ASN.1-encoded PublicKeyInfo structure follows.
    // The signature follows.
  };

  enum Error {
    kWrongMagic,
    kInvalidVersion,
    kInvalidKeyTooLarge,
    kInvalidKeyTooSmall,
    kInvalidSignatureTooLarge,
    kInvalidSignatureTooSmall,
  };

  // Construct a new CRX file header object with bytes of a header
  // read from a CRX file. If a null std::unique_ptr is returned, |error|
  // contains an error code with additional information.
  static std::unique_ptr<CrxFile> Parse(const Header& header, Error* error);

  // Construct a new header for the given key and signature sizes.
  // Returns a null std::unique_ptr if erroneous values of |key_size| and/or
  // |signature_size| are provided. |error| contains an error code with
  // additional information.
  // Use this constructor and then .header() to obtain the Header
  // for writing out to a CRX file.
  static std::unique_ptr<CrxFile> Create(const uint32_t key_size,
                                         const uint32_t signature_size,
                                         Error* error);

  // Returns the header structure for writing out to a CRX file.
  const Header& header() const { return header_; }

  // Checks a valid |header| to determine whether or not the CRX represents a
  // differential CRX.
  static bool HeaderIsDelta(const Header& header);

 private:
  Header header_;

  // Constructor is private. Clients should use static factory methods above.
  explicit CrxFile(const Header& header);

  // Checks the |header| for validity and returns true if the values are valid.
  // If false is returned, more detailed error code is returned in |error|.
  static bool HeaderIsValid(const Header& header, Error* error);
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_COMPONENTS_CRX_FILE_H_
