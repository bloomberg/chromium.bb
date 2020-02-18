// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_IMPORTER_NSS_DECRYPTOR_SYSTEM_NSS_H_
#define CHROME_UTILITY_IMPORTER_NSS_DECRYPTOR_SYSTEM_NSS_H_

#include <secmodt.h>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"

struct FirefoxRawPasswordInfo;

namespace autofill {
struct PasswordForm;
}

namespace base {
class FilePath;
}

// A wrapper for Firefox NSS decrypt component.
class NSSDecryptor {
 public:
  NSSDecryptor();
  ~NSSDecryptor();

  // Initializes NSS if it hasn't already been initialized.
  bool Init(const base::FilePath& dll_path, const base::FilePath& db_path);

  // Decrypts Firefox stored passwords. Before using this method,
  // make sure Init() returns true.
  base::string16 Decrypt(const std::string& crypt) const;

  // Parses the Firefox password file content, decrypts the
  // username/password and reads other related information.
  // The result will be stored in |forms|.
  void ParseSignons(const base::FilePath& signon_file,
                    std::vector<autofill::PasswordForm>* forms);

  // Reads and parses the Firefox password sqlite db, decrypts the
  // username/password and reads other related information.
  // The result will be stored in |forms|.
  bool ReadAndParseSignons(const base::FilePath& sqlite_file,
                           std::vector<autofill::PasswordForm>* forms);

  // Reads and parses the Firefox password file logins.json, decrypts the
  // username/password and reads other related information.
  // The result will be stored in |forms|.
  bool ReadAndParseLogins(const base::FilePath& json_file,
                          std::vector<autofill::PasswordForm>* forms);

 private:
  // Does not actually free the slot, since we'll free it when NSSDecryptor is
  // destroyed.
  void FreeSlot(PK11SlotInfo* slot) const {}

  // Turns unprocessed information extracted from Firefox's password file
  // into PasswordForm.
  bool CreatePasswordFormFromRawInfo(
      const FirefoxRawPasswordInfo& raw_password_info,
      autofill::PasswordForm* form);

  PK11SlotInfo* GetKeySlotForDB() const { return db_slot_; }

  SECStatus PK11SDR_DecryptWithSlot(
      PK11SlotInfo* slot, SECItem* data, SECItem* result, void* cx) const;

  bool is_nss_initialized_;
  PK11SlotInfo* db_slot_;

  DISALLOW_COPY_AND_ASSIGN(NSSDecryptor);
};

#endif  // CHROME_UTILITY_IMPORTER_NSS_DECRYPTOR_SYSTEM_NSS_H_
