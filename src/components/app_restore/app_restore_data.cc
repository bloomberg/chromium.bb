// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_restore/app_restore_data.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/window_info.h"
#include "components/services/app_service/public/cpp/intent_util.h"

namespace app_restore {

namespace {

constexpr char kEventFlagKey[] = "event_flag";
constexpr char kContainerKey[] = "container";
constexpr char kDispositionKey[] = "disposition";
constexpr char kDisplayIdKey[] = "display_id";
constexpr char kHandlerIdKey[] = "handler_id";
constexpr char kUrlsKey[] = "urls";
constexpr char kActiveTabIndexKey[] = "active_tab_index";
constexpr char kIntentKey[] = "intent";
constexpr char kFilePathsKey[] = "file_paths";
constexpr char kAppTypeBrowserKey[] = "is_app";
constexpr char kAppNameKey[] = "app_name";
constexpr char kActivationIndexKey[] = "index";
constexpr char kDeskIdKey[] = "desk_id";
constexpr char kCurrentBoundsKey[] = "current_bounds";
constexpr char kWindowStateTypeKey[] = "window_state_type";
constexpr char kPreMinimizedShowStateTypeKey[] = "pre_min_state";
constexpr char kMinimumSizeKey[] = "min_size";
constexpr char kMaximumSizeKey[] = "max_size";
constexpr char kTitleKey[] = "title";
constexpr char kBoundsInRoot[] = "bounds_in_root";
constexpr char kPrimaryColorKey[] = "primary_color";
constexpr char kStatusBarColorKey[] = "status_bar_color";

// Converts |size| to base::Value, e.g. { 100, 300 }.
base::Value ConvertSizeToValue(const gfx::Size& size) {
  base::Value size_list(base::Value::Type::LIST);
  size_list.Append(base::Value(size.width()));
  size_list.Append(base::Value(size.height()));
  return size_list;
}

// Converts |rect| to base::Value, e.g. { 0, 100, 200, 300 }.
base::Value ConvertRectToValue(const gfx::Rect& rect) {
  base::Value rect_list(base::Value::Type::LIST);
  rect_list.Append(base::Value(rect.x()));
  rect_list.Append(base::Value(rect.y()));
  rect_list.Append(base::Value(rect.width()));
  rect_list.Append(base::Value(rect.height()));
  return rect_list;
}

// Converts |uint32_t| to base::Value in string, e.g 123 to "123".
base::Value ConvertUintToValue(uint32_t number) {
  return base::Value(base::NumberToString(number));
}

// Gets bool value from base::DictionaryValue, e.g. { "key": true } returns
// true.
absl::optional<bool> GetBoolValueFromDict(const base::DictionaryValue& dict,
                                          const std::string& key_name) {
  return dict.HasKey(key_name) ? dict.FindBoolKey(key_name) : absl::nullopt;
}

// Gets int value from base::DictionaryValue, e.g. { "key": 100 } returns 100.
absl::optional<int32_t> GetIntValueFromDict(const base::DictionaryValue& dict,
                                            const std::string& key_name) {
  return dict.HasKey(key_name) ? dict.FindIntKey(key_name) : absl::nullopt;
}

// Gets uint32_t value from base::DictionaryValue, e.g. { "key": "123" } returns
// 123.
absl::optional<uint32_t> GetUIntValueFromDict(const base::DictionaryValue& dict,
                                              const std::string& key_name) {
  uint32_t result = 0;
  if (!dict.HasKey(key_name) ||
      !base::StringToUint(dict.FindStringKey(key_name)->c_str(), &result)) {
    return absl::nullopt;
  }
  return result;
}

absl::optional<std::string> GetStringValueFromDict(
    const base::DictionaryValue& dict,
    const std::string& key_name) {
  if (!dict.HasKey(key_name))
    return absl::nullopt;
  const std::string* value = dict.FindStringKey(key_name);
  return value ? absl::optional<std::string>(*value) : absl::nullopt;
}

absl::optional<std::u16string> GetU16StringValueFromDict(
    const base::DictionaryValue& dict,
    const std::string& key_name) {
  std::u16string result;
  if (!dict.HasKey(key_name))
    return absl::nullopt;
  const std::string* value = dict.FindStringKey(key_name);
  if (!base::UTF8ToUTF16(value->c_str(), value->length(), &result))
    return absl::nullopt;
  return result;
}

// Gets display id from base::DictionaryValue, e.g. { "display_id": "22000000" }
// returns 22000000.
absl::optional<int64_t> GetDisplayIdFromDict(
    const base::DictionaryValue& dict) {
  if (!dict.HasKey(kDisplayIdKey))
    return absl::nullopt;

  const std::string* display_id_str = dict.FindStringKey(kDisplayIdKey);
  int64_t display_id_value;
  if (display_id_str &&
      base::StringToInt64(*display_id_str, &display_id_value)) {
    return display_id_value;
  }

  return absl::nullopt;
}

// Gets urls from the dictionary value.
absl::optional<std::vector<GURL>> GetUrlsFromDict(
    const base::DictionaryValue& dict) {
  if (!dict.HasKey(kUrlsKey))
    return absl::nullopt;

  const base::Value* urls_path_value = dict.FindListKey(kUrlsKey);
  if (!urls_path_value || !urls_path_value->is_list() ||
      urls_path_value->GetList().empty()) {
    return absl::nullopt;
  }

  std::vector<GURL> url_paths;
  for (const auto& item : urls_path_value->GetList()) {
    if (item.GetString().empty())
      continue;
    GURL url(item.GetString());
    if (!url.is_valid())
      continue;
    url_paths.push_back(url);
  }

  return url_paths;
}

// Gets std::vector<base::FilePath> from base::DictionaryValue, e.g.
// {"file_paths": { "aa.cc", "bb.h", ... }} returns
// std::vector<base::FilePath>{"aa.cc", "bb.h", ...}.
absl::optional<std::vector<base::FilePath>> GetFilePathsFromDict(
    const base::DictionaryValue& dict) {
  if (!dict.HasKey(kFilePathsKey))
    return absl::nullopt;

  const base::Value* file_paths_value = dict.FindListKey(kFilePathsKey);
  if (!file_paths_value || !file_paths_value->is_list() ||
      file_paths_value->GetList().empty())
    return absl::nullopt;

  std::vector<base::FilePath> file_paths;
  for (const auto& item : file_paths_value->GetList()) {
    if (item.GetString().empty())
      continue;
    file_paths.push_back(base::FilePath(item.GetString()));
  }

  return file_paths;
}

// Gets gfx::Size from base::Value, e.g. { 100, 300 } returns
// gfx::Size(100, 300).
absl::optional<gfx::Size> GetSizeFromDict(const base::DictionaryValue& dict,
                                          const std::string& key_name) {
  if (!dict.HasKey(key_name))
    return absl::nullopt;

  const base::Value* size_value = dict.FindListKey(key_name);
  if (!size_value || !size_value->is_list() ||
      size_value->GetList().size() != 2) {
    return absl::nullopt;
  }

  std::vector<int> size;
  for (const auto& item : size_value->GetList())
    size.push_back(item.GetInt());

  return gfx::Size(size[0], size[1]);
}

// Gets gfx::Rect from base::Value, e.g. { 0, 100, 200, 300 } returns
// gfx::Rect(0, 100, 200, 300).
absl::optional<gfx::Rect> GetBoundsRectFromDict(
    const base::DictionaryValue& dict,
    const std::string& key_name) {
  if (!dict.HasKey(key_name))
    return absl::nullopt;

  const base::Value* rect_value = dict.FindListKey(key_name);
  if (!rect_value || !rect_value->is_list() || rect_value->GetList().empty())
    return absl::nullopt;

  std::vector<int> rect;
  for (const auto& item : rect_value->GetList())
    rect.push_back(item.GetInt());

  if (rect.size() != 4)
    return absl::nullopt;

  return gfx::Rect(rect[0], rect[1], rect[2], rect[3]);
}

// Gets WindowStateType from base::DictionaryValue, e.g. { "window_state_type":
// 2 } returns WindowStateType::kMinimized.
absl::optional<chromeos::WindowStateType> GetWindowStateTypeFromDict(
    const base::DictionaryValue& dict) {
  return dict.HasKey(kWindowStateTypeKey)
             ? absl::make_optional(static_cast<chromeos::WindowStateType>(
                   dict.FindIntKey(kWindowStateTypeKey).value()))
             : absl::nullopt;
}

absl::optional<ui::WindowShowState> GetPreMinimizedShowStateTypeFromDict(
    const base::DictionaryValue& dict) {
  return dict.HasKey(kPreMinimizedShowStateTypeKey)
             ? absl::make_optional(static_cast<ui::WindowShowState>(
                   dict.FindIntKey(kPreMinimizedShowStateTypeKey).value()))
             : absl::nullopt;
}

}  // namespace

AppRestoreData::AppRestoreData() = default;

AppRestoreData::AppRestoreData(base::Value&& value) {
  base::DictionaryValue* data_dict = nullptr;
  if (!value.is_dict() || !value.GetAsDictionary(&data_dict) || !data_dict) {
    DVLOG(0) << "Fail to parse app restore data. "
             << "Cannot find the app restore data dict.";
    return;
  }

  event_flag = GetIntValueFromDict(*data_dict, kEventFlagKey);
  container = GetIntValueFromDict(*data_dict, kContainerKey);
  disposition = GetIntValueFromDict(*data_dict, kDispositionKey);
  display_id = GetDisplayIdFromDict(*data_dict);
  handler_id = GetStringValueFromDict(*data_dict, kHandlerIdKey);
  urls = GetUrlsFromDict(*data_dict);
  active_tab_index = GetIntValueFromDict(*data_dict, kActiveTabIndexKey);
  file_paths = GetFilePathsFromDict(*data_dict);
  app_type_browser = GetBoolValueFromDict(*data_dict, kAppTypeBrowserKey);
  app_name = GetStringValueFromDict(*data_dict, kAppNameKey);
  activation_index = GetIntValueFromDict(*data_dict, kActivationIndexKey);
  desk_id = GetIntValueFromDict(*data_dict, kDeskIdKey);
  current_bounds = GetBoundsRectFromDict(*data_dict, kCurrentBoundsKey);
  window_state_type = GetWindowStateTypeFromDict(*data_dict);
  pre_minimized_show_state_type =
      GetPreMinimizedShowStateTypeFromDict(*data_dict);
  maximum_size = GetSizeFromDict(*data_dict, kMaximumSizeKey);
  minimum_size = GetSizeFromDict(*data_dict, kMinimumSizeKey);
  title = GetU16StringValueFromDict(*data_dict, kTitleKey);
  bounds_in_root = GetBoundsRectFromDict(*data_dict, kBoundsInRoot);
  primary_color = GetUIntValueFromDict(*data_dict, kPrimaryColorKey);
  status_bar_color = GetUIntValueFromDict(*data_dict, kStatusBarColorKey);

  if (data_dict->HasKey(kIntentKey)) {
    intent = apps_util::ConvertValueToIntent(
        std::move(*data_dict->FindDictKey(kIntentKey)));
  }
}

AppRestoreData::AppRestoreData(std::unique_ptr<AppLaunchInfo> app_launch_info) {
  if (!app_launch_info)
    return;

  event_flag = std::move(app_launch_info->event_flag);
  container = std::move(app_launch_info->container);
  disposition = std::move(app_launch_info->disposition);
  display_id = std::move(app_launch_info->display_id);
  handler_id = std::move(app_launch_info->handler_id);
  urls = std::move(app_launch_info->urls);
  active_tab_index = std::move(app_launch_info->active_tab_index);
  file_paths = std::move(app_launch_info->file_paths);
  intent = std::move(app_launch_info->intent);
  app_type_browser = std::move(app_launch_info->app_type_browser);
  app_name = std::move(app_launch_info->app_name);
}

AppRestoreData::~AppRestoreData() = default;

std::unique_ptr<AppRestoreData> AppRestoreData::Clone() const {
  std::unique_ptr<AppRestoreData> data = std::make_unique<AppRestoreData>();

  if (event_flag.has_value())
    data->event_flag = event_flag.value();

  if (container.has_value())
    data->container = container.value();

  if (disposition.has_value())
    data->disposition = disposition.value();

  if (display_id.has_value())
    data->display_id = display_id.value();

  if (handler_id.has_value())
    data->handler_id = handler_id.value();

  if (urls.has_value())
    data->urls = urls.value();

  if (active_tab_index.has_value())
    data->active_tab_index = active_tab_index.value();

  if (intent.has_value() && intent.value())
    data->intent = intent.value()->Clone();

  if (file_paths.has_value())
    data->file_paths = file_paths.value();

  if (app_type_browser.has_value())
    data->app_type_browser = app_type_browser.value();

  if (app_name.has_value())
    data->app_name = app_name.value();

  if (activation_index.has_value())
    data->activation_index = activation_index.value();

  if (desk_id.has_value())
    data->desk_id = desk_id.value();

  if (current_bounds.has_value())
    data->current_bounds = current_bounds.value();

  if (window_state_type.has_value())
    data->window_state_type = window_state_type.value();

  if (pre_minimized_show_state_type.has_value())
    data->pre_minimized_show_state_type = pre_minimized_show_state_type.value();

  if (maximum_size.has_value())
    data->maximum_size = maximum_size.value();

  if (minimum_size.has_value())
    data->minimum_size = minimum_size.value();

  if (title.has_value())
    data->title = title.value();

  if (bounds_in_root.has_value())
    data->bounds_in_root = bounds_in_root.value();

  if (primary_color.has_value())
    data->primary_color = primary_color.value();

  if (status_bar_color.has_value())
    data->status_bar_color = status_bar_color.value();

  return data;
}

base::Value AppRestoreData::ConvertToValue() const {
  base::Value launch_info_dict(base::Value::Type::DICTIONARY);

  if (event_flag.has_value())
    launch_info_dict.SetIntKey(kEventFlagKey, event_flag.value());

  if (container.has_value())
    launch_info_dict.SetIntKey(kContainerKey, container.value());

  if (disposition.has_value())
    launch_info_dict.SetIntKey(kDispositionKey, disposition.value());

  if (display_id.has_value()) {
    launch_info_dict.SetStringKey(kDisplayIdKey,
                                  base::NumberToString(display_id.value()));
  }

  if (handler_id.has_value())
    launch_info_dict.SetStringKey(kHandlerIdKey, handler_id.value());

  if (urls.has_value() && !urls.value().empty()) {
    base::Value urls_list(base::Value::Type::LIST);
    for (auto& url : urls.value())
      urls_list.Append(base::Value(url.spec()));
    launch_info_dict.SetKey(kUrlsKey, std::move(urls_list));
  }

  if (active_tab_index.has_value())
    launch_info_dict.SetIntKey(kActiveTabIndexKey, active_tab_index.value());

  if (intent.has_value() && intent.value()) {
    launch_info_dict.SetKey(kIntentKey,
                            apps_util::ConvertIntentToValue(intent.value()));
  }

  if (file_paths.has_value() && !file_paths.value().empty()) {
    base::Value file_paths_list(base::Value::Type::LIST);
    for (auto& file_path : file_paths.value())
      file_paths_list.Append(base::Value(file_path.value()));
    launch_info_dict.SetKey(kFilePathsKey, std::move(file_paths_list));
  }

  if (app_type_browser.has_value())
    launch_info_dict.SetBoolKey(kAppTypeBrowserKey, app_type_browser.value());

  if (app_name.has_value())
    launch_info_dict.SetStringKey(kAppNameKey, app_name.value());

  if (activation_index.has_value())
    launch_info_dict.SetIntKey(kActivationIndexKey, activation_index.value());

  if (desk_id.has_value())
    launch_info_dict.SetIntKey(kDeskIdKey, desk_id.value());

  if (current_bounds.has_value()) {
    launch_info_dict.SetKey(kCurrentBoundsKey,
                            ConvertRectToValue(current_bounds.value()));
  }

  if (window_state_type.has_value()) {
    launch_info_dict.SetIntKey(kWindowStateTypeKey,
                               static_cast<int>(window_state_type.value()));
  }

  if (pre_minimized_show_state_type.has_value()) {
    launch_info_dict.SetIntKey(
        kPreMinimizedShowStateTypeKey,
        static_cast<int>(pre_minimized_show_state_type.value()));
  }

  if (maximum_size.has_value()) {
    launch_info_dict.SetKey(kMaximumSizeKey,
                            ConvertSizeToValue(maximum_size.value()));
  }

  if (minimum_size.has_value()) {
    launch_info_dict.SetKey(kMinimumSizeKey,
                            ConvertSizeToValue(minimum_size.value()));
  }

  if (title.has_value())
    launch_info_dict.SetStringKey(kTitleKey, base::UTF16ToUTF8(title.value()));

  if (bounds_in_root.has_value()) {
    launch_info_dict.SetKey(kBoundsInRoot,
                            ConvertRectToValue(bounds_in_root.value()));
  }

  if (primary_color.has_value()) {
    launch_info_dict.SetKey(kPrimaryColorKey,
                            ConvertUintToValue(primary_color.value()));
  }

  if (status_bar_color.has_value()) {
    launch_info_dict.SetKey(kStatusBarColorKey,
                            ConvertUintToValue(status_bar_color.value()));
  }

  return launch_info_dict;
}

void AppRestoreData::ModifyWindowInfo(const WindowInfo& window_info) {
  if (window_info.activation_index.has_value())
    activation_index = window_info.activation_index.value();

  if (window_info.desk_id.has_value())
    desk_id = window_info.desk_id.value();

  if (window_info.current_bounds.has_value())
    current_bounds = window_info.current_bounds.value();

  if (window_info.window_state_type.has_value())
    window_state_type = window_info.window_state_type.value();

  if (window_info.pre_minimized_show_state_type.has_value()) {
    pre_minimized_show_state_type =
        window_info.pre_minimized_show_state_type.value();
  }

  if (window_info.display_id.has_value())
    display_id = window_info.display_id.value();

  if (window_info.arc_extra_info.has_value()) {
    minimum_size = window_info.arc_extra_info->minimum_size;
    maximum_size = window_info.arc_extra_info->maximum_size;
    title = window_info.arc_extra_info->title;
    bounds_in_root = window_info.arc_extra_info->bounds_in_root;
  }
}

void AppRestoreData::ModifyThemeColor(uint32_t window_primary_color,
                                      uint32_t window_status_bar_color) {
  primary_color = window_primary_color;
  status_bar_color = window_status_bar_color;
}

void AppRestoreData::ClearWindowInfo() {
  activation_index.reset();
  desk_id.reset();
  current_bounds.reset();
  window_state_type.reset();
  pre_minimized_show_state_type.reset();
  minimum_size.reset();
  maximum_size.reset();
  title.reset();
  bounds_in_root.reset();
  primary_color.reset();
  status_bar_color.reset();
}

std::unique_ptr<AppLaunchInfo> AppRestoreData::GetAppLaunchInfo(
    const std::string& app_id,
    int window_id) const {
  auto app_launch_info = std::make_unique<AppLaunchInfo>(app_id, window_id);

  app_launch_info->event_flag = event_flag;
  app_launch_info->container = container;
  app_launch_info->disposition = disposition;
  app_launch_info->display_id = display_id;
  app_launch_info->handler_id = handler_id;
  app_launch_info->urls = urls;
  app_launch_info->file_paths = file_paths;
  if (intent.has_value())
    app_launch_info->intent = intent->Clone();
  app_launch_info->app_type_browser = app_type_browser;
  app_launch_info->app_name = app_name;
  return app_launch_info;
}

std::unique_ptr<WindowInfo> AppRestoreData::GetWindowInfo() const {
  auto window_info = std::make_unique<WindowInfo>();

  if (activation_index.has_value())
    window_info->activation_index = activation_index;

  if (desk_id.has_value())
    window_info->desk_id = desk_id.value();

  if (current_bounds.has_value())
    window_info->current_bounds = current_bounds.value();

  if (window_state_type.has_value())
    window_info->window_state_type = window_state_type.value();

  if (pre_minimized_show_state_type.has_value()) {
    window_info->pre_minimized_show_state_type =
        pre_minimized_show_state_type.value();
  }

  if (maximum_size.has_value() || minimum_size.has_value() ||
      title.has_value() || bounds_in_root.has_value()) {
    window_info->arc_extra_info = WindowInfo::ArcExtraInfo();
    window_info->arc_extra_info->maximum_size = maximum_size;
    window_info->arc_extra_info->minimum_size = minimum_size;
    window_info->arc_extra_info->title = title;
    window_info->arc_extra_info->bounds_in_root = bounds_in_root;
  }

  // Display id is set as the app launch parameter, so we don't need to return
  // the display id to restore the display id.
  return window_info;
}

apps::mojom::WindowInfoPtr AppRestoreData::GetAppWindowInfo() const {
  apps::mojom::WindowInfoPtr window_info = apps::mojom::WindowInfo::New();

  if (display_id.has_value())
    window_info->display_id = display_id.value();

  if (bounds_in_root.has_value()) {
    window_info->bounds = apps::mojom::Rect::New();
    window_info->bounds->x = bounds_in_root.value().x();
    window_info->bounds->y = bounds_in_root.value().y();
    window_info->bounds->width = bounds_in_root.value().width();
    window_info->bounds->height = bounds_in_root.value().height();
  } else if (current_bounds.has_value()) {
    window_info->bounds = apps::mojom::Rect::New();
    window_info->bounds->x = current_bounds.value().x();
    window_info->bounds->y = current_bounds.value().y();
    window_info->bounds->width = current_bounds.value().width();
    window_info->bounds->height = current_bounds.value().height();
  }

  if (window_state_type.has_value())
    window_info->state = static_cast<int32_t>(window_state_type.value());

  return window_info;
}

}  // namespace app_restore
