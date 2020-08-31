// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/user_metrics.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/mixed_content_settings_tab_helper.h"
#include "chrome/browser/content_settings/tab_specific_content_settings_delegate.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/permission_bubble_media_access_handler.h"
#include "chrome/browser/media/webrtc/system_media_capture_permissions_mac.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/plugins/plugin_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subresource_filter/chrome_subresource_filter_client.h"
#include "chrome/browser/ui/blocked_content/popup_blocker_tab_helper.h"
#include "chrome/browser/ui/collected_cookies_infobar_delegate.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model_delegate.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/content_settings/browser/tab_specific_content_settings.h"
#include "components/content_settings/common/content_settings_agent.mojom.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/url_formatter/elide_url.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/origin_util.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/device/public/cpp/device_features.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/resources/grit/ui_resources.h"

using base::UserMetricsAction;
using content::WebContents;
using content_settings::SETTING_SOURCE_NONE;
using content_settings::SETTING_SOURCE_USER;
using content_settings::SettingInfo;
using content_settings::SettingSource;
using content_settings::TabSpecificContentSettings;

namespace {

using QuietUiReason = permissions::PermissionRequestManager::QuietUiReason;

#if defined(OS_MACOSX)
static constexpr char kCameraSettingsURI[] =
    "x-apple.systempreferences:com.apple.preference.security?Privacy_"
    "Camera";
static constexpr char kMicSettingsURI[] =
    "x-apple.systempreferences:com.apple.preference.security?Privacy_"
    "Microphone";
#endif  // defined(OS_MACOSX)

// Returns a boolean indicating whether the setting should be managed by the
// user (i.e. it is not controlled by policy). Also takes a (nullable) out-param
// which is populated by the actual setting for the given URL.
bool GetSettingManagedByUser(const GURL& url,
                             ContentSettingsType type,
                             Profile* profile,
                             ContentSetting* out_setting) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  SettingSource source;
  ContentSetting setting;
  if (type == ContentSettingsType::COOKIES) {
    CookieSettingsFactory::GetForProfile(profile)->GetCookieSetting(
        url, url, &source, &setting);
  } else {
    SettingInfo info;
    std::unique_ptr<base::Value> value =
        map->GetWebsiteSetting(url, url, type, std::string(), &info);
    setting = content_settings::ValueToContentSetting(value.get());
    source = info.source;
  }

  if (out_setting)
    *out_setting = setting;

  // Prevent creation of content settings for illegal urls like about:blank by
  // disallowing user management.
  return source == SETTING_SOURCE_USER &&
         map->CanSetNarrowestContentSetting(url, url, type);
}

ContentSettingBubbleModel::ListItem CreateUrlListItem(int32_t id,
                                                      const GURL& url) {
  // Empty URLs should get a placeholder.
  // TODO(csharrison): See if we can DCHECK that the URL will be valid here.
  base::string16 title = url.spec().empty()
                             ? l10n_util::GetStringUTF16(IDS_TAB_LOADING_TITLE)
                             : base::UTF8ToUTF16(url.spec());

  // Format the title to include the unicode single dot bullet code-point
  // \u2022 and two spaces.
  title = l10n_util::GetStringFUTF16(IDS_LIST_BULLET, title);
  return ContentSettingBubbleModel::ListItem(nullptr, title, base::string16(),
                                             true /* has_link */,
                                             false /* has_blocked_badge */, id);
}

struct ContentSettingsTypeIdEntry {
  ContentSettingsType type;
  int id;
};

int GetIdForContentType(const ContentSettingsTypeIdEntry* entries,
                        size_t num_entries,
                        ContentSettingsType type) {
  for (size_t i = 0; i < num_entries; ++i) {
    if (entries[i].type == type)
      return entries[i].id;
  }
  return 0;
}

void SetAllowRunningInsecureContent(content::RenderFrameHost* frame) {
  mojo::AssociatedRemote<content_settings::mojom::ContentSettingsAgent> agent;
  frame->GetRemoteAssociatedInterfaces()->GetInterface(&agent);
  agent->SetAllowRunningInsecureContent();
}

}  // namespace

// ContentSettingSimpleBubbleModel ---------------------------------------------
ContentSettingBubbleModel::ListItem::ListItem(const gfx::VectorIcon* image,
                                              const base::string16& title,
                                              const base::string16& description,
                                              bool has_link,
                                              bool has_blocked_badge,
                                              int32_t item_id)
    : image(image),
      title(title),
      description(description),
      has_link(has_link),
      has_blocked_badge(has_blocked_badge),
      item_id(item_id) {}

ContentSettingBubbleModel::ListItem::ListItem(const ListItem& other) = default;
ContentSettingBubbleModel::ListItem& ContentSettingBubbleModel::ListItem::
operator=(const ListItem& other) = default;

ContentSettingSimpleBubbleModel::ContentSettingSimpleBubbleModel(
    Delegate* delegate,
    WebContents* web_contents,
    ContentSettingsType content_type)
    : ContentSettingBubbleModel(delegate, web_contents),
      content_type_(content_type) {
  SetTitle();
  SetMessage();
  SetManageText();
  SetCustomLink();
}

ContentSettingSimpleBubbleModel*
    ContentSettingSimpleBubbleModel::AsSimpleBubbleModel() {
  return this;
}

void ContentSettingSimpleBubbleModel::SetTitle() {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());

  static const ContentSettingsTypeIdEntry kBlockedTitleIDs[] = {
      {ContentSettingsType::COOKIES, IDS_BLOCKED_COOKIES_TITLE},
      {ContentSettingsType::IMAGES, IDS_BLOCKED_IMAGES_TITLE},
      {ContentSettingsType::JAVASCRIPT, IDS_BLOCKED_JAVASCRIPT_TITLE},
      {ContentSettingsType::PLUGINS, IDS_BLOCKED_PLUGINS_TITLE},
      {ContentSettingsType::MIXEDSCRIPT,
       IDS_BLOCKED_DISPLAYING_INSECURE_CONTENT_TITLE},
      {ContentSettingsType::PPAPI_BROKER, IDS_BLOCKED_PPAPI_BROKER_TITLE},
      {ContentSettingsType::SOUND, IDS_BLOCKED_SOUND_TITLE},
      {ContentSettingsType::CLIPBOARD_READ_WRITE, IDS_BLOCKED_CLIPBOARD_TITLE},
      {ContentSettingsType::SENSORS, IDS_BLOCKED_SENSORS_TITLE},
  };
  // Fields as for kBlockedTitleIDs, above.
  static const ContentSettingsTypeIdEntry kAccessedTitleIDs[] = {
      {ContentSettingsType::COOKIES, IDS_ACCESSED_COOKIES_TITLE},
      {ContentSettingsType::PPAPI_BROKER, IDS_ALLOWED_PPAPI_BROKER_TITLE},
      {ContentSettingsType::CLIPBOARD_READ_WRITE, IDS_ALLOWED_CLIPBOARD_TITLE},
      {ContentSettingsType::SENSORS, IDS_ALLOWED_SENSORS_TITLE},
  };
  const ContentSettingsTypeIdEntry* title_ids = kBlockedTitleIDs;
  size_t num_title_ids = base::size(kBlockedTitleIDs);
  if (content_settings->IsContentAllowed(content_type()) &&
      !content_settings->IsContentBlocked(content_type())) {
    title_ids = kAccessedTitleIDs;
    num_title_ids = base::size(kAccessedTitleIDs);
  }
  int title_id = GetIdForContentType(title_ids, num_title_ids, content_type());
  if (title_id)
    set_title(l10n_util::GetStringUTF16(title_id));
}

void ContentSettingSimpleBubbleModel::SetMessage() {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());

  // TODO(https://crbug.com/978882): Make the two arrays below static again once
  // we no longer need to check base::FeatureList.
  const ContentSettingsTypeIdEntry kBlockedMessageIDs[] = {
      {ContentSettingsType::COOKIES, IDS_BLOCKED_COOKIES_MESSAGE},
      {ContentSettingsType::IMAGES, IDS_BLOCKED_IMAGES_MESSAGE},
      {ContentSettingsType::JAVASCRIPT, IDS_BLOCKED_JAVASCRIPT_MESSAGE},
      // {ContentSettingsType::POPUPS, No message. intentionally left out},
      {ContentSettingsType::MIXEDSCRIPT,
       IDS_BLOCKED_DISPLAYING_INSECURE_CONTENT},
      {ContentSettingsType::PPAPI_BROKER, IDS_BLOCKED_PPAPI_BROKER_MESSAGE},
      {ContentSettingsType::CLIPBOARD_READ_WRITE,
       IDS_BLOCKED_CLIPBOARD_MESSAGE},
      {ContentSettingsType::SENSORS,
       base::FeatureList::IsEnabled(features::kGenericSensorExtraClasses)
           ? IDS_BLOCKED_SENSORS_MESSAGE
           : IDS_BLOCKED_MOTION_SENSORS_MESSAGE},
  };
  // Fields as for kBlockedMessageIDs, above.
  const ContentSettingsTypeIdEntry kAccessedMessageIDs[] = {
      {ContentSettingsType::COOKIES, IDS_ACCESSED_COOKIES_MESSAGE},
      {ContentSettingsType::PPAPI_BROKER, IDS_ALLOWED_PPAPI_BROKER_MESSAGE},
      {ContentSettingsType::CLIPBOARD_READ_WRITE,
       IDS_ALLOWED_CLIPBOARD_MESSAGE},
      {ContentSettingsType::SENSORS,
       base::FeatureList::IsEnabled(features::kGenericSensorExtraClasses)
           ? IDS_ALLOWED_SENSORS_MESSAGE
           : IDS_ALLOWED_MOTION_SENSORS_MESSAGE},
  };
  const ContentSettingsTypeIdEntry* message_ids = kBlockedMessageIDs;
  size_t num_message_ids = base::size(kBlockedMessageIDs);
  if (content_settings->IsContentAllowed(content_type()) &&
      !content_settings->IsContentBlocked(content_type())) {
    message_ids = kAccessedMessageIDs;
    num_message_ids = base::size(kAccessedMessageIDs);
  }
  int message_id =
      GetIdForContentType(message_ids, num_message_ids, content_type());
  if (message_id)
    set_message(l10n_util::GetStringUTF16(message_id));
}

void ContentSettingSimpleBubbleModel::SetManageText() {
  set_manage_text(l10n_util::GetStringUTF16(IDS_MANAGE));
}

void ContentSettingSimpleBubbleModel::OnManageButtonClicked() {
  if (delegate())
    delegate()->ShowContentSettingsPage(content_type());

  if (content_type() == ContentSettingsType::PLUGINS) {
    content_settings::RecordPluginsAction(
        content_settings::PLUGINS_ACTION_CLICKED_MANAGE_PLUGIN_BLOCKING);
  }

  if (content_type() == ContentSettingsType::POPUPS) {
    content_settings::RecordPopupsAction(
        content_settings::POPUPS_ACTION_CLICKED_MANAGE_POPUPS_BLOCKING);
  }
}

void ContentSettingSimpleBubbleModel::SetCustomLink() {
  static const ContentSettingsTypeIdEntry kCustomIDs[] = {
      {ContentSettingsType::COOKIES, IDS_BLOCKED_COOKIES_INFO},
      {ContentSettingsType::MIXEDSCRIPT, IDS_ALLOW_INSECURE_CONTENT_BUTTON},
  };
  int custom_link_id =
      GetIdForContentType(kCustomIDs, base::size(kCustomIDs), content_type());
  if (custom_link_id)
    set_custom_link(l10n_util::GetStringUTF16(custom_link_id));
}

void ContentSettingSimpleBubbleModel::OnCustomLinkClicked() {
}

// ContentSettingMixedScriptBubbleModel ----------------------------------------

class ContentSettingMixedScriptBubbleModel
    : public ContentSettingSimpleBubbleModel {
 public:
  ContentSettingMixedScriptBubbleModel(Delegate* delegate,
                                       WebContents* web_contents);

  ~ContentSettingMixedScriptBubbleModel() override {}

 private:
  void SetManageText();

  // ContentSettingBubbleModel:
  void OnLearnMoreClicked() override;
  void OnCustomLinkClicked() override;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingMixedScriptBubbleModel);
};

ContentSettingMixedScriptBubbleModel::ContentSettingMixedScriptBubbleModel(
    Delegate* delegate,
    WebContents* web_contents)
    : ContentSettingSimpleBubbleModel(delegate,
                                      web_contents,
                                      ContentSettingsType::MIXEDSCRIPT) {
  set_custom_link_enabled(true);
  set_show_learn_more(true);
  SetManageText();
}

void ContentSettingMixedScriptBubbleModel::OnLearnMoreClicked() {
  if (delegate())
    delegate()->ShowLearnMorePage(content_type());
}

void ContentSettingMixedScriptBubbleModel::OnCustomLinkClicked() {
  DCHECK(rappor_service());
  MixedContentSettingsTabHelper* mixed_content_settings =
      MixedContentSettingsTabHelper::FromWebContents(web_contents());
  if (mixed_content_settings) {
    // Update browser side settings to allow active mixed content.
    mixed_content_settings->AllowRunningOfInsecureContent();
  }

  // Update renderer side settings to allow active mixed content.
  web_contents()->ForEachFrame(
      base::BindRepeating(&::SetAllowRunningInsecureContent));
}

// Don't set any manage text since none is displayed.
void ContentSettingMixedScriptBubbleModel::SetManageText() {
  set_manage_text_style(ContentSettingBubbleModel::ManageTextStyle::kNone);
}

// ContentSettingRPHBubbleModel ------------------------------------------------

namespace {

// These states must match the order of appearance of the radio buttons
// in the XIB file for the Mac port.
enum RPHState {
  RPH_ALLOW = 0,
  RPH_BLOCK,
  RPH_IGNORE,
};

}  // namespace

ContentSettingRPHBubbleModel::ContentSettingRPHBubbleModel(
    Delegate* delegate,
    WebContents* web_contents,
    ProtocolHandlerRegistry* registry)
    : ContentSettingSimpleBubbleModel(delegate,
                                      web_contents,
                                      ContentSettingsType::PROTOCOL_HANDLERS),
      registry_(registry),
      pending_handler_(ProtocolHandler::EmptyProtocolHandler()),
      previous_handler_(ProtocolHandler::EmptyProtocolHandler()) {
  auto* content_settings =
      chrome::TabSpecificContentSettingsDelegate::FromWebContents(web_contents);
  pending_handler_ = content_settings->pending_protocol_handler();
  previous_handler_ = content_settings->previous_protocol_handler();

  base::string16 protocol = pending_handler_.GetProtocolDisplayName();

  // Note that we ignore the |title| parameter.
  if (previous_handler_.IsEmpty()) {
    set_title(l10n_util::GetStringFUTF16(
        IDS_REGISTER_PROTOCOL_HANDLER_CONFIRM,
        base::UTF8ToUTF16(pending_handler_.url().host()), protocol));
  } else {
    set_title(l10n_util::GetStringFUTF16(
        IDS_REGISTER_PROTOCOL_HANDLER_CONFIRM_REPLACE,
        base::UTF8ToUTF16(pending_handler_.url().host()), protocol,
        base::UTF8ToUTF16(previous_handler_.url().host())));
  }

  base::string16 radio_allow_label =
      l10n_util::GetStringUTF16(IDS_REGISTER_PROTOCOL_HANDLER_ACCEPT);
  base::string16 radio_deny_label =
      l10n_util::GetStringUTF16(IDS_REGISTER_PROTOCOL_HANDLER_DENY);
  base::string16 radio_ignore_label =
      l10n_util::GetStringUTF16(IDS_REGISTER_PROTOCOL_HANDLER_IGNORE);

  const GURL& url = web_contents->GetURL();
  RadioGroup radio_group;
  radio_group.url = url;

  radio_group.radio_items = {radio_allow_label, radio_deny_label,
                             radio_ignore_label};
  ContentSetting setting = content_settings->pending_protocol_handler_setting();
  if (setting == CONTENT_SETTING_ALLOW)
    radio_group.default_item = RPH_ALLOW;
  else if (setting == CONTENT_SETTING_BLOCK)
    radio_group.default_item = RPH_BLOCK;
  else
    radio_group.default_item = RPH_IGNORE;

  radio_group.user_managed = true;
  set_radio_group(radio_group);
}

ContentSettingRPHBubbleModel::~ContentSettingRPHBubbleModel() {}

void ContentSettingRPHBubbleModel::CommitChanges() {
  PerformActionForSelectedItem();

  // The user has one chance to deal with the RPH content setting UI,
  // then we remove it.
  chrome::TabSpecificContentSettingsDelegate::FromWebContents(web_contents())
      ->ClearPendingProtocolHandler();
  content_settings::UpdateLocationBarUiForWebContents(web_contents());
}

void ContentSettingRPHBubbleModel::RegisterProtocolHandler() {
  // A no-op if the handler hasn't been ignored, but needed in case the user
  // selects sequences like register/ignore/register.
  registry_->RemoveIgnoredHandler(pending_handler_);

  registry_->OnAcceptRegisterProtocolHandler(pending_handler_);
  chrome::TabSpecificContentSettingsDelegate::FromWebContents(web_contents())
      ->set_pending_protocol_handler_setting(CONTENT_SETTING_ALLOW);
}

void ContentSettingRPHBubbleModel::UnregisterProtocolHandler() {
  registry_->OnDenyRegisterProtocolHandler(pending_handler_);
  chrome::TabSpecificContentSettingsDelegate::FromWebContents(web_contents())
      ->set_pending_protocol_handler_setting(CONTENT_SETTING_BLOCK);
  ClearOrSetPreviousHandler();
}

void ContentSettingRPHBubbleModel::IgnoreProtocolHandler() {
  registry_->OnIgnoreRegisterProtocolHandler(pending_handler_);
  chrome::TabSpecificContentSettingsDelegate::FromWebContents(web_contents())
      ->set_pending_protocol_handler_setting(CONTENT_SETTING_DEFAULT);
  ClearOrSetPreviousHandler();
}

void ContentSettingRPHBubbleModel::ClearOrSetPreviousHandler() {
  if (previous_handler_.IsEmpty()) {
    registry_->ClearDefault(pending_handler_.protocol());
  } else {
    registry_->OnAcceptRegisterProtocolHandler(previous_handler_);
  }
}

void ContentSettingRPHBubbleModel::PerformActionForSelectedItem() {
  if (selected_item() == RPH_ALLOW)
    RegisterProtocolHandler();
  else if (selected_item() == RPH_BLOCK)
    UnregisterProtocolHandler();
  else if (selected_item() == RPH_IGNORE)
    IgnoreProtocolHandler();
  else
    NOTREACHED();
}

// ContentSettingMidiSysExBubbleModel ------------------------------------------

class ContentSettingMidiSysExBubbleModel
    : public ContentSettingSimpleBubbleModel {
 public:
  ContentSettingMidiSysExBubbleModel(Delegate* delegate,
                                     WebContents* web_contents);
  ~ContentSettingMidiSysExBubbleModel() override {}

 private:
  void MaybeAddDomainList(const std::set<std::string>& hosts, int title_id);
  void SetDomainsAndCustomLink();
  void OnCustomLinkClicked() override;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingMidiSysExBubbleModel);
};

ContentSettingMidiSysExBubbleModel::ContentSettingMidiSysExBubbleModel(
    Delegate* delegate,
    WebContents* web_contents)
    : ContentSettingSimpleBubbleModel(delegate,
                                      web_contents,
                                      ContentSettingsType::MIDI_SYSEX) {
  SetDomainsAndCustomLink();
}

void ContentSettingMidiSysExBubbleModel::MaybeAddDomainList(
    const std::set<std::string>& hosts,
    int title_id) {
  if (!hosts.empty()) {
    DomainList domain_list;
    domain_list.title = l10n_util::GetStringUTF16(title_id);
    domain_list.hosts = hosts;
    add_domain_list(domain_list);
  }
}

void ContentSettingMidiSysExBubbleModel::SetDomainsAndCustomLink() {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  const ContentSettingsUsagesState& usages_state =
      content_settings->midi_usages_state();
  ContentSettingsUsagesState::FormattedHostsPerState formatted_hosts_per_state;
  unsigned int tab_state_flags = 0;
  usages_state.GetDetailedInfo(&formatted_hosts_per_state, &tab_state_flags);
  // Divide the tab's current MIDI sysex users into sets according to their
  // permission state.
  MaybeAddDomainList(formatted_hosts_per_state[CONTENT_SETTING_ALLOW],
                     IDS_MIDI_SYSEX_BUBBLE_ALLOWED);

  MaybeAddDomainList(formatted_hosts_per_state[CONTENT_SETTING_BLOCK],
                     IDS_MIDI_SYSEX_BUBBLE_DENIED);

  if (tab_state_flags & ContentSettingsUsagesState::TABSTATE_HAS_EXCEPTION) {
    set_custom_link(
        l10n_util::GetStringUTF16(IDS_MIDI_SYSEX_BUBBLE_CLEAR_LINK));
    set_custom_link_enabled(true);
  } else if (tab_state_flags &
             ContentSettingsUsagesState::TABSTATE_HAS_CHANGED) {
    set_custom_link(l10n_util::GetStringUTF16(
        IDS_MIDI_SYSEX_BUBBLE_REQUIRE_RELOAD_TO_CLEAR));
  }
}

void ContentSettingMidiSysExBubbleModel::OnCustomLinkClicked() {
  // Reset this embedder's entry to default for each of the requesting
  // origins currently on the page.
  const GURL& embedder_url = web_contents()->GetURL();
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  const ContentSettingsUsagesState::StateMap& state_map =
      content_settings->midi_usages_state().state_map();
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());
  for (const std::pair<const GURL, ContentSetting>& map_entry : state_map) {
    permissions::PermissionUmaUtil::ScopedRevocationReporter(
        GetProfile(), map_entry.first, embedder_url,
        ContentSettingsType::MIDI_SYSEX,
        permissions::PermissionSourceUI::PAGE_ACTION);
    map->SetContentSettingDefaultScope(map_entry.first, embedder_url,
                                       ContentSettingsType::MIDI_SYSEX,
                                       std::string(), CONTENT_SETTING_DEFAULT);
  }
}

// ContentSettingDomainListBubbleModel -----------------------------------------

class ContentSettingDomainListBubbleModel
    : public ContentSettingSimpleBubbleModel {
 public:
  ContentSettingDomainListBubbleModel(Delegate* delegate,
                                      WebContents* web_contents,
                                      ContentSettingsType content_type);
  ~ContentSettingDomainListBubbleModel() override {}

 private:
  void MaybeAddDomainList(const std::set<std::string>& hosts, int title_id);
  void SetDomainsAndCustomLink();
  void OnCustomLinkClicked() override;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingDomainListBubbleModel);
};

ContentSettingDomainListBubbleModel::ContentSettingDomainListBubbleModel(
    Delegate* delegate,
    WebContents* web_contents,
    ContentSettingsType content_type)
    : ContentSettingSimpleBubbleModel(delegate,
                                      web_contents,
                                      content_type) {
  DCHECK_EQ(ContentSettingsType::GEOLOCATION, content_type)
      << "SetDomains currently only supports geolocation content type";
  SetDomainsAndCustomLink();
}

void ContentSettingDomainListBubbleModel::MaybeAddDomainList(
    const std::set<std::string>& hosts,
    int title_id) {
  if (!hosts.empty()) {
    DomainList domain_list;
    domain_list.title = l10n_util::GetStringUTF16(title_id);
    domain_list.hosts = hosts;
    add_domain_list(domain_list);
  }
}

void ContentSettingDomainListBubbleModel::SetDomainsAndCustomLink() {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  const ContentSettingsUsagesState& usages =
      content_settings->geolocation_usages_state();
  ContentSettingsUsagesState::FormattedHostsPerState formatted_hosts_per_state;
  unsigned int tab_state_flags = 0;
  usages.GetDetailedInfo(&formatted_hosts_per_state, &tab_state_flags);
  // Divide the tab's current geolocation users into sets according to their
  // permission state.
  MaybeAddDomainList(formatted_hosts_per_state[CONTENT_SETTING_ALLOW],
                     IDS_GEOLOCATION_BUBBLE_SECTION_ALLOWED);

  MaybeAddDomainList(formatted_hosts_per_state[CONTENT_SETTING_BLOCK],
                     IDS_GEOLOCATION_BUBBLE_SECTION_DENIED);

  if (tab_state_flags & ContentSettingsUsagesState::TABSTATE_HAS_EXCEPTION) {
    set_custom_link(
        l10n_util::GetStringUTF16(IDS_GEOLOCATION_BUBBLE_CLEAR_LINK));
    set_custom_link_enabled(true);
  } else if (tab_state_flags &
             ContentSettingsUsagesState::TABSTATE_HAS_CHANGED) {
    set_custom_link(l10n_util::GetStringUTF16(
        IDS_GEOLOCATION_BUBBLE_REQUIRE_RELOAD_TO_CLEAR));
  }
}

void ContentSettingDomainListBubbleModel::OnCustomLinkClicked() {
  // Reset this embedder's entry to default for each of the requesting
  // origins currently on the page.
  const GURL& embedder_url = web_contents()->GetURL();
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  const ContentSettingsUsagesState::StateMap& state_map =
      content_settings->geolocation_usages_state().state_map();
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());
  for (const std::pair<const GURL, ContentSetting>& map_entry : state_map) {
    permissions::PermissionUmaUtil::ScopedRevocationReporter(
        GetProfile(), map_entry.first, embedder_url,
        ContentSettingsType::GEOLOCATION,
        permissions::PermissionSourceUI::PAGE_ACTION);
    map->SetContentSettingDefaultScope(map_entry.first, embedder_url,
                                       ContentSettingsType::GEOLOCATION,
                                       std::string(), CONTENT_SETTING_DEFAULT);
  }
}

// ContentSettingPluginBubbleModel ---------------------------------------------

class ContentSettingPluginBubbleModel : public ContentSettingSimpleBubbleModel {
 public:
  ContentSettingPluginBubbleModel(Delegate* delegate,
                                  WebContents* web_contents);

 private:
  void OnLearnMoreClicked() override;
  void OnCustomLinkClicked() override;

  void RunPluginsOnPage();

  DISALLOW_COPY_AND_ASSIGN(ContentSettingPluginBubbleModel);
};

ContentSettingPluginBubbleModel::ContentSettingPluginBubbleModel(
    Delegate* delegate,
    WebContents* web_contents)
    : ContentSettingSimpleBubbleModel(delegate,
                                      web_contents,
                                      ContentSettingsType::PLUGINS) {
  const GURL& url = web_contents->GetURL();
  bool managed_by_user =
      GetSettingManagedByUser(url, content_type(), GetProfile(), nullptr);
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());
  ContentSetting setting = PluginUtils::GetFlashPluginContentSetting(
      map, url::Origin::Create(url), url, nullptr);

  // If the setting is not managed by the user, hide the "Manage" button.
  if (!managed_by_user)
    set_manage_text_style(ContentSettingBubbleModel::ManageTextStyle::kNone);

  // The user can only load Flash dynamically if not on the BLOCK setting.
  if (setting != CONTENT_SETTING_BLOCK) {
    set_custom_link(l10n_util::GetStringUTF16(IDS_BLOCKED_PLUGINS_LOAD_ALL));
    // Disable the "Run all plugins this time" link if the user already clicked
    // on the link and ran all plugins.
    set_custom_link_enabled(
        TabSpecificContentSettings::FromWebContents(web_contents)
            ->load_plugins_link_enabled());
  }

  set_show_learn_more(true);

  content_settings::RecordPluginsAction(
      content_settings::PLUGINS_ACTION_DISPLAYED_BUBBLE);
}

void ContentSettingPluginBubbleModel::OnLearnMoreClicked() {
  if (delegate())
    delegate()->ShowLearnMorePage(ContentSettingsType::PLUGINS);

  content_settings::RecordPluginsAction(
      content_settings::PLUGINS_ACTION_CLICKED_LEARN_MORE);
}

void ContentSettingPluginBubbleModel::OnCustomLinkClicked() {
  base::RecordAction(UserMetricsAction("ClickToPlay_LoadAll_Bubble"));
  content_settings::RecordPluginsAction(
      content_settings::PLUGINS_ACTION_CLICKED_RUN_ALL_PLUGINS_THIS_TIME);

  RunPluginsOnPage();
}

void ContentSettingPluginBubbleModel::RunPluginsOnPage() {
#if BUILDFLAG(ENABLE_PLUGINS)
  // TODO(bauerb): We should send the identifiers of blocked plugins here.
  ChromePluginServiceFilter::GetInstance()->AuthorizeAllPlugins(
      web_contents(), true, std::string());
#endif
  set_custom_link_enabled(false);
  TabSpecificContentSettings::FromWebContents(web_contents())
      ->set_load_plugins_link_enabled(false);
}

// ContentSettingSingleRadioGroup ----------------------------------------------

ContentSettingSingleRadioGroup::ContentSettingSingleRadioGroup(
    Delegate* delegate,
    WebContents* web_contents,
    ContentSettingsType content_type)
    : ContentSettingSimpleBubbleModel(delegate,
                                      web_contents,
                                      content_type),
      block_setting_(CONTENT_SETTING_BLOCK) {
  SetRadioGroup();
}

ContentSettingSingleRadioGroup::~ContentSettingSingleRadioGroup() {}

void ContentSettingSingleRadioGroup::CommitChanges() {
  if (settings_changed()) {
    ContentSetting setting = selected_item() == kAllowButtonIndex
                                 ? CONTENT_SETTING_ALLOW
                                 : block_setting_;
    SetNarrowestContentSetting(setting);
  }
}

bool ContentSettingSingleRadioGroup::settings_changed() const {
  return selected_item() != bubble_content().radio_group.default_item;
}

// Initialize the radio group by setting the appropriate labels for the
// content type and setting the default value based on the content setting.
void ContentSettingSingleRadioGroup::SetRadioGroup() {
  const GURL& url = web_contents()->GetURL();
  base::string16 display_host = url_formatter::FormatUrlForSecurityDisplay(url);

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  bool allowed = !content_settings->IsContentBlocked(content_type());

  // For the frame busting case the content is blocked but its content type is
  // popup, and the popup TabSpecificContentSettings is unaware of the frame
  // busting block. Since the popup bubble won't happen without blocking, it's
  // safe to manually set this.
  if (content_type() == ContentSettingsType::POPUPS)
    allowed = false;

  DCHECK(!allowed || content_settings->IsContentAllowed(content_type()));

  RadioGroup radio_group;
  radio_group.url = url;

  static const ContentSettingsTypeIdEntry kBlockedAllowIDs[] = {
      {ContentSettingsType::COOKIES, IDS_BLOCKED_COOKIES_UNBLOCK},
      {ContentSettingsType::IMAGES, IDS_BLOCKED_IMAGES_UNBLOCK},
      {ContentSettingsType::JAVASCRIPT, IDS_BLOCKED_JAVASCRIPT_UNBLOCK},
      {ContentSettingsType::POPUPS, IDS_BLOCKED_POPUPS_REDIRECTS_UNBLOCK},
      {ContentSettingsType::PPAPI_BROKER, IDS_BLOCKED_PPAPI_BROKER_UNBLOCK},
      {ContentSettingsType::SOUND, IDS_BLOCKED_SOUND_UNBLOCK},
      {ContentSettingsType::CLIPBOARD_READ_WRITE,
       IDS_BLOCKED_CLIPBOARD_UNBLOCK},
      {ContentSettingsType::SENSORS, IDS_BLOCKED_SENSORS_UNBLOCK},
  };
  // Fields as for kBlockedAllowIDs, above.
  static const ContentSettingsTypeIdEntry kAllowedAllowIDs[] = {
      {ContentSettingsType::COOKIES, IDS_ALLOWED_COOKIES_NO_ACTION},
      {ContentSettingsType::PPAPI_BROKER, IDS_ALLOWED_PPAPI_BROKER_NO_ACTION},
      {ContentSettingsType::CLIPBOARD_READ_WRITE,
       IDS_ALLOWED_CLIPBOARD_NO_ACTION},
      {ContentSettingsType::SENSORS, IDS_ALLOWED_SENSORS_NO_ACTION},
  };

  base::string16 radio_allow_label;
  if (allowed) {
    int resource_id = GetIdForContentType(
        kAllowedAllowIDs, base::size(kAllowedAllowIDs), content_type());
    radio_allow_label = l10n_util::GetStringUTF16(resource_id);
  } else {
    radio_allow_label = l10n_util::GetStringFUTF16(
        GetIdForContentType(kBlockedAllowIDs, base::size(kBlockedAllowIDs),
                            content_type()),
        display_host);
  }

  static const ContentSettingsTypeIdEntry kBlockedBlockIDs[] = {
      {ContentSettingsType::COOKIES, IDS_BLOCKED_COOKIES_NO_ACTION},
      {ContentSettingsType::IMAGES, IDS_BLOCKED_IMAGES_NO_ACTION},
      {ContentSettingsType::JAVASCRIPT, IDS_BLOCKED_JAVASCRIPT_NO_ACTION},
      {ContentSettingsType::POPUPS, IDS_BLOCKED_POPUPS_REDIRECTS_NO_ACTION},
      {ContentSettingsType::PPAPI_BROKER, IDS_BLOCKED_PPAPI_BROKER_NO_ACTION},
      {ContentSettingsType::SOUND, IDS_BLOCKED_SOUND_NO_ACTION},
      {ContentSettingsType::CLIPBOARD_READ_WRITE,
       IDS_BLOCKED_CLIPBOARD_NO_ACTION},
      {ContentSettingsType::SENSORS, IDS_BLOCKED_SENSORS_NO_ACTION},
  };
  static const ContentSettingsTypeIdEntry kAllowedBlockIDs[] = {
      {ContentSettingsType::COOKIES, IDS_ALLOWED_COOKIES_BLOCK},
      {ContentSettingsType::PPAPI_BROKER, IDS_ALLOWED_PPAPI_BROKER_BLOCK},
      {ContentSettingsType::CLIPBOARD_READ_WRITE, IDS_ALLOWED_CLIPBOARD_BLOCK},
      {ContentSettingsType::SENSORS, IDS_ALLOWED_SENSORS_BLOCK},
  };

  base::string16 radio_block_label;
  if (allowed) {
    int resource_id = GetIdForContentType(
        kAllowedBlockIDs, base::size(kAllowedBlockIDs), content_type());
    radio_block_label = l10n_util::GetStringFUTF16(resource_id, display_host);
  } else {
    radio_block_label = l10n_util::GetStringUTF16(GetIdForContentType(
        kBlockedBlockIDs, base::size(kBlockedBlockIDs), content_type()));
  }

  radio_group.radio_items = {radio_allow_label, radio_block_label};

  ContentSetting setting;
  radio_group.user_managed =
      GetSettingManagedByUser(url, content_type(), GetProfile(), &setting);
  if (setting == CONTENT_SETTING_ALLOW) {
    radio_group.default_item = kAllowButtonIndex;
    // |block_setting_| is already set to |CONTENT_SETTING_BLOCK|.
  } else {
    radio_group.default_item = 1;
    block_setting_ = setting;
  }
  set_radio_group(radio_group);
}

void ContentSettingSingleRadioGroup::SetNarrowestContentSetting(
    ContentSetting setting) {
  if (!GetProfile())
    return;

  auto* map = HostContentSettingsMapFactory::GetForProfile(GetProfile());
  map->SetNarrowestContentSetting(bubble_content().radio_group.url,
                                  bubble_content().radio_group.url,
                                  content_type(), setting);
}

// ContentSettingCookiesBubbleModel --------------------------------------------

class ContentSettingCookiesBubbleModel : public ContentSettingSingleRadioGroup {
 public:
  ContentSettingCookiesBubbleModel(Delegate* delegate,
                                   WebContents* web_contents);
  ~ContentSettingCookiesBubbleModel() override;

  // ContentSettingBubbleModel:
  void CommitChanges() override;

 private:
  void OnCustomLinkClicked() override;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingCookiesBubbleModel);
};

ContentSettingCookiesBubbleModel::ContentSettingCookiesBubbleModel(
    Delegate* delegate,
    WebContents* web_contents)
    : ContentSettingSingleRadioGroup(delegate,
                                     web_contents,
                                     ContentSettingsType::COOKIES) {
  set_custom_link_enabled(true);
}

ContentSettingCookiesBubbleModel::~ContentSettingCookiesBubbleModel() {}

void ContentSettingCookiesBubbleModel::CommitChanges() {
  // On some plattforms e.g. MacOS X it is possible to close a tab while the
  // cookies settings bubble is open. This resets the web contents to NULL.
  if (settings_changed()) {
    CollectedCookiesInfoBarDelegate::Create(
        InfoBarService::FromWebContents(web_contents()));
  }
  ContentSettingSingleRadioGroup::CommitChanges();
}

void ContentSettingCookiesBubbleModel::OnCustomLinkClicked() {
  delegate()->ShowCollectedCookiesDialog(web_contents());
}

// ContentSettingPopupBubbleModel ----------------------------------------------

class ContentSettingPopupBubbleModel : public ContentSettingSingleRadioGroup,
                                       public UrlListManager::Observer {
 public:
  ContentSettingPopupBubbleModel(Delegate* delegate, WebContents* web_contents);
  ~ContentSettingPopupBubbleModel() override;

  // ContentSettingBubbleModel:
  void CommitChanges() override;

  // PopupBlockerTabHelper::Observer:
  void BlockedUrlAdded(int32_t id, const GURL& url) override;

 private:
  void OnListItemClicked(int index, int event_flags) override;

  int32_t item_id_from_item_index(int index) const {
    return bubble_content().list_items[index].item_id;
  }

  ScopedObserver<UrlListManager, UrlListManager::Observer> url_list_observer_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingPopupBubbleModel);
};

ContentSettingPopupBubbleModel::ContentSettingPopupBubbleModel(
    Delegate* delegate,
    WebContents* web_contents)
    : ContentSettingSingleRadioGroup(delegate,
                                     web_contents,
                                     ContentSettingsType::POPUPS),
      url_list_observer_(this) {
  set_title(l10n_util::GetStringUTF16(IDS_BLOCKED_POPUPS_TITLE));

  // Build blocked popup list.
  auto* helper = PopupBlockerTabHelper::FromWebContents(web_contents);
  std::map<int32_t, GURL> blocked_popups = helper->GetBlockedPopupRequests();
  for (const auto& blocked_popup : blocked_popups)
    AddListItem(CreateUrlListItem(blocked_popup.first, blocked_popup.second));

  url_list_observer_.Add(helper->manager());
  content_settings::RecordPopupsAction(
      content_settings::POPUPS_ACTION_DISPLAYED_BUBBLE);
}

void ContentSettingPopupBubbleModel::BlockedUrlAdded(int32_t id,
                                                     const GURL& url) {
  AddListItem(CreateUrlListItem(id, url));
}

void ContentSettingPopupBubbleModel::OnListItemClicked(int index,
                                                       int event_flags) {
  auto* helper = PopupBlockerTabHelper::FromWebContents(web_contents());
  helper->ShowBlockedPopup(item_id_from_item_index(index),
                           ui::DispositionFromEventFlags(event_flags));
  RemoveListItem(index);
  content_settings::RecordPopupsAction(
      content_settings::POPUPS_ACTION_CLICKED_LIST_ITEM_CLICKED);
}

void ContentSettingPopupBubbleModel::CommitChanges() {
  // User selected to always allow pop-ups from.
  if (settings_changed() && selected_item() == kAllowButtonIndex) {
    // Increases the counter.
    content_settings::RecordPopupsAction(
        content_settings::POPUPS_ACTION_SELECTED_ALWAYS_ALLOW_POPUPS_FROM);
  }
  ContentSettingSingleRadioGroup::CommitChanges();
}

ContentSettingPopupBubbleModel::~ContentSettingPopupBubbleModel() = default;

// ContentSettingMediaStreamBubbleModel ----------------------------------------

namespace {

const blink::MediaStreamDevice& GetMediaDeviceById(
    const std::string& device_id,
    const blink::MediaStreamDevices& devices) {
  DCHECK(!devices.empty());
  for (const blink::MediaStreamDevice& device : devices) {
    if (device.id == device_id)
      return device;
  }

  // A device with the |device_id| was not found. It is likely that the device
  // has been unplugged from the OS. Return the first device as the default
  // device.
  return *devices.begin();
}

}  // namespace

ContentSettingMediaStreamBubbleModel::ContentSettingMediaStreamBubbleModel(
    Delegate* delegate,
    WebContents* web_contents)
    : ContentSettingBubbleModel(delegate, web_contents),
      state_(TabSpecificContentSettings::MICROPHONE_CAMERA_NOT_ACCESSED) {
  // TODO(msramek): The media bubble has three states - mic only, camera only,
  // and both. There is a lot of duplicated code which does the same thing
  // for camera and microphone separately. Consider refactoring it to avoid
  // duplication.

  // Initialize the content settings associated with the individual radio
  // buttons.
  radio_item_setting_[0] = CONTENT_SETTING_ASK;
  radio_item_setting_[1] = CONTENT_SETTING_BLOCK;

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  state_ = content_settings->GetMicrophoneCameraState();
  DCHECK(CameraAccessed() || MicrophoneAccessed());

  // If the permission is turned off in MacOS system preferences, overwrite
  // the bubble to enable the user to trigger the system dialog.
  if (ShouldShowSystemMediaPermissions()) {
#if defined(OS_MACOSX)
    InitializeSystemMediaPermissionBubble();
    return;
#endif  // defined(OS_MACOSX)
  }

  SetTitle();
  SetMessage();
  SetRadioGroup();
  SetMediaMenus();
  SetManageText();
  SetCustomLink();
}

ContentSettingMediaStreamBubbleModel::~ContentSettingMediaStreamBubbleModel() {}

void ContentSettingMediaStreamBubbleModel::CommitChanges() {
  for (const auto& media_menu : bubble_content().media_menus) {
    const MediaMenu& menu = media_menu.second;
    if (menu.selected_device.id != menu.default_device.id)
      UpdateDefaultDeviceForType(media_menu.first, menu.selected_device.id);
  }

  // No need for radio group in the bubble UI shown when permission is blocked
  // on a system level.
  if (!ShouldShowSystemMediaPermissions()) {
    // Update the media settings if the radio button selection was changed.
    if (selected_item() != bubble_content().radio_group.default_item)
      UpdateSettings(radio_item_setting_[selected_item()]);
  }
}

ContentSettingMediaStreamBubbleModel*
ContentSettingMediaStreamBubbleModel::AsMediaStreamBubbleModel() {
  return this;
}

void ContentSettingMediaStreamBubbleModel::OnManageButtonClicked() {
  DCHECK(CameraAccessed() || MicrophoneAccessed());
  if (!delegate())
    return;

  if (MicrophoneAccessed() && CameraAccessed()) {
    delegate()->ShowMediaSettingsPage();
  } else {
    delegate()->ShowContentSettingsPage(
        CameraAccessed() ? ContentSettingsType::MEDIASTREAM_CAMERA
                         : ContentSettingsType::MEDIASTREAM_MIC);
  }
}

void ContentSettingMediaStreamBubbleModel::OnDoneButtonClicked() {
  if (ShouldShowSystemMediaPermissions()) {
#if defined(OS_MACOSX)
    DCHECK(CameraAccessed() || MicrophoneAccessed());

    base::RecordAction(UserMetricsAction("Media.OpenPreferencesClicked"));
    DCHECK(ShouldShowSystemMediaPermissions());

    if (CameraAccessed()) {
      ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck(
          GURL(kCameraSettingsURI), web_contents());
    } else if (MicrophoneAccessed()) {
      ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck(
          GURL(kMicSettingsURI), web_contents());
    }
    return;
#endif  // defined(OS_MACOSX)
  }
}

bool ContentSettingMediaStreamBubbleModel::MicrophoneAccessed() const {
  return (state_ & TabSpecificContentSettings::MICROPHONE_ACCESSED) != 0;
}

bool ContentSettingMediaStreamBubbleModel::CameraAccessed() const {
  return (state_ & TabSpecificContentSettings::CAMERA_ACCESSED) != 0;
}

bool ContentSettingMediaStreamBubbleModel::MicrophoneBlocked() const {
  return (state_ & TabSpecificContentSettings::MICROPHONE_BLOCKED) != 0;
}

bool ContentSettingMediaStreamBubbleModel::CameraBlocked() const {
  return (state_ & TabSpecificContentSettings::CAMERA_BLOCKED) != 0;
}

void ContentSettingMediaStreamBubbleModel::SetTitle() {
  DCHECK(CameraAccessed() || MicrophoneAccessed());
  int title_id = 0;
  if (MicrophoneBlocked() && CameraBlocked())
    title_id = IDS_MICROPHONE_CAMERA_BLOCKED_TITLE;
  else if (MicrophoneBlocked())
    title_id = IDS_MICROPHONE_BLOCKED_TITLE;
  else if (CameraBlocked())
    title_id = IDS_CAMERA_BLOCKED_TITLE;
  else if (MicrophoneAccessed() && CameraAccessed())
    title_id = IDS_MICROPHONE_CAMERA_ALLOWED_TITLE;
  else if (MicrophoneAccessed())
    title_id = IDS_MICROPHONE_ACCESSED_TITLE;
  else if (CameraAccessed())
    title_id = IDS_CAMERA_ACCESSED_TITLE;
  else
    NOTREACHED();
  set_title(l10n_util::GetStringUTF16(title_id));
}

void ContentSettingMediaStreamBubbleModel::SetMessage() {
  DCHECK(CameraAccessed() || MicrophoneAccessed());
  int message_id = 0;
  if (MicrophoneBlocked() && CameraBlocked())
    message_id = IDS_MICROPHONE_CAMERA_BLOCKED;
  else if (MicrophoneBlocked())
    message_id = IDS_MICROPHONE_BLOCKED;
  else if (CameraBlocked())
    message_id = IDS_CAMERA_BLOCKED;
  else if (MicrophoneAccessed() && CameraAccessed())
    message_id = IDS_MICROPHONE_CAMERA_ALLOWED;
  else if (MicrophoneAccessed())
    message_id = IDS_MICROPHONE_ACCESSED;
  else if (CameraAccessed())
    message_id = IDS_CAMERA_ACCESSED;
  else
    NOTREACHED();
  set_message(l10n_util::GetStringUTF16(message_id));
}

void ContentSettingMediaStreamBubbleModel::SetRadioGroup() {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  GURL url = content_settings->media_stream_access_origin();
  RadioGroup radio_group;
  radio_group.url = url;

  base::string16 display_host = url_formatter::FormatUrlForSecurityDisplay(url);

  DCHECK(CameraAccessed() || MicrophoneAccessed());
  int radio_allow_label_id = 0;
  int radio_block_label_id = 0;
  if (state_ & (TabSpecificContentSettings::MICROPHONE_BLOCKED |
                TabSpecificContentSettings::CAMERA_BLOCKED)) {
    if (content::IsOriginSecure(url)) {
      radio_item_setting_[0] = CONTENT_SETTING_ALLOW;
      radio_allow_label_id = IDS_BLOCKED_MEDIASTREAM_CAMERA_ALLOW;
      if (MicrophoneAccessed())
        radio_allow_label_id =
            CameraAccessed() ? IDS_BLOCKED_MEDIASTREAM_MIC_AND_CAMERA_ALLOW
                             : IDS_BLOCKED_MEDIASTREAM_MIC_ALLOW;
    } else {
      radio_allow_label_id = IDS_BLOCKED_MEDIASTREAM_CAMERA_ASK;
      if (MicrophoneAccessed())
        radio_allow_label_id = CameraAccessed()
                                   ? IDS_BLOCKED_MEDIASTREAM_MIC_AND_CAMERA_ASK
                                   : IDS_BLOCKED_MEDIASTREAM_MIC_ASK;
    }
    radio_block_label_id = IDS_BLOCKED_MEDIASTREAM_CAMERA_NO_ACTION;
    if (MicrophoneAccessed())
      radio_block_label_id =
          CameraAccessed() ? IDS_BLOCKED_MEDIASTREAM_MIC_AND_CAMERA_NO_ACTION
                           : IDS_BLOCKED_MEDIASTREAM_MIC_NO_ACTION;
  } else {
    if (MicrophoneAccessed() && CameraAccessed()) {
      radio_allow_label_id = IDS_ALLOWED_MEDIASTREAM_MIC_AND_CAMERA_NO_ACTION;
      radio_block_label_id = IDS_ALLOWED_MEDIASTREAM_MIC_AND_CAMERA_BLOCK;
    } else if (MicrophoneAccessed()) {
      radio_allow_label_id = IDS_ALLOWED_MEDIASTREAM_MIC_NO_ACTION;
      radio_block_label_id = IDS_ALLOWED_MEDIASTREAM_MIC_BLOCK;
    } else {
      radio_allow_label_id = IDS_ALLOWED_MEDIASTREAM_CAMERA_NO_ACTION;
      radio_block_label_id = IDS_ALLOWED_MEDIASTREAM_CAMERA_BLOCK;
    }
  }

  base::string16 radio_allow_label =
      l10n_util::GetStringFUTF16(radio_allow_label_id, display_host);
  base::string16 radio_block_label =
      l10n_util::GetStringUTF16(radio_block_label_id);

  radio_group.default_item =
      (MicrophoneAccessed() && content_settings->IsContentBlocked(
                                   ContentSettingsType::MEDIASTREAM_MIC)) ||
              (CameraAccessed() && content_settings->IsContentBlocked(
                                       ContentSettingsType::MEDIASTREAM_CAMERA))
          ? 1
          : 0;
  radio_group.radio_items = {radio_allow_label, radio_block_label};
  radio_group.user_managed = true;

  set_radio_group(radio_group);
}

void ContentSettingMediaStreamBubbleModel::UpdateSettings(
    ContentSetting setting) {
  TabSpecificContentSettings* tab_content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  // The same urls must be used as in other places (e.g. the infobar) in
  // order to override the existing rule. Otherwise a new rule is created.
  // TODO(markusheintz): Extract to a helper so that there is only a single
  // place to touch.
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());
  if (MicrophoneAccessed()) {
    permissions::PermissionUmaUtil::ScopedRevocationReporter
        scoped_revocation_reporter(
            GetProfile(), tab_content_settings->media_stream_access_origin(),
            GURL(), ContentSettingsType::MEDIASTREAM_MIC,
            permissions::PermissionSourceUI::PAGE_ACTION);
    map->SetContentSettingDefaultScope(
        tab_content_settings->media_stream_access_origin(), GURL(),
        ContentSettingsType::MEDIASTREAM_MIC, std::string(), setting);
  }
  if (CameraAccessed()) {
    permissions::PermissionUmaUtil::ScopedRevocationReporter
        scoped_revocation_reporter(
            GetProfile(), tab_content_settings->media_stream_access_origin(),
            GURL(), ContentSettingsType::MEDIASTREAM_CAMERA,
            permissions::PermissionSourceUI::PAGE_ACTION);
    map->SetContentSettingDefaultScope(
        tab_content_settings->media_stream_access_origin(), GURL(),
        ContentSettingsType::MEDIASTREAM_CAMERA, std::string(), setting);
  }
}

#if defined(OS_MACOSX)
void ContentSettingMediaStreamBubbleModel::
    InitializeSystemMediaPermissionBubble() {
  DCHECK(CameraAccessed() || MicrophoneAccessed());
  base::RecordAction(
      base::UserMetricsAction("Media.ShowSystemMediaPermissionBubble"));
  int title_id = 0;
  if (MicrophoneAccessed() && CameraAccessed() &&
      (system_media_permissions::CheckSystemVideoCapturePermission() ==
           system_media_permissions::SystemPermission::kDenied ||
       system_media_permissions::CheckSystemAudioCapturePermission() ==
           system_media_permissions::SystemPermission::kDenied)) {
    title_id = IDS_CAMERA_MIC_TURNED_OFF_IN_MACOS;
    AddListItem(ContentSettingBubbleModel::ListItem(
        &vector_icons::kVideocamIcon, l10n_util::GetStringUTF16(IDS_CAMERA),
        l10n_util::GetStringUTF16(IDS_TURNED_OFF), false, true, 0));
    AddListItem(ContentSettingBubbleModel::ListItem(
        &vector_icons::kMicIcon, l10n_util::GetStringUTF16(IDS_MIC),
        l10n_util::GetStringUTF16(IDS_TURNED_OFF), false, true, 1));
  } else if (CameraAccessed() &&
             system_media_permissions::CheckSystemVideoCapturePermission() ==
                 system_media_permissions::SystemPermission::kDenied) {
    title_id = IDS_CAMERA_TURNED_OFF_IN_MACOS;
    AddListItem(ContentSettingBubbleModel::ListItem(
        &vector_icons::kVideocamIcon, l10n_util::GetStringUTF16(IDS_CAMERA),
        l10n_util::GetStringUTF16(IDS_TURNED_OFF), false, true, 0));
  } else if (MicrophoneAccessed() &&
             system_media_permissions::CheckSystemAudioCapturePermission() ==
                 system_media_permissions::SystemPermission::kDenied) {
    title_id = IDS_MIC_TURNED_OFF_IN_MACOS;
    AddListItem(ContentSettingBubbleModel::ListItem(
        &vector_icons::kMicIcon, l10n_util::GetStringUTF16(IDS_MIC),
        l10n_util::GetStringUTF16(IDS_TURNED_OFF), false, true, 1));
  }

  set_title(l10n_util::GetStringUTF16(title_id));
  set_manage_text_style(ContentSettingBubbleModel::ManageTextStyle::kNone);
  SetCustomLink();
  set_done_button_text(l10n_util::GetStringUTF16(IDS_OPEN_PREFERENCES_LINK));
}
#endif  // defined(OS_MACOSX)

bool ContentSettingMediaStreamBubbleModel::ShouldShowSystemMediaPermissions() {
#if defined(OS_MACOSX)
  return (((system_media_permissions::CheckSystemVideoCapturePermission() ==
                system_media_permissions::SystemPermission::kDenied &&
            CameraAccessed() && !CameraBlocked()) ||
           (system_media_permissions::CheckSystemAudioCapturePermission() ==
                system_media_permissions::SystemPermission::kDenied &&
            MicrophoneAccessed() && !MicrophoneBlocked())) &&
          !(CameraAccessed() && CameraBlocked()) &&
          !(MicrophoneAccessed() && MicrophoneBlocked()) &&
          base::FeatureList::IsEnabled(
              ::features::kMacSystemMediaPermissionsInfoUi));
#else
  return false;
#endif  // defined(OS_MACOSX)
}

void ContentSettingMediaStreamBubbleModel::UpdateDefaultDeviceForType(
    blink::mojom::MediaStreamType type,
    const std::string& device) {
  PrefService* prefs = GetProfile()->GetPrefs();
  if (type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
    prefs->SetString(prefs::kDefaultAudioCaptureDevice, device);
  } else {
    DCHECK_EQ(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, type);
    prefs->SetString(prefs::kDefaultVideoCaptureDevice, device);
  }
}

void ContentSettingMediaStreamBubbleModel::SetMediaMenus() {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  const std::string& requested_microphone =
      content_settings->media_stream_requested_audio_device();
  const std::string& requested_camera =
      content_settings->media_stream_requested_video_device();

  // Add microphone menu.
  PrefService* prefs = GetProfile()->GetPrefs();
  MediaCaptureDevicesDispatcher* dispatcher =
      MediaCaptureDevicesDispatcher::GetInstance();

  if (MicrophoneAccessed()) {
    const blink::MediaStreamDevices& microphones =
        dispatcher->GetAudioCaptureDevices();
    MediaMenu mic_menu;
    mic_menu.label = l10n_util::GetStringUTF16(IDS_MEDIA_SELECTED_MIC_LABEL);
    if (!microphones.empty()) {
      std::string preferred_mic;
      if (requested_microphone.empty()) {
        preferred_mic = prefs->GetString(prefs::kDefaultAudioCaptureDevice);
        mic_menu.disabled = false;
      } else {
        // Set the |disabled| to true in order to disable the device selection
        // menu on the media settings bubble. This must be done if the website
        // manages the microphone devices itself.
        preferred_mic = requested_microphone;
        mic_menu.disabled = true;
      }

      mic_menu.default_device = GetMediaDeviceById(preferred_mic, microphones);
      mic_menu.selected_device = mic_menu.default_device;
    }
    add_media_menu(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
                   mic_menu);
  }

  if (CameraAccessed()) {
    const blink::MediaStreamDevices& cameras =
        dispatcher->GetVideoCaptureDevices();
    MediaMenu camera_menu;
    camera_menu.label =
        l10n_util::GetStringUTF16(IDS_MEDIA_SELECTED_CAMERA_LABEL);
    if (!cameras.empty()) {
      std::string preferred_camera;
      if (requested_camera.empty()) {
        preferred_camera = prefs->GetString(prefs::kDefaultVideoCaptureDevice);
        camera_menu.disabled = false;
      } else {
        // Disable the menu since the website is managing the camera devices
        // itself.
        preferred_camera = requested_camera;
        camera_menu.disabled = true;
      }

      camera_menu.default_device =
          GetMediaDeviceById(preferred_camera, cameras);
      camera_menu.selected_device = camera_menu.default_device;
    }
    add_media_menu(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
                   camera_menu);
  }
}

void ContentSettingMediaStreamBubbleModel::SetManageText() {
  DCHECK(CameraAccessed() || MicrophoneAccessed());
  set_manage_text(l10n_util::GetStringUTF16(IDS_MANAGE));
}

void ContentSettingMediaStreamBubbleModel::SetCustomLink() {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  if (content_settings->IsMicrophoneCameraStateChanged()) {
    set_custom_link(
        l10n_util::GetStringUTF16(IDS_MEDIASTREAM_SETTING_CHANGED_MESSAGE));
  }
}

void ContentSettingMediaStreamBubbleModel::OnMediaMenuClicked(
    blink::mojom::MediaStreamType type,
    const std::string& selected_device_id) {
  DCHECK(type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE ||
         type == blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  DCHECK_EQ(1U, bubble_content().media_menus.count(type));
  MediaCaptureDevicesDispatcher* dispatcher =
      MediaCaptureDevicesDispatcher::GetInstance();
  const blink::MediaStreamDevices& devices =
      (type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE)
          ? dispatcher->GetAudioCaptureDevices()
          : dispatcher->GetVideoCaptureDevices();
  set_selected_device(GetMediaDeviceById(selected_device_id, devices));
}

// ContentSettingSubresourceFilterBubbleModel ----------------------------------

ContentSettingSubresourceFilterBubbleModel::
    ContentSettingSubresourceFilterBubbleModel(Delegate* delegate,
                                               WebContents* web_contents)
    : ContentSettingBubbleModel(delegate, web_contents) {
  SetTitle();
  SetMessage();
  SetManageText();
  set_done_button_text(l10n_util::GetStringUTF16(IDS_OK));
  set_show_learn_more(true);
  ChromeSubresourceFilterClient::LogAction(
      SubresourceFilterAction::kDetailsShown);
}

ContentSettingSubresourceFilterBubbleModel::
    ~ContentSettingSubresourceFilterBubbleModel() = default;

void ContentSettingSubresourceFilterBubbleModel::SetTitle() {
  set_title(l10n_util::GetStringUTF16(IDS_BLOCKED_ADS_PROMPT_TITLE));
}

void ContentSettingSubresourceFilterBubbleModel::SetManageText() {
  set_manage_text(l10n_util::GetStringUTF16(IDS_ALWAYS_ALLOW_ADS));
  set_manage_text_style(ContentSettingBubbleModel::ManageTextStyle::kCheckbox);
}

void ContentSettingSubresourceFilterBubbleModel::SetMessage() {
  set_message(l10n_util::GetStringUTF16(IDS_BLOCKED_ADS_PROMPT_EXPLANATION));
}

void ContentSettingSubresourceFilterBubbleModel::OnManageCheckboxChecked(
    bool is_checked) {
  set_done_button_text(
      l10n_util::GetStringUTF16(is_checked ? IDS_APP_MENU_RELOAD : IDS_OK));
  is_checked_ = is_checked;
}

void ContentSettingSubresourceFilterBubbleModel::OnLearnMoreClicked() {
  DCHECK(delegate());
  ChromeSubresourceFilterClient::LogAction(
      SubresourceFilterAction::kClickedLearnMore);
  delegate()->ShowLearnMorePage(ContentSettingsType::ADS);
}

void ContentSettingSubresourceFilterBubbleModel::CommitChanges() {
  if (is_checked_) {
    ChromeSubresourceFilterClient::FromWebContents(web_contents())
        ->OnReloadRequested();
  }
}

ContentSettingSubresourceFilterBubbleModel*
ContentSettingSubresourceFilterBubbleModel::AsSubresourceFilterBubbleModel() {
  return this;
}

// ContentSettingDownloadsBubbleModel ------------------------------------------

ContentSettingDownloadsBubbleModel::ContentSettingDownloadsBubbleModel(
    Delegate* delegate,
    WebContents* web_contents)
    : ContentSettingBubbleModel(delegate, web_contents) {
  SetTitle();
  SetManageText();
  SetRadioGroup();
}

ContentSettingDownloadsBubbleModel::~ContentSettingDownloadsBubbleModel() {}

void ContentSettingDownloadsBubbleModel::CommitChanges() {
  if (selected_item() != bubble_content().radio_group.default_item) {
    ContentSetting setting = selected_item() == kAllowButtonIndex
                                 ? CONTENT_SETTING_ALLOW
                                 : CONTENT_SETTING_BLOCK;
    auto* map = HostContentSettingsMapFactory::GetForProfile(GetProfile());
    map->SetNarrowestContentSetting(
        bubble_content().radio_group.url, bubble_content().radio_group.url,
        ContentSettingsType::AUTOMATIC_DOWNLOADS, setting);
  }
}

ContentSettingDownloadsBubbleModel*
ContentSettingDownloadsBubbleModel::AsDownloadsBubbleModel() {
  return this;
}

// Initialize the radio group by setting the appropriate labels for the
// content type and setting the default value based on the content setting.
void ContentSettingDownloadsBubbleModel::SetRadioGroup() {
  DownloadRequestLimiter* download_request_limiter =
      g_browser_process->download_request_limiter();
  const GURL& download_origin =
      download_request_limiter->GetDownloadOrigin(web_contents());
  base::string16 display_host =
      url_formatter::FormatUrlForSecurityDisplay(download_origin);
  DCHECK(download_request_limiter);

  RadioGroup radio_group;
  radio_group.url = download_origin;
  switch (download_request_limiter->GetDownloadUiStatus(web_contents())) {
    case DownloadRequestLimiter::DOWNLOAD_UI_ALLOWED:
      radio_group.radio_items = {
          l10n_util::GetStringUTF16(IDS_ALLOWED_DOWNLOAD_NO_ACTION),
          l10n_util::GetStringFUTF16(IDS_ALLOWED_DOWNLOAD_BLOCK, display_host)};
      radio_group.default_item = kAllowButtonIndex;
      break;
    case DownloadRequestLimiter::DOWNLOAD_UI_BLOCKED:
      radio_group.radio_items = {
          l10n_util::GetStringFUTF16(IDS_BLOCKED_DOWNLOAD_UNBLOCK,
                                     display_host),
          l10n_util::GetStringUTF16(IDS_BLOCKED_DOWNLOAD_NO_ACTION)};
      radio_group.default_item = 1;
      break;
    case DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT:
      NOTREACHED();
      return;
  }
  radio_group.user_managed = GetSettingManagedByUser(
      download_origin, ContentSettingsType::AUTOMATIC_DOWNLOADS, GetProfile(),
      nullptr);
  set_radio_group(radio_group);
}

void ContentSettingDownloadsBubbleModel::SetTitle() {
  DownloadRequestLimiter* download_request_limiter =
      g_browser_process->download_request_limiter();
  DCHECK(download_request_limiter);

  switch (download_request_limiter->GetDownloadUiStatus(web_contents())) {
    case DownloadRequestLimiter::DOWNLOAD_UI_ALLOWED:
      set_title(l10n_util::GetStringUTF16(IDS_ALLOWED_DOWNLOAD_TITLE));
      return;
    case DownloadRequestLimiter::DOWNLOAD_UI_BLOCKED:
      set_title(l10n_util::GetStringUTF16(IDS_BLOCKED_DOWNLOAD_TITLE));
      return;
    case DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT:
      // No title otherwise.
      return;
  }
}

void ContentSettingDownloadsBubbleModel::SetManageText() {
  set_manage_text(l10n_util::GetStringUTF16(IDS_MANAGE));
}

void ContentSettingDownloadsBubbleModel::OnManageButtonClicked() {
  if (delegate())
    delegate()->ShowContentSettingsPage(
        ContentSettingsType::AUTOMATIC_DOWNLOADS);
}

// ContentSettingFramebustBlockBubbleModel -------------------------------------
ContentSettingFramebustBlockBubbleModel::
    ContentSettingFramebustBlockBubbleModel(Delegate* delegate,
                                            WebContents* web_contents)
    : ContentSettingSingleRadioGroup(delegate,
                                     web_contents,
                                     ContentSettingsType::POPUPS),
      url_list_observer_(this) {
  set_title(l10n_util::GetStringUTF16(IDS_REDIRECT_BLOCKED_MESSAGE));
  auto* helper = FramebustBlockTabHelper::FromWebContents(web_contents);

  // Build the blocked urls list.
  for (const auto& blocked_url : helper->blocked_urls())
    AddListItem(CreateUrlListItem(0 /* id */, blocked_url));

  url_list_observer_.Add(helper->manager());
}

ContentSettingFramebustBlockBubbleModel::
    ~ContentSettingFramebustBlockBubbleModel() = default;

void ContentSettingFramebustBlockBubbleModel::OnListItemClicked(
    int index,
    int event_flags) {
  FramebustBlockTabHelper::FromWebContents(web_contents())
      ->OnBlockedUrlClicked(index);
}

ContentSettingFramebustBlockBubbleModel*
ContentSettingFramebustBlockBubbleModel::AsFramebustBlockBubbleModel() {
  return this;
}

void ContentSettingFramebustBlockBubbleModel::BlockedUrlAdded(
    int32_t id,
    const GURL& blocked_url) {
  AddListItem(CreateUrlListItem(0 /* id */, blocked_url));
}

// ContentSettingNotificationsBubbleModel ----------------------------------
ContentSettingNotificationsBubbleModel::ContentSettingNotificationsBubbleModel(
    Delegate* delegate,
    WebContents* web_contents)
    : ContentSettingBubbleModel(delegate, web_contents) {
  set_title(l10n_util::GetStringUTF16(
      IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_TITLE));

  // TODO(crbug.com/1030633): This block is more defensive than it needs to be
  // because ContentSettingImageModelBrowserTest exercises it without setting up
  // the correct PermissionRequestManager state. Fix that.
  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);
  if (!manager->ShouldCurrentRequestUseQuietUI())
    return;
  switch (manager->ReasonForUsingQuietUi()) {
    case QuietUiReason::kEnabledInPrefs:
      set_message(l10n_util::GetStringUTF16(
          IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_DESCRIPTION));
      set_done_button_text(l10n_util::GetStringUTF16(
          IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_ALLOW_BUTTON));
      set_show_learn_more(false);
      base::RecordAction(
          base::UserMetricsAction("Notifications.Quiet.AnimatedIconClicked"));
      break;
    case QuietUiReason::kTriggeredByCrowdDeny:
      set_message(l10n_util::GetStringUTF16(
          IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_CROWD_DENY_DESCRIPTION));
      set_done_button_text(l10n_util::GetStringUTF16(
          IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_ALLOW_BUTTON));
      set_show_learn_more(false);
      base::RecordAction(
          base::UserMetricsAction("Notifications.Quiet.StaticIconClicked"));
      break;
    case QuietUiReason::kTriggeredDueToAbusiveRequests:
      set_message(l10n_util::GetStringUTF16(
          IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_ABUSIVE_DESCRIPTION));
      // TODO(crbug.com/1082738): It is rather confusing to have the `Cancel`
      // button allow the permission, but we want the primary to block.
      set_cancel_button_text(l10n_util::GetStringUTF16(
          IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_COMPACT_ALLOW_BUTTON));
      set_done_button_text(l10n_util::GetStringUTF16(
          IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_CONTINUE_BLOCKING_BUTTON));
      set_show_learn_more(true);
      set_manage_text_style(ManageTextStyle::kNone);
      base::RecordAction(
          base::UserMetricsAction("Notifications.Quiet.StaticIconClicked"));
      break;
  }
}

ContentSettingNotificationsBubbleModel::
    ~ContentSettingNotificationsBubbleModel() = default;

ContentSettingNotificationsBubbleModel*
ContentSettingNotificationsBubbleModel::AsNotificationsBubbleModel() {
  return this;
}

void ContentSettingNotificationsBubbleModel::OnManageButtonClicked() {
  if (delegate())
    delegate()->ShowContentSettingsPage(ContentSettingsType::NOTIFICATIONS);

  base::RecordAction(
      base::UserMetricsAction("Notifications.Quiet.ManageClicked"));
}

void ContentSettingNotificationsBubbleModel::OnLearnMoreClicked() {
  if (delegate())
    delegate()->ShowLearnMorePage(ContentSettingsType::NOTIFICATIONS);
}

void ContentSettingNotificationsBubbleModel::OnDoneButtonClicked() {
  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());

  DCHECK(manager->ShouldCurrentRequestUseQuietUI());
  switch (manager->ReasonForUsingQuietUi()) {
    case QuietUiReason::kEnabledInPrefs:
    case QuietUiReason::kTriggeredByCrowdDeny:
      manager->Accept();
      base::RecordAction(
          base::UserMetricsAction("Notifications.Quiet.ShowForSiteClicked"));
      break;
    case QuietUiReason::kTriggeredDueToAbusiveRequests:
      manager->Deny();
      base::RecordAction(base::UserMetricsAction(
          "Notifications.Quiet.ContinueBlockingClicked"));
      break;
  }
}

void ContentSettingNotificationsBubbleModel::OnCancelButtonClicked() {
  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());

  switch (manager->ReasonForUsingQuietUi()) {
    case QuietUiReason::kEnabledInPrefs:
    case QuietUiReason::kTriggeredByCrowdDeny:
      // No-op.
      break;
    case QuietUiReason::kTriggeredDueToAbusiveRequests:
      manager->Accept();
      base::RecordAction(
          base::UserMetricsAction("Notifications.Quiet.ShowForSiteClicked"));
      break;
  }
}

// ContentSettingBubbleModel ---------------------------------------------------

// This class must be placed last because it needs the definition of the other
// classes declared in this file.

const int ContentSettingBubbleModel::kAllowButtonIndex = 0;

// static
std::unique_ptr<ContentSettingBubbleModel>
ContentSettingBubbleModel::CreateContentSettingBubbleModel(
    Delegate* delegate,
    WebContents* web_contents,
    ContentSettingsType content_type) {
  DCHECK(web_contents);
  if (content_type == ContentSettingsType::COOKIES) {
    return std::make_unique<ContentSettingCookiesBubbleModel>(delegate,
                                                              web_contents);
  }
  if (content_type == ContentSettingsType::POPUPS) {
    return std::make_unique<ContentSettingPopupBubbleModel>(delegate,
                                                            web_contents);
  }
  if (content_type == ContentSettingsType::GEOLOCATION) {
    return std::make_unique<ContentSettingDomainListBubbleModel>(
        delegate, web_contents, content_type);
  }
  if (content_type == ContentSettingsType::PLUGINS) {
    return std::make_unique<ContentSettingPluginBubbleModel>(delegate,
                                                             web_contents);
  }
  if (content_type == ContentSettingsType::MIXEDSCRIPT) {
    return std::make_unique<ContentSettingMixedScriptBubbleModel>(delegate,
                                                                  web_contents);
  }
  if (content_type == ContentSettingsType::PROTOCOL_HANDLERS) {
    ProtocolHandlerRegistry* registry =
        ProtocolHandlerRegistryFactory::GetForBrowserContext(
            web_contents->GetBrowserContext());
    return std::make_unique<ContentSettingRPHBubbleModel>(
        delegate, web_contents, registry);
  }
  if (content_type == ContentSettingsType::MIDI_SYSEX) {
    return std::make_unique<ContentSettingMidiSysExBubbleModel>(delegate,
                                                                web_contents);
  }
  if (content_type == ContentSettingsType::AUTOMATIC_DOWNLOADS) {
    return std::make_unique<ContentSettingDownloadsBubbleModel>(delegate,
                                                                web_contents);
  }
  if (content_type == ContentSettingsType::ADS) {
    return std::make_unique<ContentSettingSubresourceFilterBubbleModel>(
        delegate, web_contents);
  }
  if (content_type == ContentSettingsType::IMAGES ||
      content_type == ContentSettingsType::JAVASCRIPT ||
      content_type == ContentSettingsType::PPAPI_BROKER ||
      content_type == ContentSettingsType::SOUND ||
      content_type == ContentSettingsType::CLIPBOARD_READ_WRITE ||
      content_type == ContentSettingsType::SENSORS) {
    return std::make_unique<ContentSettingSingleRadioGroup>(
        delegate, web_contents, content_type);
  }
  NOTREACHED() << "No bubble for the content type "
               << static_cast<int32_t>(content_type) << ".";
  return nullptr;
}

ContentSettingBubbleModel::ContentSettingBubbleModel(Delegate* delegate,
                                                     WebContents* web_contents)
    : web_contents_(web_contents),
      owner_(nullptr),
      delegate_(delegate),
      rappor_service_(g_browser_process->rappor_service()) {
  DCHECK(web_contents_);
}

ContentSettingBubbleModel::~ContentSettingBubbleModel() {
}

ContentSettingBubbleModel::RadioGroup::RadioGroup() : default_item(0) {}

ContentSettingBubbleModel::RadioGroup::~RadioGroup() {}

ContentSettingBubbleModel::DomainList::DomainList() {}

ContentSettingBubbleModel::DomainList::DomainList(const DomainList& other) =
    default;

ContentSettingBubbleModel::DomainList::~DomainList() {}

ContentSettingBubbleModel::MediaMenu::MediaMenu() : disabled(false) {}

ContentSettingBubbleModel::MediaMenu::MediaMenu(const MediaMenu& other) =
    default;

ContentSettingBubbleModel::MediaMenu::~MediaMenu() {}

ContentSettingBubbleModel::BubbleContent::BubbleContent() {}

ContentSettingBubbleModel::BubbleContent::~BubbleContent() {}

ContentSettingSimpleBubbleModel*
    ContentSettingBubbleModel::AsSimpleBubbleModel() {
  // In general, bubble models might not inherit from the simple bubble model.
  return nullptr;
}

ContentSettingMediaStreamBubbleModel*
    ContentSettingBubbleModel::AsMediaStreamBubbleModel() {
  // In general, bubble models might not inherit from the media bubble model.
  return nullptr;
}

ContentSettingNotificationsBubbleModel*
ContentSettingBubbleModel::AsNotificationsBubbleModel() {
  return nullptr;
}

ContentSettingSubresourceFilterBubbleModel*
ContentSettingBubbleModel::AsSubresourceFilterBubbleModel() {
  return nullptr;
}

ContentSettingDownloadsBubbleModel*
ContentSettingBubbleModel::AsDownloadsBubbleModel() {
  return nullptr;
}

ContentSettingFramebustBlockBubbleModel*
ContentSettingBubbleModel::AsFramebustBlockBubbleModel() {
  return nullptr;
}

Profile* ContentSettingBubbleModel::GetProfile() const {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

void ContentSettingBubbleModel::AddListItem(const ListItem& item) {
  bubble_content_.list_items.push_back(item);
  if (owner_)
    owner_->OnListItemAdded(item);
}

void ContentSettingBubbleModel::RemoveListItem(int index) {
  if (owner_)
    owner_->OnListItemRemovedAt(index);

  bubble_content_.list_items.erase(bubble_content_.list_items.begin() + index);
}
