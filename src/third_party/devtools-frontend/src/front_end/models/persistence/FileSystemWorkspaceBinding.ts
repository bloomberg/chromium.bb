/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
import * as Platform from '../../core/platform/platform.js';
import * as TextUtils from '../text_utils/text_utils.js';
import * as Workspace from '../workspace/workspace.js';

import type {IsolatedFileSystem} from './IsolatedFileSystem.js';
import type {IsolatedFileSystemManager} from './IsolatedFileSystemManager.js';
import {Events} from './IsolatedFileSystemManager.js';
import type {PlatformFileSystem} from './PlatformFileSystem.js';

export class FileSystemWorkspaceBinding {
  readonly isolatedFileSystemManager: IsolatedFileSystemManager;
  private readonly workspace: Workspace.Workspace.WorkspaceImpl;
  private readonly eventListeners: Common.EventTarget.EventDescriptor[];
  private readonly boundFileSystems: Map<string, FileSystem>;
  constructor(isolatedFileSystemManager: IsolatedFileSystemManager, workspace: Workspace.Workspace.WorkspaceImpl) {
    this.isolatedFileSystemManager = isolatedFileSystemManager;
    this.workspace = workspace;
    this.eventListeners = [
      this.isolatedFileSystemManager.addEventListener(Events.FileSystemAdded, this.onFileSystemAdded, this),
      this.isolatedFileSystemManager.addEventListener(Events.FileSystemRemoved, this.onFileSystemRemoved, this),
      this.isolatedFileSystemManager.addEventListener(Events.FileSystemFilesChanged, this.fileSystemFilesChanged, this),
    ];
    this.boundFileSystems = new Map();
    this.isolatedFileSystemManager.waitForFileSystems().then(this.onFileSystemsLoaded.bind(this));
  }

  static projectId(fileSystemPath: string): string {
    return fileSystemPath;
  }

  static relativePath(uiSourceCode: Workspace.UISourceCode.UISourceCode): string[] {
    const baseURL = (uiSourceCode.project() as FileSystem).fileSystemBaseURL;
    return uiSourceCode.url().substring(baseURL.length).split('/');
  }

  static tooltipForUISourceCode(uiSourceCode: Workspace.UISourceCode.UISourceCode): string {
    const fileSystem = (uiSourceCode.project() as FileSystem).fileSystemInternal;
    return fileSystem.tooltipForURL(uiSourceCode.url());
  }

  static fileSystemType(project: Workspace.Workspace.Project): string {
    const fileSystem = (project as FileSystem).fileSystemInternal;
    return fileSystem.type();
  }

  static fileSystemSupportsAutomapping(project: Workspace.Workspace.Project): boolean {
    const fileSystem = (project as FileSystem).fileSystemInternal;
    return fileSystem.supportsAutomapping();
  }

  static completeURL(project: Workspace.Workspace.Project, relativePath: string): string {
    const fsProject = project as FileSystem;
    return fsProject.fileSystemBaseURL + relativePath;
  }

  static fileSystemPath(projectId: string): string {
    return projectId;
  }

  fileSystemManager(): IsolatedFileSystemManager {
    return this.isolatedFileSystemManager;
  }

  private onFileSystemsLoaded(fileSystems: IsolatedFileSystem[]): void {
    for (const fileSystem of fileSystems) {
      this.addFileSystem(fileSystem);
    }
  }

  private onFileSystemAdded(event: Common.EventTarget.EventTargetEvent<PlatformFileSystem>): void {
    const fileSystem = event.data;
    this.addFileSystem(fileSystem);
  }

  private addFileSystem(fileSystem: PlatformFileSystem): void {
    const boundFileSystem = new FileSystem(this, fileSystem, this.workspace);
    this.boundFileSystems.set(fileSystem.path(), boundFileSystem);
  }

  private onFileSystemRemoved(event: Common.EventTarget.EventTargetEvent<PlatformFileSystem>): void {
    const fileSystem = event.data;
    const boundFileSystem = this.boundFileSystems.get(fileSystem.path());
    if (boundFileSystem) {
      boundFileSystem.dispose();
    }
    this.boundFileSystems.delete(fileSystem.path());
  }

  private fileSystemFilesChanged(event: Common.EventTarget.EventTargetEvent<FilesChangedData>): void {
    const paths = event.data;
    for (const fileSystemPath of paths.changed.keysArray()) {
      const fileSystem = this.boundFileSystems.get(fileSystemPath);
      if (!fileSystem) {
        continue;
      }
      paths.changed.get(fileSystemPath).forEach(path => fileSystem.fileChanged(path));
    }

    for (const fileSystemPath of paths.added.keysArray()) {
      const fileSystem = this.boundFileSystems.get(fileSystemPath);
      if (!fileSystem) {
        continue;
      }
      paths.added.get(fileSystemPath).forEach(path => fileSystem.fileChanged(path));
    }

    for (const fileSystemPath of paths.removed.keysArray()) {
      const fileSystem = this.boundFileSystems.get(fileSystemPath);
      if (!fileSystem) {
        continue;
      }
      paths.removed.get(fileSystemPath).forEach(path => fileSystem.removeUISourceCode(path));
    }
  }

  dispose(): void {
    Common.EventTarget.removeEventListeners(this.eventListeners);
    for (const fileSystem of this.boundFileSystems.values()) {
      fileSystem.dispose();
      this.boundFileSystems.delete(fileSystem.fileSystemInternal.path());
    }
  }
}

export class FileSystem extends Workspace.Workspace.ProjectStore implements Workspace.Workspace.Project {
  readonly fileSystemInternal: PlatformFileSystem;
  readonly fileSystemBaseURL: string;
  private readonly fileSystemParentURL: string;
  private readonly fileSystemWorkspaceBinding: FileSystemWorkspaceBinding;
  private readonly fileSystemPathInternal: string;
  private readonly creatingFilesGuard: Set<string>;
  constructor(
      fileSystemWorkspaceBinding: FileSystemWorkspaceBinding, isolatedFileSystem: PlatformFileSystem,
      workspace: Workspace.Workspace.WorkspaceImpl) {
    const fileSystemPath = isolatedFileSystem.path();
    const id = FileSystemWorkspaceBinding.projectId(fileSystemPath);
    console.assert(!workspace.project(id));
    const displayName = fileSystemPath.substr(fileSystemPath.lastIndexOf('/') + 1);

    super(workspace, id, Workspace.Workspace.projectTypes.FileSystem, displayName);

    this.fileSystemInternal = isolatedFileSystem;
    this.fileSystemBaseURL = this.fileSystemInternal.path() + '/';
    this.fileSystemParentURL = this.fileSystemBaseURL.substr(0, fileSystemPath.lastIndexOf('/') + 1);
    this.fileSystemWorkspaceBinding = fileSystemWorkspaceBinding;
    this.fileSystemPathInternal = fileSystemPath;
    this.creatingFilesGuard = new Set();

    workspace.addProject(this);
    this.populate();
  }

  fileSystemPath(): string {
    return this.fileSystemPathInternal;
  }

  fileSystem(): PlatformFileSystem {
    return this.fileSystemInternal;
  }

  mimeType(uiSourceCode: Workspace.UISourceCode.UISourceCode): string {
    return this.fileSystemInternal.mimeFromPath(uiSourceCode.url());
  }

  initialGitFolders(): string[] {
    return this.fileSystemInternal.initialGitFolders().map(folder => this.fileSystemPathInternal + '/' + folder);
  }

  private filePathForUISourceCode(uiSourceCode: Workspace.UISourceCode.UISourceCode): string {
    return uiSourceCode.url().substring(this.fileSystemPathInternal.length);
  }

  isServiceProject(): boolean {
    return false;
  }

  requestMetadata(uiSourceCode: Workspace.UISourceCode.UISourceCode):
      Promise<Workspace.UISourceCode.UISourceCodeMetadata|null> {
    const metadata = sourceCodeToMetadataMap.get(uiSourceCode);
    if (metadata) {
      return metadata;
    }
    const relativePath = this.filePathForUISourceCode(uiSourceCode);
    const promise = this.fileSystemInternal.getMetadata(relativePath).then(onMetadata);
    sourceCodeToMetadataMap.set(uiSourceCode, promise);
    return promise;

    function onMetadata(metadata: {modificationTime: Date, size: number}|
                        null): Workspace.UISourceCode.UISourceCodeMetadata|null {
      if (!metadata) {
        return null;
      }
      return new Workspace.UISourceCode.UISourceCodeMetadata(metadata.modificationTime, metadata.size);
    }
  }

  requestFileBlob(uiSourceCode: Workspace.UISourceCode.UISourceCode): Promise<Blob|null> {
    return this.fileSystemInternal.requestFileBlob(this.filePathForUISourceCode(uiSourceCode));
  }

  requestFileContent(uiSourceCode: Workspace.UISourceCode.UISourceCode):
      Promise<TextUtils.ContentProvider.DeferredContent> {
    const filePath = this.filePathForUISourceCode(uiSourceCode);
    return this.fileSystemInternal.requestFileContent(filePath);
  }

  canSetFileContent(): boolean {
    return true;
  }

  async setFileContent(uiSourceCode: Workspace.UISourceCode.UISourceCode, newContent: string, isBase64: boolean):
      Promise<void> {
    const filePath = this.filePathForUISourceCode(uiSourceCode);
    await this.fileSystemInternal.setFileContent(filePath, newContent, isBase64);
  }

  fullDisplayName(uiSourceCode: Workspace.UISourceCode.UISourceCode): string {
    const baseURL = (uiSourceCode.project() as FileSystem).fileSystemParentURL;
    return uiSourceCode.url().substring(baseURL.length);
  }

  canRename(): boolean {
    return true;
  }

  rename(
      uiSourceCode: Workspace.UISourceCode.UISourceCode, newName: string,
      callback:
          (arg0: boolean, arg1?: string|undefined, arg2?: string|undefined,
           arg3?: Common.ResourceType.ResourceType|undefined) => void): void {
    if (newName === uiSourceCode.name()) {
      callback(true, uiSourceCode.name(), uiSourceCode.url(), uiSourceCode.contentType());
      return;
    }

    let filePath = this.filePathForUISourceCode(uiSourceCode);
    this.fileSystemInternal.renameFile(filePath, newName, innerCallback.bind(this));

    function innerCallback(this: FileSystem, success: boolean, newName?: string): void {
      if (!success || !newName) {
        callback(false, newName);
        return;
      }
      console.assert(Boolean(newName));
      const slash = filePath.lastIndexOf('/');
      const parentPath = filePath.substring(0, slash);
      filePath = parentPath + '/' + newName;
      filePath = filePath.substr(1);
      const newURL = this.fileSystemBaseURL + filePath;
      const newContentType = this.fileSystemInternal.contentType(newName);
      this.renameUISourceCode(uiSourceCode, newName);
      callback(true, newName, newURL, newContentType);
    }
  }

  async searchInFileContent(
      uiSourceCode: Workspace.UISourceCode.UISourceCode, query: string, caseSensitive: boolean,
      isRegex: boolean): Promise<TextUtils.ContentProvider.SearchMatch[]> {
    const filePath = this.filePathForUISourceCode(uiSourceCode);
    const {content} = await this.fileSystemInternal.requestFileContent(filePath);
    if (content) {
      return TextUtils.TextUtils.performSearchInContent(content, query, caseSensitive, isRegex);
    }
    return [];
  }

  async findFilesMatchingSearchRequest(
      searchConfig: Workspace.Workspace.ProjectSearchConfig, filesMathingFileQuery: string[],
      progress: Common.Progress.Progress): Promise<string[]> {
    let result: string[] = filesMathingFileQuery;
    const queriesToRun = searchConfig.queries().slice();
    if (!queriesToRun.length) {
      queriesToRun.push('');
    }
    progress.setTotalWork(queriesToRun.length);

    for (const query of queriesToRun) {
      const files = await this.fileSystemInternal.searchInPath(searchConfig.isRegex() ? '' : query, progress);
      files.sort(Platform.StringUtilities.naturalOrderComparator);
      result = Platform.ArrayUtilities.intersectOrdered(result, files, Platform.StringUtilities.naturalOrderComparator);
      progress.incrementWorked(1);
    }

    progress.done();
    return result;
  }

  indexContent(progress: Common.Progress.Progress): void {
    this.fileSystemInternal.indexContent(progress);
  }

  populate(): void {
    const chunkSize = 1000;
    const filePaths = this.fileSystemInternal.initialFilePaths();
    reportFileChunk.call(this, 0);

    function reportFileChunk(this: FileSystem, from: number): void {
      const to = Math.min(from + chunkSize, filePaths.length);
      for (let i = from; i < to; ++i) {
        this.addFile(filePaths[i]);
      }
      if (to < filePaths.length) {
        setTimeout(reportFileChunk.bind(this, to), 100);
      }
    }
  }

  excludeFolder(url: string): void {
    let relativeFolder = url.substring(this.fileSystemBaseURL.length);
    if (!relativeFolder.startsWith('/')) {
      relativeFolder = '/' + relativeFolder;
    }
    if (!relativeFolder.endsWith('/')) {
      relativeFolder += '/';
    }
    this.fileSystemInternal.addExcludedFolder(relativeFolder);

    const uiSourceCodes = this.uiSourceCodes().slice();
    for (let i = 0; i < uiSourceCodes.length; ++i) {
      const uiSourceCode = uiSourceCodes[i];
      if (uiSourceCode.url().startsWith(url)) {
        this.removeUISourceCode(uiSourceCode.url());
      }
    }
  }

  canExcludeFolder(path: string): boolean {
    return this.fileSystemInternal.canExcludeFolder(path);
  }

  canCreateFile(): boolean {
    return true;
  }

  async createFile(path: string, name: string|null, content: string, isBase64?: boolean):
      Promise<Workspace.UISourceCode.UISourceCode|null> {
    const guardFileName = this.fileSystemPathInternal + path + (!path.endsWith('/') ? '/' : '') + name;
    this.creatingFilesGuard.add(guardFileName);
    const filePath = await this.fileSystemInternal.createFile(path, name);
    if (!filePath) {
      return null;
    }
    const uiSourceCode = this.addFile(filePath);
    uiSourceCode.setContent(content, Boolean(isBase64));
    this.creatingFilesGuard.delete(guardFileName);
    return uiSourceCode;
  }

  deleteFile(uiSourceCode: Workspace.UISourceCode.UISourceCode): void {
    const relativePath = this.filePathForUISourceCode(uiSourceCode);
    this.fileSystemInternal.deleteFile(relativePath).then(success => {
      if (success) {
        this.removeUISourceCode(uiSourceCode.url());
      }
    });
  }

  remove(): void {
    this.fileSystemWorkspaceBinding.isolatedFileSystemManager.removeFileSystem(this.fileSystemInternal);
  }

  private addFile(filePath: string): Workspace.UISourceCode.UISourceCode {
    const contentType = this.fileSystemInternal.contentType(filePath);
    const uiSourceCode = this.createUISourceCode(this.fileSystemBaseURL + filePath, contentType);
    this.addUISourceCode(uiSourceCode);
    return uiSourceCode;
  }

  fileChanged(path: string): void {
    // Ignore files that are being created but do not have content yet.
    if (this.creatingFilesGuard.has(path)) {
      return;
    }
    const uiSourceCode = this.uiSourceCodeForURL(path);
    if (!uiSourceCode) {
      const contentType = this.fileSystemInternal.contentType(path);
      this.addUISourceCode(this.createUISourceCode(path, contentType));
      return;
    }
    sourceCodeToMetadataMap.delete(uiSourceCode);
    uiSourceCode.checkContentUpdated();
  }

  tooltipForURL(url: string): string {
    return this.fileSystemInternal.tooltipForURL(url);
  }

  dispose(): void {
    this.removeProject();
  }
}

const sourceCodeToMetadataMap =
    new WeakMap<Workspace.UISourceCode.UISourceCode, Promise<Workspace.UISourceCode.UISourceCodeMetadata|null>>();
export interface FilesChangedData {
  changed: Platform.MapUtilities.Multimap<string, string>;
  added: Platform.MapUtilities.Multimap<string, string>;
  removed: Platform.MapUtilities.Multimap<string, string>;
}
