// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/preferences/autofill/autofill_payment_methods_delegate.h"

#include <memory>

#include "base/android/callback_android.h"
#include "chrome/android/chrome_jni_headers/AutofillPaymentMethodsDelegate_jni.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/risk_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/android/autofill/virtual_card_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;

namespace autofill {

// static
void RunVirtualCardEnrollmentFieldsLoadedCallback(
    const JavaRef<jobject>& j_callback,
    VirtualCardEnrollmentFields* virtual_card_enrollment_fields) {
  RunObjectCallbackAndroid(
      j_callback, autofill::CreateVirtualCardEnrollmentFieldsJavaObject(
                      virtual_card_enrollment_fields));
}

AutofillPaymentMethodsDelegate::AutofillPaymentMethodsDelegate(Profile* profile)
    : profile_(profile) {
  personal_data_manager_ = PersonalDataManagerFactory::GetForProfile(profile);
  payments_client_ = std::make_unique<payments::PaymentsClient>(
      profile->GetURLLoaderFactory(),
      IdentityManagerFactory::GetForProfile(profile), personal_data_manager_);
  virtual_card_enrollment_manager_ =
      std::make_unique<VirtualCardEnrollmentManager>(personal_data_manager_,
                                                     payments_client_.get());
}

AutofillPaymentMethodsDelegate::~AutofillPaymentMethodsDelegate() = default;

// Initializes an instance of AutofillPaymentMethodsDelegate from the
// Java side.
static jlong JNI_AutofillPaymentMethodsDelegate_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile) {
  AutofillPaymentMethodsDelegate* instance = new AutofillPaymentMethodsDelegate(
      ProfileAndroid::FromProfileAndroid(j_profile));
  return reinterpret_cast<intptr_t>(instance);
}

void AutofillPaymentMethodsDelegate::Cleanup(JNIEnv* env) {
  delete this;
}

void AutofillPaymentMethodsDelegate::OfferVirtualCardEnrollment(
    JNIEnv* env,
    int64_t instrument_id,
    const JavaParamRef<jobject>& jcallback) {
  CreditCard* credit_card =
      personal_data_manager_->GetCreditCardByInstrumentId(instrument_id);
  virtual_card_enrollment_manager_->OfferVirtualCardEnroll(
      *credit_card, VirtualCardEnrollmentSource::kSettingsPage,
      profile_->GetPrefs(), base::BindOnce(&risk_util::LoadRiskDataHelper),
      base::BindOnce(&RunVirtualCardEnrollmentFieldsLoadedCallback,
                     ScopedJavaGlobalRef<jobject>(jcallback)));
}

void AutofillPaymentMethodsDelegate::EnrollOfferedVirtualCard(JNIEnv* env) {
  virtual_card_enrollment_manager_->Enroll();
}

void AutofillPaymentMethodsDelegate::UnenrollVirtualCard(
    JNIEnv* env,
    int64_t instrument_id) {
  virtual_card_enrollment_manager_->Unenroll(instrument_id);
}
}  // namespace autofill
