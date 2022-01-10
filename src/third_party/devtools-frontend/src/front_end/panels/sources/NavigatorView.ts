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

import * as Common from '../../core/common/common.js';
import * as Host from '../../core/host/host.js';
import * as i18n from '../../core/i18n/i18n.js';
import * as Platform from '../../core/platform/platform.js';
import * as SDK from '../../core/sdk/sdk.js';
import * as Bindings from '../../models/bindings/bindings.js';
import * as Persistence from '../../models/persistence/persistence.js';
import * as Workspace from '../../models/workspace/workspace.js';
import * as UI from '../../ui/legacy/legacy.js';
import * as Snippets from '../snippets/snippets.js';

import navigatorTreeStyles from './navigatorTree.css.js';
import navigatorViewStyles from './navigatorView.css.js';
import {SearchSourcesView} from './SearchSourcesView.js';

const UIStrings = {
  /**
  *@description Text in Navigator View of the Sources panel
  */
  searchInFolder: 'Search in folder',
  /**
  *@description Search label in Navigator View of the Sources panel
  */
  searchInAllFiles: 'Search in all files',
  /**
  *@description Text in Navigator View of the Sources panel
  */
  noDomain: '(no domain)',
  /**
  *@description Text in Navigator View of the Sources panel
  */
  areYouSureYouWantToExcludeThis: 'Are you sure you want to exclude this folder?',
  /**
  *@description Text in Navigator View of the Sources panel
  */
  areYouSureYouWantToDeleteThis: 'Are you sure you want to delete this file?',
  /**
  *@description A context menu item in the Navigator View of the Sources panel
  */
  rename: 'Rename…',
  /**
  *@description A context menu item in the Navigator View of the Sources panel
  */
  makeACopy: 'Make a copy…',
  /**
  *@description Text to delete something
  */
  delete: 'Delete',
  /**
  *@description Text in Navigator View of the Sources panel
  */
  areYouSureYouWantToDeleteAll: 'Are you sure you want to delete all overrides contained in this folder?',
  /**
  *@description A context menu item in the Navigator View of the Sources panel
  */
  openFolder: 'Open folder',
  /**
  *@description A context menu item in the Navigator View of the Sources panel
  */
  newFile: 'New file',
  /**
  *@description A context menu item in the Navigator View of the Sources panel
  */
  excludeFolder: 'Exclude folder',
  /**
  *@description A context menu item in the Navigator View of the Sources panel
  */
  removeFolderFromWorkspace: 'Remove folder from workspace',
  /**
  *@description Text in Navigator View of the Sources panel
  */
  areYouSureYouWantToRemoveThis: 'Are you sure you want to remove this folder?',
  /**
  *@description A context menu item in the Navigator View of the Sources panel
  */
  deleteAllOverrides: 'Delete all overrides',
  /**
  *@description Name of an item from source map
  *@example {compile.html} PH1
  */
  sFromSourceMap: '{PH1} (from source map)',
};
const str_ = i18n.i18n.registerUIStrings('panels/sources/NavigatorView.ts', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);
export const Types = {
  Domain: 'domain',
  File: 'file',
  FileSystem: 'fs',
  FileSystemFolder: 'fs-folder',
  Frame: 'frame',
  NetworkFolder: 'nw-folder',
  Root: 'root',
  SourceMapFolder: 'sm-folder',
  Worker: 'worker',
};

const TYPE_ORDERS = new Map([
  [Types.Root, 1],
  [Types.Domain, 10],
  [Types.FileSystemFolder, 1],
  [Types.NetworkFolder, 1],
  [Types.SourceMapFolder, 2],
  [Types.File, 10],
  [Types.Frame, 70],
  [Types.Worker, 90],
  [Types.FileSystem, 100],
]);

export class NavigatorView extends UI.Widget.VBox implements SDK.TargetManager.Observer {
  private placeholder: UI.Widget.Widget|null;
  scriptsTree: UI.TreeOutline.TreeOutlineInShadow;
  private readonly uiSourceCodeNodes:
      Platform.MapUtilities.Multimap<Workspace.UISourceCode.UISourceCode, NavigatorUISourceCodeTreeNode>;
  private readonly subfolderNodes: Map<string, NavigatorFolderTreeNode>;
  private readonly rootNode: NavigatorRootTreeNode;
  private readonly frameNodes: Map<SDK.ResourceTreeModel.ResourceTreeFrame, NavigatorGroupTreeNode>;
  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration)
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private navigatorGroupByFolderSetting: Common.Settings.Setting<any>;
  private workspaceInternal!: Workspace.Workspace.WorkspaceImpl;
  private lastSelectedUISourceCode?: Workspace.UISourceCode.UISourceCode;
  private groupByFrame?: boolean;
  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration)
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private groupByDomain?: any;
  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration)
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private groupByFolder?: any;
  constructor() {
    super(true);

    this.placeholder = null;
    this.scriptsTree = new UI.TreeOutline.TreeOutlineInShadow();

    this.scriptsTree.setComparator(NavigatorView.treeElementsCompare);
    this.scriptsTree.setFocusable(false);
    this.contentElement.appendChild(this.scriptsTree.element);
    this.setDefaultFocusedElement(this.scriptsTree.element);

    this.uiSourceCodeNodes = new Platform.MapUtilities.Multimap();
    this.subfolderNodes = new Map();

    this.rootNode = new NavigatorRootTreeNode(this);
    this.rootNode.populate();

    this.frameNodes = new Map();

    this.contentElement.addEventListener('contextmenu', this.handleContextMenu.bind(this), false);
    UI.ShortcutRegistry.ShortcutRegistry.instance().addShortcutListener(
        this.contentElement, {'sources.rename': this.renameShortcut.bind(this)});

    this.navigatorGroupByFolderSetting = Common.Settings.Settings.instance().moduleSetting('navigatorGroupByFolder');
    this.navigatorGroupByFolderSetting.addChangeListener(this.groupingChanged.bind(this));

    this.initGrouping();

    Persistence.Persistence.PersistenceImpl.instance().addEventListener(
        Persistence.Persistence.Events.BindingCreated, this.onBindingChanged, this);
    Persistence.Persistence.PersistenceImpl.instance().addEventListener(
        Persistence.Persistence.Events.BindingRemoved, this.onBindingChanged, this);
    SDK.TargetManager.TargetManager.instance().addEventListener(
        SDK.TargetManager.Events.NameChanged, this.targetNameChanged, this);

    SDK.TargetManager.TargetManager.instance().observeTargets(this);
    this.resetWorkspace(Workspace.Workspace.WorkspaceImpl.instance());
    this.workspaceInternal.uiSourceCodes().forEach(this.addUISourceCode.bind(this));
    Bindings.NetworkProject.NetworkProjectManager.instance().addEventListener(
        Bindings.NetworkProject.Events.FrameAttributionAdded, this.frameAttributionAdded, this);
    Bindings.NetworkProject.NetworkProjectManager.instance().addEventListener(
        Bindings.NetworkProject.Events.FrameAttributionRemoved, this.frameAttributionRemoved, this);
  }

  private static treeElementOrder(treeElement: UI.TreeOutline.TreeElement): number {
    if (boostOrderForNode.has(treeElement)) {
      return 0;
    }

    const actualElement = (treeElement as NavigatorSourceTreeElement);

    let order = TYPE_ORDERS.get(actualElement.nodeType) || 0;
    if (actualElement.uiSourceCode) {
      const contentType = actualElement.uiSourceCode.contentType();
      if (contentType.isDocument()) {
        order += 3;
      } else if (contentType.isScript()) {
        order += 5;
      } else if (contentType.isStyleSheet()) {
        order += 10;
      } else {
        order += 15;
      }
    }

    return order;
  }

  static appendSearchItem(contextMenu: UI.ContextMenu.ContextMenu, path?: string): void {
    let searchLabel = i18nString(UIStrings.searchInFolder);
    if (!path || !path.trim()) {
      path = '*';
      searchLabel = i18nString(UIStrings.searchInAllFiles);
    }
    contextMenu.viewSection().appendItem(searchLabel, () => {
      if (path) {
        SearchSourcesView.openSearch(`file:${path.trim()}`);
      }
    });
  }

  private static treeElementsCompare(
      treeElement1: UI.TreeOutline.TreeElement, treeElement2: UI.TreeOutline.TreeElement): number {
    const typeWeight1 = NavigatorView.treeElementOrder(treeElement1);
    const typeWeight2 = NavigatorView.treeElementOrder(treeElement2);

    if (typeWeight1 > typeWeight2) {
      return 1;
    }
    if (typeWeight1 < typeWeight2) {
      return -1;
    }
    return Platform.StringUtilities.naturalOrderComparator(treeElement1.titleAsText(), treeElement2.titleAsText());
  }

  setPlaceholder(placeholder: UI.Widget.Widget): void {
    console.assert(!this.placeholder, 'A placeholder widget was already set');
    this.placeholder = placeholder;
    placeholder.show(this.contentElement, this.contentElement.firstChild);
    updateVisibility.call(this);
    this.scriptsTree.addEventListener(UI.TreeOutline.Events.ElementAttached, updateVisibility.bind(this));
    this.scriptsTree.addEventListener(UI.TreeOutline.Events.ElementsDetached, updateVisibility.bind(this));

    function updateVisibility(this: NavigatorView): void {
      const showTree = this.scriptsTree.firstChild();
      if (showTree) {
        placeholder.hideWidget();
      } else {
        placeholder.showWidget();
      }
      this.scriptsTree.element.classList.toggle('hidden', !showTree);
    }
  }

  private onBindingChanged(event: Common.EventTarget.EventTargetEvent<Persistence.Persistence.PersistenceBinding>):
      void {
    const binding = event.data;

    // Update UISourceCode titles.
    const networkNodes = this.uiSourceCodeNodes.get(binding.network);
    for (const networkNode of networkNodes) {
      networkNode.updateTitle();
    }
    const fileSystemNodes = this.uiSourceCodeNodes.get(binding.fileSystem);
    for (const fileSystemNode of fileSystemNodes) {
      fileSystemNode.updateTitle();
    }

    // Update folder titles.
    const pathTokens =
        Persistence.FileSystemWorkspaceBinding.FileSystemWorkspaceBinding.relativePath(binding.fileSystem);
    let folderPath = '';
    for (let i = 0; i < pathTokens.length - 1; ++i) {
      folderPath += pathTokens[i];
      const folderId =
          this.folderNodeId(binding.fileSystem.project(), null, null, binding.fileSystem.origin(), folderPath);
      const folderNode = this.subfolderNodes.get(folderId);
      if (folderNode) {
        folderNode.updateTitle();
      }
      folderPath += '/';
    }

    // Update fileSystem root title.
    const fileSystemRoot = this.rootNode.child(binding.fileSystem.project().id());
    if (fileSystemRoot) {
      fileSystemRoot.updateTitle();
    }
  }

  focus(): void {
    this.scriptsTree.focus();
  }

  /**
   * Central place to add elements to the tree to
   * enable focus if the tree has elements
   */
  appendChild(parent: UI.TreeOutline.TreeElement, child: UI.TreeOutline.TreeElement): void {
    this.scriptsTree.setFocusable(true);
    parent.appendChild(child);
  }

  /**
   * Central place to remove elements from the tree to
   * disable focus if the tree is empty
   */
  removeChild(parent: UI.TreeOutline.TreeElement, child: UI.TreeOutline.TreeElement): void {
    parent.removeChild(child);
    if (this.scriptsTree.rootElement().childCount() === 0) {
      this.scriptsTree.setFocusable(false);
    }
  }

  private resetWorkspace(workspace: Workspace.Workspace.WorkspaceImpl): void {
    // Clear old event listeners first.
    if (this.workspaceInternal) {
      this.workspaceInternal.removeEventListener(
          Workspace.Workspace.Events.UISourceCodeAdded, this.uiSourceCodeAddedCallback, this);
      this.workspaceInternal.removeEventListener(
          Workspace.Workspace.Events.UISourceCodeRemoved, this.uiSourceCodeRemovedCallback, this);
      this.workspaceInternal.removeEventListener(
          Workspace.Workspace.Events.ProjectAdded, this.projectAddedCallback, this);
      this.workspaceInternal.removeEventListener(
          Workspace.Workspace.Events.ProjectRemoved, this.projectRemovedCallback, this);
    }

    this.workspaceInternal = workspace;
    this.workspaceInternal.addEventListener(
        Workspace.Workspace.Events.UISourceCodeAdded, this.uiSourceCodeAddedCallback, this);
    this.workspaceInternal.addEventListener(
        Workspace.Workspace.Events.UISourceCodeRemoved, this.uiSourceCodeRemovedCallback, this);
    this.workspaceInternal.addEventListener(Workspace.Workspace.Events.ProjectAdded, this.projectAddedCallback, this);
    this.workspaceInternal.addEventListener(
        Workspace.Workspace.Events.ProjectRemoved, this.projectRemovedCallback, this);
    this.workspaceInternal.projects().forEach(this.projectAdded.bind(this));
    this.computeUniqueFileSystemProjectNames();
  }

  private projectAddedCallback(event: Common.EventTarget.EventTargetEvent<Workspace.Workspace.Project>): void {
    const project = event.data;
    this.projectAdded(project);
    if (project.type() === Workspace.Workspace.projectTypes.FileSystem) {
      this.computeUniqueFileSystemProjectNames();
    }
  }

  private projectRemovedCallback(event: Common.EventTarget.EventTargetEvent<Workspace.Workspace.Project>): void {
    const project = event.data;
    this.removeProject(project);
    if (project.type() === Workspace.Workspace.projectTypes.FileSystem) {
      this.computeUniqueFileSystemProjectNames();
    }
  }

  workspace(): Workspace.Workspace.WorkspaceImpl {
    return this.workspaceInternal;
  }

  acceptProject(project: Workspace.Workspace.Project): boolean {
    return !project.isServiceProject();
  }

  private frameAttributionAdded(
      event: Common.EventTarget.EventTargetEvent<Bindings.NetworkProject.FrameAttributionEvent>): void {
    const {uiSourceCode} = event.data;
    if (!this.acceptsUISourceCode(uiSourceCode)) {
      return;
    }

    const addedFrame = (event.data.frame as SDK.ResourceTreeModel.ResourceTreeFrame | null);
    // This event does not happen for UISourceCodes without initial attribution.
    this.addUISourceCodeNode(uiSourceCode, addedFrame);
  }

  private frameAttributionRemoved(
      event: Common.EventTarget.EventTargetEvent<Bindings.NetworkProject.FrameAttributionEvent>): void {
    const {uiSourceCode} = event.data;
    if (!this.acceptsUISourceCode(uiSourceCode)) {
      return;
    }

    const removedFrame = (event.data.frame as SDK.ResourceTreeModel.ResourceTreeFrame | null);
    const node = Array.from(this.uiSourceCodeNodes.get(uiSourceCode)).find(node => node.frame() === removedFrame);
    this.removeUISourceCodeNode((node as NavigatorUISourceCodeTreeNode));
  }

  private acceptsUISourceCode(uiSourceCode: Workspace.UISourceCode.UISourceCode): boolean {
    return this.acceptProject(uiSourceCode.project());
  }

  private addUISourceCode(uiSourceCode: Workspace.UISourceCode.UISourceCode): void {
    if (!this.acceptsUISourceCode(uiSourceCode)) {
      return;
    }

    const frames = Bindings.NetworkProject.NetworkProject.framesForUISourceCode(uiSourceCode);
    if (frames.length) {
      for (const frame of frames) {
        this.addUISourceCodeNode(uiSourceCode, frame);
      }
    } else {
      this.addUISourceCodeNode(uiSourceCode, null);
    }
    this.uiSourceCodeAdded(uiSourceCode);
  }

  private addUISourceCodeNode(
      uiSourceCode: Workspace.UISourceCode.UISourceCode, frame: SDK.ResourceTreeModel.ResourceTreeFrame|null): void {
    const isFromSourceMap = uiSourceCode.contentType().isFromSourceMap();
    let path;
    if (uiSourceCode.project().type() === Workspace.Workspace.projectTypes.FileSystem) {
      path = Persistence.FileSystemWorkspaceBinding.FileSystemWorkspaceBinding.relativePath(uiSourceCode).slice(0, -1);
    } else {
      path = Common.ParsedURL.ParsedURL.extractPath(uiSourceCode.url()).split('/').slice(1, -1);
    }

    const project = uiSourceCode.project();
    const target = Bindings.NetworkProject.NetworkProject.targetForUISourceCode(uiSourceCode);
    const folderNode =
        this.folderNode(uiSourceCode, project, target, frame, uiSourceCode.origin(), path, isFromSourceMap);
    const uiSourceCodeNode = new NavigatorUISourceCodeTreeNode(this, uiSourceCode, frame);
    folderNode.appendChild(uiSourceCodeNode);
    this.uiSourceCodeNodes.set(uiSourceCode, uiSourceCodeNode);
    this.selectDefaultTreeNode();
  }

  uiSourceCodeAdded(_uiSourceCode: Workspace.UISourceCode.UISourceCode): void {
  }

  private uiSourceCodeAddedCallback(event: Common.EventTarget.EventTargetEvent<Workspace.UISourceCode.UISourceCode>):
      void {
    const uiSourceCode = event.data;
    this.addUISourceCode(uiSourceCode);
  }

  private uiSourceCodeRemovedCallback(event: Common.EventTarget.EventTargetEvent<Workspace.UISourceCode.UISourceCode>):
      void {
    const uiSourceCode = event.data;
    this.removeUISourceCode(uiSourceCode);
  }

  tryAddProject(project: Workspace.Workspace.Project): void {
    this.projectAdded(project);
    project.uiSourceCodes().forEach(this.addUISourceCode.bind(this));
  }

  private projectAdded(project: Workspace.Workspace.Project): void {
    if (!this.acceptProject(project) || project.type() !== Workspace.Workspace.projectTypes.FileSystem ||
        Snippets.ScriptSnippetFileSystem.isSnippetsProject(project) || this.rootNode.child(project.id())) {
      return;
    }
    this.rootNode.appendChild(
        new NavigatorGroupTreeNode(this, project, project.id(), Types.FileSystem, project.displayName()));
    this.selectDefaultTreeNode();
  }

  // TODO(einbinder) remove this code after crbug.com/964075 is fixed
  private selectDefaultTreeNode(): void {
    const children = this.rootNode.children();
    if (children.length && !this.scriptsTree.selectedTreeElement) {
      children[0].treeNode().select(true /* omitFocus */, false /* selectedByUser */);
    }
  }

  private computeUniqueFileSystemProjectNames(): void {
    const fileSystemProjects = this.workspaceInternal.projectsForType(Workspace.Workspace.projectTypes.FileSystem);
    if (!fileSystemProjects.length) {
      return;
    }
    const encoder = new Persistence.Persistence.PathEncoder();
    const reversedPaths = fileSystemProjects.map(project => {
      const fileSystem = (project as Persistence.FileSystemWorkspaceBinding.FileSystem);
      return Platform.StringUtilities.reverse(encoder.encode(fileSystem.fileSystemPath()));
    });
    const reversedIndex = new Common.Trie.Trie();
    for (const reversedPath of reversedPaths) {
      reversedIndex.add(reversedPath);
    }

    for (let i = 0; i < fileSystemProjects.length; ++i) {
      const reversedPath = reversedPaths[i];
      const project = fileSystemProjects[i];
      reversedIndex.remove(reversedPath);
      const commonPrefix = reversedIndex.longestPrefix(reversedPath, false /* fullWordOnly */);
      reversedIndex.add(reversedPath);
      const prefixPath = reversedPath.substring(0, commonPrefix.length + 1);
      const path = encoder.decode(Platform.StringUtilities.reverse(prefixPath));

      const fileSystemNode = this.rootNode.child(project.id());
      if (fileSystemNode) {
        fileSystemNode.setTitle(path);
      }
    }
  }

  private removeProject(project: Workspace.Workspace.Project): void {
    const uiSourceCodes = project.uiSourceCodes();
    for (let i = 0; i < uiSourceCodes.length; ++i) {
      this.removeUISourceCode(uiSourceCodes[i]);
    }
    if (project.type() !== Workspace.Workspace.projectTypes.FileSystem) {
      return;
    }
    const fileSystemNode = this.rootNode.child(project.id());
    if (!fileSystemNode) {
      return;
    }
    this.rootNode.removeChild(fileSystemNode);
  }

  private folderNodeId(
      project: Workspace.Workspace.Project, target: SDK.Target.Target|null,
      frame: SDK.ResourceTreeModel.ResourceTreeFrame|null, projectOrigin: string, path: string): string {
    const targetId = target ? target.id() : '';
    const projectId = project.type() === Workspace.Workspace.projectTypes.FileSystem ? project.id() : '';
    const frameId = this.groupByFrame && frame ? frame.id : '';
    return targetId + ':' + projectId + ':' + frameId + ':' + projectOrigin + ':' + path;
  }

  private folderNode(
      uiSourceCode: Workspace.UISourceCode.UISourceCode, project: Workspace.Workspace.Project,
      target: SDK.Target.Target|null, frame: SDK.ResourceTreeModel.ResourceTreeFrame|null, projectOrigin: string,
      path: string[], fromSourceMap: boolean): NavigatorTreeNode {
    if (Snippets.ScriptSnippetFileSystem.isSnippetsUISourceCode(uiSourceCode)) {
      return this.rootNode;
    }

    if (target && !this.groupByFolder && !fromSourceMap) {
      return this.domainNode(uiSourceCode, project, target, frame, projectOrigin);
    }

    const folderPath = path.join('/');
    const folderId = this.folderNodeId(project, target, frame, projectOrigin, folderPath);
    let folderNode = this.subfolderNodes.get(folderId);
    if (folderNode) {
      return folderNode;
    }

    if (!path.length) {
      if (target) {
        return this.domainNode(uiSourceCode, project, target, frame, projectOrigin);
      }
      return /** @type {!NavigatorTreeNode} */ this.rootNode.child(project.id()) as NavigatorTreeNode;
    }

    const parentNode =
        this.folderNode(uiSourceCode, project, target, frame, projectOrigin, path.slice(0, -1), fromSourceMap);
    let type: string = fromSourceMap ? Types.SourceMapFolder : Types.NetworkFolder;
    if (project.type() === Workspace.Workspace.projectTypes.FileSystem) {
      type = Types.FileSystemFolder;
    }
    const name = path[path.length - 1];

    folderNode = new NavigatorFolderTreeNode(this, project, folderId, type, folderPath, name);
    this.subfolderNodes.set(folderId, folderNode);
    parentNode.appendChild(folderNode);
    return folderNode;
  }

  private domainNode(
      uiSourceCode: Workspace.UISourceCode.UISourceCode, project: Workspace.Workspace.Project,
      target: SDK.Target.Target, frame: SDK.ResourceTreeModel.ResourceTreeFrame|null,
      projectOrigin: string): NavigatorTreeNode {
    const frameNode = this.frameNode(project, target, frame);
    if (!this.groupByDomain) {
      return frameNode;
    }
    let domainNode = frameNode.child(projectOrigin);
    if (domainNode) {
      return domainNode;
    }

    domainNode = new NavigatorGroupTreeNode(
        this, project, projectOrigin, Types.Domain, this.computeProjectDisplayName(target, projectOrigin));
    if (frame && projectOrigin === Common.ParsedURL.ParsedURL.extractOrigin(frame.url)) {
      boostOrderForNode.add(domainNode.treeNode());
    }
    frameNode.appendChild(domainNode);
    return domainNode;
  }

  private frameNode(
      project: Workspace.Workspace.Project, target: SDK.Target.Target,
      frame: SDK.ResourceTreeModel.ResourceTreeFrame|null): NavigatorTreeNode {
    if (!this.groupByFrame || !frame) {
      return this.targetNode(project, target);
    }

    let frameNode = this.frameNodes.get(frame);
    if (frameNode) {
      return frameNode;
    }

    frameNode =
        new NavigatorGroupTreeNode(this, project, target.id() + ':' + frame.id, Types.Frame, frame.displayName());
    frameNode.setHoverCallback(hoverCallback);
    this.frameNodes.set(frame, frameNode);

    const parentFrame = frame.parentFrame();
    this.frameNode(project, parentFrame ? parentFrame.resourceTreeModel().target() : target, parentFrame)
        .appendChild(frameNode);
    if (!parentFrame) {
      boostOrderForNode.add(frameNode.treeNode());
      frameNode.treeNode().expand();
    }

    function hoverCallback(hovered: boolean): void {
      if (hovered) {
        const overlayModel = target.model(SDK.OverlayModel.OverlayModel);
        if (overlayModel && frame) {
          overlayModel.highlightFrame(frame.id);
        }
      } else {
        SDK.OverlayModel.OverlayModel.hideDOMNodeHighlight();
      }
    }
    return frameNode;
  }

  private targetNode(project: Workspace.Workspace.Project, target: SDK.Target.Target): NavigatorTreeNode {
    if (target === SDK.TargetManager.TargetManager.instance().mainTarget()) {
      return this.rootNode;
    }

    let targetNode = this.rootNode.child('target:' + target.id());
    if (!targetNode) {
      targetNode = new NavigatorGroupTreeNode(
          this, project, 'target:' + target.id(), target.type() === SDK.Target.Type.Frame ? Types.Frame : Types.Worker,
          target.name());
      this.rootNode.appendChild(targetNode);
    }
    return targetNode;
  }

  private computeProjectDisplayName(target: SDK.Target.Target, projectOrigin: string): string {
    const runtimeModel = target.model(SDK.RuntimeModel.RuntimeModel);
    const executionContexts = runtimeModel ? runtimeModel.executionContexts() : [];
    for (const context of executionContexts) {
      if (context.name && context.origin && projectOrigin.startsWith(context.origin)) {
        return context.name;
      }
    }

    if (!projectOrigin) {
      return i18nString(UIStrings.noDomain);
    }

    const parsedURL = new Common.ParsedURL.ParsedURL(projectOrigin);
    const prettyURL = parsedURL.isValid ? parsedURL.host + (parsedURL.port ? (':' + parsedURL.port) : '') : '';

    return (prettyURL || projectOrigin);
  }

  revealUISourceCode(uiSourceCode: Workspace.UISourceCode.UISourceCode, select?: boolean): NavigatorUISourceCodeTreeNode
      |null {
    const nodes = this.uiSourceCodeNodes.get(uiSourceCode);
    if (nodes.size === 0) {
      return null;
    }
    const node = nodes.values().next().value;
    if (!node) {
      return null;
    }
    if (this.scriptsTree.selectedTreeElement) {
      this.scriptsTree.selectedTreeElement.deselect();
    }
    this.lastSelectedUISourceCode = uiSourceCode;
    // TODO(dgozman): figure out revealing multiple.
    node.reveal(select);
    return node;
  }

  sourceSelected(uiSourceCode: Workspace.UISourceCode.UISourceCode, focusSource: boolean): void {
    this.lastSelectedUISourceCode = uiSourceCode;
    Common.Revealer.reveal(uiSourceCode, !focusSource);
  }

  private removeUISourceCode(uiSourceCode: Workspace.UISourceCode.UISourceCode): void {
    const nodes = this.uiSourceCodeNodes.get(uiSourceCode);
    for (const node of nodes) {
      this.removeUISourceCodeNode(node);
    }
  }

  private removeUISourceCodeNode(node: NavigatorUISourceCodeTreeNode): void {
    const uiSourceCode = node.uiSourceCode();
    this.uiSourceCodeNodes.delete(uiSourceCode, node);
    const project = uiSourceCode.project();
    const target = Bindings.NetworkProject.NetworkProject.targetForUISourceCode(uiSourceCode);
    const frame = node.frame();

    let parentNode: (NavigatorTreeNode|null) = node.parent;
    if (!parentNode) {
      return;
    }
    parentNode.removeChild(node);
    let currentNode: (NavigatorUISourceCodeTreeNode|null) = (parentNode as NavigatorUISourceCodeTreeNode | null);

    while (currentNode) {
      parentNode = currentNode.parent;
      if (!parentNode || !currentNode.isEmpty()) {
        break;
      }
      if (parentNode === this.rootNode && project.type() === Workspace.Workspace.projectTypes.FileSystem) {
        break;
      }
      if (!(currentNode instanceof NavigatorGroupTreeNode || currentNode instanceof NavigatorFolderTreeNode)) {
        break;
      }
      if (currentNode.type === Types.Frame) {
        this.discardFrame((frame as SDK.ResourceTreeModel.ResourceTreeFrame));
        break;
      }

      const folderId = this.folderNodeId(
          project, target, frame, uiSourceCode.origin(),
          currentNode instanceof NavigatorFolderTreeNode && currentNode.folderPath || '');
      this.subfolderNodes.delete(folderId);
      parentNode.removeChild(currentNode);
      currentNode = (parentNode as NavigatorUISourceCodeTreeNode | null);
    }
  }

  reset(): void {
    for (const node of this.uiSourceCodeNodes.valuesArray()) {
      node.dispose();
    }

    this.scriptsTree.removeChildren();
    this.scriptsTree.setFocusable(false);
    this.uiSourceCodeNodes.clear();
    this.subfolderNodes.clear();
    this.frameNodes.clear();
    this.rootNode.reset();
    // Reset the workspace to repopulate filesystem folders.
    this.resetWorkspace(Workspace.Workspace.WorkspaceImpl.instance());
  }

  handleContextMenu(_event: Event): void {
  }

  private async renameShortcut(): Promise<boolean> {
    const selectedTreeElement = (this.scriptsTree.selectedTreeElement as NavigatorSourceTreeElement | null);
    const node = selectedTreeElement && selectedTreeElement.node;
    if (!node || !node.uiSourceCode() || !node.uiSourceCode().canRename()) {
      return false;
    }
    this.rename(node, false);
    return true;
  }

  private handleContextMenuCreate(
      project: Workspace.Workspace.Project, path: string, uiSourceCode?: Workspace.UISourceCode.UISourceCode): void {
    if (uiSourceCode) {
      const relativePath = Persistence.FileSystemWorkspaceBinding.FileSystemWorkspaceBinding.relativePath(uiSourceCode);
      relativePath.pop();
      path = relativePath.join('/');
    }
    this.create(project, path, uiSourceCode);
  }

  private handleContextMenuRename(node: NavigatorUISourceCodeTreeNode): void {
    this.rename(node, false);
  }

  private async handleContextMenuExclude(project: Workspace.Workspace.Project, path: string): Promise<void> {
    const shouldExclude = await UI.UIUtils.ConfirmDialog.show(i18nString(UIStrings.areYouSureYouWantToExcludeThis));
    if (shouldExclude) {
      UI.UIUtils.startBatchUpdate();
      project.excludeFolder(
          Persistence.FileSystemWorkspaceBinding.FileSystemWorkspaceBinding.completeURL(project, path));
      UI.UIUtils.endBatchUpdate();
    }
  }

  private async handleContextMenuDelete(uiSourceCode: Workspace.UISourceCode.UISourceCode): Promise<void> {
    const shouldDelete = await UI.UIUtils.ConfirmDialog.show(i18nString(UIStrings.areYouSureYouWantToDeleteThis));
    if (shouldDelete) {
      uiSourceCode.project().deleteFile(uiSourceCode);
    }
  }

  handleFileContextMenu(event: Event, node: NavigatorUISourceCodeTreeNode): void {
    const uiSourceCode = node.uiSourceCode();
    const contextMenu = new UI.ContextMenu.ContextMenu(event);
    contextMenu.appendApplicableItems(uiSourceCode);

    const project = uiSourceCode.project();
    if (project.type() === Workspace.Workspace.projectTypes.FileSystem) {
      contextMenu.editSection().appendItem(i18nString(UIStrings.rename), this.handleContextMenuRename.bind(this, node));
      contextMenu.editSection().appendItem(
          i18nString(UIStrings.makeACopy), this.handleContextMenuCreate.bind(this, project, '', uiSourceCode));
      contextMenu.editSection().appendItem(
          i18nString(UIStrings.delete), this.handleContextMenuDelete.bind(this, uiSourceCode));
    }

    contextMenu.show();
  }

  private async handleDeleteOverrides(node: NavigatorTreeNode): Promise<void> {
    const shouldRemove = await UI.UIUtils.ConfirmDialog.show(i18nString(UIStrings.areYouSureYouWantToDeleteAll));
    if (shouldRemove) {
      this.handleDeleteOverridesHelper(node);
    }
  }

  private handleDeleteOverridesHelper(node: NavigatorTreeNode): void {
    node.children().forEach(child => {
      this.handleDeleteOverridesHelper(child);
    });
    if (node instanceof NavigatorUISourceCodeTreeNode) {
      // Only delete confirmed overrides and not just any file that happens to be in the folder.
      const binding = Persistence.Persistence.PersistenceImpl.instance().binding(node.uiSourceCode());
      if (binding) {
        node.uiSourceCode().project().deleteFile(node.uiSourceCode());
      }
    }
  }

  handleFolderContextMenu(event: Event, node: NavigatorTreeNode): void {
    const path = (node as NavigatorFolderTreeNode).folderPath || '';
    const project = (node as NavigatorFolderTreeNode).project || null;

    const contextMenu = new UI.ContextMenu.ContextMenu(event);
    NavigatorView.appendSearchItem(contextMenu, path);

    if (!project) {
      return;
    }

    if (project.type() === Workspace.Workspace.projectTypes.FileSystem) {
      // TODO(crbug.com/1253323): Cast to RawPathString will be removed when migration to branded types is complete.
      const folderPath = Common.ParsedURL.ParsedURL.capFilePrefix(
          Persistence.FileSystemWorkspaceBinding.FileSystemWorkspaceBinding.completeURL(project, path) as
              Platform.DevToolsPath.UrlString,
          Host.Platform.isWin());
      contextMenu.revealSection().appendItem(
          i18nString(UIStrings.openFolder),
          () => Host.InspectorFrontendHost.InspectorFrontendHostInstance.showItemInFolder(folderPath));
      if (project.canCreateFile()) {
        contextMenu.defaultSection().appendItem(i18nString(UIStrings.newFile), () => {
          this.handleContextMenuCreate(project, path, undefined);
        });
      }
    }

    if (project.canExcludeFolder(path)) {
      contextMenu.defaultSection().appendItem(
          i18nString(UIStrings.excludeFolder), this.handleContextMenuExclude.bind(this, project, path));
    }

    if (project.type() === Workspace.Workspace.projectTypes.FileSystem) {
      contextMenu.defaultSection().appendAction('sources.add-folder-to-workspace', undefined, true);
      if (node instanceof NavigatorGroupTreeNode) {
        contextMenu.defaultSection().appendItem(i18nString(UIStrings.removeFolderFromWorkspace), async () => {
          const shouldRemove = await UI.UIUtils.ConfirmDialog.show(i18nString(UIStrings.areYouSureYouWantToRemoveThis));
          if (shouldRemove) {
            project.remove();
          }
        });
      }
      if ((project as Persistence.FileSystemWorkspaceBinding.FileSystem).fileSystem().type() === 'overrides') {
        contextMenu.defaultSection().appendItem(
            i18nString(UIStrings.deleteAllOverrides), this.handleDeleteOverrides.bind(this, node));
      }
    }

    contextMenu.show();
  }

  rename(node: NavigatorUISourceCodeTreeNode, creatingNewUISourceCode: boolean): void {
    const uiSourceCode = node.uiSourceCode();
    node.rename(callback.bind(this));

    function callback(this: NavigatorView, committed: boolean): void {
      if (!creatingNewUISourceCode) {
        return;
      }
      if (!committed) {
        uiSourceCode.remove();
      } else if (node.treeElement && node.treeElement.listItemElement.hasFocus()) {
        this.sourceSelected(uiSourceCode, true);
      }
    }
  }

  async create(
      project: Workspace.Workspace.Project, path: string,
      uiSourceCodeToCopy?: Workspace.UISourceCode.UISourceCode): Promise<void> {
    let content = '';
    if (uiSourceCodeToCopy) {
      content = (await uiSourceCodeToCopy.requestContent()).content || '';
    }
    const uiSourceCode = await project.createFile(path, null, content);
    if (!uiSourceCode) {
      return;
    }
    this.sourceSelected(uiSourceCode, false);
    const node = this.revealUISourceCode(uiSourceCode, true);
    if (node) {
      this.rename(node, true);
    }
  }

  private groupingChanged(): void {
    this.reset();
    this.initGrouping();
    this.workspaceInternal.uiSourceCodes().forEach(this.addUISourceCode.bind(this));
  }

  private initGrouping(): void {
    this.groupByFrame = true;
    this.groupByDomain = this.navigatorGroupByFolderSetting.get();
    this.groupByFolder = this.groupByDomain;
  }

  private resetForTest(): void {
    this.reset();
    this.workspaceInternal.uiSourceCodes().forEach(this.addUISourceCode.bind(this));
  }

  private discardFrame(frame: SDK.ResourceTreeModel.ResourceTreeFrame): void {
    const node = this.frameNodes.get(frame);
    if (!node) {
      return;
    }

    if (node.parent) {
      node.parent.removeChild(node);
    }
    this.frameNodes.delete(frame);
    for (const child of frame.childFrames) {
      this.discardFrame(child);
    }
  }

  targetAdded(_target: SDK.Target.Target): void {
  }

  targetRemoved(target: SDK.Target.Target): void {
    const targetNode = this.rootNode.child('target:' + target.id());
    if (targetNode) {
      this.rootNode.removeChild(targetNode);
    }
  }

  private targetNameChanged(event: Common.EventTarget.EventTargetEvent<SDK.Target.Target>): void {
    const target = event.data;
    const targetNode = this.rootNode.child('target:' + target.id());
    if (targetNode) {
      targetNode.setTitle(target.name());
    }
  }
  wasShown(): void {
    super.wasShown();
    this.scriptsTree.registerCSSFiles([navigatorTreeStyles]);
    this.registerCSSFiles([navigatorViewStyles]);
  }
}

const boostOrderForNode = new WeakSet<UI.TreeOutline.TreeElement>();

export class NavigatorFolderTreeElement extends UI.TreeOutline.TreeElement {
  private readonly nodeType: string;
  private readonly navigatorView: NavigatorView;
  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration)
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private hoverCallback: ((arg0: boolean) => any)|undefined;
  node!: NavigatorTreeNode;
  private hovered?: boolean;

  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration)
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  constructor(navigatorView: NavigatorView, type: string, title: string, hoverCallback?: ((arg0: boolean) => any)) {
    super('', true);
    this.listItemElement.classList.add('navigator-' + type + '-tree-item', 'navigator-folder-tree-item');
    UI.ARIAUtils.setAccessibleName(this.listItemElement, `${title}, ${type}`);
    this.nodeType = type;
    this.title = title;
    this.tooltip = title;
    this.navigatorView = navigatorView;
    this.hoverCallback = hoverCallback;
    let iconType = 'largeicon-navigator-folder';
    if (type === Types.Domain) {
      iconType = 'largeicon-navigator-domain';
    } else if (type === Types.Frame) {
      iconType = 'largeicon-navigator-frame';
    } else if (type === Types.Worker) {
      iconType = 'largeicon-navigator-worker';
    }
    this.setLeadingIcons([UI.Icon.Icon.create(iconType, 'icon')]);
  }

  async onpopulate(): Promise<void> {
    this.node.populate();
  }

  onattach(): void {
    this.collapse();
    this.node.onattach();
    this.listItemElement.addEventListener('contextmenu', this.handleContextMenuEvent.bind(this), false);
    this.listItemElement.addEventListener('mousemove', this.mouseMove.bind(this), false);
    this.listItemElement.addEventListener('mouseleave', this.mouseLeave.bind(this), false);
  }

  setNode(node: NavigatorTreeNode): void {
    this.node = node;
    const paths = [];
    let currentNode: NavigatorTreeNode|null = node;
    while (currentNode && !currentNode.isRoot()) {
      paths.push(currentNode.title);
      currentNode = currentNode.parent;
    }
    paths.reverse();
    this.tooltip = paths.join('/');
    UI.ARIAUtils.setAccessibleName(this.listItemElement, `${this.title}, ${this.nodeType}`);
  }

  private handleContextMenuEvent(event: Event): void {
    if (!this.node) {
      return;
    }
    this.select();
    this.navigatorView.handleFolderContextMenu(event, this.node);
  }

  private mouseMove(_event: Event): void {
    if (this.hovered || !this.hoverCallback) {
      return;
    }
    this.hovered = true;
    this.hoverCallback(true);
  }

  private mouseLeave(_event: Event): void {
    if (!this.hoverCallback) {
      return;
    }
    this.hovered = false;
    this.hoverCallback(false);
  }
}

export class NavigatorSourceTreeElement extends UI.TreeOutline.TreeElement {
  readonly nodeType: string;
  readonly node: NavigatorUISourceCodeTreeNode;
  private readonly navigatorView: NavigatorView;
  uiSourceCodeInternal: Workspace.UISourceCode.UISourceCode;

  constructor(
      navigatorView: NavigatorView, uiSourceCode: Workspace.UISourceCode.UISourceCode, title: string,
      node: NavigatorUISourceCodeTreeNode) {
    super('', false);
    this.nodeType = Types.File;
    this.node = node;
    this.title = title;
    this.listItemElement.classList.add(
        'navigator-' + uiSourceCode.contentType().name() + '-tree-item', 'navigator-file-tree-item');
    this.tooltip = uiSourceCode.url();
    UI.ARIAUtils.setAccessibleName(this.listItemElement, `${uiSourceCode.name()}, ${this.nodeType}`);
    Common.EventTarget.fireEvent('source-tree-file-added', uiSourceCode.fullDisplayName());
    this.navigatorView = navigatorView;
    this.uiSourceCodeInternal = uiSourceCode;
    this.updateIcon();
  }

  updateIcon(): void {
    const binding = Persistence.Persistence.PersistenceImpl.instance().binding(this.uiSourceCodeInternal);
    if (binding) {
      const container = document.createElement('span');
      container.classList.add('icon-stack');
      let iconType = 'largeicon-navigator-file-sync';
      if (Snippets.ScriptSnippetFileSystem.isSnippetsUISourceCode(binding.fileSystem)) {
        iconType = 'largeicon-navigator-snippet';
      }
      const icon = UI.Icon.Icon.create(iconType, 'icon');
      const badge = UI.Icon.Icon.create('badge-navigator-file-sync', 'icon-badge');
      // TODO(allada) This does not play well with dark theme. Add an actual icon and use it.
      if (Persistence.NetworkPersistenceManager.NetworkPersistenceManager.instance().project() ===
          binding.fileSystem.project()) {
        badge.style.filter = 'hue-rotate(160deg)';
      }
      container.appendChild(icon);
      container.appendChild(badge);
      UI.Tooltip.Tooltip.install(
          container, Persistence.PersistenceUtils.PersistenceUtils.tooltipForUISourceCode(this.uiSourceCodeInternal));
      this.setLeadingIcons([(container as UI.Icon.Icon)]);
    } else {
      let iconType = 'largeicon-navigator-file';
      if (Snippets.ScriptSnippetFileSystem.isSnippetsUISourceCode(this.uiSourceCodeInternal)) {
        iconType = 'largeicon-navigator-snippet';
      }
      const defaultIcon = UI.Icon.Icon.create(iconType, 'icon');
      this.setLeadingIcons([defaultIcon]);
    }
  }

  get uiSourceCode(): Workspace.UISourceCode.UISourceCode {
    return this.uiSourceCodeInternal;
  }

  onattach(): void {
    this.listItemElement.draggable = true;
    this.listItemElement.addEventListener('click', this.onclick.bind(this), false);
    this.listItemElement.addEventListener('contextmenu', this.handleContextMenuEvent.bind(this), false);
    this.listItemElement.addEventListener('dragstart', this.ondragstart.bind(this), false);
  }

  private shouldRenameOnMouseDown(): boolean {
    if (!this.uiSourceCodeInternal.canRename()) {
      return false;
    }
    if (!this.treeOutline) {
      return false;
    }
    const isSelected = this === this.treeOutline.selectedTreeElement;
    return isSelected && this.treeOutline.element.hasFocus() && !UI.UIUtils.isBeingEdited(this.treeOutline.element);
  }

  selectOnMouseDown(event: MouseEvent): void {
    if (event.which !== 1 || !this.shouldRenameOnMouseDown()) {
      super.selectOnMouseDown(event);
      return;
    }
    setTimeout(rename.bind(this), 300);

    function rename(this: NavigatorSourceTreeElement): void {
      if (this.shouldRenameOnMouseDown()) {
        this.navigatorView.rename(this.node, false);
      }
    }
  }

  private ondragstart(event: DragEvent): void {
    if (!event.dataTransfer) {
      return;
    }
    event.dataTransfer.setData('text/plain', this.uiSourceCodeInternal.url());
    event.dataTransfer.effectAllowed = 'copy';
  }

  onspace(): boolean {
    this.navigatorView.sourceSelected(this.uiSourceCode, true);
    return true;
  }

  private onclick(_event: Event): void {
    this.navigatorView.sourceSelected(this.uiSourceCode, false);
  }

  ondblclick(event: Event): boolean {
    const middleClick = (event as MouseEvent).button === 1;
    this.navigatorView.sourceSelected(this.uiSourceCode, !middleClick);
    return false;
  }

  onenter(): boolean {
    this.navigatorView.sourceSelected(this.uiSourceCode, true);
    return true;
  }

  ondelete(): boolean {
    return true;
  }

  private handleContextMenuEvent(event: Event): void {
    this.select();
    this.navigatorView.handleFileContextMenu(event, this.node);
  }
}

export class NavigatorTreeNode {
  id: string;
  protected navigatorView: NavigatorView;
  type: string;
  childrenInternal: Map<string, NavigatorTreeNode>;
  private populated: boolean;
  isMerged: boolean;
  parent!: NavigatorTreeNode|null;
  title!: string;

  constructor(navigatorView: NavigatorView, id: string, type: string) {
    this.id = id;
    this.navigatorView = navigatorView;
    this.type = type;
    this.childrenInternal = new Map();

    this.populated = false;
    this.isMerged = false;
  }

  treeNode(): UI.TreeOutline.TreeElement {
    throw 'Not implemented';
  }

  dispose(): void {
  }

  updateTitle(): void {
  }

  isRoot(): boolean {
    return false;
  }

  hasChildren(): boolean {
    return true;
  }

  onattach(): void {
  }

  setTitle(_title: string): void {
    throw 'Not implemented';
  }

  populate(): void {
    if (this.isPopulated()) {
      return;
    }
    if (this.parent) {
      this.parent.populate();
    }
    this.populated = true;
    this.wasPopulated();
  }

  wasPopulated(): void {
    const children = this.children();
    for (let i = 0; i < children.length; ++i) {
      this.navigatorView.appendChild(this.treeNode(), (children[i].treeNode() as UI.TreeOutline.TreeElement));
    }
  }

  didAddChild(node: NavigatorTreeNode): void {
    if (this.isPopulated()) {
      this.navigatorView.appendChild(this.treeNode(), (node.treeNode() as UI.TreeOutline.TreeElement));
    }
  }

  willRemoveChild(node: NavigatorTreeNode): void {
    if (this.isPopulated()) {
      this.navigatorView.removeChild(this.treeNode(), (node.treeNode() as UI.TreeOutline.TreeElement));
    }
  }

  isPopulated(): boolean {
    return this.populated;
  }

  isEmpty(): boolean {
    return !this.childrenInternal.size;
  }

  children(): NavigatorTreeNode[] {
    return [...this.childrenInternal.values()];
  }

  child(id: string): NavigatorTreeNode|null {
    return this.childrenInternal.get(id) || null;
  }

  appendChild(node: NavigatorTreeNode): void {
    this.childrenInternal.set(node.id, node);
    node.parent = this;
    this.didAddChild(node);
  }

  removeChild(node: NavigatorTreeNode): void {
    this.willRemoveChild(node);
    this.childrenInternal.delete(node.id);
    node.parent = null;
    node.dispose();
  }

  reset(): void {
    this.childrenInternal.clear();
  }
}

export class NavigatorRootTreeNode extends NavigatorTreeNode {
  constructor(navigatorView: NavigatorView) {
    super(navigatorView, '', Types.Root);
  }

  isRoot(): boolean {
    return true;
  }

  treeNode(): UI.TreeOutline.TreeElement {
    return this.navigatorView.scriptsTree.rootElement();
  }
}

export class NavigatorUISourceCodeTreeNode extends NavigatorTreeNode {
  uiSourceCodeInternal: Workspace.UISourceCode.UISourceCode;
  treeElement: NavigatorSourceTreeElement|null;
  private eventListeners: Common.EventTarget.EventDescriptor[];
  private readonly frameInternal: SDK.ResourceTreeModel.ResourceTreeFrame|null;
  constructor(
      navigatorView: NavigatorView, uiSourceCode: Workspace.UISourceCode.UISourceCode,
      frame: SDK.ResourceTreeModel.ResourceTreeFrame|null) {
    super(navigatorView, uiSourceCode.project().id() + ':' + uiSourceCode.url(), Types.File);
    this.uiSourceCodeInternal = uiSourceCode;
    this.treeElement = null;
    this.eventListeners = [];
    this.frameInternal = frame;
  }

  frame(): SDK.ResourceTreeModel.ResourceTreeFrame|null {
    return this.frameInternal;
  }

  uiSourceCode(): Workspace.UISourceCode.UISourceCode {
    return this.uiSourceCodeInternal;
  }

  treeNode(): UI.TreeOutline.TreeElement {
    if (this.treeElement) {
      return this.treeElement;
    }

    this.treeElement = new NavigatorSourceTreeElement(this.navigatorView, this.uiSourceCodeInternal, '', this);
    this.updateTitle();

    const updateTitleBound = this.updateTitle.bind(this, undefined);
    this.eventListeners = [
      this.uiSourceCodeInternal.addEventListener(Workspace.UISourceCode.Events.TitleChanged, updateTitleBound),
      this.uiSourceCodeInternal.addEventListener(Workspace.UISourceCode.Events.WorkingCopyChanged, updateTitleBound),
      this.uiSourceCodeInternal.addEventListener(Workspace.UISourceCode.Events.WorkingCopyCommitted, updateTitleBound),
    ];
    return this.treeElement;
  }

  updateTitle(ignoreIsDirty?: boolean): void {
    if (!this.treeElement) {
      return;
    }

    let titleText = this.uiSourceCodeInternal.displayName();
    if (!ignoreIsDirty && this.uiSourceCodeInternal.isDirty()) {
      titleText = '*' + titleText;
    }

    this.treeElement.title = titleText;
    this.treeElement.updateIcon();

    let tooltip = this.uiSourceCodeInternal.url();
    if (this.uiSourceCodeInternal.contentType().isFromSourceMap()) {
      tooltip = i18nString(UIStrings.sFromSourceMap, {PH1: this.uiSourceCodeInternal.displayName()});
    }
    this.treeElement.tooltip = tooltip;
  }

  hasChildren(): boolean {
    return false;
  }

  dispose(): void {
    Common.EventTarget.removeEventListeners(this.eventListeners);
  }

  reveal(select?: boolean): void {
    if (this.parent) {
      this.parent.populate();
      this.parent.treeNode().expand();
    }
    if (this.treeElement) {
      this.treeElement.reveal(true);
      if (select) {
        this.treeElement.select(true);
      }
    }
  }

  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration)
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  rename(callback?: ((arg0: boolean) => any)): void {
    if (!this.treeElement) {
      return;
    }

    this.treeElement.listItemElement.focus();

    if (!this.treeElement.treeOutline) {
      return;
    }

    // Tree outline should be marked as edited as well as the tree element to prevent search from starting.
    const treeOutlineElement = this.treeElement.treeOutline.element;
    UI.UIUtils.markBeingEdited(treeOutlineElement, true);

    function commitHandler(
        this: NavigatorUISourceCodeTreeNode, element: Element, newTitle: string, oldTitle: string): void {
      if (newTitle !== oldTitle) {
        if (this.treeElement) {
          this.treeElement.title = newTitle;
        }
        this.uiSourceCodeInternal.rename(newTitle).then(renameCallback.bind(this));
        return;
      }
      afterEditing.call(this, true);
    }

    function renameCallback(this: NavigatorUISourceCodeTreeNode, success: boolean): void {
      if (!success) {
        UI.UIUtils.markBeingEdited(treeOutlineElement, false);
        this.updateTitle();
        this.rename(callback);
        return;
      }
      if (this.treeElement) {
        const {parent} = this.treeElement;
        if (parent) {
          parent.removeChild(this.treeElement);
          parent.appendChild(this.treeElement);
          this.treeElement.select();
        }
      }
      afterEditing.call(this, true);
    }

    function afterEditing(this: NavigatorUISourceCodeTreeNode, committed: boolean): void {
      UI.UIUtils.markBeingEdited(treeOutlineElement, false);
      this.updateTitle();
      if (callback) {
        callback(committed);
      }
    }

    this.updateTitle(true);
    this.treeElement.startEditingTitle(
        new UI.InplaceEditor.Config(commitHandler.bind(this), afterEditing.bind(this, false)));
  }
}

export class NavigatorFolderTreeNode extends NavigatorTreeNode {
  project: Workspace.Workspace.Project|null;
  readonly folderPath: string;
  title: string;
  treeElement!: NavigatorFolderTreeElement|null;
  constructor(
      navigatorView: NavigatorView, project: Workspace.Workspace.Project|null, id: string, type: string,
      folderPath: string, title: string) {
    super(navigatorView, id, type);
    this.project = project;
    this.folderPath = folderPath;
    this.title = title;
  }

  treeNode(): UI.TreeOutline.TreeElement {
    if (this.treeElement) {
      return this.treeElement;
    }
    this.treeElement = this.createTreeElement(this.title, this);
    this.updateTitle();
    return this.treeElement;
  }

  updateTitle(): void {
    if (!this.treeElement || !this.project || this.project.type() !== Workspace.Workspace.projectTypes.FileSystem) {
      return;
    }
    const absoluteFileSystemPath =
        Persistence.FileSystemWorkspaceBinding.FileSystemWorkspaceBinding.fileSystemPath(this.project.id()) + '/' +
        this.folderPath;
    const hasMappedFiles =
        Persistence.Persistence.PersistenceImpl.instance().filePathHasBindings(absoluteFileSystemPath);
    this.treeElement.listItemElement.classList.toggle('has-mapped-files', hasMappedFiles);
  }

  private createTreeElement(title: string, node: NavigatorTreeNode): NavigatorFolderTreeElement {
    if (this.project && this.project.type() !== Workspace.Workspace.projectTypes.FileSystem) {
      try {
        title = decodeURI(title);
      } catch (e) {
      }
    }
    const treeElement = new NavigatorFolderTreeElement(this.navigatorView, this.type, title);
    treeElement.setNode(node);
    return treeElement;
  }

  wasPopulated(): void {
    // @ts-ignore These types are invalid, but removing this check causes wrong behavior
    if (!this.treeElement || this.treeElement.node !== this) {
      return;
    }
    this.addChildrenRecursive();
  }

  private addChildrenRecursive(): void {
    const children = this.children();
    for (let i = 0; i < children.length; ++i) {
      const child = children[i];
      this.didAddChild(child);
      if (child instanceof NavigatorFolderTreeNode) {
        child.addChildrenRecursive();
      }
    }
  }

  private shouldMerge(node: NavigatorTreeNode): boolean {
    return this.type !== Types.Domain && node instanceof NavigatorFolderTreeNode;
  }

  didAddChild(node: NavigatorTreeNode): void {
    if (!this.treeElement) {
      return;
    }

    let children = this.children();

    if (children.length === 1 && this.shouldMerge(node)) {
      node.isMerged = true;
      this.treeElement.title = this.treeElement.title + '/' + node.title;
      (node as NavigatorFolderTreeNode).treeElement = this.treeElement;
      this.treeElement.setNode(node);
      return;
    }

    let oldNode;
    if (children.length === 2) {
      oldNode = children[0] !== node ? children[0] : children[1];
    }
    if (oldNode && oldNode.isMerged) {
      oldNode.isMerged = false;
      const mergedToNodes = [];
      mergedToNodes.push(this);
      let treeNode: (NavigatorTreeNode|null)|NavigatorTreeNode|this = this;
      while (treeNode && treeNode.isMerged) {
        treeNode = treeNode.parent;
        if (treeNode) {
          mergedToNodes.push(treeNode);
        }
      }
      mergedToNodes.reverse();
      const titleText = mergedToNodes.map(node => node.title).join('/');

      const nodes = [];
      treeNode = oldNode;
      do {
        nodes.push(treeNode);
        children = treeNode.children();
        treeNode = children.length === 1 ? children[0] : null;
      } while (treeNode && treeNode.isMerged);

      if (!this.isPopulated()) {
        this.treeElement.title = titleText;
        this.treeElement.setNode(this);
        for (let i = 0; i < nodes.length; ++i) {
          (nodes[i] as NavigatorFolderTreeNode).treeElement = null;
          nodes[i].isMerged = false;
        }
        return;
      }
      const oldTreeElement = this.treeElement;
      const treeElement = this.createTreeElement(titleText, this);
      for (let i = 0; i < mergedToNodes.length; ++i) {
        (mergedToNodes[i] as NavigatorFolderTreeNode).treeElement = treeElement;
      }
      if (oldTreeElement.parent) {
        this.navigatorView.appendChild(oldTreeElement.parent, treeElement);
      }

      oldTreeElement.setNode(nodes[nodes.length - 1]);
      oldTreeElement.title = nodes.map(node => node.title).join('/');
      if (oldTreeElement.parent) {
        this.navigatorView.removeChild(oldTreeElement.parent, oldTreeElement);
      }
      this.navigatorView.appendChild(this.treeElement, oldTreeElement);
      if (oldTreeElement.expanded) {
        treeElement.expand();
      }
    }
    if (this.isPopulated()) {
      this.navigatorView.appendChild(this.treeElement, node.treeNode());
    }
  }

  willRemoveChild(node: NavigatorTreeNode): void {
    const actualNode = (node as NavigatorFolderTreeNode);
    if (actualNode.isMerged || !this.isPopulated() || !this.treeElement || !actualNode.treeElement) {
      return;
    }
    this.navigatorView.removeChild(this.treeElement, actualNode.treeElement);
  }
}

export class NavigatorGroupTreeNode extends NavigatorTreeNode {
  private readonly project: Workspace.Workspace.Project;
  title: string;
  private hoverCallback?: ((arg0: boolean) => void);
  private treeElement?: NavigatorFolderTreeElement;
  constructor(
      navigatorView: NavigatorView, project: Workspace.Workspace.Project, id: string, type: string, title: string) {
    super(navigatorView, id, type);
    this.project = project;
    this.title = title;
    this.populate();
  }

  setHoverCallback(hoverCallback: (arg0: boolean) => void): void {
    this.hoverCallback = hoverCallback;
  }

  treeNode(): UI.TreeOutline.TreeElement {
    if (this.treeElement) {
      return this.treeElement;
    }
    this.treeElement = new NavigatorFolderTreeElement(this.navigatorView, this.type, this.title, this.hoverCallback);
    this.treeElement.setNode(this);
    return this.treeElement;
  }

  onattach(): void {
    this.updateTitle();
  }

  updateTitle(): void {
    if (!this.treeElement || this.project.type() !== Workspace.Workspace.projectTypes.FileSystem) {
      return;
    }
    const fileSystemPath =
        Persistence.FileSystemWorkspaceBinding.FileSystemWorkspaceBinding.fileSystemPath(this.project.id());
    const wasActive = this.treeElement.listItemElement.classList.contains('has-mapped-files');
    const isActive = Persistence.Persistence.PersistenceImpl.instance().filePathHasBindings(fileSystemPath);
    if (wasActive === isActive) {
      return;
    }
    this.treeElement.listItemElement.classList.toggle('has-mapped-files', isActive);
    if (this.treeElement.childrenListElement.hasFocus()) {
      return;
    }
    if (isActive) {
      this.treeElement.expand();
    } else {
      this.treeElement.collapse();
    }
  }

  setTitle(title: string): void {
    this.title = title;
    if (this.treeElement) {
      this.treeElement.title = this.title;
    }
  }
}
