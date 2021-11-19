// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/intent_util.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/extend.h"
#include "base/containers/flat_map.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/apps/app_service/file_utils.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/web_applications/web_app.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/file_handler_info.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/file_handler_info.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chromeos/crosapi/mojom/app_service_types.mojom.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "base/files/file_path.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/arc/intent_helper/intent_constants.h"
#include "components/arc/intent_helper/intent_filter.h"
#include "components/arc/mojom/intent_common.mojom.h"
#include "components/arc/mojom/intent_helper.mojom.h"
#include "storage/browser/file_system/file_system_url.h"
#endif

namespace {

constexpr char kTextPlain[] = "text/plain";

apps::mojom::IntentFilterPtr CreateFileFilter(
    const std::vector<std::string>& intent_actions,
    const std::vector<std::string>& mime_types,
    const std::vector<std::string>& file_extensions,
    const std::string& activity_name = "",
    bool include_directories = false) {
  DCHECK(!mime_types.empty() || !file_extensions.empty());
  auto intent_filter = apps::mojom::IntentFilter::New();

  // kAction == View, Share etc.
  std::vector<apps::mojom::ConditionValuePtr> action_condition_values;
  for (auto& action : intent_actions) {
    action_condition_values.push_back(apps_util::MakeConditionValue(
        action, apps::mojom::PatternMatchType::kNone));
  }
  if (!action_condition_values.empty()) {
    auto action_condition =
        apps_util::MakeCondition(apps::mojom::ConditionType::kAction,
                                 std::move(action_condition_values));
    intent_filter->conditions.push_back(std::move(action_condition));
  }

  std::vector<apps::mojom::ConditionValuePtr> condition_values;
  // Mime types.
  for (auto& mime_type : mime_types) {
    condition_values.push_back(apps_util::MakeConditionValue(
        mime_type, apps::mojom::PatternMatchType::kMimeType));
  }
  // And file extensions.
  for (const std::string& extension : file_extensions) {
    condition_values.push_back(apps_util::MakeConditionValue(
        extension, apps::mojom::PatternMatchType::kFileExtension));
  }
  if (include_directories) {
    condition_values.push_back(apps_util::MakeConditionValue(
        "", apps::mojom::PatternMatchType::kIsDirectory));
  }

  DCHECK(!condition_values.empty());
  if (!condition_values.empty()) {
    auto file_condition = apps_util::MakeCondition(
        apps::mojom::ConditionType::kFile, std::move(condition_values));
    intent_filter->conditions.push_back(std::move(file_condition));
  }

  if (!activity_name.empty()) {
    intent_filter->activity_name = activity_name;
  }

  return intent_filter;
}

apps::mojom::IntentFilterPtr CreateMimeTypeShareFilter(
    const std::vector<std::string>& mime_types) {
  DCHECK(!mime_types.empty());
  auto intent_filter = apps::mojom::IntentFilter::New();

  std::vector<apps::mojom::ConditionValuePtr> action_condition_values;
  action_condition_values.push_back(apps_util::MakeConditionValue(
      apps_util::kIntentActionSend, apps::mojom::PatternMatchType::kNone));
  auto action_condition = apps_util::MakeCondition(
      apps::mojom::ConditionType::kAction, std::move(action_condition_values));
  intent_filter->conditions.push_back(std::move(action_condition));

  std::vector<apps::mojom::ConditionValuePtr> condition_values;
  for (auto& mime_type : mime_types) {
    condition_values.push_back(apps_util::MakeConditionValue(
        mime_type, apps::mojom::PatternMatchType::kMimeType));
  }
  auto mime_condition = apps_util::MakeCondition(
      apps::mojom::ConditionType::kMimeType, std::move(condition_values));
  intent_filter->conditions.push_back(std::move(mime_condition));

  return intent_filter;
}

std::vector<apps::mojom::IntentFilterPtr> CreateWebAppShareIntentFilters(
    const web_app::WebApp& web_app) {
  if (!web_app.share_target().has_value()) {
    return {};
  }

  std::vector<apps::mojom::IntentFilterPtr> filters;
  const apps::ShareTarget& share_target = web_app.share_target().value();

  if (!share_target.params.text.empty()) {
    // The share target accepts navigator.share() calls with text.
    filters.push_back(CreateMimeTypeShareFilter({kTextPlain}));
  }

  std::vector<std::string> content_types;
  for (const auto& files_entry : share_target.params.files) {
    for (const auto& file_type : files_entry.accept) {
      // Skip any file_type that is not a MIME type.
      if (file_type.empty() || file_type[0] == '.' ||
          std::count(file_type.begin(), file_type.end(), '/') != 1) {
        continue;
      }

      content_types.push_back(file_type);
    }
  }

  if (!content_types.empty()) {
    const std::vector<std::string> intent_actions(
        {apps_util::kIntentActionSend, apps_util::kIntentActionSendMultiple});
    filters.push_back(CreateFileFilter(intent_actions, content_types, {}));
  }

  return filters;
}

std::vector<apps::mojom::IntentFilterPtr> CreateWebAppFileHandlerIntentFilters(
    const web_app::WebApp& web_app) {
  std::vector<apps::mojom::IntentFilterPtr> filters;
  for (const apps::FileHandler& handler : web_app.file_handlers()) {
    std::vector<std::string> mime_types;
    std::vector<std::string> file_extensions;
    std::string action_url = handler.action.spec();
    // TODO(petermarshall): Use GetFileExtensionsFromFileHandlers /
    // GetMimeTypesFromFileHandlers?
    for (const apps::FileHandler::AcceptEntry& accept_entry : handler.accept) {
      mime_types.push_back(accept_entry.mime_type);
      for (const std::string& extension : accept_entry.file_extensions) {
        file_extensions.push_back(extension);
      }
    }
    filters.push_back(CreateFileFilter({apps_util::kIntentActionView},
                                       mime_types, file_extensions,
                                       action_url));
  }

  return filters;
}

bool IsNoteTakingWebApp(const web_app::WebApp& web_app) {
  return web_app.note_taking_new_note_url().is_valid();
}

apps::mojom::IntentFilterPtr CreateNoteTakingIntentFilter(
    const web_app::WebApp& web_app) {
  DCHECK(IsNoteTakingWebApp(web_app));

  auto intent_filter = apps::mojom::IntentFilter::New();
  apps_util::AddSingleValueCondition(
      apps::mojom::ConditionType::kAction, apps_util::kIntentActionCreateNote,
      apps::mojom::PatternMatchType::kNone, intent_filter);
  return intent_filter;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kIntentExtraText[] = "S.android.intent.extra.TEXT";
constexpr char kIntentExtraSubject[] = "S.android.intent.extra.SUBJECT";
constexpr char kIntentExtraStartType[] = "S.org.chromium.arc.start_type";
constexpr char kIntentActionPrefix[] = "android.intent.action";
constexpr char kType[] = "type";

constexpr int kIntentPrefixLength = 2;

const char* ConvertAppServiceToArcIntentAction(const std::string& action) {
  if (action == apps_util::kIntentActionMain) {
    return arc::kIntentActionMain;
  } else if (action == apps_util::kIntentActionView) {
    return arc::kIntentActionView;
  } else if (action == apps_util::kIntentActionSend) {
    return arc::kIntentActionSend;
  } else if (action == apps_util::kIntentActionSendMultiple) {
    return arc::kIntentActionSendMultiple;
  } else if (action.compare(0, strlen(kIntentActionPrefix),
                            kIntentActionPrefix) == 0) {
    return action.c_str();
  } else {
    return arc::kIntentActionView;
  }
}

// Returns true if |pattern| is a Glob (as in PatternMatchType::kGlob) which
// behaves like a Prefix pattern. That is, the only special characters are a
// ".*" at the end of the string.
bool IsPrefixOnlyGlob(base::StringPiece pattern) {
  if (!base::EndsWith(pattern, ".*")) {
    return false;
  }

  size_t i = 0;
  while (i < pattern.size() - 2) {
    if (pattern[i] == '.' || pattern[i] == '*') {
      return false;
    }
    i++;
  }
  return true;
}

apps::mojom::ConditionValuePtr ConvertArcPatternMatcherToConditionValue(
    const arc::IntentFilter::PatternMatcher& path) {
  apps::mojom::PatternMatchType match_type;

  switch (path.match_type()) {
    case arc::mojom::PatternType::PATTERN_LITERAL:
      match_type = apps::mojom::PatternMatchType::kLiteral;
      break;
    case arc::mojom::PatternType::PATTERN_PREFIX:
      match_type = apps::mojom::PatternMatchType::kPrefix;
      break;
    case arc::mojom::PatternType::PATTERN_SIMPLE_GLOB:
      match_type = apps::mojom::PatternMatchType::kGlob;

      // It's common for Globs to be used to encode patterns which are actually
      // prefixes. Detect and convert these, since prefix matching is easier &
      // cheaper.
      if (IsPrefixOnlyGlob(path.pattern())) {
        DCHECK(path.pattern().size() >= 2);
        return apps_util::MakeConditionValue(
            path.pattern().substr(0, path.pattern().size() - 2),
            apps::mojom::PatternMatchType::kPrefix);
      }
      break;
  }

  return apps_util::MakeConditionValue(path.pattern(), match_type);
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

namespace apps_util {

std::vector<apps::mojom::IntentFilterPtr> CreateWebAppIntentFilters(
    const web_app::WebApp& web_app,
    const GURL& scope) {
  std::vector<apps::mojom::IntentFilterPtr> filters;
  if (!scope.is_empty())
    filters.push_back(apps_util::CreateIntentFilterForUrlScope(scope));

  base::Extend(filters, CreateWebAppShareIntentFilters(web_app));
  base::Extend(filters, CreateWebAppFileHandlerIntentFilters(web_app));

  if (IsNoteTakingWebApp(web_app))
    filters.push_back(CreateNoteTakingIntentFilter(web_app));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash::features::IsProjectorEnabled() &&
      web_app.app_id() == ash::kChromeUITrustedProjectorSwaAppId) {
    filters.push_back(CreateIntentFilterForUrlScope(
        GURL(ash::kChromeUIUntrustedProjectorPwaUrl)));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return filters;
}

std::vector<apps::mojom::IntentFilterPtr> CreateChromeAppIntentFilters(
    const extensions::Extension* extension) {
  const extensions::FileHandlersInfo* file_handlers =
      extensions::FileHandlers::GetFileHandlers(extension);
  if (!file_handlers)
    return {};
  // Check that the extension can be launched with files. This includes all
  // platform apps and allowlisted extensions.
  if (!CanLaunchViaEvent(extension))
    return {};

  std::vector<apps::mojom::IntentFilterPtr> filters;
  for (const apps::FileHandlerInfo& handler : *file_handlers) {
    // "share_with", "add_to" and "pack_with" are ignored in the Files app
    // frontend.
    if (handler.verb != apps::file_handler_verbs::kOpenWith)
      continue;
    std::vector<std::string> mime_types(handler.types.begin(),
                                        handler.types.end());
    std::vector<std::string> file_extensions(handler.extensions.begin(),
                                             handler.extensions.end());
    filters.push_back(CreateFileFilter({apps_util::kIntentActionView},
                                       mime_types, file_extensions, handler.id,
                                       handler.include_directories));
    filters.back()->activity_label = extension->name();
  }

  return filters;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// TODO(crbug.com/853604): Make this not link to file manager extension if
// possible. Added support to not link with file manager extension but still
// need to refactor all users of the util functions to switch to it (alanding).
apps::mojom::IntentPtr CreateShareIntentFromFiles(
    absl::optional<Profile*> profile,
    const std::vector<base::FilePath>& file_paths,
    const std::vector<std::string>& mime_types) {
  auto file_urls = profile.has_value()
                       ? apps::GetFileSystemUrls(profile.value(), file_paths)
                       : apps::GetFileUrls(file_paths);
  return CreateShareIntentFromFiles(file_urls, mime_types);
}

apps::mojom::IntentPtr CreateShareIntentFromFiles(
    absl::optional<Profile*> profile,
    const std::vector<base::FilePath>& file_paths,
    const std::vector<std::string>& mime_types,
    const std::string& share_text,
    const std::string& share_title) {
  auto file_urls = profile.has_value()
                       ? apps::GetFileSystemUrls(profile.value(), file_paths)
                       : apps::GetFileUrls(file_paths);
  return CreateShareIntentFromFiles(file_urls, mime_types, share_text,
                                    share_title);
}

apps::mojom::IntentPtr CreateIntentForArcIntentAndActivity(
    arc::mojom::IntentInfoPtr arc_intent,
    arc::mojom::ActivityNamePtr activity) {
  auto intent = apps::mojom::Intent::New();
  if (arc_intent) {
    intent->action = std::move(arc_intent->action);
    intent->data = std::move(arc_intent->data);
    intent->mime_type = std::move(arc_intent->type);
    intent->categories = std::move(arc_intent->categories);
    intent->ui_bypassed = arc_intent->ui_bypassed
                              ? apps::mojom::OptionalBool::kTrue
                              : apps::mojom::OptionalBool::kFalse;
    intent->extras = std::move(arc_intent->extras);
  }

  if (activity) {
    intent->activity_name = std::move(activity->activity_name);
  }

  return intent;
}

base::flat_map<std::string, std::string> CreateArcIntentExtras(
    const apps::mojom::IntentPtr& intent) {
  auto extras = base::flat_map<std::string, std::string>();
  if (intent->share_text.has_value()) {
    // Slice off the "S." prefix for the key.
    extras.insert(std::make_pair(kIntentExtraText + kIntentPrefixLength,
                                 intent->share_text.value()));
  }
  if (intent->share_title.has_value()) {
    // Slice off the "S." prefix for the key.
    extras.insert(std::make_pair(kIntentExtraSubject + kIntentPrefixLength,
                                 intent->share_title.value()));
  }
  if (intent->start_type.has_value()) {
    // Slice off the "S." prefix for the key.
    extras.insert(std::make_pair(kIntentExtraStartType + kIntentPrefixLength,
                                 intent->start_type.value()));
  }
  return extras;
}

arc::mojom::IntentInfoPtr ConvertAppServiceToArcIntent(
    const apps::mojom::IntentPtr& intent) {
  arc::mojom::IntentInfoPtr arc_intent;
  if (!intent->url.has_value() && !intent->share_text.has_value() &&
      !intent->activity_name.has_value()) {
    return arc_intent;
  }

  arc_intent = arc::mojom::IntentInfo::New();
  arc_intent->action = ConvertAppServiceToArcIntentAction(intent->action);
  if (intent->url.has_value()) {
    arc_intent->data = intent->url->spec();
  }
  if (intent->share_text.has_value() || intent->share_title.has_value() ||
      intent->start_type.has_value()) {
    arc_intent->extras = CreateArcIntentExtras(intent);
  }
  if (intent->categories.has_value()) {
    arc_intent->categories = intent->categories;
  }
  if (intent->data.has_value()) {
    arc_intent->data = intent->data;
  }
  if (intent->mime_type.has_value()) {
    arc_intent->type = intent->mime_type;
  }
  if (intent->ui_bypassed != apps::mojom::OptionalBool::kUnknown) {
    arc_intent->ui_bypassed =
        intent->ui_bypassed == apps::mojom::OptionalBool::kTrue ? true : false;
  }
  if (intent->extras.has_value()) {
    arc_intent->extras = intent->extras;
  }
  return arc_intent;
}

const char* ConvertArcToAppServiceIntentAction(const std::string& arc_action) {
  if (arc_action == arc::kIntentActionMain) {
    return apps_util::kIntentActionMain;
  } else if (arc_action == arc::kIntentActionView) {
    return apps_util::kIntentActionView;
  } else if (arc_action == arc::kIntentActionSend) {
    return apps_util::kIntentActionSend;
  } else if (arc_action == arc::kIntentActionSendMultiple) {
    return apps_util::kIntentActionSendMultiple;
  }

  return nullptr;
}

std::string CreateLaunchIntent(const std::string& package_name,
                               const apps::mojom::IntentPtr& intent) {
  // If |intent| has |ui_bypassed|, |url| or |data|, it is too complex to
  // convert to a string, so return the empty string.
  if (intent->ui_bypassed != apps::mojom::OptionalBool::kUnknown ||
      intent->url.has_value() || intent->data.has_value()) {
    return std::string();
  }

  std::string ret = base::StringPrintf("%s;", arc::kIntentPrefix);

  // Convert action.
  std::string action = ConvertAppServiceToArcIntentAction(intent->action);
  ret += base::StringPrintf("%s=%s;", arc::kAction,
                            ConvertAppServiceToArcIntentAction(intent->action));

  // Convert categories.
  if (intent->categories.has_value()) {
    for (const auto& category : intent->categories.value()) {
      ret += base::StringPrintf("%s=%s;", arc::kCategory, category.c_str());
    }
  }

  // Set launch flags.
  ret +=
      base::StringPrintf("%s=0x%x;", arc::kLaunchFlags,
                         arc::Intent::FLAG_ACTIVITY_NEW_TASK |
                             arc::Intent::FLAG_ACTIVITY_RESET_TASK_IF_NEEDED);

  // Convert activity_name.
  if (intent->activity_name.has_value()) {
    // Remove the |package_name| prefix, if activity starts with it.
    const std::string& activity = intent->activity_name.value();
    const char* activity_compact_name =
        activity.find(package_name.c_str()) == 0
            ? activity.c_str() + package_name.length()
            : activity.c_str();
    ret += base::StringPrintf("%s=%s/%s;", arc::kComponent,
                              package_name.c_str(), activity_compact_name);
  } else {
    ret += base::StringPrintf("%s=%s/;", arc::kComponent, package_name.c_str());
  }

  if (intent->mime_type.has_value()) {
    ret +=
        base::StringPrintf("%s=%s;", kType, intent->mime_type.value().c_str());
  }

  if (intent->share_text.has_value()) {
    ret += base::StringPrintf("%s=%s;", kIntentExtraText,
                              intent->share_text.value().c_str());
  }

  if (intent->share_title.has_value()) {
    ret += base::StringPrintf("%s=%s;", kIntentExtraSubject,
                              intent->share_title.value().c_str());
  }

  if (intent->start_type.has_value()) {
    ret += base::StringPrintf("%s=%s;", kIntentExtraStartType,
                              intent->start_type.value().c_str());
  }

  if (intent->extras.has_value()) {
    for (auto it : intent->extras.value()) {
      ret += base::StringPrintf("%s=%s;", it.first.c_str(), it.second.c_str());
    }
  }

  ret += arc::kEndSuffix;
  DCHECK(!ret.empty());
  return ret;
}

arc::IntentFilter ConvertAppServiceToArcIntentFilter(
    const std::string& package_name,
    const apps::mojom::IntentFilterPtr& intent_filter) {
  std::vector<std::string> actions;
  std::vector<std::string> schemes;
  std::vector<arc::IntentFilter::AuthorityEntry> authorities;
  std::vector<arc::IntentFilter::PatternMatcher> paths;
  std::vector<std::string> mime_types;
  for (auto& condition : intent_filter->conditions) {
    switch (condition->condition_type) {
      case apps::mojom::ConditionType::kScheme:
        for (auto& condition_value : condition->condition_values) {
          schemes.push_back(condition_value->value);
        }
        break;
      case apps::mojom::ConditionType::kHost:
        for (auto& condition_value : condition->condition_values) {
          authorities.push_back(arc::IntentFilter::AuthorityEntry(
              /*host=*/condition_value->value, /*port=*/0));
        }
        break;
      case apps::mojom::ConditionType::kPattern:
        for (auto& condition_value : condition->condition_values) {
          arc::mojom::PatternType match_type;
          switch (condition_value->match_type) {
            case apps::mojom::PatternMatchType::kLiteral:
              match_type = arc::mojom::PatternType::PATTERN_LITERAL;
              break;
            case apps::mojom::PatternMatchType::kPrefix:
              match_type = arc::mojom::PatternType::PATTERN_PREFIX;
              break;
            case apps::mojom::PatternMatchType::kGlob:
              match_type = arc::mojom::PatternType::PATTERN_SIMPLE_GLOB;
              break;
            case apps::mojom::PatternMatchType::kNone:
            case apps::mojom::PatternMatchType::kMimeType:
            case apps::mojom::PatternMatchType::kFileExtension:
            case apps::mojom::PatternMatchType::kIsDirectory:
              NOTREACHED();
              return arc::IntentFilter();
          }
          paths.push_back(arc::IntentFilter::PatternMatcher(
              condition_value->value, match_type));
        }
        break;
      case apps::mojom::ConditionType::kAction:
        for (auto& condition_value : condition->condition_values) {
          actions.push_back(
              ConvertAppServiceToArcIntentAction(condition_value->value));
        }
        break;
      case apps::mojom::ConditionType::kMimeType:
        for (auto& condition_value : condition->condition_values) {
          mime_types.push_back(condition_value->value);
        }
        break;
      case apps::mojom::ConditionType::kFile:
        NOTREACHED();
        return arc::IntentFilter();
    }
  }
  // TODO(crbug.com/853604): Add support for other category types.
  return arc::IntentFilter(package_name, std::move(actions),
                           std::move(authorities), std::move(paths),
                           std::move(schemes), std::move(mime_types));
}

apps::mojom::IntentFilterPtr ConvertArcToAppServiceIntentFilter(
    const arc::IntentFilter& arc_intent_filter) {
  auto intent_filter = apps::mojom::IntentFilter::New();

  bool has_view_action = false;

  std::vector<apps::mojom::ConditionValuePtr> action_condition_values;
  for (auto& arc_action : arc_intent_filter.actions()) {
    const char* action = ConvertArcToAppServiceIntentAction(arc_action);
    has_view_action = has_view_action || action == kIntentActionView;

    if (!action) {
      continue;
    }

    action_condition_values.push_back(apps_util::MakeConditionValue(
        action, apps::mojom::PatternMatchType::kNone));
  }
  if (!action_condition_values.empty()) {
    auto action_condition =
        apps_util::MakeCondition(apps::mojom::ConditionType::kAction,
                                 std::move(action_condition_values));
    intent_filter->conditions.push_back(std::move(action_condition));
  }

  std::vector<apps::mojom::ConditionValuePtr> scheme_condition_values;
  for (auto& scheme : arc_intent_filter.schemes()) {
    scheme_condition_values.push_back(apps_util::MakeConditionValue(
        scheme, apps::mojom::PatternMatchType::kNone));
  }
  if (!scheme_condition_values.empty()) {
    auto scheme_condition =
        apps_util::MakeCondition(apps::mojom::ConditionType::kScheme,
                                 std::move(scheme_condition_values));
    intent_filter->conditions.push_back(std::move(scheme_condition));
  }

  std::vector<apps::mojom::ConditionValuePtr> host_condition_values;
  for (auto& authority : arc_intent_filter.authorities()) {
    host_condition_values.push_back(apps_util::MakeConditionValue(
        authority.host(), apps::mojom::PatternMatchType::kNone));
  }
  if (!host_condition_values.empty()) {
    // It's common for Android apps to include duplicate host conditions, we can
    // de-duplicate these to reduce time/space usage down the line.
    std::sort(host_condition_values.begin(), host_condition_values.end());
    host_condition_values.erase(
        std::unique(host_condition_values.begin(), host_condition_values.end()),
        host_condition_values.end());

    auto host_condition = apps_util::MakeCondition(
        apps::mojom::ConditionType::kHost, std::move(host_condition_values));
    intent_filter->conditions.push_back(std::move(host_condition));
  }

  std::vector<apps::mojom::ConditionValuePtr> path_condition_values;
  for (auto& path : arc_intent_filter.paths()) {
    path_condition_values.push_back(
        ConvertArcPatternMatcherToConditionValue(path));
  }

  // For ARC apps, specifying a path is optional. For any intent filters which
  // match every URL on a host with a "view" action, add a path which matches
  // everything to ensure the filter is treated as a supported link.
  if (path_condition_values.empty() && has_view_action &&
      arc_intent_filter.authorities().size() > 0 &&
      arc_intent_filter.schemes().size() > 0) {
    path_condition_values.push_back(apps_util::MakeConditionValue(
        "/", apps::mojom::PatternMatchType::kPrefix));
  }
  if (!path_condition_values.empty()) {
    auto path_condition = apps_util::MakeCondition(
        apps::mojom::ConditionType::kPattern, std::move(path_condition_values));
    intent_filter->conditions.push_back(std::move(path_condition));
  }

  std::vector<apps::mojom::ConditionValuePtr> mime_type_condition_values;
  for (auto& mime_type : arc_intent_filter.mime_types()) {
    mime_type_condition_values.push_back(apps_util::MakeConditionValue(
        mime_type, apps::mojom::PatternMatchType::kMimeType));
  }
  if (!mime_type_condition_values.empty()) {
    auto mime_type_condition =
        apps_util::MakeCondition(apps::mojom::ConditionType::kMimeType,
                                 std::move(mime_type_condition_values));
    intent_filter->conditions.push_back(std::move(mime_type_condition));
  }
  if (!arc_intent_filter.activity_name().empty()) {
    intent_filter->activity_name = arc_intent_filter.activity_name();
  }
  if (!arc_intent_filter.activity_label().empty()) {
    intent_filter->activity_label = arc_intent_filter.activity_label();
  }

  return intent_filter;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_CHROMEOS)
crosapi::mojom::IntentPtr ConvertAppServiceToCrosapiIntent(
    const apps::mojom::IntentPtr& app_service_intent,
    absl::optional<Profile*> profile) {
  auto crosapi_intent = crosapi::mojom::Intent::New();
  crosapi_intent->action = app_service_intent->action;
  if (app_service_intent->url.has_value()) {
    crosapi_intent->url = app_service_intent->url.value();
  }
  if (app_service_intent->mime_type.has_value()) {
    crosapi_intent->mime_type = app_service_intent->mime_type.value();
  }
  if (app_service_intent->share_text.has_value()) {
    crosapi_intent->share_text = app_service_intent->share_text.value();
  }
  if (app_service_intent->share_title.has_value()) {
    crosapi_intent->share_title = app_service_intent->share_title.value();
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (app_service_intent->files.has_value() && profile.has_value()) {
    std::vector<crosapi::mojom::IntentFilePtr> crosapi_files;
    for (const auto& file : app_service_intent->files.value()) {
      auto file_system_url = apps::GetFileSystemURL(profile.value(), file->url);
      if (file_system_url.is_valid()) {
        auto crosapi_file = crosapi::mojom::IntentFile::New();
        crosapi_file->file_path = file_system_url.path();
        crosapi_files.push_back(std::move(crosapi_file));
      }
    }
    crosapi_intent->files = std::move(crosapi_files);
  }
#endif
  return crosapi_intent;
}

apps::mojom::IntentPtr ConvertCrosapiToAppServiceIntent(
    const crosapi::mojom::IntentPtr& crosapi_intent,
    absl::optional<Profile*> profile) {
  auto app_service_intent = apps::mojom::Intent::New();
  app_service_intent->action = crosapi_intent->action;
  if (crosapi_intent->url.has_value()) {
    app_service_intent->url = crosapi_intent->url.value();
  }
  if (crosapi_intent->mime_type.has_value()) {
    app_service_intent->mime_type = crosapi_intent->mime_type.value();
  }
  if (crosapi_intent->share_text.has_value()) {
    app_service_intent->share_text = crosapi_intent->share_text.value();
  }
  if (crosapi_intent->share_title.has_value()) {
    app_service_intent->share_title = crosapi_intent->share_title.value();
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (crosapi_intent->files.has_value() && profile.has_value()) {
    std::vector<apps::mojom::IntentFilePtr> intent_files;
    for (const auto& file : crosapi_intent->files.value()) {
      auto file_url = apps::GetFileSystemUrl(profile.value(), file->file_path);
      if (file_url.is_empty()) {
        continue;
      }
      auto intent_file = apps::mojom::IntentFile::New();
      intent_file->url = file_url;
      intent_files.push_back(std::move(intent_file));
    }
    if (intent_files.size() > 0) {
      app_service_intent->files = std::move(intent_files);
    }
  }
#endif
  return app_service_intent;
}

crosapi::mojom::IntentPtr CreateCrosapiIntentForViewFiles(
    const apps::mojom::FilePathsPtr& file_paths) {
  auto intent = crosapi::mojom::Intent::New();
  intent->action = kIntentActionView;
  std::vector<crosapi::mojom::IntentFilePtr> crosapi_files;
  for (const auto& file_path : file_paths->file_paths) {
    auto crosapi_file = crosapi::mojom::IntentFile::New();
    crosapi_file->file_path = file_path;
    crosapi_files.push_back(std::move(crosapi_file));
  }
  intent->files = std::move(crosapi_files);
  return intent;
}
#endif

}  // namespace apps_util
