// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Copyright (C) 2007, 2008, 2010 Apple Inc.  All rights reserved.
 * Copyright (C) 2009 Joseph Pecoraro
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

import * as Common from '../../core/common/common.js';
import * as Host from '../../core/host/host.js';
import * as i18n from '../../core/i18n/i18n.js';
import * as Platform from '../../core/platform/platform.js';
import * as Root from '../../core/root/root.js';
import * as SDK from '../../core/sdk/sdk.js';
import * as Protocol from '../../generated/protocol.js';
import * as SourceFrame from '../../ui/legacy/components/source_frame/source_frame.js';
import * as UI from '../../ui/legacy/legacy.js';

import {BackForwardCacheTreeElement, ServiceWorkerCacheTreeElement} from './ApplicationPanelCacheSection.js';
import {ApplicationPanelTreeElement, ExpandableApplicationPanelTreeElement} from './ApplicationPanelTreeElement.js';
import {AppManifestView} from './AppManifestView.js';
import {BackgroundServiceModel} from './BackgroundServiceModel.js';
import {BackgroundServiceView} from './BackgroundServiceView.js';
import * as ApplicationComponents from './components/components.js';
import resourcesSidebarStyles from './resourcesSidebar.css.js';

import type {Database as DatabaseModelDatabase} from './DatabaseModel.js';
import {DatabaseModel, Events as DatabaseModelEvents} from './DatabaseModel.js';
import {DatabaseQueryView, Events as DatabaseQueryViewEvents} from './DatabaseQueryView.js';
import {DatabaseTableView} from './DatabaseTableView.js';
import type {DOMStorage} from './DOMStorageModel.js';
import {DOMStorageModel, Events as DOMStorageModelEvents} from './DOMStorageModel.js';
import type {Database as IndexedDBModelDatabase, DatabaseId, Index, ObjectStore} from './IndexedDBModel.js';
import {Events as IndexedDBModelEvents, IndexedDBModel} from './IndexedDBModel.js';
import {IDBDatabaseView, IDBDataView} from './IndexedDBViews.js';
import {OpenedWindowDetailsView, WorkerDetailsView} from './OpenedWindowDetailsView.js';
import type {ResourcesPanel} from './ResourcesPanel.js';
import {ServiceWorkersView} from './ServiceWorkersView.js';
import {StorageView} from './StorageView.js';
import {TrustTokensTreeElement} from './TrustTokensTreeElement.js';
import {ReportingApiTreeElement} from './ReportingApiTreeElement.js';

const UIStrings = {
  /**
  *@description Text in Application Panel Sidebar of the Application panel
  */
  application: 'Application',
  /**
  *@description Text in Application Panel Sidebar of the Application panel
  */
  storage: 'Storage',
  /**
  *@description Text in Application Panel Sidebar of the Application panel
  */
  localStorage: 'Local Storage',
  /**
  *@description Text in Application Panel Sidebar of the Application panel
  */
  sessionStorage: 'Session Storage',
  /**
  *@description Text in Application Panel Sidebar of the Application panel
  */
  webSql: 'Web SQL',
  /**
  *@description Text for web cookies
  */
  cookies: 'Cookies',
  /**
  *@description Text in Application Panel Sidebar of the Application panel
  */
  cache: 'Cache',
  /**
  *@description Text in Application Panel Sidebar of the Application panel
  */
  backgroundServices: 'Background Services',
  /**
  *@description Text for rendering frames
  */
  frames: 'Frames',
  /**
  *@description Text that appears on a button for the manifest resource type filter.
  */
  manifest: 'Manifest',
  /**
  *@description Text in Application Panel Sidebar of the Application panel
  */
  indexeddb: 'IndexedDB',
  /**
  *@description A context menu item in the Application Panel Sidebar of the Application panel
  */
  refreshIndexeddb: 'Refresh IndexedDB',
  /**
  *@description Tooltip in Application Panel Sidebar of the Application panel
  *@example {1.0} PH1
  */
  versionSEmpty: 'Version: {PH1} (empty)',
  /**
  *@description Tooltip in Application Panel Sidebar of the Application panel
  *@example {1.0} PH1
  */
  versionS: 'Version: {PH1}',
  /**
  *@description Text to clear content
  */
  clear: 'Clear',
  /**
  *@description Text in Application Panel Sidebar of the Application panel
  *@example {"key path"} PH1
  */
  keyPathS: 'Key path: {PH1}',
  /**
  *@description Text in Application Panel Sidebar of the Application panel
  */
  localFiles: 'Local Files',
  /**
  *@description Tooltip in Application Panel Sidebar of the Application panel
  *@example {https://example.com} PH1
  */
  cookiesUsedByFramesFromS: 'Cookies used by frames from {PH1}',
  /**
  *@description Text in Frames View of the Application panel
  */
  openedWindows: 'Opened Windows',
  /**
  *@description Label for plural of worker type: web workers
  */
  webWorkers: 'Web Workers',
  /**
  *@description Label in frame tree for unavailable document
  */
  documentNotAvailable: 'Document not available',
  /**
  *@description Description of content of unavailable document in Application panel
  */
  theContentOfThisDocumentHasBeen:
      'The content of this document has been generated dynamically via \'document.write()\'.',
  /**
  *@description Text in Frames View of the Application panel
  */
  windowWithoutTitle: 'Window without title',
  /**
  *@description Default name for worker
  */
  worker: 'worker',
};
const str_ = i18n.i18n.registerUIStrings('panels/application/ApplicationPanelSidebar.ts', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);

function assertNotMainTarget(targetId: Protocol.Target.TargetID|'main'): asserts targetId is Protocol.Target.TargetID {
  if (targetId === 'main') {
    throw new Error('Unexpected main target id');
  }
}

export class ApplicationPanelSidebar extends UI.Widget.VBox implements SDK.TargetManager.Observer {
  panel: ResourcesPanel;
  private readonly sidebarTree: UI.TreeOutline.TreeOutlineInShadow;
  private readonly applicationTreeElement: UI.TreeOutline.TreeElement;
  serviceWorkersTreeElement: ServiceWorkersTreeElement;
  localStorageListTreeElement: ExpandableApplicationPanelTreeElement;
  sessionStorageListTreeElement: ExpandableApplicationPanelTreeElement;
  indexedDBListTreeElement: IndexedDBTreeElement;
  databasesListTreeElement: ExpandableApplicationPanelTreeElement;
  cookieListTreeElement: ExpandableApplicationPanelTreeElement;
  trustTokensTreeElement: TrustTokensTreeElement;
  cacheStorageListTreeElement: ServiceWorkerCacheTreeElement;
  private backForwardCacheListTreeElement?: BackForwardCacheTreeElement;
  backgroundFetchTreeElement: BackgroundServiceTreeElement|undefined;
  backgroundSyncTreeElement: BackgroundServiceTreeElement|undefined;
  notificationsTreeElement: BackgroundServiceTreeElement|undefined;
  paymentHandlerTreeElement: BackgroundServiceTreeElement|undefined;
  periodicBackgroundSyncTreeElement: BackgroundServiceTreeElement|undefined;
  pushMessagingTreeElement: BackgroundServiceTreeElement|undefined;
  reportingApiTreeElement: ReportingApiTreeElement|undefined;
  private readonly resourcesSection: ResourcesSection;
  private readonly databaseTableViews: Map<DatabaseModelDatabase, {
    [x: string]: DatabaseTableView,
  }>;
  private databaseQueryViews: Map<DatabaseModelDatabase, DatabaseQueryView>;
  private readonly databaseTreeElements: Map<DatabaseModelDatabase, DatabaseTreeElement>;
  private domStorageTreeElements: Map<DOMStorage, DOMStorageTreeElement>;
  private domains: {
    [x: string]: boolean,
  };
  private target?: SDK.Target.Target;
  private databaseModel?: DatabaseModel|null;
  private previousHoveredElement?: FrameTreeElement;

  constructor(panel: ResourcesPanel) {
    super();

    this.panel = panel;

    this.sidebarTree = new UI.TreeOutline.TreeOutlineInShadow();
    this.sidebarTree.element.classList.add('resources-sidebar');

    this.sidebarTree.element.classList.add('filter-all');
    // Listener needs to have been set up before the elements are added
    this.sidebarTree.addEventListener(UI.TreeOutline.Events.ElementAttached, this.treeElementAdded, this);

    this.contentElement.appendChild(this.sidebarTree.element);

    const applicationSectionTitle = i18nString(UIStrings.application);
    this.applicationTreeElement = this.addSidebarSection(applicationSectionTitle);
    const manifestTreeElement = new AppManifestTreeElement(panel);
    this.applicationTreeElement.appendChild(manifestTreeElement);
    this.serviceWorkersTreeElement = new ServiceWorkersTreeElement(panel);
    this.applicationTreeElement.appendChild(this.serviceWorkersTreeElement);
    const clearStorageTreeElement = new ClearStorageTreeElement(panel);
    this.applicationTreeElement.appendChild(clearStorageTreeElement);

    const storageSectionTitle = i18nString(UIStrings.storage);
    const storageTreeElement = this.addSidebarSection(storageSectionTitle);
    this.localStorageListTreeElement =
        new ExpandableApplicationPanelTreeElement(panel, i18nString(UIStrings.localStorage), 'LocalStorage');
    this.localStorageListTreeElement.setLink(
        'https://developer.chrome.com/docs/devtools/storage/localstorage/?utm_source=devtools');
    const localStorageIcon = UI.Icon.Icon.create('mediumicon-table', 'resource-tree-item');
    this.localStorageListTreeElement.setLeadingIcons([localStorageIcon]);

    storageTreeElement.appendChild(this.localStorageListTreeElement);
    this.sessionStorageListTreeElement =
        new ExpandableApplicationPanelTreeElement(panel, i18nString(UIStrings.sessionStorage), 'SessionStorage');
    this.sessionStorageListTreeElement.setLink(
        'https://developer.chrome.com/docs/devtools/storage/sessionstorage/?utm_source=devtools');
    const sessionStorageIcon = UI.Icon.Icon.create('mediumicon-table', 'resource-tree-item');
    this.sessionStorageListTreeElement.setLeadingIcons([sessionStorageIcon]);

    storageTreeElement.appendChild(this.sessionStorageListTreeElement);
    this.indexedDBListTreeElement = new IndexedDBTreeElement(panel);
    this.indexedDBListTreeElement.setLink(
        'https://developer.chrome.com/docs/devtools/storage/indexeddb/?utm_source=devtools');
    storageTreeElement.appendChild(this.indexedDBListTreeElement);
    this.databasesListTreeElement =
        new ExpandableApplicationPanelTreeElement(panel, i18nString(UIStrings.webSql), 'Databases');
    this.databasesListTreeElement.setLink(
        'https://developer.chrome.com/docs/devtools/storage/websql/?utm_source=devtools');
    const databaseIcon = UI.Icon.Icon.create('mediumicon-database', 'resource-tree-item');
    this.databasesListTreeElement.setLeadingIcons([databaseIcon]);

    storageTreeElement.appendChild(this.databasesListTreeElement);
    this.cookieListTreeElement =
        new ExpandableApplicationPanelTreeElement(panel, i18nString(UIStrings.cookies), 'Cookies');
    this.cookieListTreeElement.setLink(
        'https://developer.chrome.com/docs/devtools/storage/cookies/?utm_source=devtools');
    const cookieIcon = UI.Icon.Icon.create('mediumicon-cookie', 'resource-tree-item');
    this.cookieListTreeElement.setLeadingIcons([cookieIcon]);
    storageTreeElement.appendChild(this.cookieListTreeElement);

    this.trustTokensTreeElement = new TrustTokensTreeElement(panel);
    storageTreeElement.appendChild(this.trustTokensTreeElement);

    const cacheSectionTitle = i18nString(UIStrings.cache);
    const cacheTreeElement = this.addSidebarSection(cacheSectionTitle);
    this.cacheStorageListTreeElement = new ServiceWorkerCacheTreeElement(panel);
    cacheTreeElement.appendChild(this.cacheStorageListTreeElement);

    this.backForwardCacheListTreeElement = new BackForwardCacheTreeElement(panel);
    cacheTreeElement.appendChild(this.backForwardCacheListTreeElement);

    if (Root.Runtime.experiments.isEnabled('backgroundServices')) {
      const backgroundServiceSectionTitle = i18nString(UIStrings.backgroundServices);
      const backgroundServiceTreeElement = this.addSidebarSection(backgroundServiceSectionTitle);

      this.backgroundFetchTreeElement =
          new BackgroundServiceTreeElement(panel, Protocol.BackgroundService.ServiceName.BackgroundFetch);
      backgroundServiceTreeElement.appendChild(this.backgroundFetchTreeElement);
      this.backgroundSyncTreeElement =
          new BackgroundServiceTreeElement(panel, Protocol.BackgroundService.ServiceName.BackgroundSync);
      backgroundServiceTreeElement.appendChild(this.backgroundSyncTreeElement);

      if (Root.Runtime.experiments.isEnabled('backgroundServicesNotifications')) {
        this.notificationsTreeElement =
            new BackgroundServiceTreeElement(panel, Protocol.BackgroundService.ServiceName.Notifications);
        backgroundServiceTreeElement.appendChild(this.notificationsTreeElement);
      }
      if (Root.Runtime.experiments.isEnabled('backgroundServicesPaymentHandler')) {
        this.paymentHandlerTreeElement =
            new BackgroundServiceTreeElement(panel, Protocol.BackgroundService.ServiceName.PaymentHandler);
        backgroundServiceTreeElement.appendChild(this.paymentHandlerTreeElement);
      }
      this.periodicBackgroundSyncTreeElement =
          new BackgroundServiceTreeElement(panel, Protocol.BackgroundService.ServiceName.PeriodicBackgroundSync);
      backgroundServiceTreeElement.appendChild(this.periodicBackgroundSyncTreeElement);
      if (Root.Runtime.experiments.isEnabled('backgroundServicesPushMessaging')) {
        this.pushMessagingTreeElement =
            new BackgroundServiceTreeElement(panel, Protocol.BackgroundService.ServiceName.PushMessaging);
        backgroundServiceTreeElement.appendChild(this.pushMessagingTreeElement);
      }
      if (Root.Runtime.experiments.isEnabled('reportingApiDebugging')) {
        this.reportingApiTreeElement = new ReportingApiTreeElement(panel);
        backgroundServiceTreeElement.appendChild(this.reportingApiTreeElement);
      }
    }
    const resourcesSectionTitle = i18nString(UIStrings.frames);
    const resourcesTreeElement = this.addSidebarSection(resourcesSectionTitle);
    this.resourcesSection = new ResourcesSection(panel, resourcesTreeElement);

    this.databaseTableViews = new Map();
    this.databaseQueryViews = new Map();
    this.databaseTreeElements = new Map();
    this.domStorageTreeElements = new Map();
    this.domains = {};

    this.sidebarTree.contentElement.addEventListener('mousemove', this.onmousemove.bind(this), false);
    this.sidebarTree.contentElement.addEventListener('mouseleave', this.onmouseleave.bind(this), false);

    SDK.TargetManager.TargetManager.instance().observeTargets(this);
    SDK.TargetManager.TargetManager.instance().addModelListener(
        SDK.ResourceTreeModel.ResourceTreeModel, SDK.ResourceTreeModel.Events.FrameNavigated, this.frameNavigated,
        this);

    const selection = this.panel.lastSelectedItemPath();
    if (!selection.length) {
      manifestTreeElement.select();
    }

    SDK.TargetManager.TargetManager.instance().observeModels(DOMStorageModel, {
      modelAdded: (model: DOMStorageModel): void => this.domStorageModelAdded(model),
      modelRemoved: (model: DOMStorageModel): void => this.domStorageModelRemoved(model),
    });
    SDK.TargetManager.TargetManager.instance().observeModels(IndexedDBModel, {
      modelAdded: (model: IndexedDBModel): void => model.enable(),
      modelRemoved: (model: IndexedDBModel): void => this.indexedDBListTreeElement.removeIndexedDBForModel(model),
    });

    // Work-around for crbug.com/1152713: Something is wrong with custom scrollbars and size containment.
    // @ts-ignore
    this.contentElement.style.contain = 'layout style';
  }

  private addSidebarSection(title: string): UI.TreeOutline.TreeElement {
    const treeElement = new UI.TreeOutline.TreeElement(title, true);
    treeElement.listItemElement.classList.add('storage-group-list-item');
    treeElement.setCollapsible(false);
    treeElement.selectable = false;
    this.sidebarTree.appendChild(treeElement);
    UI.ARIAUtils.setAccessibleName(treeElement.childrenListElement, title);
    return treeElement;
  }

  targetAdded(target: SDK.Target.Target): void {
    if (this.target) {
      return;
    }
    this.target = target;
    this.databaseModel = target.model(DatabaseModel);
    if (this.databaseModel) {
      this.databaseModel.addEventListener(DatabaseModelEvents.DatabaseAdded, this.databaseAdded, this);
      this.databaseModel.addEventListener(DatabaseModelEvents.DatabasesRemoved, this.resetWebSQL, this);
    }

    const resourceTreeModel = target.model(SDK.ResourceTreeModel.ResourceTreeModel);
    if (!resourceTreeModel) {
      return;
    }

    if (resourceTreeModel.cachedResourcesLoaded()) {
      this.initialize();
    }

    resourceTreeModel.addEventListener(SDK.ResourceTreeModel.Events.CachedResourcesLoaded, this.initialize, this);
    resourceTreeModel.addEventListener(
        SDK.ResourceTreeModel.Events.WillLoadCachedResources, this.resetWithFrames, this);
  }

  targetRemoved(target: SDK.Target.Target): void {
    if (target !== this.target) {
      return;
    }
    delete this.target;

    const resourceTreeModel = target.model(SDK.ResourceTreeModel.ResourceTreeModel);
    if (resourceTreeModel) {
      resourceTreeModel.removeEventListener(SDK.ResourceTreeModel.Events.CachedResourcesLoaded, this.initialize, this);
      resourceTreeModel.removeEventListener(
          SDK.ResourceTreeModel.Events.WillLoadCachedResources, this.resetWithFrames, this);
    }
    if (this.databaseModel) {
      this.databaseModel.removeEventListener(DatabaseModelEvents.DatabaseAdded, this.databaseAdded, this);
      this.databaseModel.removeEventListener(DatabaseModelEvents.DatabasesRemoved, this.resetWebSQL, this);
      this.databaseModel = null;
    }

    this.resetWithFrames();
  }

  focus(): void {
    this.sidebarTree.focus();
  }

  private initialize(): void {
    for (const frame of SDK.ResourceTreeModel.ResourceTreeModel.frames()) {
      this.addCookieDocument(frame);
    }
    if (this.databaseModel) {
      this.databaseModel.enable();
    }

    const cacheStorageModel = this.target && this.target.model(SDK.ServiceWorkerCacheModel.ServiceWorkerCacheModel);
    if (cacheStorageModel) {
      cacheStorageModel.enable();
    }
    const serviceWorkerCacheModel =
        this.target && this.target.model(SDK.ServiceWorkerCacheModel.ServiceWorkerCacheModel) || null;
    this.cacheStorageListTreeElement.initialize(serviceWorkerCacheModel);
    const backgroundServiceModel = this.target && this.target.model(BackgroundServiceModel) || null;
    if (Root.Runtime.experiments.isEnabled('backgroundServices')) {
      this.backgroundFetchTreeElement && this.backgroundFetchTreeElement.initialize(backgroundServiceModel);
      this.backgroundSyncTreeElement && this.backgroundSyncTreeElement.initialize(backgroundServiceModel);
      if (Root.Runtime.experiments.isEnabled('backgroundServicesNotifications') && this.notificationsTreeElement) {
        this.notificationsTreeElement.initialize(backgroundServiceModel);
      }
      if (Root.Runtime.experiments.isEnabled('backgroundServicesPaymentHandler') && this.paymentHandlerTreeElement) {
        this.paymentHandlerTreeElement.initialize(backgroundServiceModel);
      }
      this.periodicBackgroundSyncTreeElement &&
          this.periodicBackgroundSyncTreeElement.initialize(backgroundServiceModel);
      if (Root.Runtime.experiments.isEnabled('backgroundServicesPushMessaging') && this.pushMessagingTreeElement) {
        this.pushMessagingTreeElement.initialize(backgroundServiceModel);
      }
    }
  }

  private domStorageModelAdded(model: DOMStorageModel): void {
    model.enable();
    model.storages().forEach(this.addDOMStorage.bind(this));
    model.addEventListener(DOMStorageModelEvents.DOMStorageAdded, this.domStorageAdded, this);
    model.addEventListener(DOMStorageModelEvents.DOMStorageRemoved, this.domStorageRemoved, this);
  }

  private domStorageModelRemoved(model: DOMStorageModel): void {
    model.storages().forEach(this.removeDOMStorage.bind(this));
    model.removeEventListener(DOMStorageModelEvents.DOMStorageAdded, this.domStorageAdded, this);
    model.removeEventListener(DOMStorageModelEvents.DOMStorageRemoved, this.domStorageRemoved, this);
  }

  private resetWithFrames(): void {
    this.resourcesSection.reset();
    this.reset();
  }

  private resetWebSQL(): void {
    for (const queryView of this.databaseQueryViews.values()) {
      queryView.removeEventListener(DatabaseQueryViewEvents.SchemaUpdated, event => {
        this.updateDatabaseTables(event);
      }, this);
    }
    this.databaseTableViews.clear();
    this.databaseQueryViews.clear();
    this.databaseTreeElements.clear();
    this.databasesListTreeElement.removeChildren();
    this.databasesListTreeElement.setExpandable(false);
  }

  private treeElementAdded(event: Common.EventTarget.EventTargetEvent<UI.TreeOutline.TreeElement>): void {
    // On tree item selection its itemURL and those of its parents are persisted.
    // On reload/navigation we check for matches starting from the root on the
    // path to the current element. Matching nodes are expanded until we hit a
    // mismatch. This way we ensure that the longest matching path starting from
    // the root is expanded, even if we cannot match the whole path.
    const selection = this.panel.lastSelectedItemPath();
    if (!selection.length) {
      return;
    }
    const element = event.data;
    const elementPath = [element as UI.TreeOutline.TreeElement | ApplicationPanelTreeElement];
    for (let parent = element.parent as UI.TreeOutline.TreeElement | ApplicationPanelTreeElement | null;
         parent && 'itemURL' in parent && parent.itemURL; parent = parent.parent) {
      elementPath.push(parent as ApplicationPanelTreeElement);
    }

    let i = selection.length - 1;
    let j = elementPath.length - 1;
    while (i >= 0 && j >= 0 && selection[i] === (elementPath[j] as ApplicationPanelTreeElement).itemURL) {
      if (!elementPath[j].expanded) {
        if (i > 0) {
          elementPath[j].expand();
        }
        if (!elementPath[j].selected) {
          elementPath[j].select();
        }
      }
      i--;
      j--;
    }
  }

  private reset(): void {
    this.domains = {};
    this.resetWebSQL();
    this.cookieListTreeElement.removeChildren();
  }

  private frameNavigated(event: Common.EventTarget.EventTargetEvent<SDK.ResourceTreeModel.ResourceTreeFrame>): void {
    const frame = event.data;

    if (frame.isTopFrame()) {
      this.reset();
    }
    this.addCookieDocument(frame);
  }

  private databaseAdded({data: database}: Common.EventTarget.EventTargetEvent<DatabaseModelDatabase>): void {
    const databaseTreeElement = new DatabaseTreeElement(this, database);
    this.databaseTreeElements.set(database, databaseTreeElement);
    this.databasesListTreeElement.appendChild(databaseTreeElement);
  }

  private addCookieDocument(frame: SDK.ResourceTreeModel.ResourceTreeFrame): void {
    // In case the current frame was unreachable, show it's cookies
    // instead of the error interstitials because they might help to
    // debug why the frame was unreachable.
    const urlToParse = frame.unreachableUrl() || frame.url;
    const parsedURL = Common.ParsedURL.ParsedURL.fromString(urlToParse);
    if (!parsedURL || (parsedURL.scheme !== 'http' && parsedURL.scheme !== 'https' && parsedURL.scheme !== 'file')) {
      return;
    }

    const domain = parsedURL.securityOrigin();
    if (!this.domains[domain]) {
      this.domains[domain] = true;
      const cookieDomainTreeElement = new CookieTreeElement(this.panel, frame, domain);
      this.cookieListTreeElement.appendChild(cookieDomainTreeElement);
    }
  }

  private domStorageAdded(event: Common.EventTarget.EventTargetEvent<DOMStorage>): void {
    const domStorage = (event.data as DOMStorage);
    this.addDOMStorage(domStorage);
  }

  private addDOMStorage(domStorage: DOMStorage): void {
    console.assert(!this.domStorageTreeElements.get(domStorage));

    const domStorageTreeElement = new DOMStorageTreeElement(this.panel, domStorage);
    this.domStorageTreeElements.set(domStorage, domStorageTreeElement);
    if (domStorage.isLocalStorage) {
      this.localStorageListTreeElement.appendChild(domStorageTreeElement);
    } else {
      this.sessionStorageListTreeElement.appendChild(domStorageTreeElement);
    }
  }

  private domStorageRemoved(event: Common.EventTarget.EventTargetEvent<DOMStorage>): void {
    const domStorage = (event.data as DOMStorage);
    this.removeDOMStorage(domStorage);
  }

  private removeDOMStorage(domStorage: DOMStorage): void {
    const treeElement = this.domStorageTreeElements.get(domStorage);
    if (!treeElement) {
      return;
    }
    const wasSelected = treeElement.selected;
    const parentListTreeElement = treeElement.parent;
    if (parentListTreeElement) {
      parentListTreeElement.removeChild(treeElement);
      if (wasSelected) {
        parentListTreeElement.select();
      }
    }
    this.domStorageTreeElements.delete(domStorage);
  }

  selectDatabase(database: DatabaseModelDatabase): void {
    if (database) {
      this.showDatabase(database);
      const treeElement = this.databaseTreeElements.get(database);
      treeElement && treeElement.select();
    }
  }

  async showResource(resource: SDK.Resource.Resource, line?: number, column?: number): Promise<void> {
    await this.resourcesSection.revealResource(resource, line, column);
  }

  showFrame(frame: SDK.ResourceTreeModel.ResourceTreeFrame): void {
    this.resourcesSection.revealAndSelectFrame(frame);
  }

  showDatabase(database: DatabaseModelDatabase, tableName?: string): void {
    if (!database) {
      return;
    }

    let view;
    if (tableName) {
      let tableViews = this.databaseTableViews.get(database);
      if (!tableViews) {
        tableViews = ({} as {
          [x: string]: DatabaseTableView,
        });
        this.databaseTableViews.set(database, tableViews);
      }
      view = tableViews[tableName];
      if (!view) {
        view = new DatabaseTableView(database, tableName);
        tableViews[tableName] = view;
      }
    } else {
      view = this.databaseQueryViews.get(database);
      if (!view) {
        view = new DatabaseQueryView(database);
        this.databaseQueryViews.set(database, view);
        view.addEventListener(DatabaseQueryViewEvents.SchemaUpdated, event => {
          this.updateDatabaseTables(event);
        }, this);
      }
    }

    this.innerShowView(view);
  }

  showFileSystem(view: UI.Widget.Widget): void {
    this.innerShowView(view);
  }

  private innerShowView(view: UI.Widget.Widget): void {
    this.panel.showView(view);
  }

  private async updateDatabaseTables(event: Common.EventTarget.EventTargetEvent<DatabaseModelDatabase>): Promise<void> {
    const database = event.data;

    if (!database) {
      return;
    }

    const databasesTreeElement = this.databaseTreeElements.get(database);
    if (!databasesTreeElement) {
      return;
    }

    databasesTreeElement.invalidateChildren();
    const tableViews = this.databaseTableViews.get(database);

    if (!tableViews) {
      return;
    }

    const tableNamesHash = new Set<string>();
    const panel = this.panel;
    const tableNames = await database.tableNames();

    for (const tableName of tableNames) {
      tableNamesHash.add(tableName);
    }

    for (const tableName in tableViews) {
      if (!(tableNamesHash.has(tableName))) {
        if (panel.visibleView === tableViews[tableName]) {
          panel.showView(null);
        }
        delete tableViews[tableName];
      }
    }

    await databasesTreeElement.updateChildren();
  }

  private onmousemove(event: MouseEvent): void {
    const nodeUnderMouse = (event.target as Node);
    if (!nodeUnderMouse) {
      return;
    }

    const listNode = UI.UIUtils.enclosingNodeOrSelfWithNodeName(nodeUnderMouse, 'li');
    if (!listNode) {
      return;
    }

    const element = UI.TreeOutline.TreeElement.getTreeElementBylistItemNode(listNode);
    if (this.previousHoveredElement === element) {
      return;
    }

    if (this.previousHoveredElement) {
      this.previousHoveredElement.hovered = false;
      delete this.previousHoveredElement;
    }

    if (element instanceof FrameTreeElement) {
      this.previousHoveredElement = element;
      element.hovered = true;
    }
  }

  private onmouseleave(_event: MouseEvent): void {
    if (this.previousHoveredElement) {
      this.previousHoveredElement.hovered = false;
      delete this.previousHoveredElement;
    }
  }
  wasShown(): void {
    super.wasShown();
    this.sidebarTree.registerCSSFiles([resourcesSidebarStyles]);
  }
}

export class BackgroundServiceTreeElement extends ApplicationPanelTreeElement {
  private serviceName: Protocol.BackgroundService.ServiceName;
  private view: BackgroundServiceView|null;
  private model: BackgroundServiceModel|null;
  private selectedInternal: boolean;

  constructor(storagePanel: ResourcesPanel, serviceName: Protocol.BackgroundService.ServiceName) {
    super(storagePanel, BackgroundServiceView.getUIString(serviceName), false);

    this.serviceName = serviceName;

    /* Whether the element has been selected. */
    this.selectedInternal = false;

    this.view = null;

    this.model = null;

    const backgroundServiceIcon = UI.Icon.Icon.create(this.getIconType(), 'resource-tree-item');
    this.setLeadingIcons([backgroundServiceIcon]);
  }

  private getIconType(): string {
    switch (this.serviceName) {
      case Protocol.BackgroundService.ServiceName.BackgroundFetch:
        return 'mediumicon-fetch';
      case Protocol.BackgroundService.ServiceName.BackgroundSync:
        return 'mediumicon-sync';
      case Protocol.BackgroundService.ServiceName.PushMessaging:
        return 'mediumicon-cloud';
      case Protocol.BackgroundService.ServiceName.Notifications:
        return 'mediumicon-bell';
      case Protocol.BackgroundService.ServiceName.PaymentHandler:
        return 'mediumicon-payment';
      case Protocol.BackgroundService.ServiceName.PeriodicBackgroundSync:
        return 'mediumicon-schedule';
      default:
        console.error(`Service ${this.serviceName} does not have a dedicated icon`);
        return 'mediumicon-table';
    }
  }

  initialize(model: BackgroundServiceModel|null): void {
    this.model = model;
    // Show the view if the model was initialized after selection.
    if (this.selectedInternal && !this.view) {
      this.onselect(false);
    }
  }

  get itemURL(): string {
    return `background-service://${this.serviceName}`;
  }

  onselect(selectedByUser?: boolean): boolean {
    super.onselect(selectedByUser);
    this.selectedInternal = true;

    if (!this.model) {
      return false;
    }

    if (!this.view) {
      this.view = new BackgroundServiceView(this.serviceName, this.model);
    }
    this.showView(this.view);
    UI.Context.Context.instance().setFlavor(BackgroundServiceView, this.view);
    return false;
  }
}

export class DatabaseTreeElement extends ApplicationPanelTreeElement {
  private readonly sidebar: ApplicationPanelSidebar;
  private readonly database: DatabaseModelDatabase;
  constructor(sidebar: ApplicationPanelSidebar, database: DatabaseModelDatabase) {
    super(sidebar.panel, database.name, true);
    this.sidebar = sidebar;
    this.database = database;

    const icon = UI.Icon.Icon.create('mediumicon-database', 'resource-tree-item');
    this.setLeadingIcons([icon]);
  }

  get itemURL(): string {
    return 'database://' + encodeURI(this.database.name);
  }

  onselect(selectedByUser?: boolean): boolean {
    super.onselect(selectedByUser);
    this.sidebar.showDatabase(this.database);
    return false;
  }

  onexpand(): void {
    this.updateChildren();
  }

  async updateChildren(): Promise<void> {
    this.removeChildren();
    const tableNames = await this.database.tableNames();
    for (const tableName of tableNames) {
      this.appendChild(new DatabaseTableTreeElement(this.sidebar, this.database, tableName));
    }
  }
}

export class DatabaseTableTreeElement extends ApplicationPanelTreeElement {
  private readonly sidebar: ApplicationPanelSidebar;
  private readonly database: DatabaseModelDatabase;
  private readonly tableName: string;

  constructor(sidebar: ApplicationPanelSidebar, database: DatabaseModelDatabase, tableName: string) {
    super(sidebar.panel, tableName, false);
    this.sidebar = sidebar;
    this.database = database;
    this.tableName = tableName;
    const icon = UI.Icon.Icon.create('mediumicon-table', 'resource-tree-item');
    this.setLeadingIcons([icon]);
  }

  get itemURL(): string {
    return 'database://' + encodeURI(this.database.name) + '/' + encodeURI(this.tableName);
  }

  onselect(selectedByUser?: boolean): boolean {
    super.onselect(selectedByUser);
    this.sidebar.showDatabase(this.database, this.tableName);
    return false;
  }
}

export class ServiceWorkersTreeElement extends ApplicationPanelTreeElement {
  private view?: ServiceWorkersView;

  constructor(storagePanel: ResourcesPanel) {
    super(storagePanel, i18n.i18n.lockedString('Service Workers'), false);
    const icon = UI.Icon.Icon.create('mediumicon-service-worker', 'resource-tree-item');
    this.setLeadingIcons([icon]);
  }

  get itemURL(): string {
    return 'service-workers://';
  }

  onselect(selectedByUser?: boolean): boolean {
    super.onselect(selectedByUser);
    if (!this.view) {
      this.view = new ServiceWorkersView();
    }
    this.showView(this.view);
    return false;
  }
}

export class AppManifestTreeElement extends ApplicationPanelTreeElement {
  private view?: AppManifestView;
  constructor(storagePanel: ResourcesPanel) {
    super(storagePanel, i18nString(UIStrings.manifest), false);
    const icon = UI.Icon.Icon.create('mediumicon-manifest', 'resource-tree-item');
    this.setLeadingIcons([icon]);
  }

  get itemURL(): string {
    return 'manifest://';
  }

  onselect(selectedByUser?: boolean): boolean {
    super.onselect(selectedByUser);
    if (!this.view) {
      this.view = new AppManifestView();
    }
    this.showView(this.view);
    return false;
  }
}

export class ClearStorageTreeElement extends ApplicationPanelTreeElement {
  private view?: StorageView;
  constructor(storagePanel: ResourcesPanel) {
    super(storagePanel, i18nString(UIStrings.storage), false);
    const icon = UI.Icon.Icon.create('mediumicon-database', 'resource-tree-item');
    this.setLeadingIcons([icon]);
  }

  get itemURL(): string {
    return 'clear-storage://';
  }

  onselect(selectedByUser?: boolean): boolean {
    super.onselect(selectedByUser);
    if (!this.view) {
      this.view = new StorageView();
    }
    this.showView(this.view);
    return false;
  }
}

export class IndexedDBTreeElement extends ExpandableApplicationPanelTreeElement {
  private idbDatabaseTreeElements: IDBDatabaseTreeElement[];
  constructor(storagePanel: ResourcesPanel) {
    super(storagePanel, i18nString(UIStrings.indexeddb), 'IndexedDB');
    const icon = UI.Icon.Icon.create('mediumicon-database', 'resource-tree-item');
    this.setLeadingIcons([icon]);
    this.idbDatabaseTreeElements = [];
    this.initialize();
  }

  private initialize(): void {
    SDK.TargetManager.TargetManager.instance().addModelListener(
        IndexedDBModel, IndexedDBModelEvents.DatabaseAdded, this.indexedDBAdded, this);
    SDK.TargetManager.TargetManager.instance().addModelListener(
        IndexedDBModel, IndexedDBModelEvents.DatabaseRemoved, this.indexedDBRemoved, this);
    SDK.TargetManager.TargetManager.instance().addModelListener(
        IndexedDBModel, IndexedDBModelEvents.DatabaseLoaded, this.indexedDBLoaded, this);
    SDK.TargetManager.TargetManager.instance().addModelListener(
        IndexedDBModel, IndexedDBModelEvents.IndexedDBContentUpdated, this.indexedDBContentUpdated, this);
    // TODO(szuend): Replace with a Set once two web tests no longer directly access this private
    //               variable (indexeddb/live-update-indexeddb-content.js, indexeddb/delete-entry.js).
    this.idbDatabaseTreeElements = [];

    for (const indexedDBModel of SDK.TargetManager.TargetManager.instance().models(IndexedDBModel)) {
      const databases = indexedDBModel.databases();
      for (let j = 0; j < databases.length; ++j) {
        this.addIndexedDB(indexedDBModel, databases[j]);
      }
    }
  }

  removeIndexedDBForModel(model: IndexedDBModel): void {
    const idbDatabaseTreeElements = this.idbDatabaseTreeElements.filter(element => element.model === model);
    for (const idbDatabaseTreeElement of idbDatabaseTreeElements) {
      this.removeIDBDatabaseTreeElement(idbDatabaseTreeElement);
    }
  }

  onattach(): void {
    super.onattach();
    this.listItemElement.addEventListener('contextmenu', this.handleContextMenuEvent.bind(this), true);
  }

  private handleContextMenuEvent(event: MouseEvent): void {
    const contextMenu = new UI.ContextMenu.ContextMenu(event);
    contextMenu.defaultSection().appendItem(i18nString(UIStrings.refreshIndexeddb), this.refreshIndexedDB.bind(this));
    contextMenu.show();
  }

  refreshIndexedDB(): void {
    for (const indexedDBModel of SDK.TargetManager.TargetManager.instance().models(IndexedDBModel)) {
      indexedDBModel.refreshDatabaseNames();
    }
  }

  private indexedDBAdded({
    data: {databaseId, model},
  }: Common.EventTarget.EventTargetEvent<{databaseId: DatabaseId, model: IndexedDBModel}>): void {
    this.addIndexedDB(model, databaseId);
  }

  private addIndexedDB(model: IndexedDBModel, databaseId: DatabaseId): void {
    const idbDatabaseTreeElement = new IDBDatabaseTreeElement(this.resourcesPanel, model, databaseId);
    this.idbDatabaseTreeElements.push(idbDatabaseTreeElement);
    this.appendChild(idbDatabaseTreeElement);
    model.refreshDatabase(databaseId);
  }

  private indexedDBRemoved({
    data: {databaseId, model},
  }: Common.EventTarget.EventTargetEvent<{databaseId: DatabaseId, model: IndexedDBModel}>): void {
    const idbDatabaseTreeElement = this.idbDatabaseTreeElement(model, databaseId);
    if (!idbDatabaseTreeElement) {
      return;
    }
    this.removeIDBDatabaseTreeElement(idbDatabaseTreeElement);
  }

  private removeIDBDatabaseTreeElement(idbDatabaseTreeElement: IDBDatabaseTreeElement): void {
    idbDatabaseTreeElement.clear();
    this.removeChild(idbDatabaseTreeElement);
    Platform.ArrayUtilities.removeElement(this.idbDatabaseTreeElements, idbDatabaseTreeElement);
    this.setExpandable(this.childCount() > 0);
  }

  private indexedDBLoaded(
      {data: {database, model, entriesUpdated}}: Common.EventTarget
          .EventTargetEvent<{database: IndexedDBModelDatabase, model: IndexedDBModel, entriesUpdated: boolean}>): void {
    const idbDatabaseTreeElement = this.idbDatabaseTreeElement(model, database.databaseId);
    if (!idbDatabaseTreeElement) {
      return;
    }
    idbDatabaseTreeElement.update(database, entriesUpdated);
    this.indexedDBLoadedForTest();
  }

  private indexedDBLoadedForTest(): void {
    // For sniffing in tests.
  }

  private indexedDBContentUpdated({
    data: {databaseId, objectStoreName, model},
  }: Common.EventTarget.EventTargetEvent<{databaseId: DatabaseId, objectStoreName: string, model: IndexedDBModel}>):
      void {
    const idbDatabaseTreeElement = this.idbDatabaseTreeElement(model, databaseId);
    if (!idbDatabaseTreeElement) {
      return;
    }
    idbDatabaseTreeElement.indexedDBContentUpdated(objectStoreName);
  }

  private idbDatabaseTreeElement(model: IndexedDBModel, databaseId: DatabaseId): IDBDatabaseTreeElement|null {
    return this.idbDatabaseTreeElements.find(x => x.databaseId.equals(databaseId) && x.model === model) || null;
  }
}

export class IDBDatabaseTreeElement extends ApplicationPanelTreeElement {
  model: IndexedDBModel;
  databaseId: DatabaseId;
  private readonly idbObjectStoreTreeElements: Map<string, IDBObjectStoreTreeElement>;
  private database?: IndexedDBModelDatabase;
  private view?: IDBDatabaseView;

  constructor(storagePanel: ResourcesPanel, model: IndexedDBModel, databaseId: DatabaseId) {
    super(storagePanel, databaseId.name + ' - ' + databaseId.securityOrigin, false);
    this.model = model;
    this.databaseId = databaseId;
    this.idbObjectStoreTreeElements = new Map();
    const icon = UI.Icon.Icon.create('mediumicon-database', 'resource-tree-item');
    this.setLeadingIcons([icon]);
    this.model.addEventListener(IndexedDBModelEvents.DatabaseNamesRefreshed, this.refreshIndexedDB, this);
  }

  get itemURL(): string {
    return 'indexedDB://' + this.databaseId.securityOrigin + '/' + this.databaseId.name;
  }

  onattach(): void {
    super.onattach();
    this.listItemElement.addEventListener('contextmenu', this.handleContextMenuEvent.bind(this), true);
  }

  private handleContextMenuEvent(event: MouseEvent): void {
    const contextMenu = new UI.ContextMenu.ContextMenu(event);
    contextMenu.defaultSection().appendItem(i18nString(UIStrings.refreshIndexeddb), this.refreshIndexedDB.bind(this));
    contextMenu.show();
  }

  private refreshIndexedDB(): void {
    this.model.refreshDatabase(this.databaseId);
  }

  indexedDBContentUpdated(objectStoreName: string): void {
    const treeElement = this.idbObjectStoreTreeElements.get(objectStoreName);
    if (treeElement) {
      treeElement.markNeedsRefresh();
    }
  }

  update(database: IndexedDBModelDatabase, entriesUpdated: boolean): void {
    this.database = database;
    const objectStoreNames = new Set<string>();
    for (const objectStoreName of [...this.database.objectStores.keys()].sort()) {
      const objectStore = this.database.objectStores.get(objectStoreName);
      if (!objectStore) {
        continue;
      }
      objectStoreNames.add(objectStore.name);
      let treeElement = this.idbObjectStoreTreeElements.get(objectStore.name);
      if (!treeElement) {
        treeElement = new IDBObjectStoreTreeElement(this.resourcesPanel, this.model, this.databaseId, objectStore);
        this.idbObjectStoreTreeElements.set(objectStore.name, treeElement);
        this.appendChild(treeElement);
      }
      treeElement.update(objectStore, entriesUpdated);
    }
    for (const objectStoreName of this.idbObjectStoreTreeElements.keys()) {
      if (!objectStoreNames.has(objectStoreName)) {
        this.objectStoreRemoved(objectStoreName);
      }
    }

    if (this.view) {
      this.view.update(database);
    }

    this.updateTooltip();
  }

  private updateTooltip(): void {
    const version = this.database ? this.database.version : '-';
    if (Object.keys(this.idbObjectStoreTreeElements).length === 0) {
      this.tooltip = i18nString(UIStrings.versionSEmpty, {PH1: version});
    } else {
      this.tooltip = i18nString(UIStrings.versionS, {PH1: version});
    }
  }

  onselect(selectedByUser?: boolean): boolean {
    super.onselect(selectedByUser);
    if (!this.database) {
      return false;
    }
    if (!this.view) {
      this.view = new IDBDatabaseView(this.model, this.database);
    }

    this.showView(this.view);
    return false;
  }

  private objectStoreRemoved(objectStoreName: string): void {
    const objectStoreTreeElement = this.idbObjectStoreTreeElements.get(objectStoreName);
    if (objectStoreTreeElement) {
      objectStoreTreeElement.clear();
      this.removeChild(objectStoreTreeElement);
    }
    this.idbObjectStoreTreeElements.delete(objectStoreName);
    this.updateTooltip();
  }

  clear(): void {
    for (const objectStoreName of this.idbObjectStoreTreeElements.keys()) {
      this.objectStoreRemoved(objectStoreName);
    }
  }
}

export class IDBObjectStoreTreeElement extends ApplicationPanelTreeElement {
  private model: IndexedDBModel;
  private databaseId: DatabaseId;
  private readonly idbIndexTreeElements: Map<string, IDBIndexTreeElement>;
  private objectStore: ObjectStore;
  private view: IDBDataView|null;

  constructor(storagePanel: ResourcesPanel, model: IndexedDBModel, databaseId: DatabaseId, objectStore: ObjectStore) {
    super(storagePanel, objectStore.name, false);
    this.model = model;
    this.databaseId = databaseId;
    this.idbIndexTreeElements = new Map();
    this.objectStore = objectStore;
    this.view = null;
    const icon = UI.Icon.Icon.create('mediumicon-table', 'resource-tree-item');
    this.setLeadingIcons([icon]);
  }

  get itemURL(): string {
    return 'indexedDB://' + this.databaseId.securityOrigin + '/' + this.databaseId.name + '/' + this.objectStore.name;
  }

  onattach(): void {
    super.onattach();
    this.listItemElement.addEventListener('contextmenu', this.handleContextMenuEvent.bind(this), true);
  }

  markNeedsRefresh(): void {
    if (this.view) {
      this.view.markNeedsRefresh();
    }
    for (const treeElement of this.idbIndexTreeElements.values()) {
      treeElement.markNeedsRefresh();
    }
  }

  private handleContextMenuEvent(event: MouseEvent): void {
    const contextMenu = new UI.ContextMenu.ContextMenu(event);
    contextMenu.defaultSection().appendItem(i18nString(UIStrings.clear), this.clearObjectStore.bind(this));
    contextMenu.show();
  }

  private refreshObjectStore(): void {
    if (this.view) {
      this.view.refreshData();
    }
    for (const treeElement of this.idbIndexTreeElements.values()) {
      treeElement.refreshIndex();
    }
  }

  private async clearObjectStore(): Promise<void> {
    await this.model.clearObjectStore(this.databaseId, this.objectStore.name);
    this.update(this.objectStore, true);
  }

  update(objectStore: ObjectStore, entriesUpdated: boolean): void {
    this.objectStore = objectStore;

    const indexNames = new Set<string>();
    for (const index of this.objectStore.indexes.values()) {
      indexNames.add(index.name);
      let treeElement = this.idbIndexTreeElements.get(index.name);
      if (!treeElement) {
        treeElement = new IDBIndexTreeElement(
            this.resourcesPanel, this.model, this.databaseId, this.objectStore, index,
            this.refreshObjectStore.bind(this));
        this.idbIndexTreeElements.set(index.name, treeElement);
        this.appendChild(treeElement);
      }
      treeElement.update(this.objectStore, index, entriesUpdated);
    }
    for (const indexName of this.idbIndexTreeElements.keys()) {
      if (!indexNames.has(indexName)) {
        this.indexRemoved(indexName);
      }
    }
    for (const [indexName, treeElement] of this.idbIndexTreeElements.entries()) {
      if (!indexNames.has(indexName)) {
        this.removeChild((treeElement as IDBIndexTreeElement));
        this.idbIndexTreeElements.delete((indexName as string));
      }
    }

    if (this.childCount()) {
      this.expand();
    }

    if (this.view && entriesUpdated) {
      this.view.update(this.objectStore, null);
    }

    this.updateTooltip();
  }

  private updateTooltip(): void {
    const keyPathString = this.objectStore.keyPathString;
    let tooltipString = keyPathString !== null ? i18nString(UIStrings.keyPathS, {PH1: keyPathString}) : '';
    if (this.objectStore.autoIncrement) {
      tooltipString += '\n' + i18n.i18n.lockedString('autoIncrement');
    }
    this.tooltip = tooltipString;
  }

  onselect(selectedByUser?: boolean): boolean {
    super.onselect(selectedByUser);
    if (!this.view) {
      this.view =
          new IDBDataView(this.model, this.databaseId, this.objectStore, null, this.refreshObjectStore.bind(this));
    }

    this.showView(this.view);
    return false;
  }

  private indexRemoved(indexName: string): void {
    const indexTreeElement = this.idbIndexTreeElements.get(indexName);
    if (indexTreeElement) {
      indexTreeElement.clear();
      this.removeChild(indexTreeElement);
    }
    this.idbIndexTreeElements.delete(indexName);
  }

  clear(): void {
    for (const indexName of this.idbIndexTreeElements.keys()) {
      this.indexRemoved(indexName);
    }
    if (this.view) {
      this.view.clear();
    }
  }
}

export class IDBIndexTreeElement extends ApplicationPanelTreeElement {
  private model: IndexedDBModel;
  private databaseId: DatabaseId;
  private objectStore: ObjectStore;
  private index: Index;
  private refreshObjectStore: () => void;
  private view?: IDBDataView;

  constructor(
      storagePanel: ResourcesPanel, model: IndexedDBModel, databaseId: DatabaseId, objectStore: ObjectStore,
      index: Index, refreshObjectStore: () => void) {
    super(storagePanel, index.name, false);
    this.model = model;
    this.databaseId = databaseId;
    this.objectStore = objectStore;
    this.index = index;
    this.refreshObjectStore = refreshObjectStore;
  }

  get itemURL(): string {
    return 'indexedDB://' + this.databaseId.securityOrigin + '/' + this.databaseId.name + '/' + this.objectStore.name +
        '/' + this.index.name;
  }

  markNeedsRefresh(): void {
    if (this.view) {
      this.view.markNeedsRefresh();
    }
  }

  refreshIndex(): void {
    if (this.view) {
      this.view.refreshData();
    }
  }

  update(objectStore: ObjectStore, index: Index, entriesUpdated: boolean): void {
    this.objectStore = objectStore;
    this.index = index;

    if (this.view && entriesUpdated) {
      this.view.update(this.objectStore, this.index);
    }

    this.updateTooltip();
  }

  private updateTooltip(): void {
    const tooltipLines = [];
    const keyPathString = this.index.keyPathString;
    tooltipLines.push(i18nString(UIStrings.keyPathS, {PH1: keyPathString}));
    if (this.index.unique) {
      tooltipLines.push(i18n.i18n.lockedString('unique'));
    }
    if (this.index.multiEntry) {
      tooltipLines.push(i18n.i18n.lockedString('multiEntry'));
    }
    this.tooltip = tooltipLines.join('\n');
  }

  onselect(selectedByUser?: boolean): boolean {
    super.onselect(selectedByUser);
    if (!this.view) {
      this.view = new IDBDataView(this.model, this.databaseId, this.objectStore, this.index, this.refreshObjectStore);
    }

    this.showView(this.view);
    return false;
  }

  clear(): void {
    if (this.view) {
      this.view.clear();
    }
  }
}

export class DOMStorageTreeElement extends ApplicationPanelTreeElement {
  private readonly domStorage: DOMStorage;
  constructor(storagePanel: ResourcesPanel, domStorage: DOMStorage) {
    super(
        storagePanel, domStorage.securityOrigin ? domStorage.securityOrigin : i18nString(UIStrings.localFiles), false);
    this.domStorage = domStorage;
    const icon = UI.Icon.Icon.create('mediumicon-table', 'resource-tree-item');
    this.setLeadingIcons([icon]);
  }

  get itemURL(): string {
    return 'storage://' + this.domStorage.securityOrigin + '/' + (this.domStorage.isLocalStorage ? 'local' : 'session');
  }

  onselect(selectedByUser?: boolean): boolean {
    super.onselect(selectedByUser);
    this.resourcesPanel.showDOMStorage(this.domStorage);
    return false;
  }

  onattach(): void {
    super.onattach();
    this.listItemElement.addEventListener('contextmenu', this.handleContextMenuEvent.bind(this), true);
  }

  private handleContextMenuEvent(event: MouseEvent): void {
    const contextMenu = new UI.ContextMenu.ContextMenu(event);
    contextMenu.defaultSection().appendItem(i18nString(UIStrings.clear), () => this.domStorage.clear());
    contextMenu.show();
  }
}

export class CookieTreeElement extends ApplicationPanelTreeElement {
  private readonly target: SDK.Target.Target;
  private readonly cookieDomainInternal: string;

  constructor(storagePanel: ResourcesPanel, frame: SDK.ResourceTreeModel.ResourceTreeFrame, cookieDomain: string) {
    super(storagePanel, cookieDomain ? cookieDomain : i18nString(UIStrings.localFiles), false);
    this.target = frame.resourceTreeModel().target();
    this.cookieDomainInternal = cookieDomain;
    this.tooltip = i18nString(UIStrings.cookiesUsedByFramesFromS, {PH1: cookieDomain});
    const icon = UI.Icon.Icon.create('mediumicon-cookie', 'resource-tree-item');
    this.setLeadingIcons([icon]);
  }

  get itemURL(): string {
    return 'cookies://' + this.cookieDomainInternal;
  }

  cookieDomain(): string {
    return this.cookieDomainInternal;
  }

  onattach(): void {
    super.onattach();
    this.listItemElement.addEventListener('contextmenu', this.handleContextMenuEvent.bind(this), true);
  }

  private handleContextMenuEvent(event: Event): void {
    const contextMenu = new UI.ContextMenu.ContextMenu(event);
    contextMenu.defaultSection().appendItem(
        i18nString(UIStrings.clear), () => this.resourcesPanel.clearCookies(this.target, this.cookieDomainInternal));
    contextMenu.show();
  }

  onselect(selectedByUser?: boolean): boolean {
    super.onselect(selectedByUser);
    this.resourcesPanel.showCookies(this.target, this.cookieDomainInternal);
    return false;
  }
}

export class StorageCategoryView extends UI.Widget.VBox {
  private emptyWidget: UI.EmptyWidget.EmptyWidget;
  private linkElement: HTMLElement|null;

  constructor() {
    super();

    this.element.classList.add('storage-view');
    this.emptyWidget = new UI.EmptyWidget.EmptyWidget('');
    this.linkElement = null;
    this.emptyWidget.show(this.element);
  }

  setText(text: string): void {
    this.emptyWidget.text = text;
  }

  setLink(link: string|null): void {
    if (link && !this.linkElement) {
      this.linkElement = this.emptyWidget.appendLink(link);
    }
    if (!link && this.linkElement) {
      this.linkElement.classList.add('hidden');
    }
    if (link && this.linkElement) {
      this.linkElement.setAttribute('href', link);
      this.linkElement.classList.remove('hidden');
    }
  }
}

export class ResourcesSection implements SDK.TargetManager.Observer {
  panel: ResourcesPanel;
  private readonly treeElement: UI.TreeOutline.TreeElement;
  private treeElementForFrameId: Map<string, FrameTreeElement>;
  private treeElementForTargetId: Map<string, FrameTreeElement>;

  constructor(storagePanel: ResourcesPanel, treeElement: UI.TreeOutline.TreeElement) {
    this.panel = storagePanel;
    this.treeElement = treeElement;
    UI.ARIAUtils.setAccessibleName(this.treeElement.listItemNode, 'Resources Section');
    this.treeElementForFrameId = new Map();
    this.treeElementForTargetId = new Map();

    const frameManager = SDK.FrameManager.FrameManager.instance();
    frameManager.addEventListener(
        SDK.FrameManager.Events.FrameAddedToTarget, event => this.frameAdded(event.data.frame), this);
    frameManager.addEventListener(
        SDK.FrameManager.Events.FrameRemoved, event => this.frameDetached(event.data.frameId), this);
    frameManager.addEventListener(
        SDK.FrameManager.Events.FrameNavigated, event => this.frameNavigated(event.data.frame), this);
    frameManager.addEventListener(
        SDK.FrameManager.Events.ResourceAdded, event => this.resourceAdded(event.data.resource), this);

    SDK.TargetManager.TargetManager.instance().addModelListener(
        SDK.ChildTargetManager.ChildTargetManager, SDK.ChildTargetManager.Events.TargetCreated, this.windowOpened,
        this);
    SDK.TargetManager.TargetManager.instance().addModelListener(
        SDK.ChildTargetManager.ChildTargetManager, SDK.ChildTargetManager.Events.TargetInfoChanged, this.windowChanged,
        this);
    SDK.TargetManager.TargetManager.instance().addModelListener(
        SDK.ChildTargetManager.ChildTargetManager, SDK.ChildTargetManager.Events.TargetDestroyed, this.windowDestroyed,
        this);

    SDK.TargetManager.TargetManager.instance().observeTargets(this);

    for (const frame of frameManager.getAllFrames()) {
      if (!this.treeElementForFrameId.get(frame.id)) {
        this.addFrameAndParents(frame);
      }
      const childTargetManager = frame.resourceTreeModel().target().model(SDK.ChildTargetManager.ChildTargetManager);
      if (childTargetManager) {
        for (const targetInfo of childTargetManager.targetInfos()) {
          this.windowOpened({data: targetInfo});
        }
      }
    }
  }

  targetAdded(target: SDK.Target.Target): void {
    if (target.type() === SDK.Target.Type.Worker || target.type() === SDK.Target.Type.ServiceWorker) {
      this.workerAdded(target);
    }
  }

  private async workerAdded(target: SDK.Target.Target): Promise<void> {
    const parentTarget = target.parentTarget();
    if (!parentTarget) {
      return;
    }
    const parentTargetId = parentTarget.id();
    const frameTreeElement = this.treeElementForTargetId.get(parentTargetId);
    const targetId = target.id();
    assertNotMainTarget(targetId);
    const {targetInfo} = await parentTarget.targetAgent().invoke_getTargetInfo({targetId});
    if (frameTreeElement && targetInfo) {
      frameTreeElement.workerCreated(targetInfo);
    }
  }

  targetRemoved(_target: SDK.Target.Target): void {
  }

  private addFrameAndParents(frame: SDK.ResourceTreeModel.ResourceTreeFrame): void {
    const parentFrame = frame.parentFrame();
    if (parentFrame && !this.treeElementForFrameId.get(parentFrame.id)) {
      this.addFrameAndParents(parentFrame);
    }
    this.frameAdded(frame);
  }

  private expandFrame(frame: SDK.ResourceTreeModel.ResourceTreeFrame|null): boolean {
    if (!frame) {
      return false;
    }
    let treeElement = this.treeElementForFrameId.get(frame.id);
    if (!treeElement && !this.expandFrame(frame.parentFrame())) {
      return false;
    }
    treeElement = this.treeElementForFrameId.get(frame.id);
    if (!treeElement) {
      return false;
    }
    treeElement.expand();
    return true;
  }

  async revealResource(resource: SDK.Resource.Resource, line?: number, column?: number): Promise<void> {
    if (!this.expandFrame(resource.frame())) {
      return;
    }
    const resourceTreeElement = FrameResourceTreeElement.forResource(resource);
    if (resourceTreeElement) {
      await resourceTreeElement.revealResource(line, column);
    }
  }

  revealAndSelectFrame(frame: SDK.ResourceTreeModel.ResourceTreeFrame): void {
    const frameTreeElement = this.treeElementForFrameId.get(frame.id);
    frameTreeElement?.reveal();
    frameTreeElement?.select();
  }

  private frameAdded(frame: SDK.ResourceTreeModel.ResourceTreeFrame): void {
    const parentFrame = frame.parentFrame();
    const parentTreeElement = parentFrame ? this.treeElementForFrameId.get(parentFrame.id) : this.treeElement;
    if (!parentTreeElement) {
      return;
    }

    const existingElement = this.treeElementForFrameId.get(frame.id);
    if (existingElement) {
      this.treeElementForFrameId.delete(frame.id);
      if (existingElement.parent) {
        existingElement.parent.removeChild(existingElement);
      }
    }

    const frameTreeElement = new FrameTreeElement(this, frame);
    this.treeElementForFrameId.set(frame.id, frameTreeElement);
    const targetId = frame.resourceTreeModel().target().id();
    if (!this.treeElementForTargetId.get(targetId)) {
      this.treeElementForTargetId.set(targetId, frameTreeElement);
    }
    parentTreeElement.appendChild(frameTreeElement);

    for (const resource of frame.resources()) {
      this.resourceAdded(resource);
    }
  }

  private frameDetached(frameId: Protocol.Page.FrameId): void {
    const frameTreeElement = this.treeElementForFrameId.get(frameId);
    if (!frameTreeElement) {
      return;
    }

    this.treeElementForFrameId.delete(frameId);
    if (frameTreeElement.parent) {
      frameTreeElement.parent.removeChild(frameTreeElement);
    }
  }

  private frameNavigated(frame: SDK.ResourceTreeModel.ResourceTreeFrame): void {
    const frameTreeElement = this.treeElementForFrameId.get(frame.id);
    if (frameTreeElement) {
      frameTreeElement.frameNavigated(frame);
    }
  }

  private resourceAdded(resource: SDK.Resource.Resource): void {
    if (!resource.frameId) {
      return;
    }
    const frameTreeElement = this.treeElementForFrameId.get(resource.frameId);
    if (!frameTreeElement) {
      // This is a frame's main resource, it will be retained
      // and re-added by the resource manager;
      return;
    }
    frameTreeElement.appendResource(resource);
  }

  private windowOpened(event: Common.EventTarget.EventTargetEvent<Protocol.Target.TargetInfo>): void {
    const targetInfo = event.data;
    // Events for DevTools windows are ignored because they do not have an openerId
    if (targetInfo.openerId && targetInfo.type === 'page') {
      const frameTreeElement = this.treeElementForFrameId.get(targetInfo.openerId);
      if (frameTreeElement) {
        this.treeElementForTargetId.set(targetInfo.targetId, frameTreeElement);
        frameTreeElement.windowOpened(targetInfo);
      }
    }
  }

  private windowDestroyed(event: Common.EventTarget.EventTargetEvent<Protocol.Target.TargetID>): void {
    const targetId = event.data;
    const frameTreeElement = this.treeElementForTargetId.get(targetId);
    if (frameTreeElement) {
      frameTreeElement.windowDestroyed(targetId);
      this.treeElementForTargetId.delete(targetId);
    }
  }

  private windowChanged(event: Common.EventTarget.EventTargetEvent<Protocol.Target.TargetInfo>): void {
    const targetInfo = event.data;
    // Events for DevTools windows are ignored because they do not have an openerId
    if (targetInfo.openerId && targetInfo.type === 'page') {
      const frameTreeElement = this.treeElementForFrameId.get(targetInfo.openerId);
      if (frameTreeElement) {
        frameTreeElement.windowChanged(targetInfo);
      }
    }
  }

  reset(): void {
    this.treeElement.removeChildren();
    this.treeElementForFrameId.clear();
    this.treeElementForTargetId.clear();
  }
}

export class FrameTreeElement extends ApplicationPanelTreeElement {
  private section: ResourcesSection;
  private frame: SDK.ResourceTreeModel.ResourceTreeFrame;
  private frameId: string;
  private readonly categoryElements: Map<string, ExpandableApplicationPanelTreeElement>;
  private readonly treeElementForResource: Map<string, FrameResourceTreeElement>;
  private treeElementForWindow: Map<Protocol.Target.TargetID, FrameWindowTreeElement>;
  private treeElementForWorker: Map<Protocol.Target.TargetID, WorkerTreeElement>;
  private view: ApplicationComponents.FrameDetailsView.FrameDetailsView|null;

  constructor(section: ResourcesSection, frame: SDK.ResourceTreeModel.ResourceTreeFrame) {
    super(section.panel, '', false);
    this.section = section;
    this.frame = frame;
    this.frameId = frame.id;
    this.categoryElements = new Map();
    this.treeElementForResource = new Map();
    this.treeElementForWindow = new Map();
    this.treeElementForWorker = new Map();
    this.frameNavigated(frame);
    this.view = null;
  }

  getIconTypeForFrame(frame: SDK.ResourceTreeModel.ResourceTreeFrame): 'mediumicon-frame-blocked'|'mediumicon-frame'|
      'mediumicon-frame-embedded-blocked'|'mediumicon-frame-embedded' {
    if (frame.isTopFrame()) {
      return frame.unreachableUrl() ? 'mediumicon-frame-blocked' : 'mediumicon-frame';
    }
    return frame.unreachableUrl() ? 'mediumicon-frame-embedded-blocked' : 'mediumicon-frame-embedded';
  }

  async frameNavigated(frame: SDK.ResourceTreeModel.ResourceTreeFrame): Promise<void> {
    const icon = UI.Icon.Icon.create(this.getIconTypeForFrame(frame));
    if (frame.unreachableUrl()) {
      icon.classList.add('red-icon');
    }
    this.setLeadingIcons([icon]);
    this.invalidateChildren();

    this.frameId = frame.id;
    if (this.title !== frame.displayName()) {
      this.title = frame.displayName();
      UI.ARIAUtils.setAccessibleName(this.listItemElement, this.title);
      if (this.parent) {
        const parent = this.parent;
        // Insert frame at new position to preserve correct alphabetical order
        parent.removeChild(this);
        parent.appendChild(this);
      }
    }
    this.categoryElements.clear();
    this.treeElementForResource.clear();
    this.treeElementForWorker.clear();

    if (this.selected) {
      this.view = new ApplicationComponents.FrameDetailsView.FrameDetailsView(this.frame);
      this.showView(this.view);
    } else {
      this.view = null;
    }

    // Service Workers' parent is always the top frame. We need to reconstruct
    // the service worker tree elements after those navigations which allow
    // the service workers to stay alive.
    if (frame.isTopFrame()) {
      const targets = SDK.TargetManager.TargetManager.instance().targets();
      for (const target of targets) {
        if (target.type() === SDK.Target.Type.ServiceWorker) {
          const targetId = target.id();
          assertNotMainTarget(targetId);
          const agent = frame.resourceTreeModel().target().targetAgent();
          const targetInfo = (await agent.invoke_getTargetInfo({targetId})).targetInfo;
          this.workerCreated(targetInfo);
        }
      }
    }
  }

  get itemURL(): string {
    // This is used to persist over reloads/navigation which frame was selected.
    // A frame's title can change on DevTools refresh, so we resort to using
    // the URL instead (even though it is not guaranteed to be unique).
    if (this.frame.isTopFrame()) {
      return 'frame://';
    }
    return 'frame://' + encodeURI(this.frame.url);
  }

  onselect(selectedByUser?: boolean): boolean {
    super.onselect(selectedByUser);
    if (!this.view) {
      this.view = new ApplicationComponents.FrameDetailsView.FrameDetailsView(this.frame);
    } else {
      this.view.update();
    }
    this.showView(this.view);

    this.listItemElement.classList.remove('hovered');
    SDK.OverlayModel.OverlayModel.hideDOMNodeHighlight();
    return false;
  }

  set hovered(hovered: boolean) {
    if (hovered) {
      this.listItemElement.classList.add('hovered');
      this.frame.highlight();
    } else {
      this.listItemElement.classList.remove('hovered');
      SDK.OverlayModel.OverlayModel.hideDOMNodeHighlight();
    }
  }

  appendResource(resource: SDK.Resource.Resource): void {
    const statusCode = resource.statusCode();
    if (statusCode >= 301 && statusCode <= 303) {
      return;
    }

    const resourceType = resource.resourceType();
    const categoryName = resourceType.name();
    let categoryElement =
        resourceType === Common.ResourceType.resourceTypes.Document ? this : this.categoryElements.get(categoryName);
    if (!categoryElement) {
      categoryElement = new ExpandableApplicationPanelTreeElement(
          this.section.panel, resource.resourceType().category().title(), categoryName, categoryName === 'Frames');
      this.categoryElements.set(resourceType.name(), categoryElement);
      this.appendChild(categoryElement, FrameTreeElement.presentationOrderCompare);
    }
    const resourceTreeElement = new FrameResourceTreeElement(this.section.panel, resource);
    categoryElement.appendChild(resourceTreeElement, FrameTreeElement.presentationOrderCompare);
    this.treeElementForResource.set(resource.url, resourceTreeElement);
  }

  windowOpened(targetInfo: Protocol.Target.TargetInfo): void {
    const categoryKey = 'OpenedWindows';
    let categoryElement = this.categoryElements.get(categoryKey);
    if (!categoryElement) {
      categoryElement = new ExpandableApplicationPanelTreeElement(
          this.section.panel, i18nString(UIStrings.openedWindows), categoryKey);
      this.categoryElements.set(categoryKey, categoryElement);
      this.appendChild(categoryElement, FrameTreeElement.presentationOrderCompare);
    }
    if (!this.treeElementForWindow.get(targetInfo.targetId)) {
      const windowTreeElement = new FrameWindowTreeElement(this.section.panel, targetInfo);
      categoryElement.appendChild(windowTreeElement);
      this.treeElementForWindow.set(targetInfo.targetId, windowTreeElement);
    }
  }

  workerCreated(targetInfo: Protocol.Target.TargetInfo): void {
    const categoryKey = targetInfo.type === 'service_worker' ? 'Service Workers' : 'Web Workers';
    const categoryName = targetInfo.type === 'service_worker' ? i18n.i18n.lockedString('Service Workers') :
                                                                i18nString(UIStrings.webWorkers);
    let categoryElement = this.categoryElements.get(categoryKey);
    if (!categoryElement) {
      categoryElement = new ExpandableApplicationPanelTreeElement(this.section.panel, categoryName, categoryKey);
      this.categoryElements.set(categoryKey, categoryElement);
      this.appendChild(categoryElement, FrameTreeElement.presentationOrderCompare);
    }
    if (!this.treeElementForWorker.get(targetInfo.targetId)) {
      const workerTreeElement = new WorkerTreeElement(this.section.panel, targetInfo);
      categoryElement.appendChild(workerTreeElement);
      this.treeElementForWorker.set(targetInfo.targetId, workerTreeElement);
    }
  }

  windowChanged(targetInfo: Protocol.Target.TargetInfo): void {
    const windowTreeElement = this.treeElementForWindow.get(targetInfo.targetId);
    if (!windowTreeElement) {
      return;
    }
    if (windowTreeElement.title !== targetInfo.title) {
      windowTreeElement.title = targetInfo.title;
    }
    windowTreeElement.update(targetInfo);
  }

  windowDestroyed(targetId: Protocol.Target.TargetID): void {
    const windowTreeElement = this.treeElementForWindow.get(targetId);
    if (windowTreeElement) {
      windowTreeElement.windowClosed();
    }
  }

  appendChild(
      treeElement: UI.TreeOutline.TreeElement,
      comparator: ((arg0: UI.TreeOutline.TreeElement, arg1: UI.TreeOutline.TreeElement) => number)|
      undefined = FrameTreeElement.presentationOrderCompare): void {
    super.appendChild(treeElement, comparator);
  }

  /**
   * Order elements by type (first frames, then resources, last Document resources)
   * and then each of these groups in the alphabetical order.
   */
  private static presentationOrderCompare(
      treeElement1: UI.TreeOutline.TreeElement, treeElement2: UI.TreeOutline.TreeElement): number {
    function typeWeight(treeElement: UI.TreeOutline.TreeElement): number {
      if (treeElement instanceof ExpandableApplicationPanelTreeElement) {
        return 2;
      }
      if (treeElement instanceof FrameTreeElement) {
        return 1;
      }
      return 3;
    }

    const typeWeight1 = typeWeight(treeElement1);
    const typeWeight2 = typeWeight(treeElement2);
    return typeWeight1 - typeWeight2 || treeElement1.titleAsText().localeCompare(treeElement2.titleAsText());
  }
}

const resourceToFrameResourceTreeElement = new WeakMap<SDK.Resource.Resource, FrameResourceTreeElement>();

export class FrameResourceTreeElement extends ApplicationPanelTreeElement {
  private readonly panel: ResourcesPanel;
  private resource: SDK.Resource.Resource;
  private previewPromise: Promise<UI.Widget.Widget>|null;

  constructor(storagePanel: ResourcesPanel, resource: SDK.Resource.Resource) {
    super(
        storagePanel, resource.isGenerated ? i18nString(UIStrings.documentNotAvailable) : resource.displayName, false);
    this.panel = storagePanel;
    this.resource = resource;
    this.previewPromise = null;
    this.tooltip = resource.url;
    resourceToFrameResourceTreeElement.set(this.resource, this);

    const icon = UI.Icon.Icon.create('mediumicon-manifest', 'navigator-file-tree-item');
    icon.classList.add('navigator-' + resource.resourceType().name() + '-tree-item');
    this.setLeadingIcons([icon]);
  }

  static forResource(resource: SDK.Resource.Resource): FrameResourceTreeElement|undefined {
    return resourceToFrameResourceTreeElement.get(resource);
  }

  get itemURL(): string {
    return this.resource.url;
  }

  private preparePreview(): Promise<UI.Widget.Widget> {
    if (this.previewPromise) {
      return this.previewPromise;
    }
    const viewPromise = SourceFrame.PreviewFactory.PreviewFactory.createPreview(this.resource, this.resource.mimeType);
    this.previewPromise = viewPromise.then(view => {
      if (view) {
        return view;
      }
      return new UI.EmptyWidget.EmptyWidget(this.resource.url);
    });
    return this.previewPromise;
  }

  onselect(selectedByUser?: boolean): boolean {
    super.onselect(selectedByUser);
    if (this.resource.isGenerated) {
      this.panel.showCategoryView(i18nString(UIStrings.theContentOfThisDocumentHasBeen), null);
    } else {
      this.panel.scheduleShowView(this.preparePreview());
    }
    return false;
  }

  ondblclick(_event: Event): boolean {
    Host.InspectorFrontendHost.InspectorFrontendHostInstance.openInNewTab(this.resource.url);
    return false;
  }

  onattach(): void {
    super.onattach();
    this.listItemElement.draggable = true;
    this.listItemElement.addEventListener('dragstart', this.ondragstart.bind(this), false);
    this.listItemElement.addEventListener('contextmenu', this.handleContextMenuEvent.bind(this), true);
  }

  private ondragstart(event: DragEvent): boolean {
    if (!event.dataTransfer) {
      return false;
    }
    event.dataTransfer.setData('text/plain', this.resource.content || '');
    event.dataTransfer.effectAllowed = 'copy';
    return true;
  }

  private handleContextMenuEvent(event: MouseEvent): void {
    const contextMenu = new UI.ContextMenu.ContextMenu(event);
    contextMenu.appendApplicableItems(this.resource);
    contextMenu.show();
  }

  async revealResource(lineNumber?: number, columnNumber?: number): Promise<void> {
    this.revealAndSelect(true);
    const view = await this.panel.scheduleShowView(this.preparePreview());
    if (!(view instanceof SourceFrame.ResourceSourceFrame.ResourceSourceFrame) || typeof lineNumber !== 'number') {
      return;
    }
    view.revealPosition({lineNumber, columnNumber}, true);
  }
}

class FrameWindowTreeElement extends ApplicationPanelTreeElement {
  private targetInfo: Protocol.Target.TargetInfo;
  private isWindowClosed: boolean;
  private view: OpenedWindowDetailsView|null;

  constructor(storagePanel: ResourcesPanel, targetInfo: Protocol.Target.TargetInfo) {
    super(storagePanel, targetInfo.title || i18nString(UIStrings.windowWithoutTitle), false);
    this.targetInfo = targetInfo;
    this.isWindowClosed = false;
    this.view = null;
    this.updateIcon(targetInfo.canAccessOpener);
  }

  updateIcon(canAccessOpener: boolean): void {
    const iconType = canAccessOpener ? 'mediumicon-frame-opened' : 'mediumicon-frame';
    const icon = UI.Icon.Icon.create(iconType);
    this.setLeadingIcons([icon]);
  }

  update(targetInfo: Protocol.Target.TargetInfo): void {
    if (targetInfo.canAccessOpener !== this.targetInfo.canAccessOpener) {
      this.updateIcon(targetInfo.canAccessOpener);
    }
    this.targetInfo = targetInfo;
    if (this.view) {
      this.view.setTargetInfo(targetInfo);
      this.view.update();
    }
  }

  windowClosed(): void {
    this.listItemElement.classList.add('window-closed');
    this.isWindowClosed = true;
    if (this.view) {
      this.view.setIsWindowClosed(true);
      this.view.update();
    }
  }

  onselect(selectedByUser?: boolean): boolean {
    super.onselect(selectedByUser);
    if (!this.view) {
      this.view = new OpenedWindowDetailsView(this.targetInfo, this.isWindowClosed);
    } else {
      this.view.update();
    }
    this.showView(this.view);
    return false;
  }

  get itemURL(): string {
    return this.targetInfo.url;
  }
}

class WorkerTreeElement extends ApplicationPanelTreeElement {
  private targetInfo: Protocol.Target.TargetInfo;
  private view: WorkerDetailsView|null;

  constructor(storagePanel: ResourcesPanel, targetInfo: Protocol.Target.TargetInfo) {
    super(storagePanel, targetInfo.title || targetInfo.url || i18nString(UIStrings.worker), false);
    this.targetInfo = targetInfo;
    this.view = null;
    const icon = UI.Icon.Icon.create('mediumicon-service-worker', 'navigator-file-tree-item');
    this.setLeadingIcons([icon]);
  }

  onselect(selectedByUser?: boolean): boolean {
    super.onselect(selectedByUser);
    if (!this.view) {
      this.view = new WorkerDetailsView(this.targetInfo);
    } else {
      this.view.update();
    }
    this.showView(this.view);
    return false;
  }

  get itemURL(): string {
    return this.targetInfo.url;
  }
}
