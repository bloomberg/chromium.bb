// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/flash_resource.h"

#include <cmath>

#include "base/containers/mru_cache.h"
#include "base/lazy_instance.h"
#include "base/time.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/time_conversion.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {
namespace proxy {

namespace {

struct LocalTimeZoneOffsetEntry {
  base::TimeTicks expiration;
  double offset;
};

class LocalTimeZoneOffsetCache
    : public base::MRUCache<PP_Time, LocalTimeZoneOffsetEntry> {
 public:
  LocalTimeZoneOffsetCache()
      : base::MRUCache<PP_Time, LocalTimeZoneOffsetEntry>(kCacheSize) {}
 private:
  static const size_t kCacheSize = 100;
};

base::LazyInstance<LocalTimeZoneOffsetCache>::Leaky
    g_local_time_zone_offset_cache = LAZY_INSTANCE_INITIALIZER;

} //  namespace

FlashResource::FlashResource(Connection connection, PP_Instance instance)
    : PluginResource(connection, instance) {
  SendCreate(RENDERER, PpapiHostMsg_Flash_Create());
  SendCreate(BROWSER, PpapiHostMsg_Flash_Create());
}

FlashResource::~FlashResource() {
}

thunk::PPB_Flash_Functions_API* FlashResource::AsPPB_Flash_Functions_API() {
  return this;
}

PP_Var FlashResource::GetProxyForURL(PP_Instance instance,
                                     const std::string& url) {
  std::string proxy;
  int32_t result = SyncCall<PpapiPluginMsg_Flash_GetProxyForURLReply>(RENDERER,
      PpapiHostMsg_Flash_GetProxyForURL(url), &proxy);

  if (result == PP_OK)
    return StringVar::StringToPPVar(proxy);
  return PP_MakeUndefined();
}

void FlashResource::UpdateActivity(PP_Instance instance) {
  Post(BROWSER, PpapiHostMsg_Flash_UpdateActivity());
}

PP_Bool FlashResource::SetCrashData(PP_Instance instance,
                                    PP_FlashCrashKey key,
                                    PP_Var value) {
  switch (key) {
    case PP_FLASHCRASHKEY_URL: {
      StringVar* url_string_var(StringVar::FromPPVar(value));
      if (!url_string_var)
        return PP_FALSE;
      PluginGlobals::Get()->SetActiveURL(url_string_var->value());
      return PP_TRUE;
    }
  }
  return PP_FALSE;
}

double FlashResource::GetLocalTimeZoneOffset(PP_Instance instance,
                                             PP_Time t) {
  LocalTimeZoneOffsetCache& cache = g_local_time_zone_offset_cache.Get();

  // Get the minimum PP_Time value that shares the same minute as |t|.
  // Use cached offset if cache hasn't expired and |t| is in the same minute as
  // the time for the cached offset (assume offsets change on minute
  // boundaries).
  PP_Time t_minute_base = floor(t / 60.0) * 60.0;
  LocalTimeZoneOffsetCache::iterator iter = cache.Get(t_minute_base);
  base::TimeTicks now = base::TimeTicks::Now();
  if (iter != cache.end() && now < iter->second.expiration)
    return iter->second.offset;

  // Cache the local offset for ten seconds, since it's slow on XP and Linux.
  // Note that TimeTicks does not continue counting across sleep/resume on all
  // platforms. This may be acceptable for 10 seconds, but if in the future this
  // is changed to one minute or more, then we should consider using base::Time.
  const int64 kMaxCachedLocalOffsetAgeInSeconds = 10;
  base::TimeDelta expiration_delta =
      base::TimeDelta::FromSeconds(kMaxCachedLocalOffsetAgeInSeconds);

  LocalTimeZoneOffsetEntry cache_entry;
  cache_entry.expiration = now + expiration_delta;
  cache_entry.offset = 0.0;

  // We can't do the conversion here on Linux because the localtime calls
  // require filesystem access prohibited by the sandbox.
  // TODO(shess): Figure out why OSX needs the access, the sandbox warmup should
  // handle it.  http://crbug.com/149006
#if defined(OS_LINUX) || defined(OS_MACOSX)
  int32_t result = SyncCall<PpapiPluginMsg_Flash_GetLocalTimeZoneOffsetReply>(
      BROWSER,
      PpapiHostMsg_Flash_GetLocalTimeZoneOffset(PPTimeToTime(t)),
      &cache_entry.offset);
  if (result != PP_OK)
    cache_entry.offset = 0.0;
#else
  cache_entry.offset = PPGetLocalTimeZoneOffset(PPTimeToTime(t));
#endif

  cache.Put(t_minute_base, cache_entry);
  return cache_entry.offset;
}

}  // namespace proxy
}  // namespace ppapi
