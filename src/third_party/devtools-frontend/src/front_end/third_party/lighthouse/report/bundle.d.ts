export type DOM_ = any;
export type ComponentName = '3pFilter' | 'audit' | 'categoryHeader' | 'chevron' | 'clump' | 'crc' | 'crcChain' | 'elementScreenshot' | 'envItem' | 'footer' | 'gauge' | 'gaugePwa' | 'heading' | 'metric' | 'metricsToggle' | 'opportunity' | 'opportunityHeader' | 'scorescale' | 'scoresWrapper' | 'snippet' | 'snippetContent' | 'snippetHeader' | 'snippetLine' | 'topbar' | 'warningsToplevel';
export type I18n<T> = any;
export type CRCSegment = {
    node: any[string];
    isLastChild: boolean;
    hasChildren: boolean;
    startTime: number;
    transferSize: number;
    treeMarkers: Array<boolean>;
};
export type LineDetails = {
    content: string;
    lineNumber: string | number;
    contentType: LineContentType;
    truncated?: boolean;
    visibility?: LineVisibility;
};
/**
 * @license
 * Copyright 2017 The Lighthouse Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS-IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
export class DOM {
    /**
     * @param {Document} document
     */
    constructor(document: Document);
    /** @type {Document} */
    _document: Document;
    /** @type {string} */
    _lighthouseChannel: string;
    /** @type {Map<string, DocumentFragment>} */
    _componentCache: Map<string, DocumentFragment>;
    /**
     * @template {string} T
     * @param {T} name
     * @param {string=} className
     * @return {HTMLElementByTagName[T]}
     */
    createElement<T extends string>(name: T, className?: string | undefined): any;
    /**
     * @param {string} namespaceURI
     * @param {string} name
     * @param {string=} className
     * @return {Element}
     */
    createElementNS(namespaceURI: string, name: string, className?: string | undefined): Element;
    /**
     * @return {!DocumentFragment}
     */
    createFragment(): DocumentFragment;
    /**
     * @template {string} T
     * @param {Element} parentElem
     * @param {T} elementName
     * @param {string=} className
     * @return {HTMLElementByTagName[T]}
     */
    createChildOf<T_1 extends string>(parentElem: Element, elementName: T_1, className?: string | undefined): any;
    /**
     * @param {import('./components.js').ComponentName} componentName
     * @return {!DocumentFragment} A clone of the cached component.
     */
    createComponent(componentName: any): DocumentFragment;
    /**
     * @param {string} text
     * @return {Element}
     */
    convertMarkdownLinkSnippets(text: string): Element;
    /**
     * Set link href, but safely, preventing `javascript:` protocol, etc.
     * @see https://github.com/google/safevalues/
     * @param {HTMLAnchorElement} elem
     * @param {string} url
     */
    safelySetHref(elem: HTMLAnchorElement, url: string): void;
    /**
     * Only create blob URLs for JSON & HTML
     * @param {HTMLAnchorElement} elem
     * @param {Blob} blob
     */
    safelySetBlobHref(elem: HTMLAnchorElement, blob: Blob): void;
    /**
     * @param {string} markdownText
     * @return {Element}
     */
    convertMarkdownCodeSnippets(markdownText: string): Element;
    /**
     * The channel to use for UTM data when rendering links to the documentation.
     * @param {string} lighthouseChannel
     */
    setLighthouseChannel(lighthouseChannel: string): void;
    /**
     * @return {Document}
     */
    document(): Document;
    /**
     * TODO(paulirish): import and conditionally apply the DevTools frontend subclasses instead of this
     * @return {boolean}
     */
    isDevTools(): boolean;
    /**
     * Guaranteed context.querySelector. Always returns an element or throws if
     * nothing matches query.
     * @template {string} T
     * @param {T} query
     * @param {ParentNode} context
     * @return {ParseSelector<T>}
     */
    find<T_2 extends string>(query: T_2, context: ParentNode): any;
    /**
     * Helper for context.querySelectorAll. Returns an Array instead of a NodeList.
     * @template {string} T
     * @param {T} query
     * @param {ParentNode} context
     */
    findAll<T_3 extends string>(query: T_3, context: ParentNode): Element[];
}
/**
 * @license
 * Copyright 2017 The Lighthouse Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS-IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Dummy text for ensuring report robustness: </script> pre$`post %%LIGHTHOUSE_JSON%%
 * (this is handled by terser)
 */
export class ReportRenderer {
    /**
     * @param {DOM} dom
     */
    constructor(dom: DOM);
    /** @type {DOM} */
    _dom: DOM;
    /**
     * @param {LH.Result} result
     * @param {Element} container Parent element to render the report into.
     * @return {!Element}
     */
    renderReport(result: any, container: Element): Element;
    /**
     * @param {ParentNode} _
     */
    setTemplateContext(_: ParentNode): void;
    /**
     * @param {LH.ReportResult} report
     * @return {DocumentFragment}
     */
    _renderReportTopbar(report: any): DocumentFragment;
    /**
     * @return {DocumentFragment}
     */
    _renderReportHeader(): DocumentFragment;
    /**
     * @param {LH.ReportResult} report
     * @return {DocumentFragment}
     */
    _renderReportFooter(report: any): DocumentFragment;
    /**
     * Returns a div with a list of top-level warnings, or an empty div if no warnings.
     * @param {LH.ReportResult} report
     * @return {Node}
     */
    _renderReportWarnings(report: any): Node;
    /**
     * @param {LH.ReportResult} report
     * @param {CategoryRenderer} categoryRenderer
     * @param {Record<string, CategoryRenderer>} specificCategoryRenderers
     * @return {!DocumentFragment[]}
     */
    _renderScoreGauges(report: any, categoryRenderer: CategoryRenderer, specificCategoryRenderers: Record<string, CategoryRenderer>): DocumentFragment[];
    /**
     * @param {LH.ReportResult} report
     * @return {!DocumentFragment}
     */
    _renderReport(report: any): DocumentFragment;
}
export class ReportUIFeatures {
    /**
     * The popup's window.name is keyed by version+url+fetchTime, so we reuse/select tabs correctly.
     * @param {LH.Result} json
     * @protected
     */
    protected static computeWindowNameSuffix(json: any): string;
    /**
     * Opens a new tab to the online viewer and sends the local page's JSON results
     * to the online viewer using URL.fragment
     * @param {LH.Result} json
     * @protected
     */
    protected static openViewer(json: any): void;
    /**
     * Opens a new tab to the treemap app and sends the JSON results using URL.fragment
     * @param {LH.Result} json
     */
    static openTreemap(json: any): void;
    /**
     * Opens a new tab to an external page and sends data using postMessage.
     * @param {{lhr: LH.Result} | LH.Treemap.Options} data
     * @param {string} url
     * @param {string} windowName
     * @protected
     */
    protected static openTabAndSendData(data: {
        lhr: any;
    } | any, url: string, windowName: string): void;
    /**
     * Opens a new tab to an external page and sends data via base64 encoded url params.
     * @param {{lhr: LH.Result} | LH.Treemap.Options} data
     * @param {string} url_
     * @param {string} windowName
     * @protected
     */
    protected static openTabWithUrlData(data: {
        lhr: any;
    } | any, url_: string, windowName: string): Promise<void>;
    /**
     * @param {DOM} dom
     */
    constructor(dom: DOM);
    /** @type {LH.Result} */
    json: any;
    /** @type {DOM} */
    _dom: DOM;
    /** @type {Document} */
    _document: Document;
    /** @type {DropDown} */
    _dropDown: DropDown;
    /** @type {boolean} */
    _copyAttempt: boolean;
    /** @type {HTMLElement} */
    topbarEl: HTMLElement;
    /** @type {HTMLElement} */
    scoreScaleEl: HTMLElement;
    /** @type {HTMLElement} */
    stickyHeaderEl: HTMLElement;
    /** @type {HTMLElement} */
    highlightEl: HTMLElement;
    /**
     * Handle media query change events.
     * @param {MediaQueryList|MediaQueryListEvent} mql
     */
    onMediaQueryChange(mql: MediaQueryList | MediaQueryListEvent): void;
    /**
     * Handle copy events.
     * @param {ClipboardEvent} e
     */
    onCopy(e: ClipboardEvent): void;
    /**
     * Handler for tool button.
     * @param {Event} e
     */
    onDropDownMenuClick(e: Event): void;
    /**
     * Keyup handler for the document.
     * @param {KeyboardEvent} e
     */
    onKeyUp(e: KeyboardEvent): void;
    /**
     * Collapses all audit `<details>`.
     * open a `<details>` element.
     */
    collapseAllDetails(): void;
    /**
     * Expands all audit `<details>`.
     * Ideally, a print stylesheet could take care of this, but CSS has no way to
     * open a `<details>` element.
     */
    expandAllDetails(): void;
    /**
     * @private
     * @param {boolean} [force]
     */
    private _toggleDarkTheme;
    _updateStickyHeaderOnScroll(): void;
    /**
     * Adds tools button, print, and other functionality to the report. The method
     * should be called whenever the report needs to be re-rendered.
     * @param {LH.Result} report
     */
    initFeatures(report: any): void;
    /**
     * @param {ParentNode} _
     */
    setTemplateContext(_: ParentNode): void;
    /**
     * @param {{container?: Element, text: string, icon?: string, onClick: () => void}} opts
     */
    addButton(opts: {
        container?: Element;
        text: string;
        icon?: string;
        onClick: () => void;
    }): any;
    /**
     * Finds the first scrollable ancestor of `element`. Falls back to the document.
     * @param {Element} element
     * @return {Node}
     */
    _getScrollParent(element: Element): Node;
    _enableFireworks(): void;
    /**
     * Fires a custom DOM event on target.
     * @param {string} name Name of the event.
     * @param {Node=} target DOM node to fire the event on.
     * @param {*=} detail Custom data to include.
     */
    _fireEventOn(name: string, target?: Node | undefined, detail?: any | undefined): void;
    _setupMediaQueryListeners(): void;
    _setupThirdPartyFilter(): void;
    /**
     * @param {Element} el
     */
    _setupElementScreenshotOverlay(el: Element): void;
    /**
     * From a table with URL entries, finds the rows containing third-party URLs
     * and returns them.
     * @param {HTMLElement[]} rowEls
     * @param {string} finalUrl
     * @return {Array<HTMLElement>}
     */
    _getThirdPartyRows(rowEls: HTMLElement[], finalUrl: string): Array<HTMLElement>;
    _setupStickyHeaderElements(): void;
    /**
     * Copies the report JSON to the clipboard (if supported by the browser).
     */
    onCopyButtonClick(): void;
    /**
     * Resets the state of page before capturing the page for export.
     * When the user opens the exported HTML page, certain UI elements should
     * be in their closed state (not opened) and the templates should be unstamped.
     */
    _resetUIState(): void;
    _print(): void;
    /**
     * Sets up listeners to collapse audit `<details>` when the user closes the
     * print dialog, all `<details>` are collapsed.
     */
    _setUpCollapseDetailsAfterPrinting(): void;
    /**
     * Returns the html that recreates this report.
     * @return {string}
     * @protected
     */
    protected getReportHtml(): string;
    /**
     * Save json as a gist. Unimplemented in base UI features.
     * @protected
     */
    protected saveAsGist(): void;
    /**
     * Downloads a file (blob) using a[download].
     * @param {Blob|File} blob The file to save.
     * @private
     */
    private _saveFile;
}
type LineContentType = number;
declare namespace LineContentType {
    const CONTENT_NORMAL: number;
    const CONTENT_HIGHLIGHTED: number;
    const PLACEHOLDER: number;
    const MESSAGE: number;
}
type LineVisibility = number;
declare namespace LineVisibility {
    const ALWAYS: number;
    const WHEN_COLLAPSED: number;
    const WHEN_EXPANDED: number;
}
/**
 * @license
 * Copyright 2017 The Lighthouse Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS-IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
declare class CategoryRenderer {
    /**
     * @param {DOM} dom
     * @param {DetailsRenderer} detailsRenderer
     */
    constructor(dom: DOM, detailsRenderer: DetailsRenderer);
    /** @type {DOM} */
    dom: DOM;
    /** @type {DetailsRenderer} */
    detailsRenderer: DetailsRenderer;
    /**
     * Display info per top-level clump. Define on class to avoid race with Util init.
     */
    get _clumpTitles(): {
        warning: string;
        manual: string;
        passed: string;
        notApplicable: string;
    };
    /**
     * @param {LH.ReportResult.AuditRef} audit
     * @return {Element}
     */
    renderAudit(audit: any): Element;
    /**
     * Populate an DOM tree with audit details. Used by renderAudit and renderOpportunity
     * @param {LH.ReportResult.AuditRef} audit
     * @param {DocumentFragment} component
     * @return {!Element}
     */
    populateAuditValues(audit: any, component: DocumentFragment): Element;
    /**
     * @return {Element}
     */
    _createChevron(): Element;
    /**
     * @param {Element} element DOM node to populate with values.
     * @param {number|null} score
     * @param {string} scoreDisplayMode
     * @return {!Element}
     */
    _setRatingClass(element: Element, score: number | null, scoreDisplayMode: string): Element;
    /**
     * @param {LH.ReportResult.Category} category
     * @param {Record<string, LH.Result.ReportGroup>} groupDefinitions
     * @return {DocumentFragment}
     */
    renderCategoryHeader(category: any, groupDefinitions: Record<string, any>): DocumentFragment;
    /**
     * Renders the group container for a group of audits. Individual audit elements can be added
     * directly to the returned element.
     * @param {LH.Result.ReportGroup} group
     * @return {Element}
     */
    renderAuditGroup(group: any): Element;
    /**
     * Takes an array of auditRefs, groups them if requested, then returns an
     * array of audit and audit-group elements.
     * @param {Array<LH.ReportResult.AuditRef>} auditRefs
     * @param {Object<string, LH.Result.ReportGroup>} groupDefinitions
     * @return {Array<Element>}
     */
    _renderGroupedAudits(auditRefs: Array<any>, groupDefinitions: {
        [x: string]: any;
    }): Array<Element>;
    /**
     * Take a set of audits, group them if they have groups, then render in a top-level
     * clump that can't be expanded/collapsed.
     * @param {Array<LH.ReportResult.AuditRef>} auditRefs
     * @param {Object<string, LH.Result.ReportGroup>} groupDefinitions
     * @return {Element}
     */
    renderUnexpandableClump(auditRefs: Array<any>, groupDefinitions: {
        [x: string]: any;
    }): Element;
    /**
     * Take a set of audits and render in a top-level, expandable clump that starts
     * in a collapsed state.
     * @param {Exclude<TopLevelClumpId, 'failed'>} clumpId
     * @param {{auditRefs: Array<LH.ReportResult.AuditRef>, description?: string}} clumpOpts
     * @return {!Element}
     */
    renderClump(clumpId: Exclude<any, 'failed'>, { auditRefs, description }: {
        auditRefs: Array<any>;
        description?: string;
    }): Element;
    /**
     * @param {LH.ReportResult.Category} category
     * @param {Record<string, LH.Result.ReportGroup>} groupDefinitions
     * @return {DocumentFragment}
     */
    renderScoreGauge(category: any, groupDefinitions: Record<string, any>): DocumentFragment;
    /**
     * Returns true if an LH category has any non-"notApplicable" audits.
     * @param {LH.ReportResult.Category} category
     * @return {boolean}
     */
    hasApplicableAudits(category: any): boolean;
    /**
     * Define the score arc of the gauge
     * Credit to xgad for the original technique: https://codepen.io/xgad/post/svg-radial-progress-meters
     * @param {SVGCircleElement} arcElem
     * @param {number} percent
     */
    _setGaugeArc(arcElem: SVGCircleElement, percent: number): void;
    /**
     * @param {LH.ReportResult.AuditRef} audit
     * @return {boolean}
     */
    _auditHasWarning(audit: any): boolean;
    /**
     * Returns the id of the top-level clump to put this audit in.
     * @param {LH.ReportResult.AuditRef} auditRef
     * @return {TopLevelClumpId}
     */
    _getClumpIdForAuditRef(auditRef: any): any;
    /**
     * Renders a set of top level sections (clumps), under a status of failed, warning,
     * manual, passed, or notApplicable. The result ends up something like:
     *
     * failed clump
     *   ├── audit 1 (w/o group)
     *   ├── audit 2 (w/o group)
     *   ├── audit group
     *   |  ├── audit 3
     *   |  └── audit 4
     *   └── audit group
     *      ├── audit 5
     *      └── audit 6
     * other clump (e.g. 'manual')
     *   ├── audit 1
     *   ├── audit 2
     *   ├── …
     *   ⋮
     * @param {LH.ReportResult.Category} category
     * @param {Object<string, LH.Result.ReportGroup>} [groupDefinitions]
     * @return {Element}
     */
    render(category: any, groupDefinitions?: {
        [x: string]: any;
    }): Element;
    /**
     * Create a non-semantic span used for hash navigation of categories
     * @param {Element} element
     * @param {string} id
     */
    createPermalinkSpan(element: Element, id: string): void;
}
declare class DropDown {
    /**
     * @param {DOM} dom
     */
    constructor(dom: DOM);
    /** @type {DOM} */
    _dom: DOM;
    /** @type {HTMLElement} */
    _toggleEl: HTMLElement;
    /** @type {HTMLElement} */
    _menuEl: HTMLElement;
    /**
     * Keydown handler for the document.
     * @param {KeyboardEvent} e
     */
    onDocumentKeyDown(e: KeyboardEvent): void;
    /**
     * Click handler for tools button.
     * @param {Event} e
     */
    onToggleClick(e: Event): void;
    /**
     * Handler for tool button.
     * @param {KeyboardEvent} e
     */
    onToggleKeydown(e: KeyboardEvent): void;
    /**
     * Focus out handler for the drop down menu.
     * @param {FocusEvent} e
     */
    onMenuFocusOut(e: FocusEvent): void;
    /**
     * Handler for tool DropDown.
     * @param {KeyboardEvent} e
     */
    onMenuKeydown(e: KeyboardEvent): void;
    /**
     * @param {?HTMLElement=} startEl
     * @return {HTMLElement}
     */
    _getNextMenuItem(startEl?: (HTMLElement | null) | undefined): HTMLElement;
    /**
     * @param {Array<Node>} allNodes
     * @param {?HTMLElement=} startNode
     * @return {HTMLElement}
     */
    _getNextSelectableNode(allNodes: Array<Node>, startNode?: (HTMLElement | null) | undefined): HTMLElement;
    /**
     * @param {?HTMLElement=} startEl
     * @return {HTMLElement}
     */
    _getPreviousMenuItem(startEl?: (HTMLElement | null) | undefined): HTMLElement;
    /**
     * @param {function(MouseEvent): any} menuClickHandler
     */
    setup(menuClickHandler: (arg0: MouseEvent) => any): void;
    close(): void;
    /**
     * @param {HTMLElement} firstFocusElement
     */
    open(firstFocusElement: HTMLElement): void;
}
declare class DetailsRenderer {
    /**
     * @param {DOM} dom
     * @param {{fullPageScreenshot?: LH.Artifacts.FullPageScreenshot}} [options]
     */
    constructor(dom: DOM, options?: {
        fullPageScreenshot?: any;
    });
    _dom: DOM;
    _fullPageScreenshot: any;
    /**
     * @param {AuditDetails} details
     * @return {Element|null}
     */
    render(details: any): Element | null;
    /**
     * @param {{value: number, granularity?: number}} details
     * @return {Element}
     */
    _renderBytes(details: {
        value: number;
        granularity?: number;
    }): Element;
    /**
     * @param {{value: number, granularity?: number, displayUnit?: string}} details
     * @return {Element}
     */
    _renderMilliseconds(details: {
        value: number;
        granularity?: number;
        displayUnit?: string;
    }): Element;
    /**
     * @param {string} text
     * @return {HTMLElement}
     */
    renderTextURL(text: string): HTMLElement;
    /**
     * @param {{text: string, url: string}} details
     * @return {HTMLElement}
     */
    _renderLink(details: {
        text: string;
        url: string;
    }): HTMLElement;
    /**
     * @param {string} text
     * @return {HTMLDivElement}
     */
    _renderText(text: string): HTMLDivElement;
    /**
     * @param {{value: number, granularity?: number}} details
     * @return {Element}
     */
    _renderNumeric(details: {
        value: number;
        granularity?: number;
    }): Element;
    /**
     * Create small thumbnail with scaled down image asset.
     * @param {string} details
     * @return {Element}
     */
    _renderThumbnail(details: string): Element;
    /**
     * @param {string} type
     * @param {*} value
     */
    _renderUnknown(type: string, value: any): any;
    /**
     * Render a details item value for embedding in a table. Renders the value
     * based on the heading's valueType, unless the value itself has a `type`
     * property to override it.
     * @param {TableItemValue} value
     * @param {LH.Audit.Details.OpportunityColumnHeading} heading
     * @return {Element|null}
     */
    _renderTableValue(value: any, heading: any): Element | null;
    /**
     * Get the headings of a table-like details object, converted into the
     * OpportunityColumnHeading type until we have all details use the same
     * heading format.
     * @param {Table|OpportunityTable} tableLike
     * @return {OpportunityTable['headings']}
     */
    _getCanonicalizedHeadingsFromTable(tableLike: any | any): any;
    /**
     * Get the headings of a table-like details object, converted into the
     * OpportunityColumnHeading type until we have all details use the same
     * heading format.
     * @param {Table['headings'][number]} heading
     * @return {OpportunityTable['headings'][number]}
     */
    _getCanonicalizedHeading(heading: any): any;
    /**
     * @param {Exclude<LH.Audit.Details.TableColumnHeading['subItemsHeading'], undefined>} subItemsHeading
     * @param {LH.Audit.Details.TableColumnHeading} parentHeading
     * @return {LH.Audit.Details.OpportunityColumnHeading['subItemsHeading']}
     */
    _getCanonicalizedsubItemsHeading(subItemsHeading: Exclude<any['subItemsHeading'], undefined>, parentHeading: any): any;
    /**
     * Returns a new heading where the values are defined first by `heading.subItemsHeading`,
     * and secondly by `heading`. If there is no subItemsHeading, returns null, which will
     * be rendered as an empty column.
     * @param {LH.Audit.Details.OpportunityColumnHeading} heading
     * @return {LH.Audit.Details.OpportunityColumnHeading | null}
     */
    _getDerivedsubItemsHeading(heading: any): any | null;
    /**
     * @param {TableItem} item
     * @param {(LH.Audit.Details.OpportunityColumnHeading | null)[]} headings
     */
    _renderTableRow(item: any, headings: (any | null)[]): any;
    /**
     * Renders one or more rows from a details table item. A single table item can
     * expand into multiple rows, if there is a subItemsHeading.
     * @param {TableItem} item
     * @param {LH.Audit.Details.OpportunityColumnHeading[]} headings
     */
    _renderTableRowsFromItem(item: any, headings: any[]): DocumentFragment;
    /**
     * @param {OpportunityTable|Table} details
     * @return {Element}
     */
    _renderTable(details: any | any): Element;
    /**
     * @param {LH.Audit.Details.List} details
     * @return {Element}
     */
    _renderList(details: any): Element;
    /**
     * @param {LH.Audit.Details.NodeValue} item
     * @return {Element}
     */
    renderNode(item: any): Element;
    /**
     * @param {LH.Audit.Details.SourceLocationValue} item
     * @return {Element|null}
     * @protected
     */
    protected renderSourceLocation(item: any): Element | null;
    /**
     * @param {LH.Audit.Details.Filmstrip} details
     * @return {Element}
     */
    _renderFilmstrip(details: any): Element;
    /**
     * @param {string} text
     * @return {Element}
     */
    _renderCode(text: string): Element;
}
export {};
