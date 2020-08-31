// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_ARCHIVABLE_CREDENTIAL_PASSWORD_FORM_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_ARCHIVABLE_CREDENTIAL_PASSWORD_FORM_H_

#import "ios/chrome/common/credential_provider/archivable_credential.h"

namespace autofill {
struct PasswordForm;
}

// Category for adding convenience logic related to PasswordForms.
@interface ArchivableCredential (PasswordForm)

// Convenience initializer from a PasswordForm. Will return nil for forms
// blacklisted by the user, with an empty origin or Android forms.
- (instancetype)initWithPasswordForm:(const autofill::PasswordForm&)passwordForm
                             favicon:(NSString*)favicon
                validationIdentifier:(NSString*)validationIdentifier;

@end

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_ARCHIVABLE_CREDENTIAL_PASSWORD_FORM_H_
