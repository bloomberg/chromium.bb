// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Common from '../../core/common/common.js';
import * as SDK from '../../core/sdk/sdk.js';
import * as SourceFrame from '../../ui/legacy/components/source_frame/source_frame.js';
import * as UI from '../../ui/legacy/legacy.js';

import resourcesPanelStyles from './resourcesPanel.css.js';

import type {CookieTreeElement} from './ApplicationPanelSidebar.js';
import {ApplicationPanelSidebar, StorageCategoryView} from './ApplicationPanelSidebar.js';
import {CookieItemsView} from './CookieItemsView.js';
import {DatabaseQueryView} from './DatabaseQueryView.js';
import {DatabaseTableView} from './DatabaseTableView.js';
import {DOMStorageItemsView} from './DOMStorageItemsView.js';
import type {DOMStorage} from './DOMStorageModel.js';
import {StorageItemsView} from './StorageItemsView.js';

let resourcesPanelInstance: ResourcesPanel;

export class ResourcesPanel extends UI.Panel.PanelWithSidebar {
  private readonly resourcesLastSelectedItemSetting: Common.Settings.Setting<string[]>;
  visibleView: UI.Widget.Widget|null;
  private pendingViewPromise: Promise<UI.Widget.Widget>|null;
  private categoryView: StorageCategoryView|null;
  storageViews: HTMLElement;
  private readonly storageViewToolbar: UI.Toolbar.Toolbar;
  private domStorageView: DOMStorageItemsView|null;
  private cookieView: CookieItemsView|null;
  private readonly emptyWidget: UI.EmptyWidget.EmptyWidget|null;
  private readonly sidebar: ApplicationPanelSidebar;

  private constructor() {
    super('resources');

    this.resourcesLastSelectedItemSetting =
        Common.Settings.Settings.instance().createSetting('resourcesLastSelectedElementPath', []);

    this.visibleView = null;

    this.pendingViewPromise = null;

    this.categoryView = null;

    const mainContainer = new UI.Widget.VBox();
    this.storageViews = mainContainer.element.createChild('div', 'vbox flex-auto');
    this.storageViewToolbar = new UI.Toolbar.Toolbar('resources-toolbar', mainContainer.element);
    this.splitWidget().setMainWidget(mainContainer);

    this.domStorageView = null;

    this.cookieView = null;

    this.emptyWidget = null;

    this.sidebar = new ApplicationPanelSidebar(this);
    this.sidebar.show(this.panelSidebarElement());
  }

  static instance(opts: {
    forceNew: boolean|null,
  } = {forceNew: null}): ResourcesPanel {
    const {forceNew} = opts;
    if (!resourcesPanelInstance || forceNew) {
      resourcesPanelInstance = new ResourcesPanel();
    }

    return resourcesPanelInstance;
  }

  private static shouldCloseOnReset(view: UI.Widget.Widget): boolean {
    const viewClassesToClose = [
      SourceFrame.ResourceSourceFrame.ResourceSourceFrame,
      SourceFrame.ImageView.ImageView,
      SourceFrame.FontView.FontView,
      StorageItemsView,
      DatabaseQueryView,
      DatabaseTableView,
    ];
    return viewClassesToClose.some(type => view instanceof type);
  }

  static async showAndGetSidebar(): Promise<ApplicationPanelSidebar> {
    await UI.ViewManager.ViewManager.instance().showView('resources');
    return ResourcesPanel.instance().sidebar;
  }

  focus(): void {
    this.sidebar.focus();
  }

  lastSelectedItemPath(): string[] {
    return this.resourcesLastSelectedItemSetting.get();
  }

  setLastSelectedItemPath(path: string[]): void {
    this.resourcesLastSelectedItemSetting.set(path);
  }

  resetView(): void {
    if (this.visibleView && ResourcesPanel.shouldCloseOnReset(this.visibleView)) {
      this.showView(null);
    }
  }

  showView(view: UI.Widget.Widget|null): void {
    this.pendingViewPromise = null;
    if (this.visibleView === view) {
      return;
    }

    if (this.visibleView) {
      this.visibleView.detach();
    }

    if (view) {
      view.show(this.storageViews);
    }
    this.visibleView = view;

    this.storageViewToolbar.removeToolbarItems();
    this.storageViewToolbar.element.classList.toggle('hidden', true);
    if (view instanceof UI.View.SimpleView) {
      view.toolbarItems().then(items => {
        items.map(item => this.storageViewToolbar.appendToolbarItem(item));
        this.storageViewToolbar.element.classList.toggle('hidden', !items.length);
      });
    }
  }

  async scheduleShowView(viewPromise: Promise<UI.Widget.Widget>): Promise<UI.Widget.Widget|null> {
    this.pendingViewPromise = viewPromise;
    const view = await viewPromise;
    if (this.pendingViewPromise !== viewPromise) {
      return null;
    }
    this.showView(view);
    return view;
  }

  showCategoryView(categoryName: string, categoryLink: string|null): void {
    if (!this.categoryView) {
      this.categoryView = new StorageCategoryView();
    }
    this.categoryView.setText(categoryName);
    this.categoryView.setLink(categoryLink);
    this.showView(this.categoryView);
  }

  showDOMStorage(domStorage: DOMStorage): void {
    if (!domStorage) {
      return;
    }

    if (!this.domStorageView) {
      this.domStorageView = new DOMStorageItemsView(domStorage);
    } else {
      this.domStorageView.setStorage(domStorage);
    }
    this.showView(this.domStorageView);
  }

  showCookies(cookieFrameTarget: SDK.Target.Target, cookieDomain: string): void {
    const model = cookieFrameTarget.model(SDK.CookieModel.CookieModel);
    if (!model) {
      return;
    }
    if (!this.cookieView) {
      this.cookieView = new CookieItemsView(model, cookieDomain);
    } else {
      this.cookieView.setCookiesDomain(model, cookieDomain);
    }
    this.showView(this.cookieView);
  }

  clearCookies(target: SDK.Target.Target, cookieDomain: string): void {
    const model = (target.model(SDK.CookieModel.CookieModel) as SDK.CookieModel.CookieModel | null);
    if (!model) {
      return;
    }
    model.clear(cookieDomain).then(() => {
      if (this.cookieView) {
        this.cookieView.refreshItems();
      }
    });
  }
  wasShown(): void {
    super.wasShown();
    this.registerCSSFiles([resourcesPanelStyles]);
  }
}

let resourceRevealerInstance: ResourceRevealer;

export class ResourceRevealer implements Common.Revealer.Revealer {
  static instance(opts: {
    forceNew: boolean|null,
  } = {forceNew: null}): ResourceRevealer {
    const {forceNew} = opts;
    if (!resourceRevealerInstance || forceNew) {
      resourceRevealerInstance = new ResourceRevealer();
    }

    return resourceRevealerInstance;
  }

  async reveal(resource: Object): Promise<void> {
    if (!(resource instanceof SDK.Resource.Resource)) {
      throw new Error('Internal error: not a resource');
    }
    const sidebar = await ResourcesPanel.showAndGetSidebar();
    await sidebar.showResource(resource);
  }
}

let cookieReferenceRevealerInstance: CookieReferenceRevealer;

export class CookieReferenceRevealer implements Common.Revealer.Revealer {
  static instance(opts: {
    forceNew: boolean|null,
  } = {forceNew: null}): CookieReferenceRevealer {
    const {forceNew} = opts;
    if (!cookieReferenceRevealerInstance || forceNew) {
      cookieReferenceRevealerInstance = new CookieReferenceRevealer();
    }

    return cookieReferenceRevealerInstance;
  }
  async reveal(cookie: Object): Promise<void> {
    if (!(cookie instanceof SDK.Cookie.CookieReference)) {
      throw new Error('Internal error: not a cookie reference');
    }

    const sidebar = await ResourcesPanel.showAndGetSidebar();
    await sidebar.cookieListTreeElement.select();

    const contextUrl = cookie.contextUrl();
    if (contextUrl && await this.revealByDomain(sidebar, contextUrl)) {
      return;
    }
    // Fallback: try to reveal the cookie using its domain as context, which may not work, because the
    // Application Panel shows cookies grouped by context, see crbug.com/1060563.
    this.revealByDomain(sidebar, cookie.domain());
  }

  private async revealByDomain(sidebar: ApplicationPanelSidebar, domain: string): Promise<boolean> {
    const item = sidebar.cookieListTreeElement.children().find(
        c => /** @type {!CookieTreeElement} */ (c as CookieTreeElement).cookieDomain().endsWith(domain));
    if (item) {
      await item.revealAndSelect();
      return true;
    }
    return false;
  }
}

let frameDetailsRevealerInstance: FrameDetailsRevealer;

export class FrameDetailsRevealer implements Common.Revealer.Revealer {
  static instance(opts: {
    forceNew: boolean|null,
  } = {forceNew: null}): FrameDetailsRevealer {
    const {forceNew} = opts;
    if (!frameDetailsRevealerInstance || forceNew) {
      frameDetailsRevealerInstance = new FrameDetailsRevealer();
    }

    return frameDetailsRevealerInstance;
  }

  async reveal(frame: Object): Promise<void> {
    if (!(frame instanceof SDK.ResourceTreeModel.ResourceTreeFrame)) {
      throw new Error('Internal error: not a frame');
    }
    const sidebar = await ResourcesPanel.showAndGetSidebar();
    sidebar.showFrame(frame);
  }
}
