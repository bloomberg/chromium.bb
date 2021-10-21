// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/task_module/task_module_service.h"

#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/common/google_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search/ntp_features.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"

namespace {
const char kXSSIResponsePreamble[] = ")]}'";

const char* GetPath(task_module::mojom::TaskModuleType task_module_type) {
  switch (task_module_type) {
    case task_module::mojom::TaskModuleType::kRecipe:
      return "/async/newtab_recipe_tasks";
    case task_module::mojom::TaskModuleType::kShopping:
      return "/async/newtab_shopping_tasks";
  }
}

// We return a reference so that base::FeatureList::CheckFeatureIdentity
// succeeds.
const base::Feature& GetFeature(
    task_module::mojom::TaskModuleType task_module_type) {
  switch (task_module_type) {
    case task_module::mojom::TaskModuleType::kRecipe:
      return ntp_features::kNtpRecipeTasksModule;
    case task_module::mojom::TaskModuleType::kShopping:
      return ntp_features::kNtpShoppingTasksModule;
  }
}

const char* GetDataParam(task_module::mojom::TaskModuleType task_module_type) {
  switch (task_module_type) {
    case task_module::mojom::TaskModuleType::kRecipe:
      return ntp_features::kNtpRecipeTasksModuleDataParam;
    case task_module::mojom::TaskModuleType::kShopping:
      return ntp_features::kNtpShoppingTasksModuleDataParam;
  }
}

const char* GetCacheMaxAgeSParam(
    task_module::mojom::TaskModuleType task_module_type) {
  switch (task_module_type) {
    case task_module::mojom::TaskModuleType::kRecipe:
      return ntp_features::kNtpRecipeTasksModuleCacheMaxAgeSParam;
    case task_module::mojom::TaskModuleType::kShopping:
      return ntp_features::kNtpShoppingTasksModuleCacheMaxAgeSParam;
  }
}

GURL GetApiUrl(task_module::mojom::TaskModuleType task_module_type,
               const std::string& application_locale) {
  GURL google_base_url = google_util::CommandLineGoogleBaseURL();
  if (!google_base_url.is_valid()) {
    google_base_url = GURL(google_util::kGoogleHomepageURL);
  }
  auto url = net::AppendQueryParameter(
      google_base_url.Resolve(GetPath(task_module_type)), "hl",
      application_locale);
  if (base::GetFieldTrialParamValueByFeature(GetFeature(task_module_type),
                                             GetDataParam(task_module_type)) ==
      "fake") {
    url = google_util::AppendToAsyncQueryParam(url, "fake_data", "1");
  }
  int cache_max_age_s = base::GetFieldTrialParamByFeatureAsInt(
      GetFeature(task_module_type), GetCacheMaxAgeSParam(task_module_type), 0);
  if (cache_max_age_s > 0) {
    url = google_util::AppendToAsyncQueryParam(
        url, "cache_max_age_s", base::NumberToString(cache_max_age_s));
  }
  return url;
}

const char* GetTasksKey(task_module::mojom::TaskModuleType task_module_type) {
  switch (task_module_type) {
    case task_module::mojom::TaskModuleType::kRecipe:
      return "recipe_tasks";
    case task_module::mojom::TaskModuleType::kShopping:
      return "shopping_tasks";
  }
}

const char* GetTaskItemsKey(
    task_module::mojom::TaskModuleType task_module_type) {
  switch (task_module_type) {
    case task_module::mojom::TaskModuleType::kRecipe:
      return "recipes";
    case task_module::mojom::TaskModuleType::kShopping:
      return "products";
  }
}

const char* GetTaskItemsName(
    task_module::mojom::TaskModuleType task_module_type) {
  switch (task_module_type) {
    case task_module::mojom::TaskModuleType::kRecipe:
      return "Recipes";
    case task_module::mojom::TaskModuleType::kShopping:
      return "Products";
  }
}

const char* GetDismissedTasksPrefName(
    task_module::mojom::TaskModuleType task_module_type) {
  switch (task_module_type) {
    case task_module::mojom::TaskModuleType::kRecipe:
      return "NewTabPage.DismissedRecipeTasks";
    case task_module::mojom::TaskModuleType::kShopping:
      return "NewTabPage.DismissedShoppingTasks";
  }
}

const char* GetModuleName(task_module::mojom::TaskModuleType task_module_type) {
  switch (task_module_type) {
    case task_module::mojom::TaskModuleType::kRecipe:
      return "RecipeTasks";
    case task_module::mojom::TaskModuleType::kShopping:
      return "ShoppingTasks";
  }
}

std::string GetViewedItemText(int viewed_timestamp) {
  // GWS timestamps are relative to the Unix Epoch.
  auto viewed_time = base::Time::UnixEpoch() + base::Seconds(viewed_timestamp);
  auto viewed_delta = base::Time::Now() - viewed_time;
  // Viewing items in the future is not supported. Assume the item was viewed
  // today to account for small shifts between the local and server clock.
  if (viewed_delta.InSeconds() < 0) {
    viewed_delta = base::TimeDelta();
  }
  if (viewed_delta.InDays() < 1) {
    return l10n_util::GetStringUTF8(
        IDS_NTP_MODULES_STATEFUL_TASKS_VIEWED_TODAY);
  }
  return base::UTF16ToUTF8(l10n_util::GetStringFUTF16(
      IDS_NTP_MODULES_STATEFUL_TASKS_VIEWED_AGO,
      ui::TimeFormat::SimpleWithMonthAndYear(
          ui::TimeFormat::Format::FORMAT_ELAPSED,
          ui::TimeFormat::Length::LENGTH_LONG, viewed_delta,
          /*use_month_and_year=*/true)));
}

std::string GetRecommendedItemText(
    task_module::mojom::TaskModuleType task_module_type) {
  switch (task_module_type) {
    case task_module::mojom::TaskModuleType::kRecipe:
      return l10n_util::GetStringUTF8(IDS_NTP_MODULES_RECIPE_TASKS_RECOMMENDED);
    case task_module::mojom::TaskModuleType::kShopping:
      return l10n_util::GetStringUTF8(IDS_NTP_MODULES_SHOPPING_TASKS_RELATED);
  }
}
}  // namespace

TaskModuleService::TaskModuleService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    Profile* profile,
    const std::string& application_locale)
    : profile_(profile),
      url_loader_factory_(url_loader_factory),
      application_locale_(application_locale) {}

TaskModuleService::~TaskModuleService() = default;

// static
void TaskModuleService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(
      GetDismissedTasksPrefName(task_module::mojom::TaskModuleType::kRecipe));
  registry->RegisterListPref(
      GetDismissedTasksPrefName(task_module::mojom::TaskModuleType::kShopping));
}

void TaskModuleService::Shutdown() {}

void TaskModuleService::GetPrimaryTask(
    task_module::mojom::TaskModuleType task_module_type,
    TaskModuleCallback callback) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("task_module_service", R"(
        semantics {
          sender: "Task Module Service"
          description: "This service downloads tasks, which is information "
            "related to the user's currently active search journeys such as "
            "visited and recommended task items, such as products to purchase "
            "or recipes. "
            "Tasks will be displayed on the new tab page to help the  user to "
            "continue their search journey. Tasks are queried on every new tab "
            "page load."
          trigger:
            "Displaying the new tab page on Desktop, if Google is the "
            "configured search provider and the user is signed in."
          data: "Credentials if user is signed in."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "Users can control this feature via selecting a non-Google default "
            "search engine in Chrome settings under 'Search Engine' or by "
            "signing out."
          chrome_policy {
            DefaultSearchProviderEnabled {
              policy_options {mode: MANDATORY}
              DefaultSearchProviderEnabled: false
            }
            BrowserSignin {
              policy_options {mode: MANDATORY}
              BrowserSignin: 0
            }
          }
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GetApiUrl(task_module_type, application_locale_);
  resource_request->credentials_mode =
      network::mojom::CredentialsMode::kInclude;
  resource_request->request_initiator =
      url::Origin::Create(GURL(chrome::kChromeUINewTabURL));
  variations::AppendVariationsHeaderUnknownSignedIn(
      resource_request->url,
      /* Modules are only shown in non-incognito. */
      variations::InIncognito::kNo, resource_request.get());

  loaders_.push_back(network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation));
  loaders_.back()->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&TaskModuleService::OnDataLoaded,
                     weak_ptr_factory_.GetWeakPtr(), task_module_type,
                     loaders_.back().get(), std::move(callback)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void TaskModuleService::DismissTask(
    task_module::mojom::TaskModuleType task_module_type,
    const std::string& task_name) {
  ListPrefUpdate update(profile_->GetPrefs(),
                        GetDismissedTasksPrefName(task_module_type));
  base::Value task_name_value(task_name);
  if (!base::Contains(update->GetList(), task_name_value))
    update->Append(std::move(task_name_value));
}

void TaskModuleService::RestoreTask(
    task_module::mojom::TaskModuleType task_module_type,
    const std::string& task_name) {
  ListPrefUpdate update(profile_->GetPrefs(),
                        GetDismissedTasksPrefName(task_module_type));
  update->EraseListValue(base::Value(task_name));
}

void TaskModuleService::OnDataLoaded(
    task_module::mojom::TaskModuleType task_module_type,
    network::SimpleURLLoader* loader,
    TaskModuleCallback callback,
    std::unique_ptr<std::string> response) {
  auto net_error = loader->NetError();
  bool loaded_from_cache = loader->LoadedFromCache();
  base::EraseIf(loaders_, [loader](const auto& target) {
    return loader == target.get();
  });

  if (!loaded_from_cache) {
    base::UmaHistogramSparse(
        "NewTabPage.Modules.DataRequest",
        base::PersistentHash(GetTasksKey(task_module_type)));
  }

  if (net_error != net::OK || !response) {
    std::move(callback).Run(nullptr);
    return;
  }

  if (base::StartsWith(*response, kXSSIResponsePreamble,
                       base::CompareCase::SENSITIVE)) {
    *response = response->substr(strlen(kXSSIResponsePreamble));
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response, base::BindOnce(&TaskModuleService::OnJsonParsed,
                                weak_ptr_factory_.GetWeakPtr(),
                                task_module_type, std::move(callback)));
}

void TaskModuleService::OnJsonParsed(
    task_module::mojom::TaskModuleType task_module_type,
    TaskModuleCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.value) {
    std::move(callback).Run(nullptr);
    return;
  }
  // We receive a list of tasks ordered from highest to lowest priority. We only
  // support showing a single task though. Therefore, pick the first task.
  auto* tasks = result.value->FindListPath(
      base::StringPrintf("update.%s", GetTasksKey(task_module_type)));
  if (!tasks || tasks->GetList().size() == 0) {
    std::move(callback).Run(nullptr);
    return;
  }

  for (const auto& task : tasks->GetList()) {
    auto* title = task.FindStringPath("title");
    auto* task_name = task.FindStringPath("task_name");
    auto* task_items = task.FindListPath(GetTaskItemsKey(task_module_type));
    auto* related_searches = task.FindListPath("related_searches");
    if (!title || !task_name || !task_items || !related_searches ||
        task_items->GetList().size() == 0) {
      continue;
    }
    if (IsTaskDismissed(task_module_type, *task_name)) {
      continue;
    }
    std::vector<task_module::mojom::TaskItemPtr> mojo_task_items;
    for (const auto& task_item : task_items->GetList()) {
      auto* name = task_item.FindStringPath("name");
      auto* image_url = task_item.FindStringPath("image_url");
      auto* price = task_item.FindStringPath("price");
      auto viewed_timestamp = task_item.FindIntPath("viewed_timestamp.seconds");
      auto* site_name = task_item.FindStringPath("site_name");
      auto* target_url = task_item.FindStringPath("target_url");
      if (!name || !image_url || !target_url) {
        continue;
      }
      if (task_module::mojom::TaskModuleType::kShopping == task_module_type &&
          !price) {
        continue;
      }
      auto mojom_task_item = task_module::mojom::TaskItem::New();
      mojom_task_item->name = *name;
      mojom_task_item->image_url = GURL(*image_url);
      mojom_task_item->info = viewed_timestamp
                                  ? GetViewedItemText(*viewed_timestamp)
                                  : GetRecommendedItemText(task_module_type);
      if (task_module_type == task_module::mojom::TaskModuleType::kRecipe &&
          site_name) {
        mojom_task_item->site_name = *site_name;
      }
      mojom_task_item->target_url = GURL(*target_url);
      if (task_module_type == task_module::mojom::TaskModuleType::kShopping) {
        mojom_task_item->price = *price;
      }
      mojo_task_items.push_back(std::move(mojom_task_item));
    }
    std::vector<task_module::mojom::RelatedSearchPtr> mojo_related_searches;
    for (const auto& related_search : related_searches->GetList()) {
      auto* text = related_search.FindStringPath("text");
      auto* target_url = related_search.FindStringPath("target_url");
      if (!text || !target_url) {
        continue;
      }
      auto mojo_related_search = task_module::mojom::RelatedSearch::New();
      mojo_related_search->text = *text;
      mojo_related_search->target_url = GURL(*target_url);
      mojo_related_searches.push_back(std::move(mojo_related_search));
    }
    auto mojo_task = task_module::mojom::Task::New();
    mojo_task->title = *title;
    mojo_task->name = *task_name;
    base::UmaHistogramCounts100(
        base::StringPrintf("NewTabPage.%s.%sDownloadCount",
                           GetModuleName(task_module_type),
                           GetTaskItemsName(task_module_type)),
        mojo_task_items.size());
    mojo_task->task_items = std::move(mojo_task_items);
    base::UmaHistogramCounts100(
        base::StringPrintf("NewTabPage.%s.RelatedSearchDownloadCount",
                           GetModuleName(task_module_type)),
        mojo_related_searches.size());
    mojo_task->related_searches = std::move(mojo_related_searches);

    std::move(callback).Run(std::move(mojo_task));
    return;
  }
  std::move(callback).Run(nullptr);
}

bool TaskModuleService::IsTaskDismissed(
    task_module::mojom::TaskModuleType task_module_type,
    const std::string& task_name) {
  const base::ListValue* dismissed_tasks = profile_->GetPrefs()->GetList(
      GetDismissedTasksPrefName(task_module_type));
  DCHECK(dismissed_tasks);
  return base::Contains(dismissed_tasks->GetList(), base::Value(task_name));
}
