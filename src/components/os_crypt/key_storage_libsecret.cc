// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/key_storage_libsecret.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "components/os_crypt/libsecret_util_linux.h"

namespace {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const char kApplicationName[] = "chrome";
#else
const char kApplicationName[] = "chromium";
#endif

const SecretSchema kKeystoreSchemaV2 = {
    "chrome_libsecret_os_crypt_password_v2",
    SECRET_SCHEMA_DONT_MATCH_NAME,
    {
        {"application", SECRET_SCHEMA_ATTRIBUTE_STRING},
        {nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING},
    }};

// From a search result, extracts a SecretValue, asserting that there was at
// most one result. Returns nullptr if there are no results.
SecretValue* ToSingleSecret(GList* secret_items) {
  GList* first = g_list_first(secret_items);
  if (first == nullptr)
    return nullptr;
  if (g_list_next(first) != nullptr) {
    VLOG(1) << "OSCrypt found more than one encryption keys.";
  }
  SecretItem* secret_item = static_cast<SecretItem*>(first->data);
  SecretValue* secret_value =
      LibsecretLoader::secret_item_get_secret(secret_item);
  return secret_value;
}

// Checks the timestamps of the secret item and prints findings to logs. We
// presume that at most one secret item can be present.
void AnalyseKeyHistory(GList* secret_items) {
  GList* first = g_list_first(secret_items);
  if (first == nullptr)
    return;

  SecretItem* secret_item = static_cast<SecretItem*>(first->data);
  auto created = base::Time::FromTimeT(
      LibsecretLoader::secret_item_get_created(secret_item));
  auto last_modified = base::Time::FromTimeT(
      LibsecretLoader::secret_item_get_modified(secret_item));

  VLOG(1) << "Libsecret key created: " << created;
  VLOG(1) << "Libsecret key last modified: " << last_modified;
  LOG_IF(WARNING, created != last_modified)
      << "the encryption key has been modified since it was created.";
}

}  // namespace

base::Optional<std::string>
KeyStorageLibsecret::AddRandomPasswordInLibsecret() {
  std::string password;
  base::Base64Encode(base::RandBytesAsString(16), &password);
  GError* error = nullptr;
  bool success = LibsecretLoader::secret_password_store_sync(
      &kKeystoreSchemaV2, nullptr, KeyStorageLinux::kKey, password.c_str(),
      nullptr, &error, "application", kApplicationName, nullptr);
  if (error) {
    VLOG(1) << "Libsecret lookup failed: " << error->message;
    g_error_free(error);
    return base::nullopt;
  }
  if (!success) {
    VLOG(1) << "Libsecret lookup failed.";
    return base::nullopt;
  }

  VLOG(1) << "OSCrypt generated a new password.";
  return password;
}

base::Optional<std::string> KeyStorageLibsecret::GetKeyImpl() {
  LibsecretAttributesBuilder attrs;
  attrs.Append("application", kApplicationName);

  LibsecretLoader::SearchHelper helper;
  helper.Search(&kKeystoreSchemaV2, attrs.Get(),
                SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS);
  if (!helper.success()) {
    VLOG(1) << "Libsecret lookup failed: " << helper.error()->message;
    return base::nullopt;
  }

  SecretValue* password_libsecret = ToSingleSecret(helper.results());
  if (!password_libsecret) {
    return AddRandomPasswordInLibsecret();
  }
  AnalyseKeyHistory(helper.results());
  base::Optional<std::string> password(
      LibsecretLoader::secret_value_get_text(password_libsecret));
  LibsecretLoader::secret_value_unref(password_libsecret);
  return password;
}

bool KeyStorageLibsecret::Init() {
  bool loaded = LibsecretLoader::EnsureLibsecretLoaded();
  if (loaded)
    LibsecretLoader::EnsureKeyringUnlocked();
  return loaded;
}
