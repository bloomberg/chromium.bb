// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/location_bar_model_impl.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/omnibox/browser/buildflags.h"
#include "components/omnibox/browser/location_bar_model_delegate.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/prefs/pref_service.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/vector_icon_types.h"

#if (!defined(OS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !defined(OS_IOS)
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#include "components/vector_icons/vector_icons.h"     // nogncheck
#endif

LocationBarModelImpl::LocationBarModelImpl(LocationBarModelDelegate* delegate,
                                           size_t max_url_display_chars)
    : delegate_(delegate), max_url_display_chars_(max_url_display_chars) {
  DCHECK(delegate_);
}

LocationBarModelImpl::~LocationBarModelImpl() {}

// LocationBarModelImpl Implementation.
base::string16 LocationBarModelImpl::GetFormattedFullURL() const {
  return GetFormattedURL(url_formatter::kFormatUrlOmitDefaults);
}

base::string16 LocationBarModelImpl::GetURLForDisplay() const {
  url_formatter::FormatUrlTypes format_types =
      url_formatter::kFormatUrlOmitDefaults;

#if defined(OS_IOS)
  format_types |= url_formatter::kFormatUrlTrimAfterHost;
#else
  if (base::FeatureList::IsEnabled(
          omnibox::kHideSteadyStateUrlPathQueryAndRef)) {
    format_types |= url_formatter::kFormatUrlTrimAfterHost;
  }
#endif

  if (OmniboxFieldTrial::IsHideSteadyStateUrlSchemeEnabled())
    format_types |= url_formatter::kFormatUrlOmitHTTPS;

  if (OmniboxFieldTrial::IsHideSteadyStateUrlTrivialSubdomainsEnabled())
    format_types |= url_formatter::kFormatUrlOmitTrivialSubdomains;

  if (base::FeatureList::IsEnabled(omnibox::kHideFileUrlScheme))
    format_types |= url_formatter::kFormatUrlOmitFileScheme;

  return GetFormattedURL(format_types);
}

base::string16 LocationBarModelImpl::GetFormattedURL(
    url_formatter::FormatUrlTypes format_types) const {
  GURL url(GetURL());
  // Note that we can't unescape spaces here, because if the user copies this
  // and pastes it into another program, that program may think the URL ends at
  // the space.
  const base::string16 formatted_text =
      delegate_->FormattedStringWithEquivalentMeaning(
          url,
          url_formatter::FormatUrl(url, format_types, net::UnescapeRule::NORMAL,
                                   nullptr, nullptr, nullptr));

  // Truncating the URL breaks editing and then pressing enter, but hopefully
  // people won't try to do much with such enormous URLs anyway. If this becomes
  // a real problem, we could perhaps try to keep some sort of different "elided
  // visible URL" where editing affects and reloads the "real underlying URL",
  // but this seems very tricky for little gain.
  return gfx::TruncateString(formatted_text, max_url_display_chars_,
                             gfx::CHARACTER_BREAK);
}

GURL LocationBarModelImpl::GetURL() const {
  GURL url;
  return delegate_->GetURL(&url) ? url : GURL(url::kAboutBlankURL);
}

security_state::SecurityLevel LocationBarModelImpl::GetSecurityLevel(
    bool ignore_editing) const {
  // When editing or empty, assume no security style.
  if ((input_in_progress() && !ignore_editing) || !ShouldDisplayURL())
    return security_state::NONE;

  security_state::SecurityInfo info;
  delegate_->GetSecurityInfo(&info);
  return info.security_level;
}

bool LocationBarModelImpl::IsSecurityInfoInitialized() const {
  security_state::SecurityInfo info;
  delegate_->GetSecurityInfo(&info);
  return info.connection_info_initialized;
}

const gfx::VectorIcon& LocationBarModelImpl::GetVectorIcon() const {
#if (!defined(OS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !defined(OS_IOS)
  auto* const icon_override = delegate_->GetVectorIconOverride();
  if (icon_override)
    return *icon_override;

  if (IsOfflinePage())
    return omnibox::kOfflinePinIcon;

  switch (GetSecurityLevel(false)) {
    case security_state::NONE:
    case security_state::HTTP_SHOW_WARNING:
      return omnibox::kHttpIcon;
    case security_state::EV_SECURE:
    case security_state::SECURE:
      return omnibox::kHttpsValidIcon;
    case security_state::SECURE_WITH_POLICY_INSTALLED_CERT:
      return vector_icons::kBusinessIcon;
    case security_state::DANGEROUS:
      return omnibox::kHttpsInvalidIcon;
    case security_state::SECURITY_LEVEL_COUNT:
      NOTREACHED();
      return omnibox::kHttpIcon;
  }
  NOTREACHED();
  return omnibox::kHttpIcon;
#else
  NOTREACHED();
  static const gfx::VectorIcon dummy = {};
  return dummy;
#endif
}

base::string16 LocationBarModelImpl::GetEVCertName() const {
  if (GetSecurityLevel(false) != security_state::EV_SECURE)
    return base::string16();

  // Note: cert is guaranteed non-NULL or the security level would be NONE.
  scoped_refptr<net::X509Certificate> cert = delegate_->GetCertificate();
  DCHECK(cert);

  // EV are required to have an organization name and country.
  DCHECK(!cert->subject().organization_names.empty());
  DCHECK(!cert->subject().country_name.empty());
  return l10n_util::GetStringFUTF16(
      IDS_SECURE_CONNECTION_EV,
      base::UTF8ToUTF16(cert->subject().organization_names[0]),
      base::UTF8ToUTF16(cert->subject().country_name));
}

LocationBarModelImpl::SecureChipText LocationBarModelImpl::GetSecureChipText()
    const {
  // Note that displayed text (the first output) will be implicitly used as the
  // accessibility text unless no display text has been specified.

  // Security UI study (https://crbug.com/803501): Change EV/Secure text.
  const std::string securityUIStudyParam =
      base::FeatureList::IsEnabled(omnibox::kSimplifyHttpsIndicator)
          ? base::GetFieldTrialParamValueByFeature(
                omnibox::kSimplifyHttpsIndicator,
                OmniboxFieldTrial::kSimplifyHttpsIndicatorParameterName)
          : std::string();

  if (IsOfflinePage())
    return SecureChipText(l10n_util::GetStringUTF16(IDS_OFFLINE_VERBOSE_STATE));

  switch (GetSecurityLevel(false)) {
    case security_state::HTTP_SHOW_WARNING:
      return SecureChipText(
          l10n_util::GetStringUTF16(IDS_NOT_SECURE_VERBOSE_STATE));
    case security_state::EV_SECURE:
      if (securityUIStudyParam ==
          OmniboxFieldTrial::kSimplifyHttpsIndicatorParameterEvToSecure)
        return SecureChipText(
            l10n_util::GetStringUTF16(IDS_SECURE_VERBOSE_STATE));
      if (securityUIStudyParam ==
          OmniboxFieldTrial::kSimplifyHttpsIndicatorParameterBothToLock)
        return SecureChipText(base::string16(), l10n_util::GetStringUTF16(
                                                    IDS_SECURE_VERBOSE_STATE));
      return SecureChipText(GetEVCertName());
    case security_state::SECURE:
      if (securityUIStudyParam !=
          OmniboxFieldTrial::kSimplifyHttpsIndicatorParameterKeepSecureChip)
        return SecureChipText(base::string16(), l10n_util::GetStringUTF16(
                                                    IDS_SECURE_VERBOSE_STATE));
      return SecureChipText(
          l10n_util::GetStringUTF16(IDS_SECURE_VERBOSE_STATE));
    case security_state::DANGEROUS:
      if (delegate_->FailsMalwareCheck())
        return SecureChipText(
            l10n_util::GetStringUTF16(IDS_DANGEROUS_VERBOSE_STATE));
      // Don't show any text in the security indicator for sites on the billing
      // interstitial list.
      return SecureChipText(
          delegate_->FailsBillingCheck()
              ? base::string16()
              : l10n_util::GetStringUTF16(IDS_NOT_SECURE_VERBOSE_STATE));
    default:
      return SecureChipText(base::string16());
  }
}

base::string16 LocationBarModelImpl::GetSecureDisplayText() const {
  return GetSecureChipText().display_text_;
}

base::string16 LocationBarModelImpl::GetSecureAccessibilityText() const {
  auto labels = GetSecureChipText();
  return labels.display_text_.empty() ? labels.accessibility_label_
                                      : labels.display_text_;
}

bool LocationBarModelImpl::ShouldDisplayURL() const {
  return delegate_->ShouldDisplayURL();
}

bool LocationBarModelImpl::IsOfflinePage() const {
  return delegate_->IsOfflinePage();
}
