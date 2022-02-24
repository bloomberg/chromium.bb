// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_searchable_drop_down/cr_searchable_drop_down.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/js/action_link.js';
import 'chrome://resources/cr_elements/action_link_css.m.js';
import 'chrome://resources/cr_elements/md_select_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../print_preview_utils.js';
import './destination_dialog_css.js';
import './destination_list.js';
import './print_preview_search_box.js';
import './print_preview_shared_css.js';
import './print_preview_vars_css.js';
import './provisional_destination_resolver.js';
import '../strings.m.js';
import './throbber_css.js';
import './destination_list_item.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {ListPropertyUpdateMixin} from 'chrome://resources/js/list_property_update_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {beforeNextRender, html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Destination, GooglePromotedDestinationId} from '../data/destination.js';
import {DestinationStore, DestinationStoreEventType} from '../data/destination_store.js';
import {PrintServerStore, PrintServerStoreEventType} from '../data/print_server_store.js';
import {DestinationSearchBucket, MetricsContext} from '../metrics.js';
import {NativeLayerImpl} from '../native_layer.js';
import {PrintServer, PrintServersConfig} from '../native_layer_cros.js';

import {PrintPreviewDestinationListItemElement} from './destination_list_item.js';
import {PrintPreviewSearchBoxElement} from './print_preview_search_box.js';
import {PrintPreviewProvisionalDestinationResolverElement} from './provisional_destination_resolver.js';

type PrintServersChangedEventDetail = {
  printServerNames: string[],
  isSingleServerFetchingMode: boolean,
};

export interface PrintPreviewDestinationDialogCrosElement {
  $: {
    dialog: CrDialogElement,
    provisionalResolver: PrintPreviewProvisionalDestinationResolverElement,
    searchBox: PrintPreviewSearchBoxElement,
  };
}

const PrintPreviewDestinationDialogCrosElementBase =
    ListPropertyUpdateMixin(WebUIListenerMixin(PolymerElement));

export class PrintPreviewDestinationDialogCrosElement extends
    PrintPreviewDestinationDialogCrosElementBase {
  static get is() {
    return 'print-preview-destination-dialog-cros';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      destinationStore: {
        type: Object,
        observer: 'onDestinationStoreSet_',
      },

      activeUser: {
        type: String,
        observer: 'onActiveUserChange_',
      },

      currentDestinationAccount: String,

      users: Array,

      printServerSelected_: {
        type: String,
        value: '',
        observer: 'onPrintServerSelected_',
      },

      destinations_: {
        type: Array,
        value: () => [],
      },

      loadingDestinations_: {
        type: Boolean,
        value: false,
      },

      metrics_: Object,

      searchQuery_: {
        type: Object,
        value: null,
      },

      isSingleServerFetchingMode_: {
        type: Boolean,
        value: false,
      },

      printServerNames_: {
        type: Array,
        value() {
          return [''];
        },
      },

      loadingServerPrinters_: {
        type: Boolean,
        value: false,
      },

      loadingAnyDestinations_: {
        type: Boolean,
        computed: 'computeLoadingDestinations_(' +
            'loadingDestinations_, loadingServerPrinters_)'
      },
    };
  }

  destinationStore: DestinationStore;
  activeUser: string;
  currentDestinationAccount: string;
  users: string[];
  private printServerSelected_: string;
  private destinations_: Destination[];
  private loadingDestinations_: boolean;
  private metrics_: MetricsContext;
  private searchQuery_: RegExp|null;
  private isSingleServerFetchingMode_: boolean;
  private printServerNames_: string[];
  private loadingServerPrinters_: boolean;
  private loadingAnyDestinations_: boolean;

  private tracker_: EventTracker = new EventTracker();
  private destinationInConfiguring_: Destination|null = null;
  private initialized_: boolean = false;
  private printServerStore_: PrintServerStore|null = null;

  disconnectedCallback() {
    super.disconnectedCallback();

    this.tracker_.removeAll();
  }

  ready() {
    super.ready();

    this.addEventListener('keydown', e => this.onKeydown_(e as KeyboardEvent));
    this.printServerStore_ = new PrintServerStore(
        (eventName: string, callback: (p: any) => void) =>
            this.addWebUIListener(eventName, callback));
    this.tracker_.add(
        this.printServerStore_, PrintServerStoreEventType.PRINT_SERVERS_CHANGED,
        this.onPrintServersChanged_.bind(this));
    this.tracker_.add(
        this.printServerStore_,
        PrintServerStoreEventType.SERVER_PRINTERS_LOADING,
        this.onServerPrintersLoading_.bind(this));
    this.printServerStore_.getPrintServersConfig().then(config => {
      this.printServerNames_ =
          config.printServers.map(printServer => printServer.name);
      this.isSingleServerFetchingMode_ = config.isSingleServerFetchingMode;
    });
    if (this.destinationStore) {
      this.printServerStore_.setDestinationStore(this.destinationStore);
    }
  }

  private fireAccountChange_(account: string) {
    this.dispatchEvent(new CustomEvent(
        'account-change', {bubbles: true, composed: true, detail: account}));
  }

  private onKeydown_(e: KeyboardEvent) {
    e.stopPropagation();
    const searchInput = this.$.searchBox.getSearchInput();
    if (e.key === 'Escape' &&
        (e.composedPath()[0] !== searchInput || !searchInput.value.trim())) {
      this.$.dialog.cancel();
      e.preventDefault();
    }
  }

  private onDestinationStoreSet_() {
    assert(this.destinations_.length === 0);
    this.tracker_.add(
        this.destinationStore, DestinationStoreEventType.DESTINATIONS_INSERTED,
        this.updateDestinations_.bind(this));
    this.tracker_.add(
        this.destinationStore,
        DestinationStoreEventType.DESTINATION_SEARCH_DONE,
        this.updateDestinations_.bind(this));
    this.initialized_ = true;
    if (this.printServerStore_) {
      this.printServerStore_.setDestinationStore(this.destinationStore);
    }
  }

  private onActiveUserChange_() {
    if (this.activeUser) {
      this.shadowRoot!.querySelector('select')!.value = this.activeUser;
    }

    this.updateDestinations_();
  }

  private updateDestinations_() {
    if (this.destinationStore === undefined || !this.initialized_) {
      return;
    }

    this.updateList(
        'destinations_', destination => destination.key,
        this.getDestinationList_());

    this.loadingDestinations_ =
        this.destinationStore.isPrintDestinationSearchInProgress;
  }

  private getDestinationList_(): Destination[] {
    // Filter out the 'Save to Drive' option so it is not shown in the
    // list of available options.
    return this.destinationStore.destinations(this.activeUser)
        .filter(
            destination =>
                destination.id !== GooglePromotedDestinationId.DOCS &&
                destination.id !==
                    GooglePromotedDestinationId.SAVE_TO_DRIVE_CROS);
  }

  private onCloseOrCancel_() {
    if (this.searchQuery_) {
      this.$.searchBox.setValue('');
    }
    const cancelled = this.$.dialog.getNative().returnValue !== 'success';
    this.metrics_.record(
        cancelled ? DestinationSearchBucket.DESTINATION_CLOSED_UNCHANGED :
                    DestinationSearchBucket.DESTINATION_CLOSED_CHANGED);
    if (this.currentDestinationAccount &&
        this.currentDestinationAccount !== this.activeUser) {
      this.fireAccountChange_(this.currentDestinationAccount);
    }
  }

  private onCancelButtonClick_() {
    this.$.dialog.cancel();
  }

  /**
   * @param e Event containing the selected destination list item element.
   */
  private onDestinationSelected_(
      e: CustomEvent<PrintPreviewDestinationListItemElement>) {
    const listItem = e.detail;
    const destination = listItem.destination;

    // ChromeOS local destinations that don't have capabilities need to be
    // configured before selecting, and provisional destinations need to be
    // resolved. Other destinations can be selected.
    if (destination.readyForSelection) {
      this.selectDestination_(destination);
      return;
    }

    // Provisional destinations
    if (destination.isProvisional) {
      this.$.provisionalResolver.resolveDestination(destination)
          .then(this.selectDestination_.bind(this))
          .catch(function() {
            console.warn(
                'Failed to resolve provisional destination: ' + destination.id);
          })
          .then(() => {
            if (this.$.dialog.open && listItem && !listItem.hidden) {
              listItem.focus();
            }
          });
      return;
    }

    // Destination must be a CrOS local destination that needs to be set up.
    // The user is only allowed to set up printer at one time.
    if (this.destinationInConfiguring_) {
      return;
    }

    // Show the configuring status to the user and resolve the destination.
    listItem.onConfigureRequestAccepted();
    this.destinationInConfiguring_ = destination;
    this.destinationStore.resolveCrosDestination(destination)
        .then(
            response => {
              this.destinationInConfiguring_ = null;
              listItem.onConfigureComplete(true);
              destination.capabilities = response.capabilities;
              this.selectDestination_(destination);
              // After destination is selected, start fetching for the EULA
              // URL.
              this.destinationStore.fetchEulaUrl(destination.id);
            },
            () => {
              this.destinationInConfiguring_ = null;
              listItem.onConfigureComplete(false);
            });
  }

  private selectDestination_(destination: Destination) {
    this.destinationStore.selectDestination(destination);
    this.$.dialog.close();
  }

  show() {
    if (!this.metrics_) {
      this.metrics_ = MetricsContext.destinationSearch();
    }
    this.$.dialog.showModal();
    this.loadingDestinations_ = this.destinationStore === undefined ||
        this.destinationStore.isPrintDestinationSearchInProgress;
    this.metrics_.record(DestinationSearchBucket.DESTINATION_SHOWN);
    if (this.activeUser) {
      beforeNextRender(assert(this.shadowRoot!.querySelector('select')), () => {
        this.shadowRoot!.querySelector('select')!.value = this.activeUser;
      });
    }
  }

  /** @return Whether the dialog is open. */
  isOpen(): boolean {
    return this.$.dialog.hasAttribute('open');
  }

  private onPrintServerSelected_(printServerName: string) {
    if (!this.printServerStore_) {
      return;
    }
    this.printServerStore_.choosePrintServers(printServerName);
  }

  /**
   * @param e Event containing the current print server names and fetching mode.
   */
  private onPrintServersChanged_(
      e: CustomEvent<PrintServersChangedEventDetail>) {
    this.isSingleServerFetchingMode_ = e.detail.isSingleServerFetchingMode;
    this.printServerNames_ = e.detail.printServerNames;
  }

  /**
   * @param e Event containing whether server printers are currently loading.
   */
  private onServerPrintersLoading_(e: CustomEvent<boolean>) {
    this.loadingServerPrinters_ = e.detail;
  }

  private computeLoadingDestinations_(): boolean {
    return this.loadingDestinations_ || this.loadingServerPrinters_;
  }

  private onUserChange_() {
    const select = this.shadowRoot!.querySelector('select')!;
    const account = select.value;
    if (account) {
      this.loadingDestinations_ = true;
      this.fireAccountChange_(account);
      this.metrics_.record(DestinationSearchBucket.ACCOUNT_CHANGED);
    } else {
      select.value = this.activeUser;
      NativeLayerImpl.getInstance().signIn();
      this.metrics_.record(DestinationSearchBucket.ADD_ACCOUNT_SELECTED);
    }
  }

  private onManageButtonClick_() {
    this.metrics_.record(DestinationSearchBucket.MANAGE_BUTTON_CLICKED);
    NativeLayerImpl.getInstance().managePrinters();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-destination-dialog-cros':
        PrintPreviewDestinationDialogCrosElement;
  }
}

customElements.define(
    PrintPreviewDestinationDialogCrosElement.is,
    PrintPreviewDestinationDialogCrosElement);
