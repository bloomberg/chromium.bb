// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CONSTANTS_H_
#define REMOTING_BASE_CONSTANTS_H_

namespace remoting {

// Service name used for authentication.
// TODO(ajwong): Remove this once we've killed off XmppToken usage.
// BUG:83897
extern const char kChromotingTokenDefaultServiceName[];

// Namespace used for chromoting XMPP stanzas.
extern const char kChromotingXmlNamespace[];

// Channel names.
extern const char kAudioChannelName[];
extern const char kControlChannelName[];
extern const char kEventChannelName[];
extern const char kVideoChannelName[];
extern const char kVideoRtpChannelName[];
extern const char kVideoRtcpChannelName[];

// MIME types for the clipboard.
extern const char kMimeTypeTextUtf8[];

}  // namespace remoting

#endif  // REMOTING_BASE_CONSTANTS_H_
