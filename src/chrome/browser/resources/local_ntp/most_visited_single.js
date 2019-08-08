/* Copyright 2015 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

// Single iframe for NTP tiles.

/**
 * Controls rendering the Most Visited iframe.
 * @return {Object} A limited interface for testing the iframe.
 */
function MostVisited() {
'use strict';


/**
 * Enum for key codes.
 * @enum {number}
 * @const
 */
const KEYCODES = {
  BACKSPACE: 8,
  DELETE: 46,
  DOWN: 40,
  ENTER: 13,
  ESC: 27,
  LEFT: 37,
  RIGHT: 39,
  SPACE: 32,
  TAB: 9,
  UP: 38,
};

/**
 * Enum for ids.
 * @enum {string}
 * @const
 */
const IDS = {
  MOST_VISITED: 'most-visited',  // Container for all tilesets.
  MV_TILES: 'mv-tiles',          // Most Visited tiles container.
};


/**
 * Enum for classnames.
 * @enum {string}
 * @const
 */
const CLASSES = {
  FAILED_FAVICON: 'failed-favicon',  // Applied when the favicon fails to load.
  GRID_LAYOUT: 'grid-layout',
  // Applied to the grid tile being moved while reordering.
  GRID_REORDER: 'grid-reorder',
  GRID_TILE: 'grid-tile',
  GRID_TILE_CONTAINER: 'grid-tile-container',
  REORDER: 'reorder',  // Applied to the tile being moved while reordering.
  // Applied while we are reordering. Disables hover styling.
  REORDERING: 'reordering',
  MAC_CHROMEOS: 'mac-chromeos',  // Reduces font weight for MacOS and ChromeOS.
  // Material Design classes.
  MD_EMPTY_TILE: 'md-empty-tile',
  MD_ICON_BACKGROUND: 'md-icon-background',
  MD_FALLBACK_LETTER: 'md-fallback-letter',
  MD_FAVICON: 'md-favicon',
  MD_ICON: 'md-icon',
  MD_ADD_ICON: 'md-add-icon',
  MD_MENU: 'md-menu',
  MD_EDIT_MENU: 'md-edit-menu',
  MD_TILE: 'md-tile',
  MD_TILE_CONTAINER: 'md-tile-container',
  MD_TILE_INNER: 'md-tile-inner',
  MD_TITLE: 'md-title',
  MD_TITLE_CONTAINER: 'md-title-container',
  NO_INITIAL_FADE: 'no-initial-fade',
};


/**
 * The different types of events that are logged from the NTP.  This enum is
 * used to transfer information from the NTP JavaScript to the renderer and is
 * not used as a UMA enum histogram's logged value.
 * Note: Keep in sync with common/ntp_logging_events.h
 * @enum {number}
 * @const
 */
const LOG_TYPE = {
  // All NTP tiles have finished loading (successfully or failing).
  NTP_ALL_TILES_LOADED: 11,
  // The data for all NTP tiles (title, URL, etc, but not the thumbnail image)
  // has been received. In contrast to NTP_ALL_TILES_LOADED, this is recorded
  // before the actual DOM elements have loaded (in particular the thumbnail
  // images).
  NTP_ALL_TILES_RECEIVED: 12,

  // Shortcuts have been customized.
  NTP_SHORTCUT_CUSTOMIZED: 39,
  // The 'Add shortcut' link was clicked.
  NTP_CUSTOMIZE_ADD_SHORTCUT_CLICKED: 44,
  // The 'Edit shortcut' link was clicked.
  NTP_CUSTOMIZE_EDIT_SHORTCUT_CLICKED: 45,
};


/**
 * The different (visual) types that an NTP tile can have.
 * Note: Keep in sync with components/ntp_tiles/tile_visual_type.h
 * @enum {number}
 * @const
 */
const TileVisualType = {
  NONE: 0,
  ICON_REAL: 1,
  ICON_COLOR: 2,
  ICON_DEFAULT: 3,
  THUMBNAIL: 7,
  THUMBNAIL_FAILED: 8,
};


/**
 * Timeout delay for the window.onresize event throttle. Set to 15 frame per
 * second.
 * @const {number}
 */
const RESIZE_TIMEOUT_DELAY = 66;


/**
 * Timeout delay in ms before starting the reorder flow.
 * @const {number}
 */
const REORDER_TIMEOUT_DELAY = 1000;


/**
 * Maximum number of tiles if custom links is enabled.
 * @const {number}
 */
const MD_MAX_NUM_CUSTOM_LINK_TILES = 10;


/**
 * Maximum number of tiles per row for Material Design.
 * @const {number}
 */
const MD_MAX_TILES_PER_ROW = 5;


/**
 * Height of a tile for Material Design. Keep in sync with
 * most_visited_single.css.
 * @const {number}
 */
const MD_TILE_HEIGHT = 128;


/**
 * Width of a tile for Material Design. Keep in sync with
 * most_visited_single.css.
 * @const {number}
 */
const MD_TILE_WIDTH = 112;


/**
 * Number of tiles that will always be visible for Material Design. Calculated
 * by dividing minimum |--content-width| (see local_ntp.css) by |MD_TILE_WIDTH|
 * and multiplying by 2 rows.
 * @const {number}
 */
const MD_NUM_TILES_ALWAYS_VISIBLE = 6;


/**
 * The origin of this request, i.e. 'https://www.google.TLD' for the remote NTP,
 * or 'chrome-search://local-ntp' for the local NTP.
 * @const {string}
 */
const DOMAIN_ORIGIN = '{{ORIGIN}}';


/**
 * Counter for DOM elements that we are waiting to finish loading. Starts out
 * at 1 because initially we're waiting for the "show" message from the parent.
 * @type {number}
 */
let loadedCounter = 1;


/**
 * DOM element containing the tiles we are going to present next.
 * Works as a double-buffer that is shown when we receive a "show" postMessage.
 * @type {Element}
 */
let tiles = null;


/**
 * Maximum number of MostVisited tiles to show at any time. If the host page
 * doesn't send enough tiles and custom links is not enabled, we fill them blank
 * tiles. This can be changed depending on what feature is enabled. Set by the
 * host page, while 8 is default.
 * @type {number}
 */
let maxNumTiles = 8;


/**
 * List of parameters passed by query args.
 * @type {Object}
 */
let queryArgs = {};


/**
 * True if we are currently reordering the tiles.
 * @type {boolean}
 */
let reordering = false;


/**
 * The tile that is being moved during the reorder flow. Null if we are
 * currently not reordering.
 * @type {?Element}
 */
let elementToReorder = null;


/**
 * True if custom links is enabled.
 * @type {boolean}
 */
let isCustomLinksEnabled = false;


/**
 * True if the grid layout is enabled.
 * @type {boolean}
 */
let isGridEnabled = false;


/**
 * The current grid of tiles.
 * @type {?Grid}
 */
let currGrid = null;


/**
 * Called by tests to enable the grid layout.
 */
function enableGridLayoutForTesting() {
  isGridEnabled = true;
  document.body.classList.add(CLASSES.GRID_LAYOUT);
}


/**
 * Additional API for Array. Moves the item at index |from| to index |to|.
 * @param {number} from Index of the item to move.
 * @param {number} to Index to move the item to.
 */
Array.prototype.move = function(from, to) {
  this.splice(to, 0, this.splice(from, 1)[0]);
};


/**
 * Class that handles layouts and animations for the tile grid. This includes
 * animations for adding, deleting, and reordering.
 */
class Grid {
  constructor() {
    /** @private {number} */
    this.tileHeight_ = 0;
    /** @private {number} */
    this.tileWidth_ = 0;
    /** @private {number} */
    this.tilesAlwaysVisible_ = 0;
    /**
     * The maximum number of tiles per row allowed by the grid parameters.
     * @private {number}
     */
    this.maxTilesPerRow_ = 0;
    /** @private {number} */
    this.maxTiles_ = 0;

    /** @private {number} */
    this.gridWidth_ = 0;
    /**
     * The maximum number of tiles per row allowed by the window width.
     * @private {number}
     */
    this.maxTilesPerRowWindow_ = 0;

    /** @private {?Element} */
    this.container_ = null;
    /** @private {?HTMLCollection} */
    this.tiles_ = null;

    /**
     * Array that stores the {x,y} positions of the tile layout.
     * @private {?Array<!Object>}
     */
    this.position_ = null;

    /**
     * Stores the current order of the tiles. Index corresponds to grid
     * position, while value is the index of the tile in |this.tiles_|.
     * @private {?Array<number>}
     */
    this.order_ = null;

    /** @private {number} The index of the tile we're reordering. */
    this.itemToReorder_ = -1;
    /** @private {number} The index to move the tile we're reordering to. */
    this.newIndexOfItemToReorder_ = -1;
  }


  /**
   * Sets up the grid for the new tileset in |container|. The old tileset is
   * discarded.
   * @param {!Element} container The grid container element.
   * @param {Object=} params Customizable parameters for the grid. Used in
   *     testing.
   */
  init(container, params = {}) {
    this.container_ = container;

    this.tileHeight_ = params.tileHeight || MD_TILE_HEIGHT;
    this.tileWidth_ = params.tileWidth || MD_TILE_WIDTH;
    this.tilesAlwaysVisible_ =
        params.tilesAlwaysVisible || MD_NUM_TILES_ALWAYS_VISIBLE;
    this.maxTilesPerRow_ = params.maxTilesPerRow || MD_MAX_TILES_PER_ROW;
    this.maxTiles_ = params.maxTiles || maxNumTiles;

    this.maxTilesPerRowWindow_ = this.getMaxTilesPerRow_();

    this.tiles_ =
        this.container_.getElementsByClassName(CLASSES.GRID_TILE_CONTAINER);
    if (this.tiles_.length > this.maxTiles_) {
      throw new Error(
          'The number of tiles (' + this.tiles_.length +
          ') exceeds the maximum (' + this.maxTiles_ + ').');
    }
    this.position_ = new Array(this.maxTiles_);
    this.order_ = new Array(this.maxTiles_);
    for (let i = 0; i < this.maxTiles_; i++) {
      this.position_[i] = {x: 0, y: 0};
      this.order_[i] = i;
    }

    if (isCustomLinksEnabled || params.enableReorder) {
      // Set up reordering for all tiles except the add shortcut button.
      for (let i = 0; i < this.tiles_.length; i++) {
        if (this.tiles_[i].getAttribute('add') !== 'true') {
          this.setupReorder_(this.tiles_[i], i);
        }
      }
    }

    this.updateLayout();
  }


  /**
   * Returns a grid tile wrapper that contains |tile|.
   * @param {!Element} tile The tile element.
   * @param {number} rid The tile's restricted id.
   * @param {boolean} isAddButton True if this is the add shortcut button.
   * @return {!Element} A grid tile wrapper.
   */
  createGridTile(tile, rid, isAddButton) {
    const gridTileContainer = document.createElement('div');
    gridTileContainer.className = CLASSES.GRID_TILE_CONTAINER;
    gridTileContainer.setAttribute('rid', rid);
    gridTileContainer.setAttribute('add', isAddButton);
    const gridTile = document.createElement('div');
    gridTile.className = CLASSES.GRID_TILE;
    gridTile.appendChild(tile);
    gridTileContainer.appendChild(gridTile);
    return gridTileContainer;
  }


  /**
   * Updates the layout of the tiles. This is called for new tilesets and when
   * the window is resized or zoomed. Translates each tile's
   * |CLASSES.GRID_TILE_CONTAINER| to the correct position.
   */
  updateLayout() {
    const tilesPerRow = this.getTilesPerRow_();

    this.gridWidth_ = tilesPerRow * this.tileWidth_;
    this.container_.style.width = this.gridWidth_ + 'px';

    const maxVisibleTiles = tilesPerRow * 2;
    let x = 0;
    let y = 0;
    for (let i = 0; i < this.tiles_.length; i++) {
      const tile = this.tiles_[i];
      // Reset the offset for row 2.
      if (i === tilesPerRow) {
        x = this.getRow2Offset_(tilesPerRow);
        y = this.tileHeight_;
      }
      // Update the tile's position.
      this.translate_(tile, x, y);
      this.position_[i].x = x;
      this.position_[i].y = y;
      x += this.tileWidth_;  // Increment for the next tile.

      // Update visibility for tiles that may be hidden by the iframe border in
      // order to prevent keyboard navigation from reaching them. Ignores tiles
      // that will always be visible, since changing 'display' prevents
      // transitions from working.
      if (i >= this.tilesAlwaysVisible_) {
        const isVisible = i < maxVisibleTiles;
        tile.style.display = isVisible ? 'block' : 'none';
      }
    }
  }


  /**
   * Called when the window is resized/zoomed. Recalculates maximums for the new
   * window size and calls |updateLayout| if necessary.
   */
  onResize() {
    // Update the layout if the max number of tiles per row changes due to the
    // new window size.
    const maxPerRowWindow = this.getMaxTilesPerRow_();
    if (maxPerRowWindow !== this.maxTilesPerRowWindow_) {
      this.maxTilesPerRowWindow_ = maxPerRowWindow;
      this.updateLayout();
    }
  }


  /**
   * Returns the number of tiles per row. This may be balanced in order to make
   * even rows.
   * @return {number} The number of tiles per row.
   * @private
   */
  getTilesPerRow_() {
    const maxTilesPerRow =
        Math.min(this.maxTilesPerRow_, this.maxTilesPerRowWindow_);
    if (this.tiles_.length >= maxTilesPerRow * 2) {
      // We have enough for two full rows, so just return the max.
      return maxTilesPerRow;
    } else if (this.tiles_.length > maxTilesPerRow) {
      // We have have a little more than one full row, so we need to rebalance.
      return Math.ceil(this.tiles_.length / 2);
    } else {
      // We have (less than) a full row, so just return the tiles we have.
      return this.tiles_.length;
    }
  }


  /**
   * Returns the maximum number of tiles per row allowed by the window size.
   * @return {number} The maximum number of tiles per row.
   * @private
   */
  getMaxTilesPerRow_() {
    return Math.floor(window.innerWidth / this.tileWidth_);
  }


  /**
   * Returns row 2's x offset from row 1 in px. This will either be 0 or half a
   * tile length.
   * @param {number} tilesPerRow The number of tiles per row.
   * @return {number} The offset for row 2.
   * @private
   */
  getRow2Offset_(tilesPerRow) {
    // An odd number of tiles requires a half tile offset in the second row,
    // unless both rows are full (i.e. for smaller window widths).
    if (this.tiles_.length % 2 === 1 && this.tiles_.length / tilesPerRow < 2) {
      return Math.round(this.tileWidth_ / 2);
    }
    return 0;
  }


  /**
   * Returns true if the browser is in RTL.
   * @return {boolean}
   * @private
   */
  isRtl_() {
    return document.documentElement.dir === 'rtl';
  }


  /**
   * Translates the |element| by (x, y).
   * @param {?Element} element The element to apply the transform to.
   * @param {number} x The x value.
   * @param {number} y The y value.
   * @private
   */
  translate_(element, x, y) {
    if (!element) {
      throw new Error('Invalid element: cannot apply transform');
    }
    const rtlX = x * (this.isRtl_() ? -1 : 1);
    element.style.transform = 'translate(' + rtlX + 'px, ' + y + 'px)';
  }


  /**
   * Sets up event listeners necessary for tile reordering.
   * @param {!Element} tile Tile on which to set the event listeners.
   * @param {number} index The tile's index.
   * @private
   */
  setupReorder_(tile, index) {
    tile.setAttribute('index', index);
    // Listen for the drag event on the tile instead of the tile container. The
    // tile container remains static during the reorder flow.
    tile.firstChild.draggable = true;
    tile.firstChild.addEventListener('dragstart', (event) => {
      this.startReorder_(tile, event);
    });
    // Listen for the mouseover event on the tile container. If this is placed
    // on the tile instead, it can be triggered while the tile is translated to
    // its new position.
    tile.addEventListener('mouseover', (event) => {
      this.reorderToIndex_(index);
    });
  }


  /**
   * Starts the reorder flow. Updates the visual style of the held tile to
   * indicate that it is being moved and sets up the relevant event listeners.
   * @param {!Element} tile Tile that is being moved.
   * @param {!Event} event The 'dragstart' event. Used to obtain the current
   *     cursor position
   * @private
   */
  startReorder_(tile, event) {
    const index = Number(tile.getAttribute('index'));

    this.itemToReorder_ = index;
    this.newIndexOfItemToReorder_ = index;

    // Apply reorder styling.
    tile.classList.add(CLASSES.GRID_REORDER);
    // Disable other hover/active styling for all tiles.
    document.body.classList.add(CLASSES.REORDERING);

    // Set up event listeners for the reorder flow.
    const mouseMove = this.trackCursor_(tile, event.pageX, event.pageY);
    document.addEventListener('mousemove', mouseMove);
    document.addEventListener('mouseup', () => {
      document.removeEventListener('mousemove', mouseMove);
      this.stopReorder_(tile);
    }, {once: true});
  }


  /**
   * Stops the reorder flow. Resets the held tile's visual style and tells the
   * EmbeddedSearchAPI that a tile has been moved.
   * @param {!Element} tile Tile that has been moved.
   * @private
   */
  stopReorder_(tile) {
    const index = Number(tile.getAttribute('index'));

    // Remove reorder styling.
    tile.classList.remove(CLASSES.GRID_REORDER);
    document.body.classList.remove(CLASSES.REORDERING);

    // Move the tile to its new position and notify EmbeddedSearchAPI that the
    // tile has been moved.
    this.applyReorder_(tile, this.newIndexOfItemToReorder_);
    chrome.embeddedSearch.newTabPage.reorderCustomLink(
        Number(this.tiles_[index].getAttribute('rid')),
        this.newIndexOfItemToReorder_);

    this.itemToReorder_ = -1;
    this.newIndexOfItemToReorder_ = -1;
  }


  /**
   * Executed only when the reorder flow is ongoing. Inserts the currently held
   * tile at |index| and shifts tiles accordingly.
   * @param {number} index The index to insert the held tile at.
   * @private
   */
  reorderToIndex_(index) {
    if (this.newIndexOfItemToReorder_ === index ||
        !document.body.classList.contains(CLASSES.REORDERING)) {
      return;
    }

    // Moves the held tile from its current position to |index|.
    this.order_.move(this.newIndexOfItemToReorder_, index);
    this.newIndexOfItemToReorder_ = index;
    // Shift tiles according to the new order.
    for (let i = 0; i < this.tiles_.length; i++) {
      const tileIndex = this.order_[i];
      // Don't move the tile we're holding nor the add shortcut button.
      if (tileIndex === this.itemToReorder_ ||
          this.tiles_[i].getAttribute('add') === 'true') {
        continue;
      }
      this.applyReorder_(this.tiles_[tileIndex], i);
    }
  }


  /**
   * Translates the |tile|'s |CLASSES.GRID_TILE| from |index| to |newIndex|.
   * This is done to prevent interference with event listeners on the |tile|'s
   * |CLASSES.GRID_TILE_CONTAINER|, particularly 'mouseover'.
   * @param {!Element} tile Tile that is being shifted.
   * @param {number} newIndex New index for the tile.
   * @private
   */
  applyReorder_(tile, newIndex) {
    if (tile.getAttribute('index') === null) {
      throw new Error('Tile does not have an index.');
    }
    const index = Number(tile.getAttribute('index'));
    const x = this.position_[newIndex].x - this.position_[index].x;
    const y = this.position_[newIndex].y - this.position_[index].y;
    this.translate_(tile.children[0], x, y);
  }


  /**
   * Moves |tile| so that it tracks the cursor's position. This is done by
   * translating the |tile|'s |CLASSES.GRID_TILE|, which prevents interference
   * with event listeners on the |tile|'s |CLASSES.GRID_TILE_CONTAINER|.
   * @param {!Element} tile Tile that is being moved.
   * @param {number} origCursorX Original x cursor position.
   * @param {number} origCursorY Original y cursor position.
   * @private
   */
  trackCursor_(tile, origCursorX, origCursorY) {
    const index = Number(tile.getAttribute('index'));
    // RTL positions align with the right side of the grid. Therefore, the x
    // value must be recalculated to align with the left.
    const origPosX = this.isRtl_() ?
        (this.gridWidth_ - (this.position_[index].x + this.tileWidth_)) :
        this.position_[index].x;
    const origPosY = this.position_[index].y;

    // Get the max translation allowed by the grid boundaries. This will be the
    // x of the last tile in a row and the y of the tiles in the second row.
    const maxTranslateX = this.gridWidth_ - this.tileWidth_;
    const maxTranslateY = this.tileHeight_;

    const maxX = maxTranslateX - origPosX;
    const maxY = maxTranslateY - origPosY;
    const minX = 0 - origPosX;
    const minY = 0 - origPosY;

    return (event) => {
      // Do not exceed the iframe borders.
      const x = Math.max(Math.min(event.pageX - origCursorX, maxX), minX);
      const y = Math.max(Math.min(event.pageY - origCursorY, maxY), minY);
      tile.firstChild.style.transform = 'translate(' + x + 'px, ' + y + 'px)';
    };
  }
}


/**
 * Log an event on the NTP.
 * @param {number} eventType Event from LOG_TYPE.
 */
function logEvent(eventType) {
  chrome.embeddedSearch.newTabPage.logEvent(eventType);
}

/**
 * Log impression of an NTP tile.
 * @param {number} tileIndex Position of the tile, >= 0 and < |maxNumTiles|.
 * @param {number} tileTitleSource The source of the tile's title as received
 *                 from getMostVisitedItemData.
 * @param {number} tileSource The tile's source as received from
 *                 getMostVisitedItemData.
 * @param {number} tileType The tile's visual type from TileVisualType.
 * @param {Date} dataGenerationTime Timestamp representing when the tile was
 *               produced by a ranking algorithm.
 */
function logMostVisitedImpression(
    tileIndex, tileTitleSource, tileSource, tileType, dataGenerationTime) {
  chrome.embeddedSearch.newTabPage.logMostVisitedImpression(
      tileIndex, tileTitleSource, tileSource, tileType, dataGenerationTime);
}

/**
 * Log click on an NTP tile.
 * @param {number} tileIndex Position of the tile, >= 0 and < |maxNumTiles|.
 * @param {number} tileTitleSource The source of the tile's title as received
 *                 from getMostVisitedItemData.
 * @param {number} tileSource The tile's source as received from
 *                 getMostVisitedItemData.
 * @param {number} tileType The tile's visual type from TileVisualType.
 * @param {Date} dataGenerationTime Timestamp representing when the tile was
 *               produced by a ranking algorithm.
 */
function logMostVisitedNavigation(
    tileIndex, tileTitleSource, tileSource, tileType, dataGenerationTime) {
  chrome.embeddedSearch.newTabPage.logMostVisitedNavigation(
      tileIndex, tileTitleSource, tileSource, tileType, dataGenerationTime);
}

/**
 * Down counts the DOM elements that we are waiting for the page to load.
 * When we get to 0, we send a message to the parent window.
 * This is usually used as an EventListener of onload/onerror.
 */
function countLoad() {
  loadedCounter -= 1;
  if (loadedCounter <= 0) {
    swapInNewTiles();
    logEvent(LOG_TYPE.NTP_ALL_TILES_LOADED);
    let tilesAreCustomLinks =
        isCustomLinksEnabled && chrome.embeddedSearch.newTabPage.isCustomLinks;
    // Note that it's easiest to capture this when all custom links are loaded,
    // rather than when the impression for each link is logged.
    if (tilesAreCustomLinks) {
      chrome.embeddedSearch.newTabPage.logEvent(
          LOG_TYPE.NTP_SHORTCUT_CUSTOMIZED);
    }
    // Tell the parent page whether to show the restore default shortcuts option
    // in the menu.
    window.parent.postMessage(
        {cmd: 'loaded', showRestoreDefault: tilesAreCustomLinks},
        DOMAIN_ORIGIN);
    tilesAreCustomLinks = false;
    // Reset to 1, so that any further 'show' message will cause us to swap in
    // fresh tiles.
    loadedCounter = 1;
  }
}


/**
 * Handles postMessages coming from the host page to the iframe.
 * Mostly, it dispatches every command to handleCommand.
 */
function handlePostMessage(event) {
  if (event.data instanceof Array) {
    for (let i = 0; i < event.data.length; ++i) {
      handleCommand(event.data[i]);
    }
  } else {
    handleCommand(event.data);
  }
}


/**
 * Handles a single command coming from the host page to the iframe.
 * We try to keep the logic here to a minimum and just dispatch to the relevant
 * functions.
 */
function handleCommand(data) {
  const cmd = data.cmd;

  if (cmd == 'tile') {
    addTile(data);
  } else if (cmd == 'show') {
    // TODO(crbug.com/946225): If this happens before we have finished loading
    // the previous tiles, we probably get into a bad state. If/when the iframe
    // is removed this might no longer be a concern.
    showTiles(data);
  } else if (cmd == 'updateTheme') {
    updateTheme(data);
  } else if (cmd === 'focusMenu') {
    focusTileMenu(data);
  } else {
    console.error('Unknown command: ' + JSON.stringify(data));
  }
}


/**
 * Handler for the 'show' message from the host page.
 * @param {!Object} info Data received in the message.
 */
function showTiles(info) {
  logEvent(LOG_TYPE.NTP_ALL_TILES_RECEIVED);
  utils.setPlatformClass(document.body);
  countLoad();
}


/**
 * Handler for the 'updateTheme' message from the host page.
 * @param {!Object} info Data received in the message.
 */
function updateTheme(info) {
  document.body.style.setProperty('--tile-title-color', info.tileTitleColor);
  document.body.style.setProperty(
      '--icon-background-color', info.iconBackgroundColor);
  document.body.classList.toggle('dark-theme', info.isThemeDark);
  document.body.classList.toggle('use-title-container', info.useTitleContainer);
  document.body.classList.toggle('custom-background', info.customBackground);
  document.body.classList.toggle('use-white-add-icon', info.useWhiteAddIcon);
  document.documentElement.setAttribute('darkmode', info.isDarkMode);

  // Reduce font weight on the default(white) background for Mac and CrOS.
  document.body.classList.toggle(
      CLASSES.MAC_CHROMEOS,
      !info.isThemeDark && !info.useTitleContainer &&
          (navigator.userAgent.indexOf('Mac') > -1 ||
           navigator.userAgent.indexOf('CrOS') > -1));
}


/**
 * Handler for 'focusMenu' message from the host page. Focuses the edited tile's
 * menu or the add shortcut tile after closing the custom link edit dialog
 * without saving.
 * @param {!Object} info Data received in the message.
 */
function focusTileMenu(info) {
  const tile = document.querySelector(`a.md-tile[data-tid="${info.tid}"]`);
  if (info.tid === -1 /* Add shortcut tile */) {
    tile.focus();
  } else {
    tile.parentNode.childNodes[1].focus();
  }
}


/**
 * Removes all old instances of |IDS.MV_TILES| that are pending for deletion.
 */
function removeAllOldTiles() {
  const parent = document.querySelector('#' + IDS.MOST_VISITED);
  const oldList = parent.querySelectorAll('.mv-tiles-old');
  for (let i = 0; i < oldList.length; ++i) {
    parent.removeChild(oldList[i]);
  }
}


/**
 * Called when all tiles have finished loading (successfully or not), including
 * their thumbnail images, and we are ready to show the new tiles and drop the
 * old ones.
 */
function swapInNewTiles() {
  // Store the tiles on the current closure.
  const cur = tiles;

  // Add an "add new custom link" button if we haven't reached the maximum
  // number of tiles.
  if (isCustomLinksEnabled && cur.childNodes.length < maxNumTiles) {
    const data = {
      'tid': -1,
      'title': queryArgs['addLink'],
      'url': '',
      'isAddButton': true,
      'dataGenerationTime': new Date(),
      'tileSource': -1,
      'tileTitleSource': -1
    };
    tiles.appendChild(renderMaterialDesignTile(data));
  }

  const parent = document.querySelector('#' + IDS.MOST_VISITED);

  const old = parent.querySelector('#' + IDS.MV_TILES);
  if (old) {
    // Mark old tile DIV for removal after the transition animation is done.
    old.removeAttribute('id');
    old.classList.add('mv-tiles-old');
    old.style.opacity = 0.0;
    cur.addEventListener('transitionend', function(ev) {
      if (ev.target === cur) {
        removeAllOldTiles();
      }
    });
  }

  // Add new tileset.
  cur.id = IDS.MV_TILES;
  parent.appendChild(cur);

  if (isGridEnabled) {
    // Initialize the new tileset before modifying opacity. This will prevent
    // the transform transition from applying after the tiles fade in.
    currGrid.init(cur);
  } else {
    // Re-balance the tiles if there are more than |MD_MAX_TILES_PER_ROW| in
    // order to make even rows.
    if (cur.childNodes.length > MD_MAX_TILES_PER_ROW) {
      cur.style.maxWidth = 'calc(var(--md-tile-width) * ' +
          Math.ceil(cur.childNodes.length / 2) + ')';
    }
  }

  const flushOpacity = () => window.getComputedStyle(cur).opacity;

  // getComputedStyle causes the initial style (opacity 0) to be applied, so
  // that when we then set it to 1, that triggers the CSS transition.
  flushOpacity();
  cur.style.opacity = 1.0;

  if (document.documentElement.classList.contains(CLASSES.NO_INITIAL_FADE)) {
    flushOpacity();
    document.documentElement.classList.remove(CLASSES.NO_INITIAL_FADE);
  }

  // Make sure the tiles variable contain the next tileset we'll use if the host
  // page sends us an updated set of tiles.
  tiles = document.createElement('div');
}


/**
 * Explicitly hide tiles that are not visible in order to prevent keyboard
 * navigation.
 */
function updateTileVisibility() {
  const allTiles = document.querySelectorAll(
      '#' + IDS.MV_TILES + ' .' + CLASSES.MD_TILE_CONTAINER);
  if (allTiles.length === 0) {
    return;
  }

  // Get the current number of tiles per row. Hide any tile after the first two
  // rows.
  const tilesPerRow = Math.trunc(document.body.offsetWidth / MD_TILE_WIDTH);
  for (let i = MD_NUM_TILES_ALWAYS_VISIBLE; i < allTiles.length; i++) {
    allTiles[i].style.display = (i < tilesPerRow * 2) ? 'block' : 'none';
  }
}


/**
 * Handler for the 'show' message from the host page, called when it wants to
 * add a suggestion tile.
 * It's also used to fill up our tiles to |maxNumTiles| if necessary.
 * @param {?MostVisitedData} args Data for the tile to be rendered.
 */
function addTile(args) {
  if (isFinite(args.rid)) {
    // An actual suggestion. Grab the data from the embeddedSearch API.
    const data =
        chrome.embeddedSearch.newTabPage.getMostVisitedItemData(args.rid);
    if (!data) {
      return;
    }

    data.tid = data.rid;
    if (!data.faviconUrl) {
      data.faviconUrl = 'chrome-search://favicon/size/16@' +
          window.devicePixelRatio + 'x/' + data.renderViewId + '/' + data.tid;
    }
    tiles.appendChild(renderTile(data));
  } else {
    // An empty tile
    tiles.appendChild(renderTile(null));
  }
}

/**
 * Called when the user decided to add a tile to the blacklist.
 * It sets off the animation for the blacklist and sends the blacklisted id
 * to the host page.
 * @param {Element} tile DOM node of the tile we want to remove.
 */
function blacklistTile(tile) {
  const tid = Number(tile.firstChild.getAttribute('data-tid'));

  if (isCustomLinksEnabled) {
    chrome.embeddedSearch.newTabPage.deleteMostVisitedItem(tid);
  } else {
    tile.classList.add('blacklisted');
    tile.addEventListener('transitionend', function(ev) {
      if (ev.propertyName != 'width') {
        return;
      }
      window.parent.postMessage(
          {cmd: 'tileBlacklisted', tid: Number(tid)}, DOMAIN_ORIGIN);
    });
  }
}


/**
 * Starts edit custom link flow. Tells host page to show the edit custom link
 * dialog and pre-populate it with data obtained using the link's id.
 * @param {?number} tid Restricted id of the tile we want to edit.
 */
function editCustomLink(tid) {
  window.parent.postMessage({cmd: 'startEditLink', tid: tid}, DOMAIN_ORIGIN);
}


/**
 * Starts the reorder flow. Updates the visual style of the held tile to
 * indicate that it is being moved.
 * @param {!Element} tile Tile that is being moved.
 */
function startReorder(tile) {
  reordering = true;
  elementToReorder = tile;

  tile.classList.add(CLASSES.REORDER);
  // Disable other hover/active styling for all tiles.
  document.body.classList.add(CLASSES.REORDERING);

  document.addEventListener('dragend', () => {
    stopReorder(tile);
  }, {once: true});
}


/**
 * Stops the reorder flow. Resets the held tile's visual style and tells the
 * EmbeddedSearchAPI that a tile has been moved.
 * @param {!Element} tile Tile that has been moved.
 */
function stopReorder(tile) {
  reordering = false;
  elementToReorder = null;

  tile.classList.remove(CLASSES.REORDER);
  document.body.classList.remove(CLASSES.REORDERING);

  // Update |data-pos| for all tiles and notify EmbeddedSearchAPI that the tile
  // has been moved.
  const allTiles = document.querySelectorAll('#mv-tiles .' + CLASSES.MD_TILE);
  for (let i = 0; i < allTiles.length; i++) {
    allTiles[i].setAttribute('data-pos', i);
  }
  chrome.embeddedSearch.newTabPage.reorderCustomLink(
      Number(tile.firstChild.getAttribute('data-tid')),
      Number(tile.firstChild.getAttribute('data-pos')));
}


/**
 * Sets up event listeners necessary for tile reordering.
 * @param {!Element} tile Tile on which to set the event listeners.
 */
function setupReorder(tile) {
  // Starts the reorder flow after the user has held the mouse button down for
  // |REORDER_TIMEOUT_DELAY|.
  tile.addEventListener('mousedown', (event) => {
    // Do not reorder if the edit menu was clicked or if ctrl/shift/alt/meta is
    // also held down.
    if (event.button == 0 /* LEFT CLICK */ && !event.ctrlKey &&
        !event.shiftKey && !event.altKey && !event.metaKey &&
        !event.target.classList.contains(CLASSES.MD_MENU)) {
      let timeout = -1;

      // Cancel the timeout if the user drags the mouse off the tile and
      // releases or if the mouse if released.
      const dragend = () => {
        window.clearTimeout(timeout);
      };
      document.addEventListener('dragend', dragend, {once: true});

      const mouseup = () => {
        if (event.button == 0 /* LEFT CLICK */) {
          window.clearTimeout(timeout);
        }
      };
      document.addEventListener('mouseup', mouseup, {once: true});

      const timeoutFunc = (dragend_in, mouseup_in) => {
        if (!reordering) {
          startReorder(tile);
        }
        document.removeEventListener('dragend', dragend_in);
        document.removeEventListener('mouseup', mouseup_in);
      };
      // Wait for |REORDER_TIMEOUT_DELAY| before starting the reorder flow.
      timeout = window.setTimeout(
          timeoutFunc.bind(dragend, mouseup), REORDER_TIMEOUT_DELAY);
    }
  });

  tile.addEventListener('dragover', (event) => {
    // Only executed when the reorder flow is ongoing. Inserts the tile that is
    // being moved before/after this |tile| according to order in the list.
    if (reordering && elementToReorder && elementToReorder != tile) {
      // Determine which side to insert the element on:
      // - If the held tile comes after the current tile, insert behind the
      //   current tile.
      // - If the held tile comes before the current tile, insert in front of
      //   the current tile.
      let insertBefore;  // Element to insert the held tile behind.
      if (tile.compareDocumentPosition(elementToReorder) &
          Node.DOCUMENT_POSITION_FOLLOWING) {
        insertBefore = tile;
      } else {
        insertBefore = tile.nextSibling;
      }
      $('mv-tiles').insertBefore(elementToReorder, insertBefore);
    }
  });
}


/**
 * Renders a MostVisited tile to the DOM.
 * @param {?MostVisitedData} data Object containing rid, url, title, favicon,
 *     thumbnail, and optionally isAddButton. isAddButton is true if you want to
 *     construct an add custom link button. data is null if you want to
 *     construct an empty tile. isAddButton can only be set if custom links is
 *     enabled.
 */
function renderTile(data) {
  return renderMaterialDesignTile(data);
}


/**
 * Renders a MostVisited tile with Material Design styles.
 * @param {?MostVisitedData} data Object containing rid, url, title, favicon,
 *     and optionally isAddButton. isAddButton is if you want to construct an
 *     add custom link button. data is null if you want to construct an empty
 *     tile.
 * @return {Element}
 */
function renderMaterialDesignTile(data) {
  const mdTileContainer = document.createElement('div');
  mdTileContainer.role = 'none';

  if (data == null) {
    mdTileContainer.className = CLASSES.MD_EMPTY_TILE;
    return mdTileContainer;
  }
  mdTileContainer.className = CLASSES.MD_TILE_CONTAINER;

  // The tile will be appended to tiles.
  const position = tiles.children.length;
  // This is set in the load/error event for the favicon image.
  let tileType = TileVisualType.NONE;

  const mdTile = document.createElement('a');
  mdTile.className = CLASSES.MD_TILE;
  mdTile.tabIndex = 0;
  mdTile.setAttribute('data-tid', data.tid);
  mdTile.setAttribute('data-pos', position);
  if (utils.isSchemeAllowed(data.url)) {
    mdTile.href = data.url;
  }
  mdTile.setAttribute('aria-label', data.title);
  mdTile.title = data.title;

  mdTile.addEventListener('click', function(ev) {
    if (data.isAddButton) {
      editCustomLink(null);
      logEvent(LOG_TYPE.NTP_CUSTOMIZE_ADD_SHORTCUT_CLICKED);
    } else {
      logMostVisitedNavigation(
          position, data.tileTitleSource, data.tileSource, tileType,
          data.dataGenerationTime);
    }
  });
  mdTile.addEventListener('keydown', function(event) {
    if ((event.keyCode === KEYCODES.DELETE ||
         event.keyCode === KEYCODES.BACKSPACE) &&
        !data.isAddButton) {
      event.preventDefault();
      event.stopPropagation();
      blacklistTile(mdTileContainer);
    } else if (
        event.keyCode === KEYCODES.ENTER || event.keyCode === KEYCODES.SPACE) {
      event.preventDefault();
      this.click();
    } else if (event.keyCode === KEYCODES.LEFT) {
      const tiles = document.querySelectorAll(
          '#' + IDS.MV_TILES + ' .' + CLASSES.MD_TILE);
      tiles[Math.max(Number(this.getAttribute('data-pos')) - 1, 0)].focus();
    } else if (event.keyCode === KEYCODES.RIGHT) {
      const tiles = document.querySelectorAll(
          '#' + IDS.MV_TILES + ' .' + CLASSES.MD_TILE);
      tiles[Math.min(
                Number(this.getAttribute('data-pos')) + 1, tiles.length - 1)]
          .focus();
    }
  });
  utils.disableOutlineOnMouseClick(mdTile);

  const mdTileInner = document.createElement('div');
  mdTileInner.className = CLASSES.MD_TILE_INNER;

  const mdIcon = document.createElement('div');
  mdIcon.classList.add(CLASSES.MD_ICON);
  mdIcon.classList.add(CLASSES.MD_ICON_BACKGROUND);

  if (data.isAddButton) {
    const mdAdd = document.createElement('div');
    mdAdd.className = CLASSES.MD_ADD_ICON;
    const addBackground = document.createElement('div');
    addBackground.className = CLASSES.MD_ICON_BACKGROUND;

    addBackground.appendChild(mdAdd);
    mdIcon.appendChild(addBackground);
  } else {
    const fi = document.createElement('img');
    // Set title and alt to empty so screen readers won't say the image name.
    fi.title = '';
    fi.alt = '';
    const url = new URL('chrome-search://ntpicon/');
    url.searchParams.set('size', '24@' + window.devicePixelRatio + 'x');
    url.searchParams.set('url', data.url);
    fi.src = url.toString();
    loadedCounter += 1;
    fi.addEventListener('load', function(ev) {
      // Store the type for a potential later navigation.
      tileType = TileVisualType.ICON_REAL;
      logMostVisitedImpression(
          position, data.tileTitleSource, data.tileSource, tileType,
          data.dataGenerationTime);
      // Note: It's important to call countLoad last, because that might emit
      // the NTP_ALL_TILES_LOADED event, which must happen after the impression
      // log.
      countLoad();
    });
    fi.addEventListener('error', function(ev) {
      const fallbackBackground = document.createElement('div');
      fallbackBackground.className = CLASSES.MD_ICON_BACKGROUND;
      const fallbackLetter = document.createElement('div');
      fallbackLetter.className = CLASSES.MD_FALLBACK_LETTER;
      fallbackLetter.textContent = data.title.charAt(0).toUpperCase();
      mdIcon.classList.add(CLASSES.FAILED_FAVICON);

      fallbackBackground.appendChild(fallbackLetter);
      mdIcon.removeChild(fi);
      mdIcon.appendChild(fallbackBackground);

      // Store the type for a potential later navigation.
      tileType = TileVisualType.ICON_DEFAULT;
      logMostVisitedImpression(
          position, data.tileTitleSource, data.tileSource, tileType,
          data.dataGenerationTime);
      // Note: It's important to call countLoad last, because that might emit
      // the NTP_ALL_TILES_LOADED event, which must happen after the impression
      // log.
      countLoad();
    });

    mdIcon.appendChild(fi);
  }

  mdTileInner.appendChild(mdIcon);

  const mdTitleContainer = document.createElement('div');
  mdTitleContainer.className = CLASSES.MD_TITLE_CONTAINER;
  const mdTitle = document.createElement('div');
  mdTitle.className = CLASSES.MD_TITLE;
  const mdTitleTextwrap = document.createElement('span');
  mdTitleTextwrap.innerText = data.title;
  mdTitle.style.direction = data.direction || 'ltr';
  mdTitleContainer.appendChild(mdTitle);
  mdTileInner.appendChild(mdTitleContainer);
  mdTile.appendChild(mdTileInner);
  mdTileContainer.appendChild(mdTile);
  mdTitle.appendChild(mdTitleTextwrap);

  if (!data.isAddButton) {
    const mdMenu = document.createElement('button');
    mdMenu.className = CLASSES.MD_MENU;
    if (isCustomLinksEnabled) {
      mdMenu.classList.add(CLASSES.MD_EDIT_MENU);
      mdMenu.title = queryArgs['editLinkTooltip'] || '';
      mdMenu.setAttribute(
          'aria-label',
          (queryArgs['editLinkTooltip'] || '') + ' ' + data.title);
      mdMenu.addEventListener('click', function(ev) {
        editCustomLink(data.tid);
        ev.preventDefault();
        ev.stopPropagation();
        logEvent(LOG_TYPE.NTP_CUSTOMIZE_EDIT_SHORTCUT_CLICKED);
      });
    } else {
      mdMenu.title = queryArgs['removeTooltip'] || '';
      mdMenu.setAttribute(
          'aria-label', (queryArgs['removeTooltip'] || '') + ' ' + data.title);
      mdMenu.addEventListener('click', function(ev) {
        removeAllOldTiles();
        blacklistTile(mdTileContainer);
        ev.preventDefault();
        ev.stopPropagation();
      });
    }
    // Don't allow the event to bubble out to the containing tile, as that would
    // trigger navigation to the tile URL.
    mdMenu.addEventListener('keydown', function(ev) {
      ev.stopPropagation();
    });
    utils.disableOutlineOnMouseClick(mdMenu);

    mdTileContainer.appendChild(mdMenu);
  }

  if (isGridEnabled) {
    return currGrid.createGridTile(
        mdTileContainer, data.tid, !!data.isAddButton);
  } else {
    // Enable reordering.
    if (isCustomLinksEnabled && !data.isAddButton) {
      mdTileContainer.draggable = 'true';
      setupReorder(mdTileContainer);
    }
    return mdTileContainer;
  }
}


/**
 * Does some initialization and parses the query arguments passed to the iframe.
 */
function init() {
  // Create a new DOM element to hold the tiles. The tiles will be added
  // one-by-one via addTile, and the whole thing will be inserted into the page
  // in swapInNewTiles, after the parent has sent us the 'show' message, and all
  // thumbnails and favicons have loaded.
  tiles = document.createElement('div');

  // Parse query arguments.
  const query = window.location.search.substring(1).split('&');
  queryArgs = {};
  for (let i = 0; i < query.length; ++i) {
    const val = query[i].split('=');
    if (val[0] == '') {
      continue;
    }
    queryArgs[decodeURIComponent(val[0])] = decodeURIComponent(val[1]);
  }

  document.title = queryArgs['title'];

  // Enable RTL.
  if (queryArgs['rtl'] == '1') {
    document.documentElement.dir = 'rtl';
  }

  // Enable custom links.
  if (queryArgs['enableCustomLinks'] == '1') {
    isCustomLinksEnabled = true;
  }

  // Enable grid layout.
  if (queryArgs['enableGrid'] == '1') {
    isGridEnabled = true;
    document.body.classList.add(CLASSES.GRID_LAYOUT);
  }

  // Set the maximum number of tiles to show.
  if (isCustomLinksEnabled) {
    maxNumTiles = MD_MAX_NUM_CUSTOM_LINK_TILES;
  }

  currGrid = new Grid();
  // Set up layout updates on window resize. Throttled according to
  // |RESIZE_TIMEOUT_DELAY|.
  let resizeTimeout;
  window.onresize = () => {
    if (resizeTimeout) {
      window.clearTimeout(resizeTimeout);
    }
    resizeTimeout = window.setTimeout(() => {
      resizeTimeout = null;
      if (isGridEnabled) {
        currGrid.onResize();
      } else {
        updateTileVisibility();
      }
    }, RESIZE_TIMEOUT_DELAY);
  };

  window.addEventListener('message', handlePostMessage);
}


/**
 * Binds event listeners.
 */
function listen() {
  document.addEventListener('DOMContentLoaded', init);
}


return {
  Grid: Grid,  // Exposed for testing.
  init: init,  // Exposed for testing.
  enableGridLayoutForTesting: enableGridLayoutForTesting,
  listen: listen,
};
}

if (!window.mostVisitedUnitTest) {
  MostVisited().listen();
}
