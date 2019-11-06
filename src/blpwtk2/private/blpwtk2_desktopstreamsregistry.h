/*
 * Copyright (C) 2015 Bloomberg Finance L.P.
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

#ifndef INCLUDED_BLPWTK2_DESKTOPSTREAMSREGISTRY_H
#define INCLUDED_BLPWTK2_DESKTOPSTREAMSREGISTRY_H

#include <blpwtk2_config.h>

#include <base/memory/singleton.h>
#include <content/public/browser/desktop_media_id.h>
#include <content/public/browser/media_stream_request.h>

#include <map>
#include <string>

namespace blpwtk2 {

// DesktopStreamsRegistry is used to store accepted desktop media streams for
// Desktop Capture API.
class DesktopStreamsRegistry {
 public:
  static DesktopStreamsRegistry* GetInstance();
  static std::string RegisterScreenForStreaming(NativeScreen screen);
  static std::string RegisterNativeViewForStreaming(NativeView view);

  // Adds new stream to the registry. Returns identifier of the new stream.
  std::string RegisterStream(const content::DesktopMediaID& source);

  // Validates stream identifier specified in getUserMedia(). Returns null
  // DesktopMediaID if the specified |id| is invalid, i.e. wasn't generated
  // using RegisterStream(). Otherwise returns ID of the source and removes
  // it from the registry.
  blink::MediaStreamDevice RequestMediaForStreamId(const std::string& id);

 private:
  friend struct base::DefaultSingletonTraits<DesktopStreamsRegistry>;
  DesktopStreamsRegistry();
  ~DesktopStreamsRegistry();

  // Type used to store list of accepted desktop media streams.
  struct ApprovedDesktopMediaStream {
    content::DesktopMediaID source;
    blink::MediaStreamDevice device;
  };
  typedef std::map<std::string, ApprovedDesktopMediaStream> StreamsMap;

  // Helper function that removes an expired stream from the registry.
  void CleanupStream(const std::string& id);

  StreamsMap approved_streams_;

  DISALLOW_COPY_AND_ASSIGN(DesktopStreamsRegistry);
};

}

#endif  // INCLUDED_BLPWTK2_DESKTOPSTREAMSREGISTRY_H

// vim: ts=4 et

