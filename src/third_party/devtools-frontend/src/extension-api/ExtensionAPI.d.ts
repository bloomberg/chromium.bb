// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export namespace Chrome {
  export namespace DevTools {
    export interface EventSink<ListenerT extends(...args: any) => any> {
      addListener(listener: ListenerT): void;
    }

    export interface Resource {
      readonly url: string;

      getContent(callback: (content: string, encoding: string) => unknown): void;
      setContent(content: string, commit: boolean, callback?: (error?: Object) => unknown): void;
    }

    export interface InspectedWindow {
      tabId: number;

      onResourceAdded: EventSink<(resource: Resource) => unknown>;
      onResourceContentCommitted: EventSink<(resource: Resource, content: string) => unknown>;

      eval(
          expression: string,
          options?: {contextSecurityOrigin?: string, frameURL?: string, useContentScriptContext?: boolean},
          callback?: (result: unknown, exceptioninfo: {
            code: string,
            description: string,
            details: unknown[],
            isError: boolean,
            isException: boolean,
            value: string
          }) => unknown): void;
      getResources(callback: (resources: Resource[]) => unknown): void;
      reload(reloadOptions?: {ignoreCache?: boolean, injectedScript?: string, userAgent?: string}): void;
    }

    export interface Button {
      onClicked: EventSink<() => unknown>;
      update(iconPath?: string, tooltipText?: string, disabled?: boolean): void;
    }

    export interface ExtensionView {
      onHidden: EventSink<() => unknown>;
      onShown: EventSink<(window?: Window) => unknown>;
    }

    export interface ExtensionPanel extends ExtensionView {
      onSearch: EventSink<(action: string, queryString?: string) => unknown>;
      createStatusBarButton(iconPath: string, tooltipText: string, disabled: boolean): Button;
    }

    export interface ExtensionSidebarPane extends ExtensionView {
      setHeight(height: string): void;
      setObject(jsonObject: string, rootTitle?: string, callback?: () => unknown): void;
      setPage(path: string): void;
    }

    export interface PanelWithSidebar {
      createSidebarPane(title: string, callback?: (result: ExtensionSidebarPane) => unknown): void;
      onSelectionChanged: EventSink<() => unknown>;
    }

    export interface Panels {
      elements: PanelWithSidebar;
      sources: PanelWithSidebar;
      themeName: string;

      create(title: string, iconPath: string, pagePath: string, callback?: (panel: ExtensionPanel) => unknown): void;
      openResource(url: string, lineNumber: number, callback?: () => unknown): void;
    }

    export interface Request {
      getContent(callback: (content: string, encoding: string) => unknown): void;
    }

    export interface Network {
      onNavigated: EventSink<(url: string) => unknown>;
      onRequestFinished: EventSink<(request: Request) => unknown>;

      getHAR(callback: (harLog: object) => unknown): void;
    }

    export interface DevToolsAPI {
      network: Network;
      panels: Panels;
      inspectedWindow: InspectedWindow;
      languageServices: LanguageExtensions;
    }

    export interface ExperimentalDevToolsAPI {
      inspectedWindow: InspectedWindow;
    }

    export interface RawModule {
      url: string;
      code?: ArrayBuffer;
    }

    export interface RawLocationRange {
      rawModuleId: string;
      startOffset: number;
      endOffset: number;
    }

    export interface RawLocation {
      rawModuleId: string;
      codeOffset: number;
      inlineFrameIndex: number;
    }

    export interface SourceLocation {
      rawModuleId: string;
      sourceFileURL: string;
      lineNumber: number;
      columnNumber: number;
    }

    export interface Variable {
      scope: string;
      name: string;
      type: string;
      nestedName?: string[];
    }

    export interface ScopeInfo {
      type: string;
      typeName: string;
      icon?: string;
    }

    export interface FunctionInfo {
      name: string;
    }

    export type Enumerator = {
      name: string,
      value: unknown,
      typeId: unknown,
    };

    export interface FieldInfo {
      name?: string;
      offset: number;
      typeId: unknown;
    }

    export interface TypeInfo {
      typeNames: string[];
      typeId: unknown;
      members: FieldInfo[];
      enumerators?: Enumerator[];
      alignment: number;
      arraySize: number;
      size: number;
      canExpand: boolean;
      hasValue: boolean;
    }

    export interface EvalBase {
      rootType: TypeInfo;
      payload: unknown;
    }

    export interface LanguageExtensionPlugin {
      /**
       * A new raw module has been loaded. If the raw wasm module references an external debug info module, its URL will be
       * passed as symbolsURL.
       */
      addRawModule(rawModuleId: string, symbolsURL: string|undefined, rawModule: RawModule): Promise<string[]>;

      /**
       * Find locations in raw modules from a location in a source file.
       */
      sourceLocationToRawLocation(sourceLocation: SourceLocation): Promise<RawLocationRange[]>;

      /**
       * Find locations in source files from a location in a raw module.
       */
      rawLocationToSourceLocation(rawLocation: RawLocation): Promise<SourceLocation[]>;

      /**
       * Return detailed information about a scope.
       */
      getScopeInfo(type: string): Promise<ScopeInfo>;

      /**
       * List all variables in lexical scope at a given location in a raw module.
       */
      listVariablesInScope(rawLocation: RawLocation): Promise<Variable[]>;

      /**
       * Notifies the plugin that a script is removed.
       */
      removeRawModule(rawModuleId: string): Promise<void>;

      /**
       * Return type information for an expression. The result describes the type (and recursively its member types) of the
       * result of the expression if it were evaluated in the given context.
       */
      getTypeInfo(expression: string, context: RawLocation): Promise<{
        typeInfos: Array<TypeInfo>,
        base: EvalBase,
      }|null>;

      /**
       * Returns a piece of JavaScript code that, if evaluated, produces a representation of the given expression or field.
       */
      getFormatter(
          expressionOrField: string|{
            base: EvalBase,
            field: Array<FieldInfo>,
          },
          context: RawLocation): Promise<{
        js: string,
      }|null>;

      /**
       * Returns a piece of JavaScript code that, if evaluated, produces the address of the given field in the wasm memory.
       */
      getInspectableAddress(field: {
        base: EvalBase,
        field: Array<FieldInfo>,
      }): Promise<{
        js: string,
      }>;

      /**
       * Find locations in source files from a location in a raw module
       */
      getFunctionInfo(rawLocation: RawLocation): Promise<{
        frames: Array<FunctionInfo>,
      }>;

      /**
       * Find locations in raw modules corresponding to the inline function
       * that rawLocation is in. Used for stepping out of an inline function.
       */
      getInlinedFunctionRanges(rawLocation: RawLocation): Promise<RawLocationRange[]>;

      /**
       * Find locations in raw modules corresponding to inline functions
       * called by the function or inline frame that rawLocation is in.
       * Used for stepping over inline functions.
       */
      getInlinedCalleesRanges(rawLocation: RawLocation): Promise<RawLocationRange[]>;

      /**
       * Retrieve a list of line numbers in a file for which line-to-raw-location mappings exist.
       */
      getMappedLines(rawModuleId: string, sourceFileURL: string): Promise<number[]|undefined>;
    }


    export interface SupportedScriptTypes {
      language: string;
      symbol_types: string[];
    }

    export interface LanguageExtensions {
      registerLanguageExtensionPlugin(
          plugin: LanguageExtensionPlugin, pluginName: string,
          supportedScriptTypes: SupportedScriptTypes): Promise<void>;
      unregisterLanguageExtensionPlugin(plugin: LanguageExtensionPlugin): Promise<void>;
    }

    export interface Chrome {
      devtools: DevToolsAPI;
      experimental: {devtools: ExperimentalDevToolsAPI};
    }
  }
}

declare global {
  interface Window {
    chrome: Chrome.DevTools.Chrome;
  }
}
