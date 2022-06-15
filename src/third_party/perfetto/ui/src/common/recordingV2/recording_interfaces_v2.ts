// Copyright (C) 2022 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import {TraceConfig} from '../protos';

// TargetFactory connects, disconnects and keeps track of targets.
// There is one factory for AndroidWebusb, AndroidWebsocket, Chrome etc.
// For instance, the AndroidWebusb factory returns a RecordingTargetV2 for each
// device.
export interface TargetFactory {
  // Store the kind explicitly as a string as opposed to using class.kind in
  // case we ever minify our code.
  readonly kind: string;

  // Executed when devices connect or disconnect.
  onDevicesChanged?: OnTargetChangedCallback;

  getName(): string;

  listTargets(): RecordingTargetV2[];
  // Returns recording problems that we encounter when not directly using the
  // target. For instance we connect webusb devices when Perfetto is loaded. If
  // there is an issue with connecting a webusb device, we do not want to crash
  // all of Perfetto, as the user may not want to use the recording
  // functionality at all.
  listRecordingProblems(): string[];

  connectNewTarget(): Promise<RecordingTargetV2>;
}

export interface AndroidTargetInfo {
  name: string;
  targetType: 'ANDROID'
  // This is the Android API level. For instance, it can be 32, 31, 30 etc.
  // It is the "API level" column here:
  // https://source.android.com/setup/start/build-numbers
  androidApiLevel: number;
}

export interface OtherTargetInfo {
  name: string;
  targetType: 'CHROME'|'CHROME_OS'|'LINUX';
}

// Holds information about a target. It's used by the logic which generates a
// config.
export type TargetInfo = AndroidTargetInfo|OtherTargetInfo;

// RecordingTargetV2 is subclassed by Android devices and the Chrome browser/OS.
// It creates tracing sessions which are used by the UI. For Android, it manages
// the connection with the device.
export interface RecordingTargetV2 {
  // Allows targets to surface target specific information such as
  // well known key/value pairs: OS, targetType('ANDROID', 'CHROME', etc.)
  getInfo(): TargetInfo;

  disconnect(disconnectMessage?: string): Promise<void>;

  createTracingSession(tracingSessionListener: TracingSessionListener):
      Promise<TracingSession>;
}

// TracingSession is used by the UI to record a trace. Depending on user
// actions, the UI can start/stop/cancel a session. During the recording, it
// provides updates about buffer usage. It is subclassed by
// TracedTracingSession, which manages the communication with traced and has
// logic for encoding/decoding Perfetto client requests/replies.
export interface TracingSession {
  // Starts the tracing session.
  start(config: TraceConfig): void;

  // Will stop the tracing session and NOT return any trace.
  cancel(): void;

  // Will stop the tracing session. The implementing class may also return
  // the trace using a callback.
  stop(): void;

  // Returns the percentage of the trace buffer that is currently being
  // occupied.
  getTraceBufferUsage(): Promise<number>;
}

// Connection with an Adb device. Implementations will have logic specific to
// the connection protocol used(Ex: WebSocket, WebUsb).
export interface AdbConnection {
  // Will establish a connection(a ByteStream) with the device.
  connectSocket(path: string): Promise<ByteStream>;
}

// A stream for a connection between a target and a tracing session.
export interface ByteStream {
  onStreamData: OnStreamDataCallback;
  onStreamClose: OnStreamCloseCallback;
  write(data: string|Uint8Array): void;
  close(): void;
}

// Handles binary messages received over the ByteStream.
export interface OnStreamDataCallback {
  (data: Uint8Array): void;
}

// Called when the ByteStream is closed.
export interface OnStreamCloseCallback {
  (): void;
}

// OnTraceDataCallback will return the entire trace when it has been fully
// assembled. This will be changed in the following CL aosp/2057640.
export interface OnTraceDataCallback {
  (trace: Uint8Array): void;
}

// Handles messages that are useful in the UI and that occur at any layer of the
// recording (trace, connection). The messages includes both status messages and
// error messages.
export interface OnMessageCallback {
  (message: string): void;
}

// Handles the loss of the connection at the connection layer (used by the
// AdbConnection).
export interface OnDisconnectCallback {
  (errorMessage?: string): void;
}

// Called when there is a change of targets.
// For instance, it's used when an Adb device becomes connected/disconnected.
export interface OnTargetChangedCallback {
  (): void;
}

// A collection of callbacks that is passed to RecordingTargetV2 and
// subsequently to TracingSession. The callbacks are decided by the UI, so the
// recording code is not coupled with the rendering logic.
export interface TracingSessionListener {
  onTraceData: OnTraceDataCallback;
  onStatus: OnMessageCallback;
  onDisconnect: OnDisconnectCallback;
  onError: OnMessageCallback;
}
