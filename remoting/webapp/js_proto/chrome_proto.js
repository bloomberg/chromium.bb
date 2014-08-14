// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains various hacks needed to inform JSCompiler of various
// WebKit- and Chrome-specific properties and methods. It is used only with
// JSCompiler to verify the type-correctness of our code.

/** @type {Object} */
var chrome = {};

/** @constructor */
chrome.Event = function() {};

/** @param {Function} callback */
chrome.Event.prototype.addListener = function(callback) {};

/** @param {Function} callback */
chrome.Event.prototype.removeListener = function(callback) {};

/** @type {Object} */
chrome.app = {};

/** @type {Object} */
chrome.app.runtime = {
  /** @type {chrome.Event} */
  onLaunched: null
};


/** @type {Object} */
chrome.app.window = {
  /**
   * @param {string} name
   * @param {Object} parameters
   * @param {function()=} opt_callback
   */
  create: function(name, parameters, opt_callback) {},
  /**
   * @return {AppWindow}
   */
  current: function() {},
  /**
   * @param {string} id
   * @param {function()=} opt_callback
   */
  get: function(id, opt_callback) {}
};


/** @type {Object} */
chrome.runtime = {
  /** @type {Object} */
  lastError: {
    /** @type {string} */
    message: ''
  },
  /** @return {{version: string, app: {background: Object}}} */
  getManifest: function() {},
  /** @type {chrome.Event} */
  onSuspend: null,
  /** @type {chrome.Event} */
  onSuspendCanceled: null,
  /** @type {chrome.Event} */
  onConnect: null,
  /** @type {chrome.Event} */
  onConnectExternal: null,
  /** @type {chrome.Event} */
  onMessage: null,
  /** @type {chrome.Event} */
  onMessageExternal: null
};

/**
 * @type {?function(string):chrome.runtime.Port}
 */
chrome.runtime.connectNative = function(name) {};

/**
 * @param {{ name: string}} config
 * @return {chrome.runtime.Port}
 */
chrome.runtime.connect = function(config) {};

/**
 * @param {string} extensionId
 * @param {*} message
 * @param {Object=} opt_options
 * @param {function(*)=} opt_callback
 */
chrome.runtime.sendMessage = function(
    extensionId, message, opt_options, opt_callback) {};

/** @constructor */
chrome.runtime.MessageSender = function(){
  /** @type {chrome.Tab} */
  this.tab = null;
};

/** @constructor */
chrome.runtime.Port = function() {
  this.onMessage = new chrome.Event();
  this.onDisconnect = new chrome.Event();

  /** @type {string} */
  this.name = '';

  /** @type {chrome.runtime.MessageSender} */
  this.sender = null;
};

/** @type {chrome.Event} */
chrome.runtime.Port.prototype.onMessage = null;

/** @type {chrome.Event} */
chrome.runtime.Port.prototype.onDisconnect = null;

chrome.runtime.Port.prototype.disconnect = function() {};

/**
 * @param {Object} message
 */
chrome.runtime.Port.prototype.postMessage = function(message) {};


/** @type {Object} */
chrome.extension = {};

/**
 * @param {*} message
 */
chrome.extension.sendMessage = function(message) {}

/** @type {chrome.Event} */
chrome.extension.onMessage;


/** @type {Object} */
chrome.i18n = {};

/**
 * @param {string} messageName
 * @param {(string|Array.<string>)=} opt_args
 * @return {string}
 */
chrome.i18n.getMessage = function(messageName, opt_args) {};


/** @type {Object} */
chrome.storage = {};

/** @type {chrome.Storage} */
chrome.storage.local;

/** @type {chrome.Storage} */
chrome.storage.sync;

/** @constructor */
chrome.Storage = function() {};

/**
 * @param {string|Array.<string>|Object.<string>} items
 * @param {function(Object.<string>):void} callback
 * @return {void}
 */
chrome.Storage.prototype.get = function(items, callback) {};

/**
 * @param {Object.<string>} items
 * @param {function():void=} opt_callback
 * @return {void}
 */
chrome.Storage.prototype.set = function(items, opt_callback) {};

/**
 * @param {string|Array.<string>} items
 * @param {function():void=} opt_callback
 * @return {void}
 */
chrome.Storage.prototype.remove = function(items, opt_callback) {};

/**
 * @param {function():void=} opt_callback
 * @return {void}
 */
chrome.Storage.prototype.clear = function(opt_callback) {};


/**
 * @type {Object}
 * src/chrome/common/extensions/api/context_menus.json
 */
chrome.contextMenus = {};
/** @type {chrome.Event} */
chrome.contextMenus.onClicked;
/**
 * @param {!Object} createProperties
 * @param {function()=} opt_callback
 */
chrome.contextMenus.create = function(createProperties, opt_callback) {};
/**
 * @param {string|number} id
 * @param {!Object} updateProperties
 * @param {function()=} opt_callback
 */
chrome.contextMenus.update = function(id, updateProperties, opt_callback) {};
/**
 * @param {string|number} menuItemId
 * @param {function()=} opt_callback
 */
chrome.contextMenus.remove = function(menuItemId, opt_callback) {};
/**
 * @param {function()=} opt_callback
 */
chrome.contextMenus.removeAll = function(opt_callback) {};

/** @constructor */
function OnClickData() {}
/** @type {string|number} */
OnClickData.prototype.menuItemId;
/** @type {string|number} */
OnClickData.prototype.parentMenuItemId;
/** @type {string} */
OnClickData.prototype.mediaType;
/** @type {string} */
OnClickData.prototype.linkUrl;
/** @type {string} */
OnClickData.prototype.srcUrl;
/** @type {string} */
OnClickData.prototype.pageUrl;
/** @type {string} */
OnClickData.prototype.frameUrl;
/** @type {string} */
OnClickData.prototype.selectionText;
/** @type {boolean} */
OnClickData.prototype.editable;
/** @type {boolean} */
OnClickData.prototype.wasChecked;
/** @type {boolean} */
OnClickData.prototype.checked;


/** @type {Object} */
chrome.identity = {
  /**
   * @param {Object.<string>} parameters
   * @param {function(string):void} callback
   */
  getAuthToken: function(parameters, callback) {},
  /**
   * @param {Object.<string>} parameters
   * @param {function():void} callback
   */
  removeCachedAuthToken: function(parameters, callback) {},
  /**
   * @param {Object.<string>} parameters
   * @param {function(string):void} callback
   */
  launchWebAuthFlow: function(parameters, callback) {}
};


/** @type {Object} */
chrome.permissions = {
  /**
   * @param {Object.<string>} permissions
   * @param {function(boolean):void} callback
   */
  contains: function(permissions, callback) {},
  /**
   * @param {Object.<string>} permissions
   * @param {function(boolean):void} callback
   */
  request: function(permissions, callback) {}
};


/** @type {Object} */
chrome.tabs = {};

/** @param {function(chrome.Tab):void} callback */
chrome.tabs.getCurrent = function(callback) {};

/**
 * @param {Object?} options
 * @param {function(chrome.Tab)=} opt_callback
 */
chrome.tabs.create = function(options, opt_callback) {};

/**
 * @param {string} id
 * @param {function(chrome.Tab)} callback
 */
chrome.tabs.get = function(id, callback) {};

/**
 * @param {string} id
 * @param {function()=} opt_callback
 */
chrome.tabs.remove = function(id, opt_callback) {};


/** @constructor */
chrome.Tab = function() {
  /** @type {boolean} */
  this.pinned = false;
  /** @type {number} */
  this.windowId = 0;
  /** @type {string} */
  this.id = '';
};


/** @type {Object} */
chrome.windows = {};

/** @param {number} id
 *  @param {Object?} getInfo
 *  @param {function(chrome.Window):void} callback */
chrome.windows.get = function(id, getInfo, callback) {};

/** @constructor */
chrome.Window = function() {
  /** @type {string} */
  this.state = '';
  /** @type {string} */
  this.type = '';
};

/** @constructor */
var AppWindow = function() {
  /** @type {Window} */
  this.contentWindow = null;
  /** @type {chrome.Event} */
  this.onRestored = null;
  /** @type {chrome.Event} */
  this.onMaximized = null;
  /** @type {chrome.Event} */
  this.onFullscreened = null;
  /** @type {string} */
  this.id = '';
};

AppWindow.prototype.close = function() {};
AppWindow.prototype.drawAttention = function() {};
AppWindow.prototype.maximize = function() {};
AppWindow.prototype.minimize = function() {};
AppWindow.prototype.restore = function() {};
AppWindow.prototype.fullscreen = function() {};
/** @return {boolean} */
AppWindow.prototype.isFullscreen = function() {};
/** @return {boolean} */
AppWindow.prototype.isMaximized = function() {};

/**
 * @param {{rects: Array.<ClientRect>}} rects
 */
AppWindow.prototype.setShape = function(rects) {};

/**
 * @param {{rects: Array.<ClientRect>}} rects
 */
AppWindow.prototype.setInputRegion = function(rects) {};

/** @constructor */
var LaunchData = function() {
  /** @type {string} */
  this.id = '';
  /** @type {Array.<{type: string, entry: FileEntry}>} */
  this.items = [];
};

/** @constructor */
function ClientRect() {
  /** @type {number} */
  this.width = 0;
  /** @type {number} */
  this.height = 0;
  /** @type {number} */
  this.top = 0;
  /** @type {number} */
  this.bottom = 0;
  /** @type {number} */
  this.left = 0;
  /** @type {number} */
  this.right = 0;
};
