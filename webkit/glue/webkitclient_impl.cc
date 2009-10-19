// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include <math.h>
#include "config.h"

#include "FrameView.h"
#include "ScrollView.h"
#include <wtf/Assertions.h>
#undef LOG

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
#include "net/base/net_util.h"
#include "webkit/api/public/WebCursorInfo.h"
#include "webkit/api/public/WebData.h"
#include "webkit/api/public/WebFrameClient.h"
#include "webkit/api/public/WebPluginListBuilder.h"
#include "webkit/api/public/WebScreenInfo.h"
#include "webkit/api/public/WebString.h"
#include "webkit/api/public/WebViewClient.h"
#include "webkit/glue/chrome_client_impl.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/plugins/plugin_instance.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webplugininfo.h"
#include "webkit/glue/weburlloader_impl.h"
#include "webkit/glue/webview_impl.h"
#include "webkit/glue/webworkerclient_impl.h"

using WebKit::WebApplicationCacheHost;
using WebKit::WebApplicationCacheHostClient;
using WebKit::WebCursorInfo;
using WebKit::WebData;
using WebKit::WebLocalizedString;
using WebKit::WebPluginListBuilder;
using WebKit::WebStorageNamespace;
using WebKit::WebString;
using WebKit::WebThemeEngine;
using WebKit::WebURLLoader;
using WebKit::WebWidgetClient;

namespace {

ChromeClientImpl* ToChromeClient(WebCore::Widget* widget) {
  WebCore::FrameView* view;
  if (widget->isFrameView()) {
    view = static_cast<WebCore::FrameView*>(widget);
  } else if (widget->parent() && widget->parent()->isFrameView()) {
    view = static_cast<WebCore::FrameView*>(widget->parent());
  } else {
    return NULL;
  }

  WebCore::Page* page = view->frame() ? view->frame()->page() : NULL;
  if (!page)
    return NULL;

  return static_cast<ChromeClientImpl*>(page->chrome()->client());
}

WebWidgetClient* ToWebWidgetClient(WebCore::Widget* widget) {
  ChromeClientImpl* chrome_client = ToChromeClient(widget);
  if (!chrome_client || !chrome_client->webview())
    return NULL;
  return chrome_client->webview()->client();
}

}

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
    { "mediaVolumeSliderThumb", IDR_MEDIA_VOLUME_SLIDER_THUMB },
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
      base::StringPiece resource = GetDataResource(resources[i].id);
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
  // By converting between double and int64 representation, we run the risk
  // of losing precision due to rounding errors. Performing computations in
  // microseconds reduces this risk somewhat. But there still is the potential
  // of us computing a fire time for the timer that is shorter than what we
  // need.
  // As the event loop will check event deadlines prior to actually firing
  // them, there is a risk of needlessly rescheduling events and of
  // needlessly looping if sleep times are too short even by small amounts.
  // This results in measurable performance degradation unless we use ceil() to
  // always round up the sleep times.
  int64 interval = static_cast<int64>(
      ceil((fire_time - currentTime()) * base::Time::kMicrosecondsPerSecond));
  if (interval < 0)
    interval = 0;

  shared_timer_.Stop();
  shared_timer_.Start(base::TimeDelta::FromMicroseconds(interval), this,
                      &WebKitClientImpl::DoTimeout);
}

void WebKitClientImpl::stopSharedTimer() {
  shared_timer_.Stop();
}

void WebKitClientImpl::callOnMainThread(void (*func)()) {
  main_loop_->PostTask(FROM_HERE, NewRunnableFunction(func));
}

void WebKitClientImpl::dispatchStorageEvent(const WebString& key,
    const WebString& oldValue, const WebString& newValue,
    const WebString& origin, bool isLocalStorage) {
  NOTREACHED();
}

base::PlatformFile WebKitClientImpl::databaseOpenFile(
    const WebKit::WebString& file_name, int desired_flags,
    base::PlatformFile* dir_handle) {
  if (dir_handle)
    *dir_handle = base::kInvalidPlatformFileValue;
  return base::kInvalidPlatformFileValue;
}

int WebKitClientImpl::databaseDeleteFile(
    const WebKit::WebString& file_name, bool sync_dir) {
  return -1;
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
  FilePath::StringType file_path = webkit_glue::WebStringToFilePathString(path);
  return file_util::PathExists(FilePath(file_path));
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

//--------------------------------------------------------------------------

// These are temporary methods that the WebKit layer can use to call to the
// Glue layer.  Once the Glue layer moves entirely into the WebKit layer, these
// methods will be deleted.

WebKit::WebMediaPlayer* WebKitClientImpl::createWebMediaPlayer(
  WebKit::WebMediaPlayerClient* client, WebCore::Frame* frame) {
  WebFrameImpl* webframe = WebFrameImpl::FromFrame(frame);
  if (!webframe->client())
    return NULL;

  return webframe->client()->createMediaPlayer(webframe, client);
}

void WebKitClientImpl::setCursorForPlugin(
    const WebKit::WebCursorInfo& cursor_info, WebCore::Frame* frame) {
  WebCore::Page* page = frame->page();
  if (!page)
      return;

  ChromeClientImpl* chrome_client =
      static_cast<ChromeClientImpl*>(page->chrome()->client());

  // A windowless plugin can change the cursor in response to the WM_MOUSEMOVE
  // event. We need to reflect the changed cursor in the frame view as the
  // mouse is moved in the boundaries of the windowless plugin.
  chrome_client->SetCursorForPlugin(cursor_info);
}

void WebKitClientImpl::notifyJSOutOfMemory(WebCore::Frame* frame) {
  if (!frame)
    return;

  WebFrameImpl* webframe = WebFrameImpl::FromFrame(frame);
  if (!webframe->client())
    return;
  webframe->client()->didExhaustMemoryAvailableForScript(webframe);
}

bool WebKitClientImpl::popupsAllowed(NPP npp) {
  bool popups_allowed = false;
  if (npp) {
    NPAPI::PluginInstance* plugin_instance =
        reinterpret_cast<NPAPI::PluginInstance*>(npp->ndata);
    if (plugin_instance)
      popups_allowed = plugin_instance->popups_allowed();
  }
  return popups_allowed;
}

WebCore::String WebKitClientImpl::uiResourceProtocol() {
  return StdStringToString(webkit_glue::GetUIResourceProtocol());
}

int WebKitClientImpl::screenDepth(WebCore::Widget* widget) {
  WebKit::WebWidgetClient* client = ToWebWidgetClient(widget);
  if (!client)
    return 0;
  return client->screenInfo().depth;
}

int WebKitClientImpl::screenDepthPerComponent(WebCore::Widget* widget) {
  WebKit::WebWidgetClient* client = ToWebWidgetClient(widget);
  if (!client)
    return 0;
  return client->screenInfo().depthPerComponent;
}

bool WebKitClientImpl::screenIsMonochrome(WebCore::Widget* widget) {
  WebKit::WebWidgetClient* client = ToWebWidgetClient(widget);
  if (!client)
    return false;
  return client->screenInfo().isMonochrome;
}

WebCore::IntRect WebKitClientImpl::screenRect(WebCore::Widget* widget) {
  WebKit::WebWidgetClient* client = ToWebWidgetClient(widget);
  if (!client)
    return WebCore::IntRect();
  return ToIntRect(client->screenInfo().rect);
}

WebCore::IntRect WebKitClientImpl::screenAvailableRect(
    WebCore::Widget* widget) {
  WebKit::WebWidgetClient* client = ToWebWidgetClient(widget);
  if (!client)
    return WebCore::IntRect();
  return ToIntRect(client->screenInfo().availableRect);
}

void WebKitClientImpl::widgetSetCursor(WebCore::Widget* widget,
                                       const WebCore::Cursor& cursor) {
  ChromeClientImpl* chrome_client = ToChromeClient(widget);
  if (chrome_client)
    chrome_client->SetCursor(CursorToWebCursorInfo(cursor));
}

void WebKitClientImpl::widgetSetFocus(WebCore::Widget* widget) {
  ChromeClientImpl* chrome_client = ToChromeClient(widget);
  if (chrome_client)
    chrome_client->focus();
}

WebCore::WorkerContextProxy* WebKitClientImpl::createWorkerContextProxy(
    WebCore::Worker* worker) {
  return WebWorkerClientImpl::createWorkerContextProxy(worker);
}

WebStorageNamespace* WebKitClientImpl::createLocalStorageNamespace(
    const WebString& path, unsigned quota) {
  NOTREACHED();
  return 0;
}

WebStorageNamespace* WebKitClientImpl::createSessionStorageNamespace() {
  NOTREACHED();
  return 0;
}

WebKit::WebString WebKitClientImpl::getAbsolutePath(
    const WebKit::WebString& path) {
  FilePath file_path(webkit_glue::WebStringToFilePathString(path));
  file_util::AbsolutePath(&file_path);
  return webkit_glue::FilePathStringToWebString(file_path.value());
}

bool WebKitClientImpl::isDirectory(const WebKit::WebString& path) {
  FilePath file_path(webkit_glue::WebStringToFilePathString(path));
  return file_util::DirectoryExists(file_path);
}

WebKit::WebURL WebKitClientImpl::filePathToURL(const WebKit::WebString& path) {
  FilePath file_path(webkit_glue::WebStringToFilePathString(path));
  GURL file_url = net::FilePathToFileURL(file_path);
  return webkit_glue::KURLToWebURL(webkit_glue::GURLToKURL(file_url));
}

}  // namespace webkit_glue
