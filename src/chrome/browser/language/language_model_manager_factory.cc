// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/language/language_model_manager_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/language/content/browser/geo_language_model.h"
#include "components/language/content/browser/geo_language_provider.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/language/core/browser/locale_util.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/browser/ulp_metrics_logger.h"
#include "components/language/core/common/language_experiments.h"
#include "components/language/core/language_model/baseline_language_model.h"
#include "components/language/core/language_model/fluent_language_model.h"
#include "components/language/core/language_model/ulp_language_model.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/translate_prefs.h"

#if defined(OS_ANDROID)
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/language/android/jni_headers/LanguageBridge_jni.h"
#endif

namespace {

#if defined(OS_ANDROID)
// Records per-initialization ULP-related metrics.
void RecordULPInitMetrics(Profile* profile,
                          const std::vector<std::string>& ulp_languages) {
  language::ULPMetricsLogger logger;

  logger.RecordInitiationLanguageCount(ulp_languages.size());

  PrefService* pref_service = profile->GetPrefs();
  const std::string app_locale = g_browser_process->GetApplicationLocale();
  logger.RecordInitiationUILanguageInULP(
      logger.DetermineLanguageStatus(app_locale, ulp_languages));

  const std::string translate_target_language =
      translate::TranslatePrefs(pref_service).GetRecentTargetLanguage();
  logger.RecordInitiationTranslateTargetInULP(
      logger.DetermineLanguageStatus(translate_target_language, ulp_languages));

  std::vector<std::string> accept_languages;
  language::LanguagePrefs(pref_service)
      .GetAcceptLanguagesList(&accept_languages);

  language::ULPLanguageStatus accept_language_status =
      language::ULPLanguageStatus::kLanguageNotInULP;
  if (accept_languages.size() > 0) {
    accept_language_status =
        logger.DetermineLanguageStatus(accept_languages[0], ulp_languages);
  }
  logger.RecordInitiationTopAcceptLanguageInULP(accept_language_status);

  logger.RecordInitiationAcceptLanguagesULPOverlap(
      logger.ULPLanguagesInAcceptLanguagesRatio(accept_languages,
                                                ulp_languages));
}

std::vector<std::string> JavaLanguageBridgeGetULPLanguagesWrapper(
    std::string account_name) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> account_name_java =
      base::android::ConvertUTF8ToJavaString(env, account_name);
  base::android::ScopedJavaLocalRef<jobjectArray> languages_java =
      Java_LanguageBridge_getULPLanguages(env, account_name_java);

  const int num_langs = (*env).GetArrayLength(languages_java.obj());
  std::vector<std::string> languages;
  for (int i = 0; i < num_langs; i++) {
    jstring language_name_java =
        (jstring)(*env).GetObjectArrayElement(languages_java.obj(), i);
    languages.emplace_back(
        base::android::ConvertJavaStringToUTF8(env, language_name_java));
  }

  return languages;
}

void CreateAndAddULPLanguageModel(Profile* profile,
                                  std::vector<std::string> languages) {
  RecordULPInitMetrics(profile, languages);

  std::unique_ptr<language::ULPLanguageModel> ulp_model =
      std::make_unique<language::ULPLanguageModel>();

  int score_divisor = 1;
  for (std::string lang : languages) {
    // List of languages is already ordered by preference, generate scores
    // accordingly.
    ulp_model->AddULPLanguage(lang, 1.0f / score_divisor);
    score_divisor++;
  }

  language::LanguageModelManager* manager =
      LanguageModelManagerFactory::GetForBrowserContext(profile);
  manager->AddModel(language::LanguageModelManager::ModelType::ULP,
                    std::move(ulp_model));
}
#endif

void PrepareLanguageModels(Profile* const profile,
                           language::LanguageModelManager* const manager) {
  // Create and set the primary Language Model to use based on the state of
  // experiments.
  switch (language::GetOverrideLanguageModel()) {
    case language::OverrideLanguageModel::FLUENT:
      manager->AddModel(
          language::LanguageModelManager::ModelType::FLUENT,
          std::make_unique<language::FluentLanguageModel>(profile->GetPrefs()));
      manager->SetPrimaryModel(
          language::LanguageModelManager::ModelType::FLUENT);
      break;
    case language::OverrideLanguageModel::GEO:
      manager->AddModel(language::LanguageModelManager::ModelType::GEO,
                        std::make_unique<language::GeoLanguageModel>(
                            language::GeoLanguageProvider::GetInstance()));
      manager->SetPrimaryModel(language::LanguageModelManager::ModelType::GEO);
      break;
    case language::OverrideLanguageModel::DEFAULT:
    default:
      manager->AddModel(
          language::LanguageModelManager::ModelType::BASELINE,
          std::make_unique<language::BaselineLanguageModel>(
              profile->GetPrefs(), g_browser_process->GetApplicationLocale(),
              language::prefs::kAcceptLanguages));
      manager->SetPrimaryModel(
          language::LanguageModelManager::ModelType::BASELINE);
      break;
  }

    // On Android, additionally create a ULPLanguageModel and populate it with
    // ULP data.
#if defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(language::kUseULPLanguagesInChrome)) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&JavaLanguageBridgeGetULPLanguagesWrapper,
                       profile->GetProfileUserName()),
        base::BindOnce(&CreateAndAddULPLanguageModel, profile));
  }
#endif
}

}  // namespace

// static
LanguageModelManagerFactory* LanguageModelManagerFactory::GetInstance() {
  return base::Singleton<LanguageModelManagerFactory>::get();
}

// static
language::LanguageModelManager*
LanguageModelManagerFactory::GetForBrowserContext(
    content::BrowserContext* const browser_context) {
  return static_cast<language::LanguageModelManager*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

LanguageModelManagerFactory::LanguageModelManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "LanguageModelManager",
          BrowserContextDependencyManager::GetInstance()) {}

LanguageModelManagerFactory::~LanguageModelManagerFactory() {}

KeyedService* LanguageModelManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* const browser_context) const {
  Profile* const profile = Profile::FromBrowserContext(browser_context);
  language::LanguageModelManager* manager = new language::LanguageModelManager(
      profile->GetPrefs(), g_browser_process->GetApplicationLocale());
  PrepareLanguageModels(profile, manager);
  return manager;
}

content::BrowserContext* LanguageModelManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Use the original profile's language model even in Incognito mode.
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}
