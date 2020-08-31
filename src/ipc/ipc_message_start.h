// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_MESSAGE_START_H_
#define IPC_IPC_MESSAGE_START_H_

// Used by IPC_BEGIN_MESSAGES so that each message class starts from a unique
// base.  Messages have unique IDs across channels in order for the IPC logging
// code to figure out the message class from its ID.
//
// You should no longer be adding any new message classes. Instead, use mojo
// for all new work.
enum IPCMessageStart {
  AutomationMsgStart = 0,
  FrameMsgStart,
  PageMsgStart,
  ViewMsgStart,
  WidgetMsgStart,
  TestMsgStart,
  WorkerMsgStart,
  NaClMsgStart,
  GpuChannelMsgStart,
  MediaMsgStart,
  PpapiMsgStart,
  ChromeMsgStart,
  DragMsgStart,
  PrintMsgStart,
  ExtensionMsgStart,
  TextInputClientMsgStart,
  PrerenderMsgStart,
  ChromotingMsgStart,
  AndroidWebViewMsgStart,
  NaClHostMsgStart,
  EncryptedMediaMsgStart,
  CastMsgStart,
  GinJavaBridgeMsgStart,
  ChromeUtilityPrintingMsgStart,
  OzoneGpuMsgStart,
  WebTestMsgStart,
  ExtensionsGuestViewMsgStart,
  GuestViewMsgStart,
  MediaPlayerDelegateMsgStart,
  ExtensionWorkerMsgStart,
  SubresourceFilterMsgStart,
  UnfreezableFrameMsgStart,
  LastIPCMsgStart  // Must come last.
};

#endif  // IPC_IPC_MESSAGE_START_H_
