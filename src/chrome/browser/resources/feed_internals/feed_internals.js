// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Reference to the backend.
 * @type {feedInternals.mojom.PageHandlerRemote}
 */
let pageHandler = null;

(function() {

/**
 * Get and display general properties.
 */
function updatePageWithProperties() {
  pageHandler.getGeneralProperties().then(response => {
    /** @type {!feedInternals.mojom.Properties} */
    const properties = response.properties;
    $('is-feed-enabled').textContent = properties.isFeedEnabled;
    $('is-feed-visible').textContent = properties.isFeedVisible;
    $('is-feed-allowed').textContent = properties.isFeedAllowed;
    $('is-prefetching-enabled').textContent = properties.isPrefetchingEnabled;
    $('load-stream-status').textContent = properties.loadStreamStatus;
    $('feed-fetch-url').textContent = properties.feedFetchUrl.url;
    $('feed-actions-url').textContent = properties.feedActionsUrl.url;
    $('enable-webfeed-follow-intro-debug').checked =
        properties.isWebFeedFollowIntroDebugEnabled;
    $('enable-webfeed-follow-intro-debug').disabled = false;
    $('use-feed-query-requests-for-web-feeds').checked =
        properties.useFeedQueryRequestsForWebFeeds;
  });
}

/**
 * Get and display user classifier properties.
 */
function updatePageWithUserClass() {
  pageHandler.getUserClassifierProperties().then(response => {
    /** @type {!feedInternals.mojom.UserClassifier} */
    const properties = response.properties;
    $('user-class-description').textContent = properties.userClassDescription;
    $('avg-hours-between-views').textContent = properties.avgHoursBetweenViews;
    $('avg-hours-between-uses').textContent = properties.avgHoursBetweenUses;
  });
}

/**
 * Get and display last fetch data.
 */
function updatePageWithLastFetchProperties() {
  pageHandler.getLastFetchProperties().then(response => {
    /** @type {!feedInternals.mojom.LastFetchProperties} */
    const properties = response.properties;
    $('last-fetch-status').textContent = properties.lastFetchStatus;
    $('last-fetch-trigger').textContent = properties.lastFetchTrigger;
    $('last-fetch-time').textContent = toDateString(properties.lastFetchTime);
    $('refresh-suppress-time').textContent =
        toDateString(properties.refreshSuppressTime);
    $('last-fetch-bless-nonce').textContent = properties.lastBlessNonce;
    $('last-action-upload-status').textContent =
        properties.lastActionUploadStatus;
    $('last-action-upload-time').textContent =
        toDateString(properties.lastActionUploadTime);
  });
}

/**
 * Get and display last known content.
 */
function updatePageWithCurrentContent() {
  pageHandler.getCurrentContent().then(response => {
    const before = $('current-content');
    const after = before.cloneNode(false);

    /** @type {!Array<feedInternals.mojom.Suggestion>} */
    const suggestions = response.suggestions;

    for (const suggestion of suggestions) {
      // Create new content item from template.
      const item = document.importNode($('suggestion-template').content, true);

      // Populate template with text metadata.
      item.querySelector('.title').textContent = suggestion.title;
      item.querySelector('.publisher').textContent = suggestion.publisherName;

      // Populate template with link metadata.
      setLinkNode(item.querySelector('a.url'), suggestion.url.url);
      setLinkNode(item.querySelector('a.image'), suggestion.imageUrl.url);
      setLinkNode(item.querySelector('a.favicon'), suggestion.faviconUrl.url);

      after.appendChild(item);
    }

    before.replaceWith(after);
  });
}

/**
 * Populate <a> node with hyperlinked URL.
 *
 * @param {Element} node
 * @param {string} url
 */
function setLinkNode(node, url) {
  node.textContent = url;
  node.href = url;
}

/**
 * Convert timeSinceEpoch to string for display.
 *
 * @param {mojoBase.mojom.TimeDelta} timeSinceEpoch
 * @return {string}
 */
function toDateString(timeSinceEpoch) {
  const microseconds = Number(timeSinceEpoch.microseconds);
  return microseconds === 0 ? '' :
                              new Date(microseconds / 1000).toLocaleString();
}

/**
 * Hook up buttons to event listeners.
 */
function setupEventListeners() {
  $('clear-user-classification').addEventListener('click', function() {
    pageHandler.clearUserClassifierProperties();
    updatePageWithUserClass();
  });

  $('clear-cached-data').addEventListener('click', function() {
    pageHandler.clearCachedDataAndRefreshFeed();
  });

  $('refresh-feed').addEventListener('click', function() {
    pageHandler.refreshFeed();
  });

  $('dump-feed-process-scope').addEventListener('click', function() {
    pageHandler.getFeedProcessScopeDump().then(response => {
      $('feed-process-scope-dump').textContent = response.dump;
      $('feed-process-scope-details').open = true;
    });
  });

  $('load-feed-histograms').addEventListener('click', function() {
    pageHandler.getFeedHistograms().then(response => {
      $('feed-histograms-log').textContent = response.log;
      $('feed-histograms-details').open = true;
    });
  });

  $('feed-host-override-apply').addEventListener('click', function() {
    pageHandler.overrideFeedHost({url: $('feed-host-override').value});
  });

  $('discover-api-override-apply').addEventListener('click', function() {
    pageHandler.overrideFeedHost({url: $('discover-api-override').value});
  });

  $('feed-stream-data-override').addEventListener('click', function() {
    const file = $('feed-stream-data-file').files[0];
    if (file && typeof pageHandler.overrideFeedStreamData == 'function') {
      const reader = new FileReader();
      reader.readAsArrayBuffer(file);
      reader.onload = function(e) {
        const typedArray = new Uint8Array(e.target.result);
        pageHandler.overrideFeedStreamData([...typedArray]);
      };
    }
  });

  $('enable-webfeed-follow-intro-debug').addEventListener('click', function() {
    pageHandler.setWebFeedFollowIntroDebugEnabled(
        $('enable-webfeed-follow-intro-debug').checked);
    $('enable-webfeed-follow-intro-debug').disabled = true;
  });

  $('use-feed-query-requests-for-web-feeds')
      .addEventListener('click', function() {
        pageHandler.setUseFeedQueryRequestsForWebFeeds(
            $('use-feed-query-requests-for-web-feeds').checked);
      });
}

function updatePage() {
  updatePageWithProperties();
  updatePageWithUserClass();
  updatePageWithLastFetchProperties();
  updatePageWithCurrentContent();
}

document.addEventListener('DOMContentLoaded', function() {
  // Setup backend mojo.
  pageHandler = feedInternals.mojom.PageHandler.getRemote();

  setInterval(updatePage, 2000);
  updatePage();

  setupEventListeners();
});
})();
