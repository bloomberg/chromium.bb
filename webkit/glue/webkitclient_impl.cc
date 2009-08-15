// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "webkit/glue/webkitclient_impl.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/platform_file.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "base/trace_event.h"
#include "grit/webkit_resources.h"
#include "grit/webkit_strings.h"
#include "webkit/api/public/WebData.h"
#include "webkit/api/public/WebPluginListBuilder.h"
#include "webkit/api/public/WebString.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webplugininfo.h"
#include "webkit/glue/weburlloader_impl.h"

using WebKit::WebApplicationCacheHost;
using WebKit::WebApplicationCacheHostClient;
using WebKit::WebData;
using WebKit::WebLocalizedString;
using WebKit::WebPluginListBuilder;
using WebKit::WebString;
using WebKit::WebThemeEngine;
using WebKit::WebURLLoader;

namespace webkit_glue {

static int ToMessageID(WebLocalizedString::Name name) {
  switch (name) {
    case WebLocalizedString::SubmitButtonDefaultLabel:
      return IDS_FORM_SUBMIT_LABEL;
    case WebLocalizedString::InputElementAltText:
      return IDS_FORM_INPUT_ALT;
    case WebLocalizedString::ResetButtonDefaultLabel:
      return IDS_FORM_RESET_LABEL;
    case WebLocalizedString::FileButtonChooseFileLabel:
      return IDS_FORM_FILE_BUTTON_LABEL;
    case WebLocalizedString::FileButtonNoFileSelectedLabel:
      return IDS_FORM_FILE_NO_FILE_LABEL;
    case WebLocalizedString::MultipleFileUploadText:
      return IDS_FORM_FILE_MULTIPLE_UPLOAD;
    case WebLocalizedString::SearchableIndexIntroduction:
      return IDS_SEARCHABLE_INDEX_INTRO;
    case WebLocalizedString::SearchMenuNoRecentSearchesText:
      return IDS_RECENT_SEARCHES_NONE;
    case WebLocalizedString::SearchMenuRecentSearchesText:
      return IDS_RECENT_SEARCHES;
    case WebLocalizedString::SearchMenuClearRecentSearchesText:
      return IDS_RECENT_SEARCHES_CLEAR;
    case WebLocalizedString::AXWebAreaText:
      return IDS_AX_ROLE_WEB_AREA;
    case WebLocalizedString::AXLinkText:
      return IDS_AX_ROLE_LINK;
    case WebLocalizedString::AXListMarkerText:
      return IDS_AX_ROLE_LIST_MARKER;
    case WebLocalizedString::AXImageMapText:
      return IDS_AX_ROLE_IMAGE_MAP;
    case WebLocalizedString::AXHeadingText:
      return IDS_AX_ROLE_HEADING;
    case WebLocalizedString::AXButtonActionVerb:
      return IDS_AX_BUTTON_ACTION_VERB;
    case WebLocalizedString::AXRadioButtonActionVerb:
      return IDS_AX_RADIO_BUTTON_ACTION_VERB;
    case WebLocalizedString::AXTextFieldActionVerb:
      return IDS_AX_TEXT_FIELD_ACTION_VERB;
    case WebLocalizedString::AXCheckedCheckBoxActionVerb:
      return IDS_AX_CHECKED_CHECK_BOX_ACTION_VERB;
    case WebLocalizedString::AXUncheckedCheckBoxActionVerb:
      return IDS_AX_UNCHECKED_CHECK_BOX_ACTION_VERB;
    case WebLocalizedString::AXLinkActionVerb:
      return IDS_AX_LINK_ACTION_VERB;
    case WebLocalizedString::KeygenMenuHighGradeKeySize:
      return IDS_KEYGEN_HIGH_GRADE_KEY;
    case WebLocalizedString::KeygenMenuMediumGradeKeySize:
      return IDS_KEYGEN_MED_GRADE_KEY;
  }
  return -1;
}

WebKitClientImpl::WebKitClientImpl()
    : main_loop_(MessageLoop::current()),
      shared_timer_func_(NULL) {
}

WebApplicationCacheHost* WebKitClientImpl::createApplicationCacheHost(
    WebApplicationCacheHostClient*) {
  return NULL;
}

WebThemeEngine* WebKitClientImpl::themeEngine() {
#if defined(OS_WIN)
  return &theme_engine_;
#else
  return NULL;
#endif
}

WebURLLoader* WebKitClientImpl::createURLLoader() {
  return new WebURLLoaderImpl();
}

void WebKitClientImpl::getPluginList(bool refresh,
                                     WebPluginListBuilder* builder) {
  std::vector<WebPluginInfo> plugins;
  GetPlugins(refresh, &plugins);

  for (size_t i = 0; i < plugins.size(); ++i) {
    const WebPluginInfo& plugin = plugins[i];

    builder->addPlugin(
        WideToUTF16Hack(plugin.name),
        WideToUTF16Hack(plugin.desc),
        FilePathStringToWebString(plugin.path.BaseName().value()));

    for (size_t j = 0; j < plugin.mime_types.size(); ++ j) {
      const WebPluginMimeType& mime_type = plugin.mime_types[j];

      builder->addMediaTypeToLastPlugin(
          WebString::fromUTF8(mime_type.mime_type),
          WideToUTF16Hack(mime_type.description));

      for (size_t k = 0; k < mime_type.file_extensions.size(); ++k) {
        builder->addFileExtensionToLastMediaType(
            UTF8ToUTF16(mime_type.file_extensions[k]));
      }
    }
  }
}

void WebKitClientImpl::decrementStatsCounter(const char* name) {
  StatsCounter(name).Decrement();
}

void WebKitClientImpl::incrementStatsCounter(const char* name) {
  StatsCounter(name).Increment();
}

void WebKitClientImpl::traceEventBegin(const char* name, void* id,
                                       const char* extra) {
  TRACE_EVENT_BEGIN(name, id, extra);
}

void WebKitClientImpl::traceEventEnd(const char* name, void* id,
                                     const char* extra) {
  TRACE_EVENT_END(name, id, extra);
}

WebData WebKitClientImpl::loadResource(const char* name) {
  struct {
    const char* name;
    int id;
  } resources[] = {
    { "textAreaResizeCorner", IDR_TEXTAREA_RESIZER },
    { "missingImage", IDR_BROKENIMAGE },
    { "tickmarkDash", IDR_TICKMARK_DASH },
    { "panIcon", IDR_PAN_SCROLL_ICON },
    { "searchCancel", IDR_SEARCH_CANCEL },
    { "searchCancelPressed", IDR_SEARCH_CANCEL_PRESSED },
    { "searchMagnifier", IDR_SEARCH_MAGNIFIER },
    { "searchMagnifierResults", IDR_SEARCH_MAGNIFIER_RESULTS },
    { "mediaPlay", IDR_MEDIA_PLAY_BUTTON },
    { "mediaPlayDisabled", IDR_MEDIA_PLAY_BUTTON_DISABLED },
    { "mediaPause", IDR_MEDIA_PAUSE_BUTTON },
    { "mediaSoundFull", IDR_MEDIA_SOUND_FULL_BUTTON },
    { "mediaSoundNone", IDR_MEDIA_SOUND_NONE_BUTTON },
    { "mediaSoundDisabled", IDR_MEDIA_SOUND_DISABLED },
    { "mediaSliderThumb", IDR_MEDIA_SLIDER_THUMB },
#if defined(OS_LINUX)
    { "linuxCheckboxOff", IDR_LINUX_CHECKBOX_OFF },
    { "linuxCheckboxOn", IDR_LINUX_CHECKBOX_ON },
    { "linuxCheckboxDisabledOff", IDR_LINUX_CHECKBOX_DISABLED_OFF },
    { "linuxCheckboxDisabledOn", IDR_LINUX_CHECKBOX_DISABLED_ON },
    { "linuxRadioOff", IDR_LINUX_RADIO_OFF },
    { "linuxRadioOn", IDR_LINUX_RADIO_ON },
    { "linuxRadioDisabledOff", IDR_LINUX_RADIO_DISABLED_OFF },
    { "linuxRadioDisabledOn", IDR_LINUX_RADIO_DISABLED_ON },
#endif
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(resources); ++i) {
    if (!strcmp(name, resources[i].name)) {
      StringPiece resource = GetDataResource(resources[i].id);
      return WebData(resource.data(), resource.size());
    }
  }
  NOTREACHED() << "Unknown image resource " << name;
  return WebData();
}

WebString WebKitClientImpl::queryLocalizedString(
    WebLocalizedString::Name name) {
  int message_id = ToMessageID(name);
  if (message_id < 0)
    return WebString();
  return GetLocalizedString(message_id);
}

WebString WebKitClientImpl::queryLocalizedString(
    WebLocalizedString::Name name, int numeric_value) {
  int message_id = ToMessageID(name);
  if (message_id < 0)
    return WebString();
  return ReplaceStringPlaceholders(GetLocalizedString(message_id),
                                   IntToString16(numeric_value),
                                   NULL);
}

double WebKitClientImpl::currentTime() {
  return base::Time::Now().ToDoubleT();
}

void WebKitClientImpl::setSharedTimerFiredFunction(void (*func)()) {
  shared_timer_func_ = func;
}

void WebKitClientImpl::setSharedTimerFireTime(double fire_time) {
  int interval = static_cast<int>((fire_time - currentTime()) * 1000);
  if (interval < 0)
    interval = 0;

  shared_timer_.Stop();
  shared_timer_.Start(base::TimeDelta::FromMilliseconds(interval), this,
                      &WebKitClientImpl::DoTimeout);
}

void WebKitClientImpl::stopSharedTimer() {
  shared_timer_.Stop();
}

void WebKitClientImpl::callOnMainThread(void (*func)()) {
  main_loop_->PostTask(FROM_HERE, NewRunnableFunction(func));
}

base::PlatformFile WebKitClientImpl::databaseOpenFile(
    const WebKit::WebString& file_name, int desired_flags) {
  return base::kInvalidPlatformFileValue;
}

bool WebKitClientImpl::databaseDeleteFile(
    const WebKit::WebString& file_name) {
  return false;
}

long WebKitClientImpl::databaseGetFileAttributes(
    const WebKit::WebString& file_name) {
  return 0;
}

long long WebKitClientImpl::databaseGetFileSize(
    const WebKit::WebString& file_name) {
  return 0;
}

bool WebKitClientImpl::fileExists(const WebKit::WebString& path) {
  NOTREACHED();
  return false;
}

bool WebKitClientImpl::deleteFile(const WebKit::WebString& path) {
  NOTREACHED();
  return false;
}

bool WebKitClientImpl::deleteEmptyDirectory(const WebKit::WebString& path) {
  NOTREACHED();
  return false;
}

bool WebKitClientImpl::getFileSize(const WebKit::WebString& path,
                                   long long& result) {
  NOTREACHED();
  return false;
}

bool WebKitClientImpl::getFileModificationTime(const WebKit::WebString& path,
                                               time_t& result) {
  NOTREACHED();
  return false;
}

WebKit::WebString WebKitClientImpl::directoryName(
    const WebKit::WebString& path) {
  NOTREACHED();
  return WebKit::WebString();
}

WebKit::WebString WebKitClientImpl::pathByAppendingComponent(
    const WebKit::WebString& webkit_path,
    const WebKit::WebString& webkit_component) {
  FilePath path(webkit_glue::WebStringToFilePathString(webkit_path));
  FilePath component(webkit_glue::WebStringToFilePathString(webkit_component));
  FilePath combined_path = path.Append(component);
  return webkit_glue::FilePathStringToWebString(combined_path.value());
}

bool WebKitClientImpl::makeAllDirectories(
    const WebKit::WebString& path) {
  DCHECK(!sandboxEnabled());
  FilePath::StringType file_path = webkit_glue::WebStringToFilePathString(path);
  return file_util::CreateDirectory(FilePath(file_path));
}

}  // namespace webkit_glue
