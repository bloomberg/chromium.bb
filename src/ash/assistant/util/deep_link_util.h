// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UTIL_DEEP_LINK_UTIL_H_
#define ASH_ASSISTANT_UTIL_DEEP_LINK_UTIL_H_

#include <map>
#include <string>

#include "base/component_export.h"
#include "base/optional.h"
#include "base/timer/timer.h"

class GURL;

namespace chromeos {
namespace assistant {
namespace mojom {
enum class AssistantEntryPoint;
enum class AssistantQuerySource;
}  // namespace mojom
}  // namespace assistant
}  // namespace chromeos

namespace ash {
namespace assistant {
namespace util {

// Enumeration of deep link types.
enum class DeepLinkType {
  kUnsupported,
  kAlarmTimer,
  kChromeSettings,
  kFeedback,
  kLists,
  kNotes,
  kOnboarding,
  kProactiveSuggestions,
  kQuery,
  kReminders,
  kScreenshot,
  kSettings,
  kTaskManager,
  kWhatsOnMyScreen,
};

// Enumeration of deep link parameters.
// Examples of usage in comments. Note that actual Assistant deeplinks are
// prefixed w/ "googleassistant"; "ga" is only used here to avoid line wrapping.
enum class DeepLinkParam {
  kAction,      // ga://proactive-suggestions?action=cardClick
  kCategory,    // ga://proactive-suggestions?category=1
  kClientId,    // ga://reminders?action=edit&clientId=1
  kDurationMs,  // ga://alarm-timer?action=addTimeToTimer&durationMs=60000
  kEid,         // ga://lists?eid=1
  kEntryPoint,  // ga://send-query?q=weather&entryPoint=11
  kHref,   // ga://proactive-suggestions?action=cardClick&href=https://g.co/
  kIndex,  // ga://proactive-suggestions?action=cardClick&index=1
  kId,     // ga://alarm-timer?action=addTimeToTimer&id=1
  kPage,   // ga://settings?page=googleAssistant
  kQuery,  // ga://send-query?q=weather
  kQuerySource,  // ga://send-query?q=weather&querySource=12
  kRelaunch,     // ga://onboarding?relaunch=true
  kType,         // ga://lists?id=1&type=shopping
  kVeId,         // ga://proactive-suggestions?action=cardClick&veId=1
};

// Enumeration of alarm/timer deep link actions.
enum class AlarmTimerAction {
  kAddTimeToTimer,
  kPauseTimer,
  kRemoveAlarmOrTimer,
  kResumeTimer,
};

// Enumeration of proactive suggestions deep link actions.
enum class ProactiveSuggestionsAction {
  kCardClick,
  kEntryPointClick,
  kEntryPointClose,
  kViewImpression,
};

// Enumeration of reminder deep link actions.
enum class ReminderAction {
  kCreate,
  kEdit,
};

// Returns a new deep link, having appended or replaced the entry point param
// from the original |deep_link| with |entry_point|.
COMPONENT_EXPORT(ASSISTANT_UTIL)
GURL AppendOrReplaceEntryPointParam(
    const GURL& deep_link,
    chromeos::assistant::mojom::AssistantEntryPoint entry_point);

// Returns a new deep link, having appended or replaced the query source param
// from the original |deep_link| with |query_source|.
COMPONENT_EXPORT(ASSISTANT_UTIL)
GURL AppendOrReplaceQuerySourceParam(
    const GURL& deep_link,
    chromeos::assistant::mojom::AssistantQuerySource query_source);

// Returns a deep link to perform an alarm/timer action.
COMPONENT_EXPORT(ASSISTANT_UTIL)
base::Optional<GURL> CreateAlarmTimerDeepLink(
    AlarmTimerAction action,
    base::Optional<std::string> alarm_timer_id,
    base::Optional<base::TimeDelta> duration = base::nullopt);

// Returns a deep link to send an Assistant query.
COMPONENT_EXPORT(ASSISTANT_UTIL)
GURL CreateAssistantQueryDeepLink(const std::string& query);

// Returns a deep link to top level Assistant Settings.
COMPONENT_EXPORT(ASSISTANT_UTIL) GURL CreateAssistantSettingsDeepLink();

// Returns a deep link to initiate a screen context interaction.
COMPONENT_EXPORT(ASSISTANT_UTIL) GURL CreateWhatsOnMyScreenDeepLink();

// Returns the parsed parameters for the specified |deep_link|. If the supplied
// argument is not a supported deep link or if no parameters are found, an empty
// map is returned.
COMPONENT_EXPORT(ASSISTANT_UTIL)
std::map<std::string, std::string> GetDeepLinkParams(const GURL& deep_link);

// Returns a specific string |param| from the given parameters. If the desired
// parameter is not found, and empty value is returned.
COMPONENT_EXPORT(ASSISTANT_UTIL)
base::Optional<std::string> GetDeepLinkParam(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param);

// Returns AlarmTimerAction from the given parameters. If the desired
// parameter is not found or is not an AlarmTimerAction, an empty value is
// returned.
COMPONENT_EXPORT(ASSISTANT_UTIL)
base::Optional<AlarmTimerAction> GetDeepLinkParamAsAlarmTimerAction(
    const std::map<std::string, std::string>& params);

// Returns a specific bool |param| from the given parameters. If the desired
// parameter is not found or is not a bool, an empty value is returned.
COMPONENT_EXPORT(ASSISTANT_UTIL)
base::Optional<bool> GetDeepLinkParamAsBool(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param);

// Returns a specific entry point |param| from the given parameters. If the
// desired parameter is not found or is not mappable to an Assistant entry
// point, an empty value is returned.
COMPONENT_EXPORT(ASSISTANT_UTIL)
base::Optional<chromeos::assistant::mojom::AssistantEntryPoint>
GetDeepLinkParamAsEntryPoint(const std::map<std::string, std::string>& params,
                             DeepLinkParam param);

// Returns a specific GURL |param| from the given parameters. If the desired
// parameter is not found, an absent value is returned.
COMPONENT_EXPORT(ASSISTANT_UTIL)
base::Optional<GURL> GetDeepLinkParamAsGURL(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param);

// Returns a specific int |param| from the given parameters. If the desired
// parameter is not found or is not an int, an empty value is returned.
COMPONENT_EXPORT(ASSISTANT_UTIL)
base::Optional<int32_t> GetDeepLinkParamAsInt(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param);

// Returns a specific int64 |param| from the given parameters. If the desired
// parameter is not found or is not an int64, an empty value is returned.
COMPONENT_EXPORT(ASSISTANT_UTIL)
base::Optional<int64_t> GetDeepLinkParamAsInt64(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param);

// Returns a specific ProactiveSuggestionsAction |param| from the given
// parameters. If the desired parameter is not found, an empty value is
// returned.
COMPONENT_EXPORT(ASSISTANT_UTIL)
base::Optional<ProactiveSuggestionsAction>
GetDeepLinkParamAsProactiveSuggestionsAction(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param);

// Returns a specific query source |param| from the given parameters. If the
// desired parameter is not found or is not mappable to an Assistant query
// source, an empty value is returned.
COMPONENT_EXPORT(ASSISTANT_UTIL)
base::Optional<chromeos::assistant::mojom::AssistantQuerySource>
GetDeepLinkParamAsQuerySource(const std::map<std::string, std::string>& params,
                              DeepLinkParam param);

// Returns a specific ReminderAction |param| from the given parameters. If the
// desired parameter is not found, an empty value is returned.
COMPONENT_EXPORT(ASSISTANT_UTIL)
base::Optional<ReminderAction> GetDeepLinkParamAsRemindersAction(
    const std::map<std::string, std::string> params,
    DeepLinkParam param);

// Returns TimeDelta from the given parameters. If the desired parameter is not
// found, can't convert to TimeDelta or not a time type parameter, an empty
// value is returned.
COMPONENT_EXPORT(ASSISTANT_UTIL)
base::Optional<base::TimeDelta> GetDeepLinkParamAsTimeDelta(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param);

// Returns the deep link type of the specified |url|. If the specified url is
// not a supported deep link, DeepLinkType::kUnsupported is returned.
COMPONENT_EXPORT(ASSISTANT_UTIL) DeepLinkType GetDeepLinkType(const GURL& url);

// Returns true if the specified |url| is a deep link of the given |type|.
COMPONENT_EXPORT(ASSISTANT_UTIL)
bool IsDeepLinkType(const GURL& url, DeepLinkType type);

// Returns true if the specified |url| is a deep link, false otherwise.
COMPONENT_EXPORT(ASSISTANT_UTIL) bool IsDeepLinkUrl(const GURL& url);

// Returns the Assistant URL for the deep link of the specified |type|. A return
// value will only be present if the deep link type is one of {kLists, kNotes,
// or kReminders}. If |id| is not contained in |params|, the returned URL will
// be for the top-level Assistant URL. Otherwise, the URL will correspond to
// the resource identified by |id|.
COMPONENT_EXPORT(ASSISTANT_UTIL)
base::Optional<GURL> GetAssistantUrl(
    DeepLinkType type,
    const std::map<std::string, std::string>& params);

// Returns the URL for the specified Chrome Settings |page|. If page is absent
// or not allowed, the URL will be for top-level Chrome Settings.
COMPONENT_EXPORT(ASSISTANT_UTIL)
GURL GetChromeSettingsUrl(const base::Optional<std::string>& page);

// Returns the web URL for the specified |deep_link|. A return value will only
// be present if |deep_link| is a web deep link as identified by the
// IsWebDeepLink(GURL) API.
COMPONENT_EXPORT(ASSISTANT_UTIL)
base::Optional<GURL> GetWebUrl(const GURL& deep_link);

// Returns the web URL for a deep link of the specified |type| with the given
// |params|. A return value will only be present if the deep link type is a web
// deep link type as identified by the IsWebDeepLinkType(DeepLinkType) API.
COMPONENT_EXPORT(ASSISTANT_UTIL)
base::Optional<GURL> GetWebUrl(
    DeepLinkType type,
    const std::map<std::string, std::string>& params);

// Returns true if the specified |deep_link| is a web deep link.
COMPONENT_EXPORT(ASSISTANT_UTIL) bool IsWebDeepLink(const GURL& deep_link);

// Returns true if the specified deep link |type| is a web deep link.
COMPONENT_EXPORT(ASSISTANT_UTIL)
bool IsWebDeepLinkType(DeepLinkType type,
                       const std::map<std::string, std::string>& params);

}  // namespace util
}  // namespace assistant
}  // namespace ash

#endif  // ASH_ASSISTANT_UTIL_DEEP_LINK_UTIL_H_
