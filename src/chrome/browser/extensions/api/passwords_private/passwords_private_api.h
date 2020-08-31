// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_API_H_

#include <string>

#include "base/optional.h"
#include "base/strings/string16.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "components/password_manager/core/browser/bulk_leak_check_service.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class PasswordsPrivateRecordPasswordsPageAccessInSettingsFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "passwordsPrivate.recordPasswordsPageAccessInSettings",
      PASSWORDSPRIVATE_RECORDPASSWORDSPAGEACCESSINSETTINGS)

 protected:
  ~PasswordsPrivateRecordPasswordsPageAccessInSettingsFunction() override =
      default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateChangeSavedPasswordFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.changeSavedPassword",
                             PASSWORDSPRIVATE_CHANGESAVEDPASSWORD)

 protected:
  ~PasswordsPrivateChangeSavedPasswordFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateRemoveSavedPasswordFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.removeSavedPassword",
                             PASSWORDSPRIVATE_REMOVESAVEDPASSWORD)

 protected:
  ~PasswordsPrivateRemoveSavedPasswordFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateRemovePasswordExceptionFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.removePasswordException",
                             PASSWORDSPRIVATE_REMOVEPASSWORDEXCEPTION)

 protected:
  ~PasswordsPrivateRemovePasswordExceptionFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateUndoRemoveSavedPasswordOrExceptionFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "passwordsPrivate.undoRemoveSavedPasswordOrException",
      PASSWORDSPRIVATE_UNDOREMOVESAVEDPASSWORDOREXCEPTION)

 protected:
  ~PasswordsPrivateUndoRemoveSavedPasswordOrExceptionFunction() override =
      default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateRequestPlaintextPasswordFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.requestPlaintextPassword",
                             PASSWORDSPRIVATE_REQUESTPLAINTEXTPASSWORD)

 protected:
  ~PasswordsPrivateRequestPlaintextPasswordFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void GotPassword(base::Optional<base::string16> password);
};

class PasswordsPrivateGetSavedPasswordListFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.getSavedPasswordList",
                             PASSWORDSPRIVATE_GETSAVEDPASSWORDLIST)

 protected:
  ~PasswordsPrivateGetSavedPasswordListFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void GetList();
  void GotList(const PasswordsPrivateDelegate::UiEntries& entries);
};

class PasswordsPrivateGetPasswordExceptionListFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.getPasswordExceptionList",
                             PASSWORDSPRIVATE_GETPASSWORDEXCEPTIONLIST)

 protected:
  ~PasswordsPrivateGetPasswordExceptionListFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void GetList();
  void GotList(const PasswordsPrivateDelegate::ExceptionEntries& entries);
};

class PasswordsPrivateImportPasswordsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.importPasswords",
                             PASSWORDSPRIVATE_IMPORTPASSWORDS)

 protected:
  ~PasswordsPrivateImportPasswordsFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateExportPasswordsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.exportPasswords",
                             PASSWORDSPRIVATE_EXPORTPASSWORDS)

 protected:
  ~PasswordsPrivateExportPasswordsFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void ExportRequestCompleted(const std::string& error);
};

class PasswordsPrivateCancelExportPasswordsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.cancelExportPasswords",
                             PASSWORDSPRIVATE_CANCELEXPORTPASSWORDS)

 protected:
  ~PasswordsPrivateCancelExportPasswordsFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateRequestExportProgressStatusFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.requestExportProgressStatus",
                             PASSWORDSPRIVATE_REQUESTEXPORTPROGRESSSTATUS)

 protected:
  ~PasswordsPrivateRequestExportProgressStatusFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateIsOptedInForAccountStorageFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.isOptedInForAccountStorage",
                             PASSWORDSPRIVATE_ISOPTEDINFORACCOUNTSTORAGE)

 protected:
  ~PasswordsPrivateIsOptedInForAccountStorageFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateOptInForAccountStorageFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.optInForAccountStorage",
                             PASSWORDSPRIVATE_OPTINFORACCOUNTSTORAGE)

 protected:
  ~PasswordsPrivateOptInForAccountStorageFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateGetCompromisedCredentialsFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.getCompromisedCredentials",
                             PASSWORDSPRIVATE_GETCOMPROMISEDCREDENTIALS)

 protected:
  ~PasswordsPrivateGetCompromisedCredentialsFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateGetPlaintextCompromisedPasswordFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.getPlaintextCompromisedPassword",
                             PASSWORDSPRIVATE_GETPLAINTEXTCOMPROMISEDPASSWORD)

 protected:
  ~PasswordsPrivateGetPlaintextCompromisedPasswordFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void GotCredential(
      base::Optional<api::passwords_private::CompromisedCredential> credential);
};

class PasswordsPrivateChangeCompromisedCredentialFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.changeCompromisedCredential",
                             PASSWORDSPRIVATE_CHANGECOMPROMISEDCREDENTIAL)

 protected:
  ~PasswordsPrivateChangeCompromisedCredentialFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateRemoveCompromisedCredentialFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.removeCompromisedCredential",
                             PASSWORDSPRIVATE_REMOVECOMPROMISEDCREDENTIAL)

 protected:
  ~PasswordsPrivateRemoveCompromisedCredentialFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateStartPasswordCheckFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.startPasswordCheck",
                             PASSWORDSPRIVATE_STARTPASSWORDCHECK)

 protected:
  ~PasswordsPrivateStartPasswordCheckFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void OnStarted(password_manager::BulkLeakCheckService::State state);
};

class PasswordsPrivateStopPasswordCheckFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.stopPasswordCheck",
                             PASSWORDSPRIVATE_STOPPASSWORDCHECK)

 protected:
  ~PasswordsPrivateStopPasswordCheckFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class PasswordsPrivateGetPasswordCheckStatusFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.getPasswordCheckStatus",
                             PASSWORDSPRIVATE_GETPASSWORDCHECKSTATUS)

 protected:
  ~PasswordsPrivateGetPasswordCheckStatusFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_API_H_
