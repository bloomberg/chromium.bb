// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Location line.
 *
 * @extends {cr.EventTarget}
 * @param {!Element} breadcrumbs Container element for breadcrumbs.
 * @param {!VolumeManagerWrapper} volumeManager Volume manager.
 * @constructor
 */
function LocationLine(breadcrumbs, volumeManager) {
  this.breadcrumbs_ = breadcrumbs;
  this.volumeManager_ = volumeManager;
  this.entry_ = null;
}

/**
 * Extends cr.EventTarget.
 */
LocationLine.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Shows breadcrumbs. This operation is done without IO.
 *
 * @param {!Entry|!Object} entry Target entry or fake entry.
 */
LocationLine.prototype.show = function(entry) {
  if (entry === this.entry_)
    return;

  this.update_(this.getComponents_(entry));
};

/**
 * Get components for the path of entry.
 * @param {!Entry|!Object} entry An entry.
 * @return {!Array<!LocationLine.PathComponent>} Components.
 * @private
 */
LocationLine.prototype.getComponents_ = function(entry) {
  var components = [];
  var locationInfo = this.volumeManager_.getLocationInfo(entry);

  if (!locationInfo)
    return components;

  if (util.isFakeEntry(entry)) {
    components.push(new LocationLine.PathComponent(
        util.getRootTypeLabel(locationInfo), entry.toURL(), entry));
    return components;
  }

  // Add volume component.
  var displayRootUrl = locationInfo.volumeInfo.displayRoot.toURL();
  var displayRootFullPath = locationInfo.volumeInfo.displayRoot.fullPath;
  if (locationInfo.rootType === VolumeManagerCommon.RootType.DRIVE_OTHER) {
    // When target path is a shared directory, volume should be shared with me.
    displayRootUrl = displayRootUrl.slice(
        0, displayRootUrl.length - '/root'.length) + '/other';
    displayRootFullPath = '/other';
    var sharedWithMeFakeEntry = locationInfo.volumeInfo.fakeEntries[
        VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME];
    components.push(new LocationLine.PathComponent(
        str('DRIVE_SHARED_WITH_ME_COLLECTION_LABEL'),
        sharedWithMeFakeEntry.toURL(),
        sharedWithMeFakeEntry));
  } else {
    components.push(new LocationLine.PathComponent(
        util.getRootTypeLabel(locationInfo), displayRootUrl));
  }

  // Get relative path to display root (e.g. /root/foo/bar -> foo/bar).
  var relativePath = entry.fullPath.slice(displayRootFullPath.length);
  if (relativePath.indexOf('/') === 0) {
    relativePath = relativePath.slice(1);
  }
  if (relativePath.length === 0)
    return components;

  // currentUrl should be without trailing slash.
  var currentUrl = /^.+\/$/.test(displayRootUrl) ?
      displayRootUrl.slice(0, displayRootUrl.length - 1) : displayRootUrl;

  // Add directory components to the target path.
  var paths = relativePath.split('/');
  for (var i = 0; i < paths.length; i++) {
    currentUrl += '/' + paths[i];
    components.push(new LocationLine.PathComponent(paths[i], currentUrl));
  }

  return components;
};

/**
 * Updates the breadcrumb display.
 * @param {!Array.<!LocationLine.PathComponent>} components Components to the
 *     target path.
 * @private
 */
LocationLine.prototype.update_ = function(components) {
  this.breadcrumbs_.textContent = '';
  this.breadcrumbs_.hidden = false;

  var doc = this.breadcrumbs_.ownerDocument;
  for (var i = 0; i < components.length; i++) {
    // Add a component.
    var component = components[i];
    var div = doc.createElement('div');
    div.classList.add('breadcrumb-path', 'entry-name');
    div.textContent = component.name;
    div.tabIndex = 8;
    div.addEventListener('click', this.execute_.bind(this, div, component));
    div.addEventListener('keydown', function(div, component, event) {
      // If the pressed key is either Enter or Space.
      if (event.keyCode == 13 || event.keyCode == 32)
        this.execute_(div, component);
    }.bind(this, div, component));
    this.breadcrumbs_.appendChild(div);

    // If this is the last component, break here.
    if (i === components.length - 1) {
      div.classList.add('breadcrumb-last');
      div.tabIndex = -1;
      break;
    }

    // Add a separator.
    var separator = doc.createElement('span');
    separator.classList.add('separator');
    this.breadcrumbs_.appendChild(separator);
  }

  this.truncate();
};

/**
 * Updates breadcrumbs widths in order to truncate it properly.
 */
LocationLine.prototype.truncate = function() {
  if (!this.breadcrumbs_.firstChild)
    return;

  // Assume style.width == clientWidth (items have no margins or paddings).

  for (var item = this.breadcrumbs_.firstChild; item; item = item.nextSibling) {
    item.removeAttribute('style');
    item.removeAttribute('collapsed');
  }

  var containerWidth = this.breadcrumbs_.clientWidth;

  var pathWidth = 0;
  var currentWidth = 0;
  var lastSeparator;
  for (var item = this.breadcrumbs_.firstChild; item; item = item.nextSibling) {
    if (item.className == 'separator') {
      pathWidth += currentWidth;
      currentWidth = item.clientWidth;
      lastSeparator = item;
    } else {
      currentWidth += item.clientWidth;
    }
  }
  if (pathWidth + currentWidth <= containerWidth)
    return;
  if (!lastSeparator) {
    this.breadcrumbs_.lastChild.style.width =
        Math.min(currentWidth, containerWidth) + 'px';
    return;
  }
  var lastCrumbSeparatorWidth = lastSeparator.clientWidth;
  // Current directory name may occupy up to 70% of space or even more if the
  // path is short.
  var maxPathWidth = Math.max(Math.round(containerWidth * 0.3),
                              containerWidth - currentWidth);
  maxPathWidth = Math.min(pathWidth, maxPathWidth);

  var parentCrumb = lastSeparator.previousSibling;
  var collapsedWidth = 0;
  if (parentCrumb && pathWidth - maxPathWidth > parentCrumb.clientWidth) {
    // At least one crumb is hidden completely (or almost completely).
    // Show sign of hidden crumbs like this:
    // root > some di... > ... > current directory.
    parentCrumb.setAttribute('collapsed', '');
    collapsedWidth = Math.min(maxPathWidth, parentCrumb.clientWidth);
    maxPathWidth -= collapsedWidth;
    if (parentCrumb.clientWidth != collapsedWidth)
      parentCrumb.style.width = collapsedWidth + 'px';

    lastSeparator = parentCrumb.previousSibling;
    if (!lastSeparator)
      return;
    collapsedWidth += lastSeparator.clientWidth;
    maxPathWidth = Math.max(0, maxPathWidth - lastSeparator.clientWidth);
  }

  pathWidth = 0;
  for (var item = this.breadcrumbs_.firstChild; item != lastSeparator;
       item = item.nextSibling) {
    // TODO(serya): Mixing access item.clientWidth and modifying style and
    // attributes could cause multiple layout reflows.
    if (pathWidth + item.clientWidth <= maxPathWidth) {
      pathWidth += item.clientWidth;
    } else if (pathWidth == maxPathWidth) {
      item.style.width = '0';
    } else if (item.classList.contains('separator')) {
      // Do not truncate separator. Instead let the last crumb be longer.
      item.style.width = '0';
      maxPathWidth = pathWidth;
    } else {
      // Truncate the last visible crumb.
      item.style.width = (maxPathWidth - pathWidth) + 'px';
      pathWidth = maxPathWidth;
    }
  }

  currentWidth = Math.min(currentWidth,
                          containerWidth - pathWidth - collapsedWidth);
  this.breadcrumbs_.lastChild.style.width =
      (currentWidth - lastCrumbSeparatorWidth) + 'px';
};

/**
 * Hide breadcrumbs div.
 */
LocationLine.prototype.hide = function() {
  this.breadcrumbs_.hidden = true;
};

/**
 * Execute an element.
 * @param {!Element} element Element to be executed.
 * @param {!LocationLine.PathComponent} pathComponent Path Component object of
 *     the element.
 * @private
 */
LocationLine.prototype.execute_ = function(element, pathComponent) {
  if (!element.classList.contains('breadcrumb-path') ||
      element.classList.contains('breadcrumb-last'))
    return;

  pathComponent.resolveEntry().then(function(entry) {
    var pathClickEvent = new Event('pathclick');
    pathClickEvent.entry = entry;
    this.dispatchEvent(pathClickEvent);
  }.bind(this));
};

/**
 * Path component.
 * @param {string} name Name.
 * @param {string} url Url.
 * @param {Object=} opt_fakeEntry Fake entry should be set when this component
 *     represents fake entry.
 * @constructor
 * @struct
 */
LocationLine.PathComponent = function(name, url, opt_fakeEntry) {
  this.name = name;
  this.url_ = url;
  this.fakeEntry_ = opt_fakeEntry || null;
};

/**
 * Resolve an entry of the component.
 * @return {!Promise<!Entry|!Object>} A promise which is resolved with an entry.
 */
LocationLine.PathComponent.prototype.resolveEntry = function() {
  if (this.fakeEntry_)
    return Promise.resolve(this.fakeEntry_);
  else
    return new Promise(
        window.webkitResolveLocalFileSystemURL.bind(null, this.url_));
};
