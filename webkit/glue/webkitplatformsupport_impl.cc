// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webkitplatformsupport_impl.h"

#if defined(OS_LINUX)
#include <malloc.h>
#endif

#include <math.h>

#include <vector>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/metrics/stats_counters.h"
#include "base/platform_file.h"
#include "base/process_util.h"
#include "base/rand_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "grit/webkit_chromium_resources.h"
#include "grit/webkit_resources.h"
#include "grit/webkit_strings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrameClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginListBuilder.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCookie.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"
#include "ui/base/layout.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/websocketstreamhandle_impl.h"
#include "webkit/glue/webthread_impl.h"
#include "webkit/glue/weburlloader_impl.h"
#include "webkit/glue/worker_task_runner.h"
#include "webkit/media/audio_decoder.h"
#include "webkit/plugins/npapi/plugin_instance.h"
#include "webkit/plugins/webplugininfo.h"

#if defined(OS_LINUX)
#include "v8/include/v8.h"
#endif

using WebKit::WebAudioBus;
using WebKit::WebCookie;
using WebKit::WebData;
using WebKit::WebLocalizedString;
using WebKit::WebPluginListBuilder;
using WebKit::WebString;
using WebKit::WebSocketStreamHandle;
using WebKit::WebThemeEngine;
using WebKit::WebURL;
using WebKit::WebURLLoader;
using WebKit::WebVector;

namespace {

// A simple class to cache the memory usage for a given amount of time.
class MemoryUsageCache {
 public:
  // Retrieves the Singleton.
  static MemoryUsageCache* GetInstance() {
    return Singleton<MemoryUsageCache>::get();
  }

  MemoryUsageCache() : memory_value_(0) { Init(); }
  ~MemoryUsageCache() {}

  void Init() {
    const unsigned int kCacheSeconds = 1;
    cache_valid_time_ = base::TimeDelta::FromSeconds(kCacheSeconds);
  }

  // Returns true if the cached value is fresh.
  // Returns false if the cached value is stale, or if |cached_value| is NULL.
  bool IsCachedValueValid(size_t* cached_value) {
    base::AutoLock scoped_lock(lock_);
    if (!cached_value)
      return false;
    if (base::Time::Now() - last_updated_time_ > cache_valid_time_)
      return false;
    *cached_value = memory_value_;
    return true;
  };

  // Setter for |memory_value_|, refreshes |last_updated_time_|.
  void SetMemoryValue(const size_t value) {
    base::AutoLock scoped_lock(lock_);
    memory_value_ = value;
    last_updated_time_ = base::Time::Now();
  }

 private:
  // The cached memory value.
  size_t memory_value_;

  // How long the cached value should remain valid.
  base::TimeDelta cache_valid_time_;

  // The last time the cached value was updated.
  base::Time last_updated_time_;

  base::Lock lock_;
};

}  // anonymous namespace

namespace webkit_glue {

static int ToMessageID(WebLocalizedString::Name name) {
  switch (name) {
    case WebLocalizedString::AXButtonActionVerb:
      return IDS_AX_BUTTON_ACTION_VERB;
    case WebLocalizedString::AXCheckedCheckBoxActionVerb:
      return IDS_AX_CHECKED_CHECK_BOX_ACTION_VERB;
    case WebLocalizedString::AXHeadingText:
      return IDS_AX_ROLE_HEADING;
    case WebLocalizedString::AXImageMapText:
      return IDS_AX_ROLE_IMAGE_MAP;
    case WebLocalizedString::AXLinkActionVerb:
      return IDS_AX_LINK_ACTION_VERB;
    case WebLocalizedString::AXLinkText:
      return IDS_AX_ROLE_LINK;
    case WebLocalizedString::AXListMarkerText:
      return IDS_AX_ROLE_LIST_MARKER;
    case WebLocalizedString::AXRadioButtonActionVerb:
      return IDS_AX_RADIO_BUTTON_ACTION_VERB;
    case WebLocalizedString::AXTextFieldActionVerb:
      return IDS_AX_TEXT_FIELD_ACTION_VERB;
    case WebLocalizedString::AXUncheckedCheckBoxActionVerb:
      return IDS_AX_UNCHECKED_CHECK_BOX_ACTION_VERB;
    case WebLocalizedString::AXWebAreaText:
      return IDS_AX_ROLE_WEB_AREA;
    case WebLocalizedString::CalendarClear:
      return IDS_FORM_CALENDAR_CLEAR;
    case WebLocalizedString::CalendarToday:
      return IDS_FORM_CALENDAR_TODAY;
    case WebLocalizedString::DateFormatDayInMonthLabel:
      return IDS_FORM_DATE_FORMAT_DAY_IN_MONTH;
    case WebLocalizedString::DateFormatMonthLabel:
      return IDS_FORM_DATE_FORMAT_MONTH;
    case WebLocalizedString::DateFormatYearLabel:
      return IDS_FORM_DATE_FORMAT_YEAR;
    case WebLocalizedString::DetailsLabel:
      return IDS_DETAILS_WITHOUT_SUMMARY_LABEL;
    case WebLocalizedString::FileButtonChooseFileLabel:
      return IDS_FORM_FILE_BUTTON_LABEL;
    case WebLocalizedString::FileButtonChooseMultipleFilesLabel:
      return IDS_FORM_MULTIPLE_FILES_BUTTON_LABEL;
    case WebLocalizedString::FileButtonNoFileSelectedLabel:
      return IDS_FORM_FILE_NO_FILE_LABEL;
    case WebLocalizedString::InputElementAltText:
      return IDS_FORM_INPUT_ALT;
    case WebLocalizedString::KeygenMenuHighGradeKeySize:
      return IDS_KEYGEN_HIGH_GRADE_KEY;
    case WebLocalizedString::KeygenMenuMediumGradeKeySize:
      return IDS_KEYGEN_MED_GRADE_KEY;
    case WebLocalizedString::MissingPluginText:
      return IDS_PLUGIN_INITIALIZATION_ERROR;
    case WebLocalizedString::MultipleFileUploadText:
      return IDS_FORM_FILE_MULTIPLE_UPLOAD;
    case WebLocalizedString::ResetButtonDefaultLabel:
      return IDS_FORM_RESET_LABEL;
    case WebLocalizedString::SearchableIndexIntroduction:
      return IDS_SEARCHABLE_INDEX_INTRO;
    case WebLocalizedString::SearchMenuClearRecentSearchesText:
      return IDS_RECENT_SEARCHES_CLEAR;
    case WebLocalizedString::SearchMenuNoRecentSearchesText:
      return IDS_RECENT_SEARCHES_NONE;
    case WebLocalizedString::SearchMenuRecentSearchesText:
      return IDS_RECENT_SEARCHES;
    case WebLocalizedString::SubmitButtonDefaultLabel:
      return IDS_FORM_SUBMIT_LABEL;
    case WebLocalizedString::ValidationPatternMismatch:
      return IDS_FORM_VALIDATION_PATTERN_MISMATCH;
    case WebLocalizedString::ValidationRangeOverflow:
      return IDS_FORM_VALIDATION_RANGE_OVERFLOW;
    case WebLocalizedString::ValidationRangeUnderflow:
      return IDS_FORM_VALIDATION_RANGE_UNDERFLOW;
    case WebLocalizedString::ValidationStepMismatch:
      return IDS_FORM_VALIDATION_STEP_MISMATCH;
    case WebLocalizedString::ValidationTooLong:
      return IDS_FORM_VALIDATION_TOO_LONG;
    case WebLocalizedString::ValidationTypeMismatch:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH;
    case WebLocalizedString::ValidationTypeMismatchForEmail:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL;
    case WebLocalizedString::ValidationTypeMismatchForMultipleEmail:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_MULTIPLE_EMAIL;
    case WebLocalizedString::ValidationTypeMismatchForURL:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_URL;
    case WebLocalizedString::ValidationValueMissing:
      return IDS_FORM_VALIDATION_VALUE_MISSING;
    case WebLocalizedString::ValidationValueMissingForCheckbox:
      return IDS_FORM_VALIDATION_VALUE_MISSING_CHECKBOX;
    case WebLocalizedString::ValidationValueMissingForFile:
      return IDS_FORM_VALIDATION_VALUE_MISSING_FILE;
    case WebLocalizedString::ValidationValueMissingForMultipleFile:
      return IDS_FORM_VALIDATION_VALUE_MISSING_MULTIPLE_FILE;
    case WebLocalizedString::ValidationValueMissingForRadio:
      return IDS_FORM_VALIDATION_VALUE_MISSING_RADIO;
    case WebLocalizedString::ValidationValueMissingForSelect:
      return IDS_FORM_VALIDATION_VALUE_MISSING_SELECT;
    // This "default:" line exists to avoid compile warnings about enum
    // coverage when we add a new symbol to WebLocalizedString.h in WebKit.
    // After a planned WebKit patch is landed, we need to add a case statement
    // for the added symbol here.
    default:
      break;
  }
  return -1;
}

WebKitPlatformSupportImpl::WebKitPlatformSupportImpl()
    : main_loop_(MessageLoop::current()),
      shared_timer_func_(NULL),
      shared_timer_fire_time_(0.0),
      shared_timer_suspended_(0),
      current_thread_slot_(&DestroyCurrentThread) {
}

WebKitPlatformSupportImpl::~WebKitPlatformSupportImpl() {
}

WebThemeEngine* WebKitPlatformSupportImpl::themeEngine() {
  return &theme_engine_;
}

WebURLLoader* WebKitPlatformSupportImpl::createURLLoader() {
  return new WebURLLoaderImpl(this);
}

WebSocketStreamHandle* WebKitPlatformSupportImpl::createSocketStreamHandle() {
  return new WebSocketStreamHandleImpl(this);
}

WebString WebKitPlatformSupportImpl::userAgent(const WebURL& url) {
  return WebString::fromUTF8(webkit_glue::GetUserAgent(url));
}

void WebKitPlatformSupportImpl::getPluginList(bool refresh,
                                     WebPluginListBuilder* builder) {
  std::vector<webkit::WebPluginInfo> plugins;
  GetPlugins(refresh, &plugins);

  for (size_t i = 0; i < plugins.size(); ++i) {
    const webkit::WebPluginInfo& plugin = plugins[i];

    builder->addPlugin(
        plugin.name, plugin.desc,
        FilePathStringToWebString(plugin.path.BaseName().value()));

    for (size_t j = 0; j < plugin.mime_types.size(); ++j) {
      const webkit::WebPluginMimeType& mime_type = plugin.mime_types[j];

      builder->addMediaTypeToLastPlugin(
          WebString::fromUTF8(mime_type.mime_type), mime_type.description);

      for (size_t k = 0; k < mime_type.file_extensions.size(); ++k) {
        builder->addFileExtensionToLastMediaType(
            UTF8ToUTF16(mime_type.file_extensions[k]));
      }
    }
  }
}

void WebKitPlatformSupportImpl::decrementStatsCounter(const char* name) {
  base::StatsCounter(name).Decrement();
}

void WebKitPlatformSupportImpl::incrementStatsCounter(const char* name) {
  base::StatsCounter(name).Increment();
}

void WebKitPlatformSupportImpl::histogramCustomCounts(
    const char* name, int sample, int min, int max, int bucket_count) {
  // Copied from histogram macro, but without the static variable caching
  // the histogram because name is dynamic.
  base::Histogram* counter =
      base::Histogram::FactoryGet(name, min, max, bucket_count,
          base::Histogram::kUmaTargetedHistogramFlag);
  DCHECK_EQ(name, counter->histogram_name());
  counter->Add(sample);
}

void WebKitPlatformSupportImpl::histogramEnumeration(
    const char* name, int sample, int boundary_value) {
  // Copied from histogram macro, but without the static variable caching
  // the histogram because name is dynamic.
  base::Histogram* counter =
      base::LinearHistogram::FactoryGet(name, 1, boundary_value,
          boundary_value + 1, base::Histogram::kUmaTargetedHistogramFlag);
  DCHECK_EQ(name, counter->histogram_name());
  counter->Add(sample);
}

const unsigned char* WebKitPlatformSupportImpl::getTraceCategoryEnabledFlag(
    const char* category_name) {
  return TRACE_EVENT_API_GET_CATEGORY_ENABLED(category_name);
}

int WebKitPlatformSupportImpl::addTraceEvent(
    char phase,
    const unsigned char* category_enabled,
    const char* name,
    unsigned long long id,
    int num_args,
    const char** arg_names,
    const unsigned char* arg_types,
    const unsigned long long* arg_values,
    int threshold_begin_id,
    long long threshold,
    unsigned char flags) {
  return TRACE_EVENT_API_ADD_TRACE_EVENT(phase, category_enabled, name, id,
                                         num_args, arg_names, arg_types,
                                         arg_values, threshold_begin_id,
                                         threshold, flags);
}

namespace {

WebData loadAudioSpatializationResource(WebKitPlatformSupportImpl* platform,
                                        const char* name) {
#ifdef IDR_AUDIO_SPATIALIZATION_T000_P000
  const size_t kExpectedSpatializationNameLength = 31;
  if (strlen(name) != kExpectedSpatializationNameLength) {
    return WebData();
  }

  // Extract the azimuth and elevation from the resource name.
  int azimuth = 0;
  int elevation = 0;
  int values_parsed =
      sscanf(name, "IRC_Composite_C_R0195_T%3d_P%3d", &azimuth, &elevation);
  if (values_parsed != 2) {
    return WebData();
  }

  // The resource index values go through the elevations first, then azimuths.
  const int kAngleSpacing = 15;

  // 0 <= elevation <= 90 (or 315 <= elevation <= 345)
  // in increments of 15 degrees.
  int elevation_index =
      elevation <= 90 ? elevation / kAngleSpacing :
      7 + (elevation - 315) / kAngleSpacing;
  bool is_elevation_index_good = 0 <= elevation_index && elevation_index < 10;

  // 0 <= azimuth < 360 in increments of 15 degrees.
  int azimuth_index = azimuth / kAngleSpacing;
  bool is_azimuth_index_good = 0 <= azimuth_index && azimuth_index < 24;

  const int kNumberOfElevations = 10;
  const int kNumberOfAudioResources = 240;
  int resource_index = kNumberOfElevations * azimuth_index + elevation_index;
  bool is_resource_index_good = 0 <= resource_index &&
      resource_index < kNumberOfAudioResources;

  if (is_azimuth_index_good && is_elevation_index_good &&
      is_resource_index_good) {
    const int kFirstAudioResourceIndex = IDR_AUDIO_SPATIALIZATION_T000_P000;
    base::StringPiece resource =
        platform->GetDataResource(kFirstAudioResourceIndex + resource_index,
                                  ui::SCALE_FACTOR_NONE);
    return WebData(resource.data(), resource.size());
  }
#endif  // IDR_AUDIO_SPATIALIZATION_T000_P000

  NOTREACHED();
  return WebData();
}

struct DataResource {
  const char* name;
  int id;
  ui::ScaleFactor scale_factor;
};

const DataResource kDataResources[] = {
  { "missingImage", IDR_BROKENIMAGE, ui::SCALE_FACTOR_100P },
  { "missingImage@2x", IDR_BROKENIMAGE, ui::SCALE_FACTOR_200P },
#if defined(OS_ANDROID)
  { "mediaFullscreen", IDR_MEDIA_FULLSCREEN_BUTTON, ui::SCALE_FACTOR_100P },
#endif
  { "mediaPause", IDR_MEDIA_PAUSE_BUTTON, ui::SCALE_FACTOR_100P },
  { "mediaPlay", IDR_MEDIA_PLAY_BUTTON, ui::SCALE_FACTOR_100P },
  { "mediaPlayDisabled",
    IDR_MEDIA_PLAY_BUTTON_DISABLED, ui::SCALE_FACTOR_100P },
  { "mediaSoundDisabled", IDR_MEDIA_SOUND_DISABLED, ui::SCALE_FACTOR_100P },
  { "mediaSoundFull", IDR_MEDIA_SOUND_FULL_BUTTON, ui::SCALE_FACTOR_100P },
  { "mediaSoundNone", IDR_MEDIA_SOUND_NONE_BUTTON, ui::SCALE_FACTOR_100P },
  { "mediaSliderThumb", IDR_MEDIA_SLIDER_THUMB, ui::SCALE_FACTOR_100P },
  { "mediaVolumeSliderThumb",
    IDR_MEDIA_VOLUME_SLIDER_THUMB, ui::SCALE_FACTOR_100P },
  { "mediaplayerPause", IDR_MEDIAPLAYER_PAUSE_BUTTON, ui::SCALE_FACTOR_100P },
  { "mediaplayerPauseHover",
    IDR_MEDIAPLAYER_PAUSE_BUTTON_HOVER, ui::SCALE_FACTOR_100P },
  { "mediaplayerPauseDown",
    IDR_MEDIAPLAYER_PAUSE_BUTTON_DOWN, ui::SCALE_FACTOR_100P },
  { "mediaplayerPlay", IDR_MEDIAPLAYER_PLAY_BUTTON, ui::SCALE_FACTOR_100P },
  { "mediaplayerPlayHover",
    IDR_MEDIAPLAYER_PLAY_BUTTON_HOVER, ui::SCALE_FACTOR_100P },
  { "mediaplayerPlayDown",
    IDR_MEDIAPLAYER_PLAY_BUTTON_DOWN, ui::SCALE_FACTOR_100P },
  { "mediaplayerPlayDisabled",
    IDR_MEDIAPLAYER_PLAY_BUTTON_DISABLED, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel3",
    IDR_MEDIAPLAYER_SOUND_LEVEL3_BUTTON, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel3Hover",
    IDR_MEDIAPLAYER_SOUND_LEVEL3_BUTTON_HOVER, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel3Down",
    IDR_MEDIAPLAYER_SOUND_LEVEL3_BUTTON_DOWN, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel2",
    IDR_MEDIAPLAYER_SOUND_LEVEL2_BUTTON, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel2Hover",
    IDR_MEDIAPLAYER_SOUND_LEVEL2_BUTTON_HOVER, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel2Down",
    IDR_MEDIAPLAYER_SOUND_LEVEL2_BUTTON_DOWN, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel1",
    IDR_MEDIAPLAYER_SOUND_LEVEL1_BUTTON, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel1Hover",
    IDR_MEDIAPLAYER_SOUND_LEVEL1_BUTTON_HOVER, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel1Down",
    IDR_MEDIAPLAYER_SOUND_LEVEL1_BUTTON_DOWN, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel0",
    IDR_MEDIAPLAYER_SOUND_LEVEL0_BUTTON, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel0Hover",
    IDR_MEDIAPLAYER_SOUND_LEVEL0_BUTTON_HOVER, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel0Down",
    IDR_MEDIAPLAYER_SOUND_LEVEL0_BUTTON_DOWN, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundDisabled",
    IDR_MEDIAPLAYER_SOUND_DISABLED, ui::SCALE_FACTOR_100P },
  { "mediaplayerSliderThumb",
    IDR_MEDIAPLAYER_SLIDER_THUMB, ui::SCALE_FACTOR_100P },
  { "mediaplayerSliderThumbHover",
    IDR_MEDIAPLAYER_SLIDER_THUMB_HOVER, ui::SCALE_FACTOR_100P },
  { "mediaplayerSliderThumbDown",
    IDR_MEDIAPLAYER_SLIDER_THUMB_DOWN, ui::SCALE_FACTOR_100P },
  { "mediaplayerVolumeSliderThumb",
    IDR_MEDIAPLAYER_VOLUME_SLIDER_THUMB, ui::SCALE_FACTOR_100P },
  { "mediaplayerVolumeSliderThumbHover",
    IDR_MEDIAPLAYER_VOLUME_SLIDER_THUMB_HOVER, ui::SCALE_FACTOR_100P },
  { "mediaplayerVolumeSliderThumbDown",
    IDR_MEDIAPLAYER_VOLUME_SLIDER_THUMB_DOWN, ui::SCALE_FACTOR_100P },
  { "mediaplayerVolumeSliderThumbDisabled",
    IDR_MEDIAPLAYER_VOLUME_SLIDER_THUMB_DISABLED, ui::SCALE_FACTOR_100P },
  { "mediaplayerFullscreen",
    IDR_MEDIAPLAYER_FULLSCREEN_BUTTON, ui::SCALE_FACTOR_100P },
  { "mediaplayerFullscreenHover",
    IDR_MEDIAPLAYER_FULLSCREEN_BUTTON_HOVER, ui::SCALE_FACTOR_100P },
  { "mediaplayerFullscreenDown",
    IDR_MEDIAPLAYER_FULLSCREEN_BUTTON_DOWN, ui::SCALE_FACTOR_100P },
  { "mediaplayerFullscreenDisabled",
    IDR_MEDIAPLAYER_FULLSCREEN_BUTTON_DISABLED, ui::SCALE_FACTOR_100P },
#if defined(OS_MACOSX)
  { "overhangPattern", IDR_OVERHANG_PATTERN, ui::SCALE_FACTOR_100P },
#endif
  { "panIcon", IDR_PAN_SCROLL_ICON, ui::SCALE_FACTOR_100P },
  { "searchCancel", IDR_SEARCH_CANCEL, ui::SCALE_FACTOR_100P },
  { "searchCancelPressed", IDR_SEARCH_CANCEL_PRESSED, ui::SCALE_FACTOR_100P },
  { "searchMagnifier", IDR_SEARCH_MAGNIFIER, ui::SCALE_FACTOR_100P },
  { "searchMagnifierResults",
    IDR_SEARCH_MAGNIFIER_RESULTS, ui::SCALE_FACTOR_100P },
  { "textAreaResizeCorner", IDR_TEXTAREA_RESIZER, ui::SCALE_FACTOR_100P },
  { "textAreaResizeCorner@2x", IDR_TEXTAREA_RESIZER, ui::SCALE_FACTOR_200P },
  { "tickmarkDash", IDR_TICKMARK_DASH, ui::SCALE_FACTOR_100P },
  { "inputSpeech", IDR_INPUT_SPEECH, ui::SCALE_FACTOR_100P },
  { "inputSpeechRecording", IDR_INPUT_SPEECH_RECORDING, ui::SCALE_FACTOR_100P },
  { "inputSpeechWaiting", IDR_INPUT_SPEECH_WAITING, ui::SCALE_FACTOR_100P },
  { "americanExpressCC", IDR_AUTOFILL_CC_AMEX, ui::SCALE_FACTOR_100P },
  { "dinersCC", IDR_AUTOFILL_CC_DINERS, ui::SCALE_FACTOR_100P },
  { "discoverCC", IDR_AUTOFILL_CC_DISCOVER, ui::SCALE_FACTOR_100P },
  { "genericCC", IDR_AUTOFILL_CC_GENERIC, ui::SCALE_FACTOR_100P },
  { "jcbCC", IDR_AUTOFILL_CC_JCB, ui::SCALE_FACTOR_100P },
  { "masterCardCC", IDR_AUTOFILL_CC_MASTERCARD, ui::SCALE_FACTOR_100P },
  { "soloCC", IDR_AUTOFILL_CC_SOLO, ui::SCALE_FACTOR_100P },
  { "visaCC", IDR_AUTOFILL_CC_VISA, ui::SCALE_FACTOR_100P },
};

}  // namespace

WebData WebKitPlatformSupportImpl::loadResource(const char* name) {
  // Some clients will call into this method with an empty |name| when they have
  // optional resources.  For example, the PopupMenuChromium code can have icons
  // for some Autofill items but not for others.
  if (!strlen(name))
    return WebData();

  // Check the name prefix to see if it's an audio resource.
  if (StartsWithASCII(name, "IRC_Composite", true))
    return loadAudioSpatializationResource(this, name);

  // TODO(flackr): We should use a better than linear search here, a trie would
  // be ideal.
  for (size_t i = 0; i < arraysize(kDataResources); ++i) {
    if (!strcmp(name, kDataResources[i].name)) {
      base::StringPiece resource =
          GetDataResource(kDataResources[i].id,
                          kDataResources[i].scale_factor);
      return WebData(resource.data(), resource.size());
    }
  }

  NOTREACHED() << "Unknown image resource " << name;
  return WebData();
}

bool WebKitPlatformSupportImpl::loadAudioResource(
    WebKit::WebAudioBus* destination_bus, const char* audio_file_data,
    size_t data_size, double sample_rate) {
  return webkit_media::DecodeAudioFileData(destination_bus,
                                           audio_file_data,
                                           data_size,
                                           sample_rate);
}

WebString WebKitPlatformSupportImpl::queryLocalizedString(
    WebLocalizedString::Name name) {
  int message_id = ToMessageID(name);
  if (message_id < 0)
    return WebString();
  return GetLocalizedString(message_id);
}

WebString WebKitPlatformSupportImpl::queryLocalizedString(
    WebLocalizedString::Name name, int numeric_value) {
  return queryLocalizedString(name, base::IntToString16(numeric_value));
}

WebString WebKitPlatformSupportImpl::queryLocalizedString(
    WebLocalizedString::Name name, const WebString& value) {
  int message_id = ToMessageID(name);
  if (message_id < 0)
    return WebString();
  return ReplaceStringPlaceholders(GetLocalizedString(message_id), value, NULL);
}

WebString WebKitPlatformSupportImpl::queryLocalizedString(
    WebLocalizedString::Name name,
    const WebString& value1,
    const WebString& value2) {
  int message_id = ToMessageID(name);
  if (message_id < 0)
    return WebString();
  std::vector<string16> values;
  values.reserve(2);
  values.push_back(value1);
  values.push_back(value2);
  return ReplaceStringPlaceholders(
      GetLocalizedString(message_id), values, NULL);
}

double WebKitPlatformSupportImpl::currentTime() {
  return base::Time::Now().ToDoubleT();
}

double WebKitPlatformSupportImpl::monotonicallyIncreasingTime() {
  return base::TimeTicks::Now().ToInternalValue() /
      static_cast<double>(base::Time::kMicrosecondsPerSecond);
}

void WebKitPlatformSupportImpl::cryptographicallyRandomValues(
    unsigned char* buffer, size_t length) {
  base::RandBytes(buffer, length);
}

void WebKitPlatformSupportImpl::setSharedTimerFiredFunction(void (*func)()) {
  shared_timer_func_ = func;
}

void WebKitPlatformSupportImpl::setSharedTimerFireInterval(
    double interval_seconds) {
  shared_timer_fire_time_ = interval_seconds + monotonicallyIncreasingTime();
  if (shared_timer_suspended_)
    return;

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
      ceil(interval_seconds * base::Time::kMillisecondsPerSecond)
      * base::Time::kMicrosecondsPerMillisecond);

  if (interval < 0)
    interval = 0;

  shared_timer_.Stop();
  shared_timer_.Start(FROM_HERE, base::TimeDelta::FromMicroseconds(interval),
                      this, &WebKitPlatformSupportImpl::DoTimeout);
  OnStartSharedTimer(base::TimeDelta::FromMicroseconds(interval));
}

void WebKitPlatformSupportImpl::stopSharedTimer() {
  shared_timer_.Stop();
}

void WebKitPlatformSupportImpl::callOnMainThread(
    void (*func)(void*), void* context) {
  main_loop_->PostTask(FROM_HERE, base::Bind(func, context));
}

WebKit::WebThread* WebKitPlatformSupportImpl::createThread(const char* name) {
  return new WebThreadImpl(name);
}

WebKit::WebThread* WebKitPlatformSupportImpl::currentThread() {
  WebThreadImplForMessageLoop* thread =
      static_cast<WebThreadImplForMessageLoop*>(current_thread_slot_.Get());
  if (thread)
    return (thread);

  scoped_refptr<base::MessageLoopProxy> message_loop =
      base::MessageLoopProxy::current();
  if (!message_loop)
    return NULL;

  thread = new WebThreadImplForMessageLoop(message_loop);
  current_thread_slot_.Set(thread);
  return thread;
}

base::PlatformFile WebKitPlatformSupportImpl::databaseOpenFile(
    const WebKit::WebString& vfs_file_name, int desired_flags) {
  return base::kInvalidPlatformFileValue;
}

int WebKitPlatformSupportImpl::databaseDeleteFile(
    const WebKit::WebString& vfs_file_name, bool sync_dir) {
  return -1;
}

long WebKitPlatformSupportImpl::databaseGetFileAttributes(
    const WebKit::WebString& vfs_file_name) {
  return 0;
}

long long WebKitPlatformSupportImpl::databaseGetFileSize(
    const WebKit::WebString& vfs_file_name) {
  return 0;
}

long long WebKitPlatformSupportImpl::databaseGetSpaceAvailableForOrigin(
    const WebKit::WebString& origin_identifier) {
  return 0;
}

WebKit::WebString WebKitPlatformSupportImpl::signedPublicKeyAndChallengeString(
    unsigned key_size_index,
    const WebKit::WebString& challenge,
    const WebKit::WebURL& url) {
  return WebKit::WebString("");
}

#if defined(OS_LINUX)
static size_t memoryUsageMBLinux() {
  struct mallinfo minfo = mallinfo();
  uint64_t mem_usage =
#if defined(USE_TCMALLOC)
      minfo.uordblks
#else
      (minfo.hblkhd + minfo.arena)
#endif
      >> 20;

  v8::HeapStatistics stat;
  v8::V8::GetHeapStatistics(&stat);
  return mem_usage + (static_cast<uint64_t>(stat.total_heap_size()) >> 20);
}
#endif

#if defined(OS_MACOSX)
static size_t memoryUsageMBMac() {
  using base::ProcessMetrics;
  static ProcessMetrics* process_metrics =
      // The default port provider is sufficient to get data for the current
      // process.
      ProcessMetrics::CreateProcessMetrics(base::GetCurrentProcessHandle(),
                                           NULL);
  DCHECK(process_metrics);
  return process_metrics->GetWorkingSetSize() >> 20;
}
#endif

#if !defined(OS_LINUX) && !defined(OS_MACOSX)
static size_t memoryUsageMBGeneric() {
  using base::ProcessMetrics;
  static ProcessMetrics* process_metrics =
      ProcessMetrics::CreateProcessMetrics(base::GetCurrentProcessHandle());
  DCHECK(process_metrics);
  return process_metrics->GetPagefileUsage() >> 20;
}
#endif

static size_t getMemoryUsageMB(bool bypass_cache) {
  size_t current_mem_usage = 0;
  MemoryUsageCache* mem_usage_cache_singleton = MemoryUsageCache::GetInstance();
  if (!bypass_cache &&
      mem_usage_cache_singleton->IsCachedValueValid(&current_mem_usage))
    return current_mem_usage;

  current_mem_usage =
#if defined(OS_LINUX)
      memoryUsageMBLinux();
#elif defined(OS_MACOSX)
      memoryUsageMBMac();
#else
      memoryUsageMBGeneric();
#endif
  mem_usage_cache_singleton->SetMemoryValue(current_mem_usage);
  return current_mem_usage;
}

size_t WebKitPlatformSupportImpl::memoryUsageMB() {
  return getMemoryUsageMB(false);
}

size_t WebKitPlatformSupportImpl::actualMemoryUsageMB() {
  return getMemoryUsageMB(true);
}

void WebKitPlatformSupportImpl::SuspendSharedTimer() {
  ++shared_timer_suspended_;
}

void WebKitPlatformSupportImpl::ResumeSharedTimer() {
  // The shared timer may have fired or been adjusted while we were suspended.
  if (--shared_timer_suspended_ == 0 && !shared_timer_.IsRunning()) {
    setSharedTimerFireInterval(
        shared_timer_fire_time_ - monotonicallyIncreasingTime());
  }
}

// static
void WebKitPlatformSupportImpl::DestroyCurrentThread(void* thread) {
  WebThreadImplForMessageLoop* impl =
      static_cast<WebThreadImplForMessageLoop*>(thread);
  delete impl;
}

void WebKitPlatformSupportImpl::didStartWorkerRunLoop(
    const WebKit::WebWorkerRunLoop& runLoop) {
  WorkerTaskRunner* worker_task_runner = WorkerTaskRunner::Instance();
  worker_task_runner->OnWorkerRunLoopStarted(runLoop);
}

void WebKitPlatformSupportImpl::didStopWorkerRunLoop(
    const WebKit::WebWorkerRunLoop& runLoop) {
  WorkerTaskRunner* worker_task_runner = WorkerTaskRunner::Instance();
  worker_task_runner->OnWorkerRunLoopStopped(runLoop);
}

}  // namespace webkit_glue
