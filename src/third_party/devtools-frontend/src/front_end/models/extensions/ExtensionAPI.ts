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

import type * as PublicAPI from '../../../extension-api/ExtensionAPI'; // eslint-disable-line rulesdir/es_modules_import
import type * as HAR from '../har/har.js';

/* eslint-disable @typescript-eslint/naming-convention,@typescript-eslint/no-non-null-assertion */
export namespace PrivateAPI {
  export namespace Panels {
    export const enum SearchAction {
      CancelSearch = 'cancelSearch',
      PerformSearch = 'performSearch',
      NextSearchResult = 'nextSearchResult',
      PreviousSearchResult = 'previousSearchResult',
    }
  }

  export const enum Events {
    ButtonClicked = 'button-clicked-',
    PanelObjectSelected = 'panel-objectSelected-',
    InspectedURLChanged = 'inspected-url-changed',
    NetworkRequestFinished = 'network-request-finished',
    OpenResource = 'open-resource',
    PanelSearch = 'panel-search-',
    RecordingStarted = 'trace-recording-started-',
    RecordingStopped = 'trace-recording-stopped-',
    ResourceAdded = 'resource-added',
    ResourceContentCommitted = 'resource-content-committed',
    ViewShown = 'view-shown-',
    ViewHidden = 'view-hidden,',
  }

  export const enum Commands {
    AddRequestHeaders = 'addRequestHeaders',
    AddTraceProvider = 'addTraceProvider',
    ApplyStyleSheet = 'applyStyleSheet',
    CompleteTraceSession = 'completeTra.eSession',
    CreatePanel = 'createPanel',
    CreateSidebarPane = 'createSidebarPane',
    CreateToolbarButton = 'createToolbarButton',
    EvaluateOnInspectedPage = 'evaluateOnInspectedPage',
    ForwardKeyboardEvent = '_forwardKeyboardEvent',
    GetHAR = 'getHAR',
    GetPageResources = 'getPageResources',
    GetRequestContent = 'getRequestContent',
    GetResourceContent = 'getResourceContent',
    OpenResource = 'openResource',
    Reload = 'Reload',
    Subscribe = 'subscribe',
    SetOpenResourceHandler = 'setOpenResourceHandler',
    SetResourceContent = 'setResourceContent',
    SetSidebarContent = 'setSidebarContent',
    SetSidebarHeight = 'setSidebarHeight',
    SetSidebarPage = 'setSidebarPage',
    ShowPanel = 'showPanel',
    Unsubscribe = 'unsubscribe',
    UpdateButton = 'updateButton',
    RegisterLanguageExtensionPlugin = 'registerLanguageExtensionPlugin',
  }

  export const enum LanguageExtensionPluginCommands {
    AddRawModule = 'addRawModule',
    RemoveRawModule = 'removeRawModule',
    SourceLocationToRawLocation = 'sourceLocationToRawLocation',
    RawLocationToSourceLocation = 'rawLocationToSourceLocation',
    GetScopeInfo = 'getScopeInfo',
    ListVariablesInScope = 'listVariablesInScope',
    GetTypeInfo = 'getTypeInfo',
    GetFormatter = 'getFormatter',
    GetInspectableAddress = 'getInspectableAddress',
    GetFunctionInfo = 'getFunctionInfo',
    GetInlinedFunctionRanges = 'getInlinedFunctionRanges',
    GetInlinedCalleesRanges = 'getInlinedCalleesRanges',
    GetMappedLines = 'getMappedLines',
  }

  export const enum LanguageExtensionPluginEvents {
    UnregisteredLanguageExtensionPlugin = 'unregisteredLanguageExtensionPlugin',
  }

  export interface EvaluateOptions {
    frameURL?: string;
    useContentScriptContext?: boolean;
    scriptExecutionContext?: string;
  }

  type RegisterLanguageExtensionPluginRequest = {
    command: Commands.RegisterLanguageExtensionPlugin,
    pluginName: string,
    port: MessagePort,
    supportedScriptTypes: PublicAPI.Chrome.DevTools.SupportedScriptTypes,
  };
  type SubscribeRequest = {command: Commands.Subscribe, type: string};
  type UnsubscribeRequest = {command: Commands.Unsubscribe, type: string};
  type AddRequestHeadersRequest = {
    command: Commands.AddRequestHeaders,
    extensionId: string,
    headers: {[key: string]: string},
  };
  type ApplyStyleSheetRequest = {command: Commands.ApplyStyleSheet, styleSheet: string};
  type CreatePanelRequest = {command: Commands.CreatePanel, id: string, title: string, page: string};
  type ShowPanelRequest = {command: Commands.ShowPanel, id: string};
  type CreateToolbarButtonRequest = {
    command: Commands.CreateToolbarButton,
    id: string,
    icon: string,
    panel: string,
    tooltip?: string,
    disabled?: boolean,
  };
  type UpdateButtonRequest =
      {command: Commands.UpdateButton, id: string, icon?: string, tooltip?: string, disabled?: boolean};
  type CompleteTraceSessionRequest =
      {command: Commands.CompleteTraceSession, id: string, url: string, timeOffset: number};
  type CreateSidebarPaneRequest = {command: Commands.CreateSidebarPane, id: string, panel: string, title: string};
  type SetSidebarHeightRequest = {command: Commands.SetSidebarHeight, id: string, height: string};
  type SetSidebarContentRequest = {
    command: Commands.SetSidebarContent,
    id: string,
    evaluateOnPage?: boolean, expression: string,
    rootTitle?: string,
    evaluateOptions?: EvaluateOptions,
  };
  type SetSidebarPageRequest = {command: Commands.SetSidebarPage, id: string, page: string};
  type OpenResourceRequest = {command: Commands.OpenResource, url: string, lineNumber: number, columnNumber: number};
  type SetOpenResourceHandlerRequest = {command: Commands.SetOpenResourceHandler, handlerPresent: boolean};
  type ReloadRequest = {
    command: Commands.Reload,
    options: null|{
      userAgent?: string,
      injectedScript?: string,
      ignoreCache?: boolean,
    },
  };
  type EvaluateOnInspectedPageRequest = {
    command: Commands.EvaluateOnInspectedPage,
    expression: string,
    evaluateOptions?: EvaluateOptions,
  };
  type GetRequestContentRequest = {command: Commands.GetRequestContent, id: number};
  type GetResourceContentRequest = {command: Commands.GetResourceContent, url: string};
  type SetResourceContentRequest =
      {command: Commands.SetResourceContent, url: string, content: string, commit: boolean};
  type AddTraceProviderRequest =
      {command: Commands.AddTraceProvider, id: string, categoryName: string, categoryTooltip: string};
  type ForwardKeyboardEventRequest = {
    command: Commands.ForwardKeyboardEvent,
    entries: Array<KeyboardEventInit&{eventType: string}>,
  };
  type GetHARRequest = {command: Commands.GetHAR};
  type GetPageResourcesRequest = {command: Commands.GetPageResources};

  export type ServerRequests = RegisterLanguageExtensionPluginRequest|SubscribeRequest|UnsubscribeRequest|
      AddRequestHeadersRequest|ApplyStyleSheetRequest|CreatePanelRequest|ShowPanelRequest|CreateToolbarButtonRequest|
      UpdateButtonRequest|CompleteTraceSessionRequest|CreateSidebarPaneRequest|SetSidebarHeightRequest|
      SetSidebarContentRequest|SetSidebarPageRequest|OpenResourceRequest|SetOpenResourceHandlerRequest|ReloadRequest|
      EvaluateOnInspectedPageRequest|GetRequestContentRequest|GetResourceContentRequest|SetResourceContentRequest|
      AddTraceProviderRequest|ForwardKeyboardEventRequest|GetHARRequest|GetPageResourcesRequest;
  export type ExtensionServerRequestMessage = PrivateAPI.ServerRequests&{requestId?: number};

  type AddRawModuleRequest = {
    method: LanguageExtensionPluginCommands.AddRawModule,
    parameters: {rawModuleId: string, symbolsURL: string|undefined, rawModule: PublicAPI.Chrome.DevTools.RawModule},
  };
  type SourceLocationToRawLocationRequest = {
    method: LanguageExtensionPluginCommands.SourceLocationToRawLocation,
    parameters: {sourceLocation: PublicAPI.Chrome.DevTools.SourceLocation},
  };
  type RawLocationToSourceLocationRequest = {
    method: LanguageExtensionPluginCommands.RawLocationToSourceLocation,
    parameters: {rawLocation: PublicAPI.Chrome.DevTools.RawLocation},
  };
  type GetScopeInfoRequest = {method: LanguageExtensionPluginCommands.GetScopeInfo, parameters: {type: string}};
  type ListVariablesInScopeRequest = {
    method: LanguageExtensionPluginCommands.ListVariablesInScope,
    parameters: {rawLocation: PublicAPI.Chrome.DevTools.RawLocation},
  };
  type RemoveRawModuleRequest = {
    method: LanguageExtensionPluginCommands.RemoveRawModule,
    parameters: {rawModuleId: string},
  };
  type GetTypeInfoRequest = {
    method: LanguageExtensionPluginCommands.GetTypeInfo,
    parameters: {expression: string, context: PublicAPI.Chrome.DevTools.RawLocation},
  };
  type GetFormatterRequest = {
    method: LanguageExtensionPluginCommands.GetFormatter,
    parameters: {
      expressionOrField: string|{
        base: PublicAPI.Chrome.DevTools.EvalBase,
        field: Array<PublicAPI.Chrome.DevTools.FieldInfo>,
      },
      context: PublicAPI.Chrome.DevTools.RawLocation,
    },
  };
  type GetInspectableAddressRequest = {
    method: LanguageExtensionPluginCommands.GetInspectableAddress,
    parameters: {
      field: {
        base: PublicAPI.Chrome.DevTools.EvalBase,
        field: Array<PublicAPI.Chrome.DevTools.FieldInfo>,
      },
    },
  };
  type GetFunctionInfoRequest = {
    method: LanguageExtensionPluginCommands.GetFunctionInfo,
    parameters: {rawLocation: PublicAPI.Chrome.DevTools.RawLocation},
  };
  type GetInlinedFunctionRangesRequest = {
    method: LanguageExtensionPluginCommands.GetInlinedFunctionRanges,
    parameters: {rawLocation: PublicAPI.Chrome.DevTools.RawLocation},
  };
  type GetInlinedCalleesRangesRequest = {
    method: LanguageExtensionPluginCommands.GetInlinedCalleesRanges,
    parameters: {rawLocation: PublicAPI.Chrome.DevTools.RawLocation},
  };
  type GetMappedLinesRequest = {
    method: LanguageExtensionPluginCommands.GetMappedLines,
    parameters: {rawModuleId: string, sourceFileURL: string},
  };

  export type LanguageExtensionRequests = AddRawModuleRequest|SourceLocationToRawLocationRequest|
      RawLocationToSourceLocationRequest|GetScopeInfoRequest|ListVariablesInScopeRequest|RemoveRawModuleRequest|
      GetTypeInfoRequest|GetFormatterRequest|GetInspectableAddressRequest|GetFunctionInfoRequest|
      GetInlinedFunctionRangesRequest|GetInlinedCalleesRangesRequest|GetMappedLinesRequest;
}

declare global {
  interface Window {
    injectedExtensionAPI:
        (extensionInfo: ExtensionDescriptor, inspectedTabId: string, themeName: string, keysToForward: number[],
         testHook:
             (extensionServer: APIImpl.ExtensionServerClient, extensionAPI: APIImpl.InspectorExtensionAPI) => unknown,
         injectedScriptId: number) => void;
    buildExtensionAPIInjectedScript(
        extensionInfo: ExtensionDescriptor, inspectedTabId: string, themeName: string, keysToForward: number[],
        testHook: undefined|((extensionServer: unknown, extensionAPI: unknown) => unknown)): string;
    chrome: PublicAPI.Chrome.DevTools.Chrome;
    webInspector?: APIImpl.InspectorExtensionAPI;
  }
}

export type ExtensionDescriptor = {
  startPage: string,
  name: string,
  exposeExperimentalAPIs: boolean,
  exposeWebInspectorNamespace?: boolean,
};

namespace APIImpl {
  export interface InspectorExtensionAPI {
    languageServices: PublicAPI.Chrome.DevTools.LanguageExtensions;
    timeline: Timeline;
    network: PublicAPI.Chrome.DevTools.Network;
    panels: PublicAPI.Chrome.DevTools.Panels;
    inspectedWindow: PublicAPI.Chrome.DevTools.InspectedWindow;
  }

  export interface ExtensionServerClient {
    _callbacks: {[key: string]: (response: unknown) => unknown};
    _handlers: {[key: string]: (request: {arguments: unknown[]}) => unknown};
    _lastRequestId: number;
    _lastObjectId: number;
    _port: MessagePort;

    _onCallback(request: unknown): void;
    _onMessage(event: MessageEvent<{command: string, requestId: number, arguments: unknown[]}>): void;
    _registerCallback(callback: (response: unknown) => unknown): number;
    registerHandler(command: string, handler: (request: {arguments: unknown[]}) => unknown): void;
    unregisterHandler(command: string): void;
    hasHandler(command: string): boolean;
    sendRequest(request: PrivateAPI.ServerRequests, callback?: ((response: unknown) => unknown), transfers?: unknown[]):
        void;
    nextObjectId(): string;
  }

  // We cannot use the stronger `unknown` type in place of `any` in the following type definition. The type is used as
  // the right-hand side of `extends` in a few places, which doesn't narrow `unknown`. Without narrowing, overload
  // resolution and meaningful type inference of arguments break, for example.
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  export type Callable = (...args: any) => any;

  export interface EventSink<ListenerT extends Callable> extends PublicAPI.Chrome.DevTools.EventSink<ListenerT> {
    _type: string;
    _listeners: ListenerT[];
    _customDispatch: undefined|((this: EventSink<ListenerT>, request: {arguments: unknown[]}) => unknown);

    _fire(..._vararg: Parameters<ListenerT>): void;
    _dispatch(request: {arguments: unknown[]}): void;
  }

  export interface Network extends PublicAPI.Chrome.DevTools.Network {
    addRequestHeaders(headers: {[key: string]: string}): void;
  }

  export interface Request extends PublicAPI.Chrome.DevTools.Request, HAR.Log.EntryDTO {
    _id: number;
  }

  export interface Panels extends PublicAPI.Chrome.DevTools.Panels {
    get SearchAction(): {[key: string]: string};
    applyStyleSheet(styleSheet: string): void;
    setOpenResourceHandler(callback?: (resource: PublicAPI.Chrome.DevTools.Resource, lineNumber: number) => unknown):
        void;
  }

  export interface ExtensionView extends PublicAPI.Chrome.DevTools.ExtensionView {
    _id: string|null;
  }

  export interface ExtensionSidebarPane extends ExtensionView, PublicAPI.Chrome.DevTools.ExtensionSidebarPane {
    setExpression(
        expression: string, rootTitle?: string, evaluteOptions?: PrivateAPI.EvaluateOptions,
        callback?: () => unknown): void;
  }

  export interface PanelWithSidebar extends ExtensionView, PublicAPI.Chrome.DevTools.PanelWithSidebar {
    _hostPanelName: string;
  }

  export interface LanguageExtensions extends PublicAPI.Chrome.DevTools.LanguageExtensions {
    _plugins: Map<PublicAPI.Chrome.DevTools.LanguageExtensionPlugin, MessagePort>;
  }

  export interface ExtensionPanel extends ExtensionView, PublicAPI.Chrome.DevTools.ExtensionPanel {
    show(): void;
  }

  export interface Button extends PublicAPI.Chrome.DevTools.Button {
    _id: string;
  }

  export interface TraceSession {
    _id: string;

    complete(url?: string, timeOffset?: number): void;
  }

  export interface TraceProvider {
    onRecordingStarted: EventSink<(session: TraceSession) => unknown>;
    onRecordingStopped: EventSink<() => unknown>;
  }

  export interface Timeline {
    addTraceProvider(categoryName: string, categoryTooltip: string): TraceProvider;
  }

  export type ResourceData = {url: string, type: string};
  export interface Resource extends PublicAPI.Chrome.DevTools.Resource {
    _type: string;
    _url: string;

    get type(): string;
  }
}

self.injectedExtensionAPI = function(
    extensionInfo: ExtensionDescriptor, inspectedTabId: string, themeName: string, keysToForward: number[],
    testHook: (extensionServer: APIImpl.ExtensionServerClient, extensionAPI: APIImpl.InspectorExtensionAPI) => unknown,
    injectedScriptId: number): void {
  const keysToForwardSet = new Set<number>(keysToForward);
  const chrome = window.chrome || {};

  const devtools_descriptor = Object.getOwnPropertyDescriptor(chrome, 'devtools');
  if (devtools_descriptor) {
    return;
  }
  let userAction = false;

  // Here and below, all constructors are private to API implementation.
  // For a public type Foo, if internal fields are present, these are on
  // a private FooImpl type, an instance of FooImpl is used in a closure
  // by Foo consutrctor to re-bind publicly exported members to an instance
  // of Foo.

  function EventSinkImpl<ListenerT extends APIImpl.Callable>(
      this: APIImpl.EventSink<ListenerT>, type: string,
      customDispatch?: (this: APIImpl.EventSink<ListenerT>, request: {arguments: unknown[]}) => unknown): void {
    this._type = type;
    this._listeners = [];
    this._customDispatch = customDispatch;
  }

  EventSinkImpl.prototype = {
    addListener: function<ListenerT extends APIImpl.Callable>(this: APIImpl.EventSink<ListenerT>, callback: ListenerT):
        void {
          if (typeof callback !== 'function') {
            throw 'addListener: callback is not a function';
          }
          if (this._listeners.length === 0) {
            extensionServer.sendRequest({command: PrivateAPI.Commands.Subscribe, type: this._type});
          }
          this._listeners.push(callback);
          extensionServer.registerHandler('notify-' + this._type, this._dispatch.bind(this));
        },

    removeListener: function<ListenerT extends APIImpl.Callable>(
        this: APIImpl.EventSink<ListenerT>, callback: ListenerT): void {
      const listeners = this._listeners;

      for (let i = 0; i < listeners.length; ++i) {
        if (listeners[i] === callback) {
          listeners.splice(i, 1);
          break;
        }
      }
      if (this._listeners.length === 0) {
        extensionServer.sendRequest({command: PrivateAPI.Commands.Unsubscribe, type: this._type});
      }
    },

    _fire: function<ListenerT extends APIImpl.Callable>(
        this: APIImpl.EventSink<ListenerT>, ..._vararg: Parameters<ListenerT>): void {
      const listeners = this._listeners.slice();
      for (let i = 0; i < listeners.length; ++i) {
        listeners[i].apply(null, Array.from(arguments));
      }
    },

    _dispatch: function<ListenerT extends APIImpl.Callable>(
        this: APIImpl.EventSink<ListenerT>, request: {arguments: unknown[]}): void {
      if (this._customDispatch) {
        this._customDispatch.call(this, request);
      } else {
        this._fire.apply(this, request.arguments as Parameters<ListenerT>);
      }
    },
  };

  function Constructor<NewT extends APIImpl.Callable>(ctor: NewT): new (...args: Parameters<NewT>) =>
      ThisParameterType<NewT> {
    return ctor as unknown as new (...args: Parameters<NewT>) => ThisParameterType<NewT>;
  }

  function InspectorExtensionAPI(this: APIImpl.InspectorExtensionAPI): void {
    this.inspectedWindow = new (Constructor(InspectedWindow))();
    this.panels = new (Constructor(Panels))();
    this.network = new (Constructor(Network))();
    this.timeline = new (Constructor(Timeline))();
    this.languageServices = new (Constructor(LanguageServicesAPI))();
    defineDeprecatedProperty(this, 'webInspector', 'resources', 'network');
  }

  function Network(this: APIImpl.Network): void {
    function dispatchRequestEvent(
        this: APIImpl.EventSink<(request: PublicAPI.Chrome.DevTools.Request) => unknown>,
        message: {arguments: unknown[]}): void {
      const request = message.arguments[1] as APIImpl.Request & {__proto__: APIImpl.Request};

      request.__proto__ = new (Constructor(Request))(message.arguments[0] as number);
      this._fire(request);
    }

    this.onRequestFinished =
        new (Constructor(EventSink))(PrivateAPI.Events.NetworkRequestFinished, dispatchRequestEvent);
    defineDeprecatedProperty(this, 'network', 'onFinished', 'onRequestFinished');

    this.onNavigated = new (Constructor(EventSink))(PrivateAPI.Events.InspectedURLChanged);
  }

  (Network.prototype as Pick<APIImpl.Network, 'getHAR'|'addRequestHeaders'>) = {
    getHAR: function(this: PublicAPI.Chrome.DevTools.Network, callback?: (harLog: Object) => unknown): void {
      function callbackWrapper(response: unknown): void {
        const result =
            response as ({entries: Array<HAR.Log.EntryDTO&{__proto__?: APIImpl.Request, _requestId?: number}>});
        const entries = (result && result.entries) || [];
        for (let i = 0; i < entries.length; ++i) {
          entries[i].__proto__ = new (Constructor(Request))(entries[i]._requestId as number);
          delete entries[i]._requestId;
        }
        callback && callback(result as Object);
      }
      extensionServer.sendRequest({command: PrivateAPI.Commands.GetHAR}, callback && callbackWrapper);
    },

    addRequestHeaders: function(headers: {[key: string]: string}): void {
      extensionServer.sendRequest(
          {command: PrivateAPI.Commands.AddRequestHeaders, headers: headers, extensionId: window.location.hostname});
    },
  };

  function RequestImpl(this: APIImpl.Request, id: number): void {
    this._id = id;
  }

  (RequestImpl.prototype as Pick<APIImpl.Request, 'getContent'>) = {
    getContent: function(this: APIImpl.Request, callback?: (content: string, encoding: string) => unknown): void {
      function callbackWrapper(response: unknown): void {
        const {content, encoding} = response as {content: string, encoding: string};
        callback && callback(content, encoding);
      }
      extensionServer.sendRequest(
          {command: PrivateAPI.Commands.GetRequestContent, id: this._id}, callback && callbackWrapper);
    },
  };

  function Panels(this: APIImpl.Panels): void {
    const panels: {[key: string]: ElementsPanel|SourcesPanel} = {
      elements: new ElementsPanel(),
      sources: new SourcesPanel(),
    };

    function panelGetter(name: string): ElementsPanel|SourcesPanel {
      return panels[name];
    }
    for (const panel in panels) {
      Object.defineProperty(this, panel, {get: panelGetter.bind(null, panel), enumerable: true});
    }
    this.applyStyleSheet = function(styleSheet: string): void {
      extensionServer.sendRequest({command: PrivateAPI.Commands.ApplyStyleSheet, styleSheet: styleSheet});
    };
  }

  (Panels.prototype as Pick<APIImpl.Panels, 'create'|'setOpenResourceHandler'|'openResource'|'SearchAction'>) = {
    create: function(
        title: string, icon: string, page: string,
        callback: (panel: PublicAPI.Chrome.DevTools.ExtensionPanel) => unknown): void {
      const id = 'extension-panel-' + extensionServer.nextObjectId();
      extensionServer.sendRequest(
          {command: PrivateAPI.Commands.CreatePanel, id, title, page},
          callback && ((): unknown => callback.call(this, new (Constructor(ExtensionPanel))(id))));
    },

    setOpenResourceHandler: function(
        callback: (resource: PublicAPI.Chrome.DevTools.Resource, lineNumber: number) => unknown): void {
      const hadHandler = extensionServer.hasHandler(PrivateAPI.Events.OpenResource);

      function callbackWrapper(message: unknown): void {
        // Allow the panel to show itself when handling the event.
        userAction = true;
        try {
          const {resource, lineNumber} = message as {resource: APIImpl.ResourceData, lineNumber: number};
          callback.call(null, new (Constructor(Resource))(resource), lineNumber);
        } finally {
          userAction = false;
        }
      }

      if (!callback) {
        extensionServer.unregisterHandler(PrivateAPI.Events.OpenResource);
      } else {
        extensionServer.registerHandler(PrivateAPI.Events.OpenResource, callbackWrapper);
      }

      // Only send command if we either removed an existing handler or added handler and had none before.
      if (hadHandler === !callback) {
        extensionServer.sendRequest(
            {command: PrivateAPI.Commands.SetOpenResourceHandler, 'handlerPresent': Boolean(callback)});
      }
    },

    openResource: function(
        url: string, lineNumber: number, columnNumber?: number, _callback?: (response: unknown) => unknown): void {
      const callbackArg = extractCallbackArgument(arguments);
      // Handle older API:
      const columnNumberArg = typeof columnNumber === 'number' ? columnNumber : 0;
      extensionServer.sendRequest(
          {command: PrivateAPI.Commands.OpenResource, url, lineNumber, columnNumber: columnNumberArg}, callbackArg);
    },

    get SearchAction(): {[key: string]: string} {
      return {
        CancelSearch: PrivateAPI.Panels.SearchAction.CancelSearch,
        PerformSearch: PrivateAPI.Panels.SearchAction.PerformSearch,
        NextSearchResult: PrivateAPI.Panels.SearchAction.NextSearchResult,
        PreviousSearchResult: PrivateAPI.Panels.SearchAction.PreviousSearchResult,
      };
    },
  };

  function ExtensionViewImpl(this: APIImpl.ExtensionView, id: string|null): void {
    this._id = id;

    function dispatchShowEvent(
        this: APIImpl.EventSink<(window?: Window) => unknown>, message: {arguments: unknown[]}): void {
      const frameIndex = message.arguments[0];
      if (typeof frameIndex === 'number') {
        this._fire(window.parent.frames[frameIndex]);
      } else {
        this._fire();
      }
    }

    if (id) {
      this.onShown = new (Constructor(EventSink))(PrivateAPI.Events.ViewShown + id, dispatchShowEvent);

      this.onHidden = new (Constructor(EventSink))(PrivateAPI.Events.ViewHidden + id);
    }
  }

  function PanelWithSidebarImpl(this: APIImpl.PanelWithSidebar, hostPanelName: string): void {
    ExtensionViewImpl.call(this, null);
    this._hostPanelName = hostPanelName;

    this.onSelectionChanged = new (Constructor(EventSink))(PrivateAPI.Events.PanelObjectSelected + hostPanelName);
  }

  (PanelWithSidebarImpl.prototype as Pick<APIImpl.PanelWithSidebar, 'createSidebarPane'>&
   {__proto__: APIImpl.ExtensionView}) = {
    createSidebarPane: function(
        this: APIImpl.PanelWithSidebar, title: string,
        callback?: (pane: PublicAPI.Chrome.DevTools.ExtensionSidebarPane) => unknown): void {
      const id = 'extension-sidebar-' + extensionServer.nextObjectId();
      function callbackWrapper(): void {
        callback && callback(new (Constructor(ExtensionSidebarPane))(id));
      }
      extensionServer.sendRequest(
          {command: PrivateAPI.Commands.CreateSidebarPane, panel: this._hostPanelName, id, title},
          callback && callbackWrapper);
    },

    __proto__: ExtensionViewImpl.prototype,
  };

  function LanguageServicesAPIImpl(this: APIImpl.LanguageExtensions): void {
    /** @type {!Map<*, !MessagePort>} */
    this._plugins = new Map();
  }

  (LanguageServicesAPIImpl.prototype as
   Pick<APIImpl.LanguageExtensions, 'registerLanguageExtensionPlugin'|'unregisterLanguageExtensionPlugin'>) = {
    registerLanguageExtensionPlugin: async function(
        this: APIImpl.LanguageExtensions, plugin: PublicAPI.Chrome.DevTools.LanguageExtensionPlugin, pluginName: string,
        supportedScriptTypes: PublicAPI.Chrome.DevTools.SupportedScriptTypes): Promise<void> {
      if (this._plugins.has(plugin)) {
        throw new Error(`Tried to register plugin '${pluginName}' twice`);
      }
      const channel = new MessageChannel();
      const port = channel.port1;
      this._plugins.set(plugin, port);
      port.onmessage = ({data}: MessageEvent<{requestId: number}&PrivateAPI.LanguageExtensionRequests>): void => {
        const {requestId} = data;
        console.time(`${requestId}: ${data.method}`);
        dispatchMethodCall(data)
            .then(result => port.postMessage({requestId, result}))
            .catch(error => port.postMessage({requestId, error: {message: error.message}}))
            .finally(() => console.timeEnd(`${requestId}: ${data.method}`));
      };

      function dispatchMethodCall(request: PrivateAPI.LanguageExtensionRequests): Promise<unknown> {
        switch (request.method) {
          case PrivateAPI.LanguageExtensionPluginCommands.AddRawModule:
            return plugin.addRawModule(
                request.parameters.rawModuleId, request.parameters.symbolsURL, request.parameters.rawModule);
          case PrivateAPI.LanguageExtensionPluginCommands.RemoveRawModule:
            return plugin.removeRawModule(request.parameters.rawModuleId);
          case PrivateAPI.LanguageExtensionPluginCommands.SourceLocationToRawLocation:
            return plugin.sourceLocationToRawLocation(request.parameters.sourceLocation);
          case PrivateAPI.LanguageExtensionPluginCommands.RawLocationToSourceLocation:
            return plugin.rawLocationToSourceLocation(request.parameters.rawLocation);
          case PrivateAPI.LanguageExtensionPluginCommands.GetScopeInfo:
            return plugin.getScopeInfo(request.parameters.type);
          case PrivateAPI.LanguageExtensionPluginCommands.ListVariablesInScope:
            return plugin.listVariablesInScope(request.parameters.rawLocation);
          case PrivateAPI.LanguageExtensionPluginCommands.GetTypeInfo:
            return plugin.getTypeInfo(request.parameters.expression, request.parameters.context);
          case PrivateAPI.LanguageExtensionPluginCommands.GetFormatter:
            return plugin.getFormatter(request.parameters.expressionOrField, request.parameters.context);
          case PrivateAPI.LanguageExtensionPluginCommands.GetInspectableAddress:
            if ('getInspectableAddress' in plugin) {
              return plugin.getInspectableAddress(request.parameters.field);
            }
            return Promise.resolve({js: ''});
          case PrivateAPI.LanguageExtensionPluginCommands.GetFunctionInfo:
            return plugin.getFunctionInfo(request.parameters.rawLocation);
          case PrivateAPI.LanguageExtensionPluginCommands.GetInlinedFunctionRanges:
            return plugin.getInlinedFunctionRanges(request.parameters.rawLocation);
          case PrivateAPI.LanguageExtensionPluginCommands.GetInlinedCalleesRanges:
            return plugin.getInlinedCalleesRanges(request.parameters.rawLocation);
          case PrivateAPI.LanguageExtensionPluginCommands.GetMappedLines:
            if ('getMappedLines' in plugin) {
              return plugin.getMappedLines(request.parameters.rawModuleId, request.parameters.sourceFileURL);
            }
            return Promise.resolve(undefined);
        }
        // @ts-expect-error
        throw new Error(`Unknown language plugin method ${request.method}`);
      }

      await new Promise<void>(resolve => {
        extensionServer.sendRequest(
            {
              command: PrivateAPI.Commands.RegisterLanguageExtensionPlugin,
              pluginName,
              port: channel.port2,
              supportedScriptTypes,
            },
            () => resolve(), [channel.port2]);
      });
    },

    unregisterLanguageExtensionPlugin: async function(
        this: APIImpl.LanguageExtensions, plugin: PublicAPI.Chrome.DevTools.LanguageExtensionPlugin): Promise<void> {
      const port = this._plugins.get(plugin);
      if (!port) {
        throw new Error('Tried to unregister a plugin that was not previously registered');
      }
      this._plugins.delete(plugin);
      port.postMessage({event: PrivateAPI.LanguageExtensionPluginEvents.UnregisteredLanguageExtensionPlugin});
      port.close();
    },
  };

  function declareInterfaceClass<ImplT extends APIImpl.Callable>(implConstructor: ImplT): (
      this: ThisParameterType<ImplT>, ...args: Parameters<ImplT>) => void {
    return function(this: ThisParameterType<ImplT>, ...args: Parameters<ImplT>): void {
      const impl = {__proto__: implConstructor.prototype};
      implConstructor.apply(impl, args);
      populateInterfaceClass(this as {[key: string]: unknown}, impl);
    };
  }

  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  function defineDeprecatedProperty(object: any, className: string, oldName: string, newName: string): void {
    let warningGiven = false;
    function getter(): unknown {
      if (!warningGiven) {
        console.warn(className + '.' + oldName + ' is deprecated. Use ' + className + '.' + newName + ' instead');
        warningGiven = true;
      }
      return object[newName];
    }
    object.__defineGetter__(oldName, getter);
  }

  function extractCallbackArgument(args: IArguments): ((...args: unknown[]) => unknown)|undefined {
    const lastArgument = args[args.length - 1];
    return typeof lastArgument === 'function' ? lastArgument as (...args: unknown[]) => unknown : undefined;
  }

  const LanguageServicesAPI = declareInterfaceClass(LanguageServicesAPIImpl);
  const Button = declareInterfaceClass(ButtonImpl);
  const EventSink = declareInterfaceClass(EventSinkImpl);
  const ExtensionPanel = declareInterfaceClass(ExtensionPanelImpl);
  const ExtensionSidebarPane = declareInterfaceClass(ExtensionSidebarPaneImpl);
  const PanelWithSidebarClass = declareInterfaceClass(PanelWithSidebarImpl);
  const Request = declareInterfaceClass(RequestImpl);
  const Resource = declareInterfaceClass(ResourceImpl);
  const TraceSession = declareInterfaceClass(TraceSessionImpl);

  class ElementsPanel extends (Constructor(PanelWithSidebarClass)) {
    constructor() {
      super('elements');
    }
  }

  class SourcesPanel extends (Constructor(PanelWithSidebarClass)) {
    constructor() {
      super('sources');
    }
  }

  function ExtensionPanelImpl(this: APIImpl.ExtensionPanel, id: string): void {
    ExtensionViewImpl.call(this, id);

    this.onSearch = new (Constructor(EventSink))(PrivateAPI.Events.PanelSearch + id);
  }

  (ExtensionPanelImpl.prototype as Pick<APIImpl.ExtensionPanel, 'createStatusBarButton'|'show'>&
   {__proto__: APIImpl.ExtensionView}) = {
    createStatusBarButton: function(
                               this: APIImpl.ExtensionPanel, iconPath: string, tooltipText: string, disabled: boolean):
                               PublicAPI.Chrome.DevTools.Button {
                                 const id = 'button-' + extensionServer.nextObjectId();
                                 extensionServer.sendRequest({
                                   command: PrivateAPI.Commands.CreateToolbarButton,
                                   panel: this._id as string,
                                   id: id,
                                   icon: iconPath,
                                   tooltip: tooltipText,
                                   disabled: Boolean(disabled),
                                 });

                                 return new (Constructor(Button))(id);
                               },

    show: function(this: APIImpl.ExtensionPanel): void {
      if (!userAction) {
        return;
      }

      extensionServer.sendRequest({command: PrivateAPI.Commands.ShowPanel, id: this._id as string});
    },

    __proto__: ExtensionViewImpl.prototype,
  };

  function ExtensionSidebarPaneImpl(this: APIImpl.ExtensionSidebarPane, id: string): void {
    ExtensionViewImpl.call(this, id);
  }

  (ExtensionSidebarPaneImpl.prototype as
       Pick<APIImpl.ExtensionSidebarPane, 'setHeight'|'setExpression'|'setObject'|'setPage'>&
   {__proto__: APIImpl.ExtensionView}) = {
    setHeight: function(this: APIImpl.ExtensionSidebarPane, height: string): void {
      extensionServer.sendRequest(
          {command: PrivateAPI.Commands.SetSidebarHeight, id: this._id as string, height: height});
    },

    setExpression: function(
        this: APIImpl.ExtensionSidebarPane, expression: string, rootTitle: string,
        evaluateOptions?: PrivateAPI.EvaluateOptions, _callback?: () => unknown): void {
      extensionServer.sendRequest(
          {
            command: PrivateAPI.Commands.SetSidebarContent,
            id: this._id as string,
            expression: expression,
            rootTitle: rootTitle,
            evaluateOnPage: true,
            evaluateOptions: (typeof evaluateOptions === 'object' ? evaluateOptions : {}),
          },
          extractCallbackArgument(arguments));
    },

    setObject: function(
        this: APIImpl.ExtensionSidebarPane, jsonObject: string, rootTitle?: string, callback?: () => unknown): void {
      extensionServer.sendRequest(
          {
            command: PrivateAPI.Commands.SetSidebarContent,
            id: this._id as string,
            expression: jsonObject,
            rootTitle: rootTitle,
          },
          callback);
    },

    setPage: function(this: APIImpl.ExtensionSidebarPane, page: string): void {
      extensionServer.sendRequest({command: PrivateAPI.Commands.SetSidebarPage, id: this._id as string, page: page});
    },

    __proto__: ExtensionViewImpl.prototype,
  };

  function ButtonImpl(this: APIImpl.Button, id: string): void {
    this._id = id;

    this.onClicked = new (Constructor(EventSink))(PrivateAPI.Events.ButtonClicked + id);
  }

  (ButtonImpl.prototype as Pick<APIImpl.Button, 'update'>) = {
    update: function(this: APIImpl.Button, iconPath?: string, tooltipText?: string, disabled?: boolean): void {
      extensionServer.sendRequest({
        command: PrivateAPI.Commands.UpdateButton,
        id: this._id,
        icon: iconPath,
        tooltip: tooltipText,
        disabled: Boolean(disabled),
      });
    },
  };

  function Timeline(this: APIImpl.Timeline): void {
  }

  (Timeline.prototype as Pick<APIImpl.Timeline, 'addTraceProvider'>) = {
    addTraceProvider: function(this: APIImpl.Timeline, categoryName: string, categoryTooltip: string):
                          APIImpl.TraceProvider {
                            const id = 'extension-trace-provider-' + extensionServer.nextObjectId();
                            extensionServer.sendRequest({
                              command: PrivateAPI.Commands.AddTraceProvider,
                              id: id,
                              categoryName: categoryName,
                              categoryTooltip: categoryTooltip,
                            });

                            return new (Constructor(TraceProvider))(id);
                          },
  };

  function TraceSessionImpl(this: APIImpl.TraceSession, id: string): void {
    this._id = id;
  }

  (TraceSessionImpl.prototype as Pick<APIImpl.TraceSession, 'complete'>) = {
    complete: function(this: APIImpl.TraceSession, url?: string, timeOffset?: number): void {
      extensionServer.sendRequest({
        command: PrivateAPI.Commands.CompleteTraceSession,
        id: this._id,
        url: url || '',
        timeOffset: timeOffset || 0,
      });
    },
  };

  function TraceProvider(this: APIImpl.TraceProvider, id: string): void {
    function dispatchRecordingStarted(
        this: APIImpl.EventSink<APIImpl.Callable>, message: {arguments: unknown[]}): void {
      const sessionId = message.arguments[0] as string;

      this._fire(new (Constructor(TraceSession))(sessionId));
    }

    this.onRecordingStarted =
        new (Constructor(EventSink))(PrivateAPI.Events.RecordingStarted + id, dispatchRecordingStarted);

    this.onRecordingStopped = new (Constructor(EventSink))(PrivateAPI.Events.RecordingStopped + id);
  }

  function InspectedWindow(this: PublicAPI.Chrome.DevTools.InspectedWindow): void {
    function dispatchResourceEvent(
        this: APIImpl.EventSink<(resource: APIImpl.Resource) => unknown>, message: {arguments: unknown[]}): void {
      this._fire(new (Constructor(Resource))(message.arguments[0] as APIImpl.ResourceData));
    }

    function dispatchResourceContentEvent(
        this: APIImpl.EventSink<(resource: APIImpl.Resource, content: string) => unknown>,
        message: {arguments: unknown[]}): void {
      this._fire(
          new (Constructor(Resource))(message.arguments[0] as APIImpl.ResourceData), message.arguments[1] as string);
    }

    this.onResourceAdded = new (Constructor(EventSink))(PrivateAPI.Events.ResourceAdded, dispatchResourceEvent);
    this.onResourceContentCommitted =
        new (Constructor(EventSink))(PrivateAPI.Events.ResourceContentCommitted, dispatchResourceContentEvent);
  }

  (InspectedWindow.prototype as Pick<PublicAPI.Chrome.DevTools.InspectedWindow, 'reload'|'eval'|'getResources'>) = {
    reload: function(optionsOrUserAgent: {
      ignoreCache?: boolean,
      injectedScript?: string,
      userAgent?: string,
    }): void {
      let options: {
        ignoreCache?: boolean,
        injectedScript?: string,
        userAgent?: string,
      }|null = null;
      if (typeof optionsOrUserAgent === 'object') {
        options = optionsOrUserAgent;
      } else if (typeof optionsOrUserAgent === 'string') {
        options = {userAgent: optionsOrUserAgent};
        console.warn(
            'Passing userAgent as string parameter to inspectedWindow.reload() is deprecated. ' +
            'Use inspectedWindow.reload({ userAgent: value}) instead.');
      }
      extensionServer.sendRequest({command: PrivateAPI.Commands.Reload, options: options});
    },

    eval: function(
              expression: string,
              evaluateOptions: {contextSecurityOrigin?: string, frameURL?: string, useContentScriptContext?: boolean}):
              Object |
        null {
          const callback = extractCallbackArgument(arguments);
          function callbackWrapper(result: unknown): void {
            const {isError, isException, value} = result as {
              isError?: boolean,
              isException?: boolean, value: unknown,
            };
            if (isError || isException) {
              callback && callback(undefined, result);
            } else {
              callback && callback(value);
            }
          }
          extensionServer.sendRequest(
              {
                command: PrivateAPI.Commands.EvaluateOnInspectedPage,
                expression: expression,
                evaluateOptions: (typeof evaluateOptions === 'object' ? evaluateOptions : undefined),
              },
              callback && callbackWrapper);
          return null;
        },

    getResources: function(callback?: (resources: PublicAPI.Chrome.DevTools.Resource[]) => unknown): void {
      function wrapResource(resourceData: APIImpl.ResourceData): APIImpl.Resource {
        return new (Constructor(Resource))(resourceData);
      }
      function callbackWrapper(resources: unknown): void {
        callback && callback((resources as APIImpl.ResourceData[]).map(wrapResource));
      }
      extensionServer.sendRequest({command: PrivateAPI.Commands.GetPageResources}, callback && callbackWrapper);
    },
  };

  function ResourceImpl(this: APIImpl.Resource, resourceData: APIImpl.ResourceData): void {
    this._url = resourceData.url;
    this._type = resourceData.type;
  }

  (ResourceImpl.prototype as Pick<APIImpl.Resource, 'url'|'type'|'getContent'|'setContent'>) = {
    get url(): string {
      return (this as APIImpl.Resource)._url;
    },

    get type(): string {
      return (this as APIImpl.Resource)._type;
    },

    getContent: function(this: APIImpl.Resource, callback?: (content: string, encoding: string) => unknown): void {
      function callbackWrapper(response: unknown): void {
        const {content, encoding} = response as {content: string, encoding: string};
        callback && callback(content, encoding);
      }

      extensionServer.sendRequest(
          {command: PrivateAPI.Commands.GetResourceContent, url: this._url}, callback && callbackWrapper);
    },

    setContent: function(
        this: APIImpl.Resource, content: string, commit: boolean, callback: (error?: Object) => unknown): void {
      extensionServer.sendRequest(
          {command: PrivateAPI.Commands.SetResourceContent, url: this._url, content: content, commit: commit},
          callback as (response: unknown) => unknown);
    },
  };

  function getTabId(): string {
    return inspectedTabId;
  }

  let keyboardEventRequestQueue: KeyboardEventInit&{eventType: string}[] = [];
  let forwardTimer: number|null = null;
  function forwardKeyboardEvent(event: KeyboardEvent): void {
    // Check if the event should be forwarded.
    // This is a workaround for crbug.com/923338.
    const focused = document.activeElement;
    if (focused) {
      const isInput = focused.nodeName === 'INPUT' || focused.nodeName === 'TEXTAREA';
      if (isInput && !(event.ctrlKey || event.altKey || event.metaKey)) {
        return;
      }
    }

    let modifiers = 0;
    if (event.shiftKey) {
      modifiers |= 1;
    }
    if (event.ctrlKey) {
      modifiers |= 2;
    }
    if (event.altKey) {
      modifiers |= 4;
    }
    if (event.metaKey) {
      modifiers |= 8;
    }
    const num = (event.keyCode & 255) | (modifiers << 8);
    // We only care about global hotkeys, not about random text
    if (!keysToForwardSet.has(num)) {
      return;
    }
    event.preventDefault();
    const requestPayload = {
      eventType: event.type,
      ctrlKey: event.ctrlKey,
      altKey: event.altKey,
      metaKey: event.metaKey,
      shiftKey: event.shiftKey,
      // @ts-expect-error keyIdentifier is a deprecated non-standard property that typescript doesn't know about.
      keyIdentifier: event.keyIdentifier,
      key: event.key,
      code: event.code,
      location: event.location,
      keyCode: event.keyCode,
    };
    keyboardEventRequestQueue.push(requestPayload);
    if (!forwardTimer) {
      forwardTimer = setTimeout(forwardEventQueue, 0);
    }
  }

  function forwardEventQueue(): void {
    forwardTimer = null;
    extensionServer.sendRequest(
        {command: PrivateAPI.Commands.ForwardKeyboardEvent, entries: keyboardEventRequestQueue});
    keyboardEventRequestQueue = [];
  }

  document.addEventListener('keydown', forwardKeyboardEvent, false);

  function ExtensionServerClient(this: APIImpl.ExtensionServerClient): void {
    this._callbacks = {};
    this._handlers = {};
    this._lastRequestId = 0;
    this._lastObjectId = 0;

    this.registerHandler('callback', this._onCallback.bind(this));

    const channel = new MessageChannel();
    this._port = channel.port1;
    this._port.addEventListener('message', this._onMessage.bind(this), false);
    this._port.start();

    window.parent.postMessage('registerExtension', '*', [channel.port2]);
  }

  (ExtensionServerClient.prototype as Pick<
       APIImpl.ExtensionServerClient,
       'sendRequest'|'hasHandler'|'registerHandler'|'unregisterHandler'|'nextObjectId'|'_registerCallback'|
       '_onCallback'|'_onMessage'>) = {
    sendRequest: function(
        this: APIImpl.ExtensionServerClient, message: PrivateAPI.ServerRequests,
        callback?: (response: unknown) => unknown, transfers?: Transferable[]): void {
      if (typeof callback === 'function') {
        (message as PrivateAPI.ExtensionServerRequestMessage).requestId = this._registerCallback(callback);
      }
      // @ts-expect-error
      this._port.postMessage(message, transfers);
    },

    hasHandler: function(this: APIImpl.ExtensionServerClient, command: string): boolean {
      return Boolean(this._handlers[command]);
    },

    registerHandler: function(
        this: APIImpl.ExtensionServerClient, command: string, handler: (request: {arguments: unknown[]}) => unknown):
        void {
          this._handlers[command] = handler;
        },

    unregisterHandler: function(this: APIImpl.ExtensionServerClient, command: string): void {
      delete this._handlers[command];
    },

    nextObjectId: function(this: APIImpl.ExtensionServerClient): string {
      return injectedScriptId.toString() + '_' + ++this._lastObjectId;
    },

    _registerCallback: function(this: APIImpl.ExtensionServerClient, callback: (response: unknown) => unknown): number {
      const id = ++this._lastRequestId;
      this._callbacks[id] = callback;
      return id;
    },

    _onCallback: function(this: APIImpl.ExtensionServerClient, request: {requestId: number, result: unknown}): void {
      if (request.requestId in this._callbacks) {
        const callback = this._callbacks[request.requestId];
        delete this._callbacks[request.requestId];
        callback(request.result);
      }
    },

    _onMessage: function(
        this: APIImpl.ExtensionServerClient,
        event: MessageEvent<{command: string, requestId: number, arguments: unknown[]}>): void {
      const request = event.data;
      const handler = this._handlers[request.command];
      if (handler) {
        handler.call(this, request);
      }
    },
  };

  function populateInterfaceClass(interfaze: {[key: string]: unknown}, implementation: {[key: string]: unknown}): void {
    for (const member in implementation) {
      if (member.charAt(0) === '_') {
        continue;
      }
      let descriptor: (PropertyDescriptor|undefined)|null = null;
      // Traverse prototype chain until we find the owner.
      for (let owner = implementation; owner && !descriptor; owner = owner.__proto__ as {[key: string]: unknown}) {
        descriptor = Object.getOwnPropertyDescriptor(owner, member);
      }
      if (!descriptor) {
        continue;
      }
      if (typeof descriptor.value === 'function') {
        interfaze[member] = descriptor.value.bind(implementation);
      } else if (typeof descriptor.get === 'function') {
        // @ts-expect-error
        interfaze.__defineGetter__(member, descriptor.get.bind(implementation));
      } else {
        Object.defineProperty(interfaze, member, descriptor);
      }
    }
  }

  const extensionServer = new (Constructor(ExtensionServerClient))();

  const coreAPI = new (Constructor(InspectorExtensionAPI))();

  Object.defineProperty(chrome, 'devtools', {value: {}, enumerable: true});

  // Only expose tabId on chrome.devtools.inspectedWindow, not webInspector.inspectedWindow.
  // @ts-expect-error
  chrome.devtools!.inspectedWindow = {};
  Object.defineProperty(chrome.devtools!.inspectedWindow, 'tabId', {get: getTabId});
  // @ts-expect-error
  chrome.devtools!.inspectedWindow.__proto__ = coreAPI.inspectedWindow;
  chrome.devtools!.network = coreAPI.network;
  chrome.devtools!.panels = coreAPI.panels;
  chrome.devtools!.panels.themeName = themeName;
  chrome.devtools!.languageServices = coreAPI.languageServices;

  // default to expose experimental APIs for now.
  if (extensionInfo.exposeExperimentalAPIs !== false) {
    chrome.experimental = chrome.experimental || {};
    chrome.experimental.devtools = chrome.experimental.devtools || {};

    const properties = Object.getOwnPropertyNames(coreAPI);
    for (let i = 0; i < properties.length; ++i) {
      const descriptor = Object.getOwnPropertyDescriptor(coreAPI, properties[i]);
      if (descriptor) {
        Object.defineProperty(chrome.experimental.devtools, properties[i], descriptor);
      }
    }
    chrome.experimental.devtools.inspectedWindow = chrome.devtools.inspectedWindow;
  }

  if (extensionInfo.exposeWebInspectorNamespace) {
    window.webInspector = coreAPI;
  }
  testHook(extensionServer, coreAPI);
};

self.buildExtensionAPIInjectedScript = function(
    extensionInfo: {
      startPage: string,
      name: string,
      exposeExperimentalAPIs: boolean,
    },
    inspectedTabId: string, themeName: string, keysToForward: number[],
    testHook:
        ((extensionServer: APIImpl.ExtensionServerClient, extensionAPI: APIImpl.InspectorExtensionAPI) => unknown)|
    undefined): string {
  const argumentsJSON =
      [extensionInfo, inspectedTabId || null, themeName, keysToForward].map(_ => JSON.stringify(_)).join(',');
  if (!testHook) {
    testHook = (): void => {};
  }
  return '(function(injectedScriptId){ ' +
      '(' + self.injectedExtensionAPI.toString() + ')(' + argumentsJSON + ',' + testHook + ', injectedScriptId);' +
      '})';
};
