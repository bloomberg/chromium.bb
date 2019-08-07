// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_SYSTEM_ENCRYPTOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_SYSTEM_ENCRYPTOR_H_

#include "base/macros.h"
#include "components/autofill/core/browser/webdata/autofill_table_encryptor.h"

namespace autofill {
// Default encryptor used in Autofill table.
class SystemEncryptor : public AutofillTableEncryptor {
 public:
  SystemEncryptor() = default;
  ~SystemEncryptor() override = default;

  bool EncryptString16(const base::string16& plaintext,
                       std::string* ciphertext) const override;

  bool DecryptString16(const std::string& ciphertext,
                       base::string16* plaintext) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemEncryptor);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_SYSTEM_ENCRYPTOR_H_
