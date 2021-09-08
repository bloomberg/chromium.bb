// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_brand.h"

#include <algorithm>
#include <string>

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/installer/util/google_update_settings.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if defined(OS_MAC)
#include "chrome/browser/mac/keystone_glue.h"
#elif BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/google/google_brand_chromeos.h"
#endif


// Helpers --------------------------------------------------------------------

namespace {

const char* g_brand_for_testing = NULL;

}  // namespace


namespace google_brand {

// Global functions -----------------------------------------------------------

#if defined(OS_WIN)

bool GetBrand(std::string* brand) {
  if (g_brand_for_testing) {
    brand->assign(g_brand_for_testing);
    return true;
  }

  // Cache brand code value, since it is queried a lot and registry queries are
  // slow enough to actually affect top-level metrics like
  // Omnibox.CharTypedToRepaintLatency.
  static const base::NoDestructor<absl::optional<std::string>> brand_code(
      []() -> absl::optional<std::string> {
        std::wstring brandw;
        if (!GoogleUpdateSettings::GetBrand(&brandw))
          return absl::nullopt;
        return base::WideToASCII(brandw);
      }());
  if (!brand_code->has_value())
    return false;
  brand->assign(**brand_code);
  return true;
}

bool GetReactivationBrand(std::string* brand) {
  std::wstring brandw;
  bool ret = GoogleUpdateSettings::GetReactivationBrand(&brandw);
  if (ret)
    brand->assign(base::WideToASCII(brandw));
  return ret;
}

#else

bool GetBrand(std::string* brand) {
  if (g_brand_for_testing) {
    brand->assign(g_brand_for_testing);
    return true;
  }

#if defined(OS_MAC)
  brand->assign(keystone_glue::BrandCode());
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  brand->assign(google_brand::chromeos::GetBrand());
#else
  brand->clear();
#endif
  return true;
}

bool GetReactivationBrand(std::string* brand) {
  brand->clear();
  return true;
}

#endif

bool GetRlzBrand(std::string* brand) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  brand->assign(google_brand::chromeos::GetRlzBrand());
  return true;
#else
  return GetBrand(brand);
#endif
}

bool IsOrganic(const std::string& brand) {
  if (brand.empty()) {
    // An empty brand string is considered the same as GGLS, which is organic.
    // On Mac, channels other than stable never have a brand code. Linux,
    // FreeBSD, and OpenBSD never have a brand code. Such installs are always
    // organic.
    return true;
  }

  const char* const kOrganicBrands[] = {
      "CHCA", "CHCB", "CHCG", "CHCH", "CHCI", "CHCJ", "CHCK", "CHCL", "CHFO",
      "CHFT", "CHHS", "CHHM", "CHMA", "CHMB", "CHME", "CHMF", "CHMG", "CHMH",
      "CHMI", "CHMQ", "CHMV", "CHNB", "CHNC", "CHNG", "CHNH", "CHNI", "CHOA",
      "CHOB", "CHOC", "CHON", "CHOO", "CHOP", "CHOQ", "CHOR", "CHOS", "CHOT",
      "CHOU", "CHOX", "CHOY", "CHOZ", "CHPD", "CHPE", "CHPF", "CHPG", "ECBA",
      "ECBB", "ECDA", "ECDB", "ECSA", "ECSB", "ECVA", "ECVB", "ECWA", "ECWB",
      "ECWC", "ECWD", "ECWE", "ECWF", "EUBB", "EUBC", "GGLA", "GGLS"};
  const char* const* end = &kOrganicBrands[base::size(kOrganicBrands)];
  if (std::binary_search(&kOrganicBrands[0], end, brand))
    return true;

  // The Chrome enterprise brand code is the only GGR* brand to be non-organic.
  if (brand == "GGRV")
    return false;

  return base::StartsWith(brand, "EUB", base::CompareCase::SENSITIVE) ||
         base::StartsWith(brand, "EUC", base::CompareCase::SENSITIVE) ||
         base::StartsWith(brand, "GGR", base::CompareCase::SENSITIVE);
}

bool IsOrganicFirstRun(const std::string& brand) {
  if (brand.empty()) {
    // An empty brand string is the same as GGLS, which is organic.
    return true;
  }

  return base::StartsWith(brand, "GG", base::CompareCase::SENSITIVE) ||
         base::StartsWith(brand, "EU", base::CompareCase::SENSITIVE);
}

bool IsInternetCafeBrandCode(const std::string& brand) {
  const char* const kBrands[] = {
    "CHIQ", "CHSG", "HLJY", "NTMO", "OOBA", "OOBB", "OOBC", "OOBD", "OOBE",
    "OOBF", "OOBG", "OOBH", "OOBI", "OOBJ", "IDCM",
  };
  return base::Contains(kBrands, brand);
}

bool IsEnterprise(const std::string& brand) {
  return brand == "GGRV" ||
         base::StartsWith(brand, "GCE", base::CompareCase::SENSITIVE);
}

// BrandForTesting ------------------------------------------------------------

BrandForTesting::BrandForTesting(const std::string& brand) : brand_(brand) {
  DCHECK(g_brand_for_testing == NULL);
  g_brand_for_testing = brand_.c_str();
}

BrandForTesting::~BrandForTesting() {
  g_brand_for_testing = NULL;
}


}  // namespace google_brand
