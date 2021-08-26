/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

import type * as Common from '../../core/common/common.js';
import * as SDK from '../../core/sdk/sdk.js';
import * as TextUtils from '../text_utils/text_utils.js';
import * as Workspace from '../workspace/workspace.js';
import * as Protocol from '../../generated/protocol.js';

import {DebuggerWorkspaceBinding} from './DebuggerWorkspaceBinding.js';
import type {LiveLocation} from './LiveLocation.js';
import {LiveLocationPool} from './LiveLocation.js';

const debuggerModelToMessageHelperMap =
    new WeakMap<SDK.DebuggerModel.DebuggerModel, PresentationConsoleMessageHelper>();

export class PresentationConsoleMessageManager implements
    SDK.TargetManager.SDKModelObserver<SDK.DebuggerModel.DebuggerModel> {
  constructor() {
    SDK.TargetManager.TargetManager.instance().observeModels(SDK.DebuggerModel.DebuggerModel, this);

    SDK.ConsoleModel.ConsoleModel.instance().addEventListener(
        SDK.ConsoleModel.Events.ConsoleCleared, this.consoleCleared, this);
    SDK.ConsoleModel.ConsoleModel.instance().addEventListener(
        SDK.ConsoleModel.Events.MessageAdded,
        event => this.consoleMessageAdded((event.data as SDK.ConsoleModel.ConsoleMessage)));
    SDK.ConsoleModel.ConsoleModel.instance().messages().forEach(this.consoleMessageAdded, this);
  }

  modelAdded(debuggerModel: SDK.DebuggerModel.DebuggerModel): void {
    debuggerModelToMessageHelperMap.set(debuggerModel, new PresentationConsoleMessageHelper(debuggerModel));
  }

  modelRemoved(debuggerModel: SDK.DebuggerModel.DebuggerModel): void {
    const helper = debuggerModelToMessageHelperMap.get(debuggerModel);
    if (helper) {
      helper.consoleCleared();
    }
  }

  private consoleMessageAdded(message: SDK.ConsoleModel.ConsoleMessage): void {
    const runtimeModel = message.runtimeModel();
    if (!message.isErrorOrWarning() || !message.runtimeModel() ||
        message.source === Protocol.Log.LogEntrySource.Violation || !runtimeModel) {
      return;
    }
    const helper = debuggerModelToMessageHelperMap.get(runtimeModel.debuggerModel());
    if (helper) {
      helper.consoleMessageAdded(message);
    }
  }

  private consoleCleared(): void {
    for (const debuggerModel of SDK.TargetManager.TargetManager.instance().models(SDK.DebuggerModel.DebuggerModel)) {
      const helper = debuggerModelToMessageHelperMap.get(debuggerModel);
      if (helper) {
        helper.consoleCleared();
      }
    }
  }
}

export class PresentationConsoleMessageHelper {
  private readonly debuggerModel: SDK.DebuggerModel.DebuggerModel;
  private pendingConsoleMessages: Map<string, SDK.ConsoleModel.ConsoleMessage[]>;
  private presentationConsoleMessages: PresentationConsoleMessage[];
  private readonly locationPool: LiveLocationPool;

  constructor(debuggerModel: SDK.DebuggerModel.DebuggerModel) {
    this.debuggerModel = debuggerModel;

    this.pendingConsoleMessages = new Map();

    this.presentationConsoleMessages = [];

    // TODO(dgozman): queueMicrotask because we race with DebuggerWorkspaceBinding on ParsedScriptSource event delivery.
    debuggerModel.addEventListener(SDK.DebuggerModel.Events.ParsedScriptSource, event => {
      queueMicrotask(() => {
        this.parsedScriptSource(event);
      });
    });
    debuggerModel.addEventListener(SDK.DebuggerModel.Events.GlobalObjectCleared, this.debuggerReset, this);

    this.locationPool = new LiveLocationPool();
  }

  consoleMessageAdded(message: SDK.ConsoleModel.ConsoleMessage): void {
    const rawLocation = this.rawLocation(message);
    if (rawLocation) {
      this.addConsoleMessageToScript(message, rawLocation);
    } else {
      this.addPendingConsoleMessage(message);
    }
  }

  private rawLocation(message: SDK.ConsoleModel.ConsoleMessage): SDK.DebuggerModel.Location|null {
    if (message.scriptId) {
      return this.debuggerModel.createRawLocationByScriptId(message.scriptId, message.line, message.column);
    }
    const callFrame = message.stackTrace && message.stackTrace.callFrames ? message.stackTrace.callFrames[0] : null;
    if (callFrame) {
      return this.debuggerModel.createRawLocationByScriptId(
          callFrame.scriptId, callFrame.lineNumber, callFrame.columnNumber);
    }
    if (message.url) {
      return this.debuggerModel.createRawLocationByURL(message.url, message.line, message.column);
    }
    return null;
  }

  private addConsoleMessageToScript(message: SDK.ConsoleModel.ConsoleMessage, rawLocation: SDK.DebuggerModel.Location):
      void {
    this.presentationConsoleMessages.push(new PresentationConsoleMessage(message, rawLocation, this.locationPool));
  }

  private addPendingConsoleMessage(message: SDK.ConsoleModel.ConsoleMessage): void {
    if (!message.url) {
      return;
    }
    const pendingMessages = this.pendingConsoleMessages.get(message.url);
    if (!pendingMessages) {
      this.pendingConsoleMessages.set(message.url, [message]);
    } else {
      pendingMessages.push(message);
    }
  }

  private parsedScriptSource(event: Common.EventTarget.EventTargetEvent<SDK.Script.Script>): void {
    const script = event.data;

    const messages = this.pendingConsoleMessages.get(script.sourceURL);
    if (!messages) {
      return;
    }

    const pendingMessages = [];
    for (const message of messages) {
      const rawLocation = this.rawLocation(message);
      if (rawLocation && script.scriptId === rawLocation.scriptId) {
        this.addConsoleMessageToScript(message, rawLocation);
      } else {
        pendingMessages.push(message);
      }
    }

    if (pendingMessages.length) {
      this.pendingConsoleMessages.set(script.sourceURL, pendingMessages);
    } else {
      this.pendingConsoleMessages.delete(script.sourceURL);
    }
  }

  consoleCleared(): void {
    this.pendingConsoleMessages = new Map();
    this.debuggerReset();
  }

  private debuggerReset(): void {
    for (const message of this.presentationConsoleMessages) {
      message.dispose();
    }
    this.presentationConsoleMessages = [];
    this.locationPool.disposeAll();
  }
}

export class PresentationConsoleMessage extends Workspace.UISourceCode.Message {
  private uiSourceCode?: Workspace.UISourceCode.UISourceCode;

  constructor(
      message: SDK.ConsoleModel.ConsoleMessage, rawLocation: SDK.DebuggerModel.Location,
      locationPool: LiveLocationPool) {
    const level = message.level === Protocol.Log.LogEntryLevel.Error ? Workspace.UISourceCode.Message.Level.Error :
                                                                       Workspace.UISourceCode.Message.Level.Warning;
    super(level, message.messageText);
    DebuggerWorkspaceBinding.instance().createLiveLocation(rawLocation, this.updateLocation.bind(this), locationPool);
  }

  private async updateLocation(liveLocation: LiveLocation): Promise<void> {
    if (this.uiSourceCode) {
      this.uiSourceCode.removeMessage(this);
    }
    const uiLocation = await liveLocation.uiLocation();
    if (!uiLocation) {
      return;
    }
    this.range = TextUtils.TextRange.TextRange.createFromLocation(uiLocation.lineNumber, uiLocation.columnNumber || 0);
    this.uiSourceCode = uiLocation.uiSourceCode;
    this.uiSourceCode.addMessage(this);
  }

  dispose(): void {
    if (this.uiSourceCode) {
      this.uiSourceCode.removeMessage(this);
    }
  }
}
