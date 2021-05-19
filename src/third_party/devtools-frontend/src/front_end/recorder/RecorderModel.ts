// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* eslint-disable rulesdir/no_underscored_properties */

import * as Common from '../core/common/common.js';
import * as SDK from '../core/sdk/sdk.js';
import * as UI from '../ui/legacy/legacy.js';
import type * as Workspace from '../workspace/workspace.js';

import {RecordingSession} from './RecordingSession.js';

const enum RecorderState {
  Recording = 'Recording',
  Idle = 'Idle',
}

export class RecorderModel extends SDK.SDKModel.SDKModel {
  _debuggerAgent: ProtocolProxyApi.DebuggerApi;
  _domDebuggerAgent: ProtocolProxyApi.DOMDebuggerApi;
  _runtimeAgent: ProtocolProxyApi.RuntimeApi;
  _accessibilityAgent: ProtocolProxyApi.AccessibilityApi;
  _toggleRecordAction: UI.ActionRegistration.Action;
  _state: RecorderState;
  _currentRecordingSession: RecordingSession|null;
  _indentation: string;

  constructor(target: SDK.SDKModel.Target) {
    super(target);
    this._debuggerAgent = target.debuggerAgent();
    this._domDebuggerAgent = target.domdebuggerAgent();
    this._runtimeAgent = target.runtimeAgent();
    this._accessibilityAgent = target.accessibilityAgent();
    this._toggleRecordAction =
        UI.ActionRegistry.ActionRegistry.instance().action('recorder.toggle-recording') as UI.ActionRegistration.Action;

    this._state = RecorderState.Idle;
    this._currentRecordingSession = null;
    this._indentation = Common.Settings.Settings.instance().moduleSetting('textEditorIndent').get();
  }

  async updateState(newState: RecorderState): Promise<void> {
    this._state = newState;
    this._toggleRecordAction.setToggled(this._state === RecorderState.Recording);
  }

  isRecording(): boolean {
    return this._state === RecorderState.Recording;
  }

  async toggleRecording(uiSourceCode: Workspace.UISourceCode.UISourceCode): Promise<void> {
    if (this._state === RecorderState.Idle) {
      await this.startRecording(uiSourceCode);
      await this.updateState(RecorderState.Recording);
    } else if (this._state === RecorderState.Recording) {
      await this.stopRecording();
      await this.updateState(RecorderState.Idle);
    }
  }

  async startRecording(uiSourceCode: Workspace.UISourceCode.UISourceCode): Promise<void> {
    this._currentRecordingSession = new RecordingSession(this.target(), uiSourceCode, this._indentation);
    await this._currentRecordingSession.start();
  }

  async stopRecording(): Promise<void> {
    if (!this._currentRecordingSession) {
      return;
    }

    this._currentRecordingSession.stop();
    this._currentRecordingSession = null;
  }
}

SDK.SDKModel.SDKModel.register(RecorderModel, SDK.SDKModel.Capability.None, false);
