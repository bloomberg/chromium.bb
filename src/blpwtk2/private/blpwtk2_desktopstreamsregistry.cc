/*
 * Copyright (C) 2013 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <blpwtk2_desktopstreamsregistry.h>

#include <base/base64.h>
#include <base/location.h>
#include <base/logging.h>
#include <base/rand_util.h>
#include <base/task/post_task.h>
#include <base/time/time.h>
#include <content/public/browser/browser_task_traits.h>
#include <content/public/browser/browser_thread.h>

namespace blpwtk2 {

namespace {

const int kStreamIdLengthBytes = 16;

const int kApprovedStreamTimeToLiveSeconds = 10;

std::string GenerateRandomStreamId() {
  char buffer[kStreamIdLengthBytes];
  base::RandBytes(buffer, base::size(buffer));
  std::string result;
  base::Base64Encode(base::StringPiece(buffer, base::size(buffer)),
                     &result);
  return result;
}

}  // namespace

DesktopStreamsRegistry* DesktopStreamsRegistry::GetInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  return base::Singleton<DesktopStreamsRegistry>::get();
}

std::string DesktopStreamsRegistry::RegisterScreenForStreaming(NativeScreen screen) {
    content::DesktopMediaID source;
    source.type = content::DesktopMediaID::TYPE_SCREEN;
    MONITORINFOEX mi;
    mi.cbSize = sizeof(MONITORINFOEX);
    GetMonitorInfo(screen, &mi);

    // enumerateDisplayDevices and match device names
    BOOL enum_result = TRUE;
    for (int device_index = 0; enum_result; ++device_index) {
      DISPLAY_DEVICE device;
      device.cb = sizeof(device);
      enum_result = EnumDisplayDevices(NULL, device_index, &device, 0);
      if (wcscmp(mi.szDevice, device.DeviceName) == 0) {
         source.id = device_index;
         break;
      }
   }
    return GetInstance()->RegisterStream(source);
}

std::string DesktopStreamsRegistry::RegisterNativeViewForStreaming(NativeView view) {
  content::DesktopMediaID source;
  source.type = content::DesktopMediaID::TYPE_WINDOW;
  source.id = reinterpret_cast<content::DesktopMediaID::Id>(view);

  return GetInstance()->RegisterStream(source);
}

DesktopStreamsRegistry::DesktopStreamsRegistry() {}
DesktopStreamsRegistry::~DesktopStreamsRegistry() {}

std::string DesktopStreamsRegistry::RegisterStream(
    const content::DesktopMediaID& source) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string id = GenerateRandomStreamId();
  DCHECK(approved_streams_.find(id) == approved_streams_.end());
  ApprovedDesktopMediaStream& stream = approved_streams_[id];
  stream.source = source;
  stream.device = blink::MediaStreamDevice(
      blink::MEDIA_GUM_DESKTOP_VIDEO_CAPTURE, source.ToString(), source.ToString());

  base::PostDelayedTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::Bind(&DesktopStreamsRegistry::CleanupStream,
                 base::Unretained(this), id),
      base::TimeDelta::FromSeconds(kApprovedStreamTimeToLiveSeconds));

  return id;
}

blink::MediaStreamDevice DesktopStreamsRegistry::RequestMediaForStreamId(
    const std::string& id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  StreamsMap::iterator it = approved_streams_.find(id);

  // Verify that if there is a request with the specified ID it was created for
  // the same origin and the same renderer.
  if (it == approved_streams_.end()) {
    return blink::MediaStreamDevice();
  }

  blink::MediaStreamDevice result = it->second.device;
  approved_streams_.erase(it);
  return result;
}

void DesktopStreamsRegistry::CleanupStream(const std::string& id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  approved_streams_.erase(id);
}

}  // close namespace blpwtk2

// vim: ts=4 et

