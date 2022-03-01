// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview @externs
 * Externs file shipped into the chromium build to typecheck uncompiled, "pure"
 * JavaScript used to interoperate with the open-source privileged WebUI.
 * TODO(b/168274868): Convert this file to ES6.
 */

/** @const */
const helpApp = {};

/**
 * Each SearchableItem maps to one Data field in the LSS Mojo API and represents
 * a single potential search result for in-app search inside the help app.
 * These originate from the untrusted frame and get parsed by the LSS.
 * @record
 * @struct
 */
helpApp.SearchableItem = function() {};
/**
 * The id of this item.
 * @type {string}
 */
helpApp.SearchableItem.prototype.id;
/**
 * Title text. Plain localized text.
 * @type {string}
 */
helpApp.SearchableItem.prototype.title;
/**
 * Body text. Plain localized text.
 * @type {string}
 */
helpApp.SearchableItem.prototype.body;
/**
 * The main category name, e.g. Perks or Help. Plain localized text.
 * @type {string}
 */
helpApp.SearchableItem.prototype.mainCategoryName;
/**
 * Any sub category name, e.g. a help topic name. Plain localized text.
 * @type {?Array<string>}
 */
helpApp.SearchableItem.prototype.subcategoryNames;
/**
 * Sub headings from the content. Removed from the body text.
 * Plain localized text.
 * @type {?Array<string>}
 */
helpApp.SearchableItem.prototype.subheadings;
/**
 * The locale that this content is localized in. Empty string means system
 *     locale. The format is language[-country] (e.g., en-US) where the language
 *     is the 2 or 3 letter code from ISO-639.
 * @type {string}
 */
helpApp.SearchableItem.prototype.locale;

/**
 * Each LauncherSearchableItem maps to one Data field in the LSS Mojo API and
 * represents a single potential help app search result in the CrOS launcher.
 * These originate from the untrusted frame and get parsed by the LSS.
 * @record
 * @struct
 */
helpApp.LauncherSearchableItem = function() {};
/**
 * The unique identifier of this item.
 * @type {string}
 */
helpApp.LauncherSearchableItem.prototype.id;
/**
 * Title text. Plain localized text.
 * @type {string}
 */
helpApp.LauncherSearchableItem.prototype.title;
/**
 * The main category name, e.g. Perks or Help. Plain localized text.
 * @type {string}
 */
helpApp.LauncherSearchableItem.prototype.mainCategoryName;
/**
 * List of tags. Each tag is plain localized text. The item will be searchable
 *     by these tags.
 * @type {!Array<!string>}
 */
helpApp.LauncherSearchableItem.prototype.tags;
/**
 * The locale of the tags. This could be different from the locale of the other
 * fields. Empty string means system locale. Same format as the locale field.
 * @type {string}
 */
helpApp.LauncherSearchableItem.prototype.tagLocale;
/**
 * The URL path containing the relevant content, which may or may not contain
 *     URL parameters. For example, if the help content is at
 *     chrome://help-app/help/sub/3399763/id/1282338#install-user, then the
 *     field would be "help/sub/3399763/id/1282338#install-user" for this page.
 * @type {string}
 */
helpApp.LauncherSearchableItem.prototype.urlPathWithParameters;
/**
 * The locale that this content is localized in. Empty string means system
 *     locale. The format is language[-country] (e.g., en-US) where the language
 *     is the 2 or 3 letter code from ISO-639.
 * @type {string}
 */
helpApp.LauncherSearchableItem.prototype.locale;

/**
 * A position in a string. For highlighting matches in snippets.
 * @record
 * @struct
 */
helpApp.Position = function() {};
/** @type {number} */
helpApp.Position.prototype.length;
/** @type {number} */
helpApp.Position.prototype.start;

/**
 * Response from calling findInSearchIndex.
 * @record
 * @struct
 */
helpApp.SearchResult = function() {};
/** @type {string} */
helpApp.SearchResult.prototype.id;
/**
 * List of positions corresponding to the title, sorted by start index. Used in
 * snippet.
 * @type {?Array<!helpApp.Position>}
 */
helpApp.SearchResult.prototype.titlePositions;
/**
 * List of positions corresponding to the body, sorted by start index. Used in
 * snippet.
 * @type {?Array<!helpApp.Position>}
 */
helpApp.SearchResult.prototype.bodyPositions;
/**
 * Index of the most relevant subheading match.
 * @type {?number}
 */
helpApp.SearchResult.prototype.subheadingIndex;
/**
 * List of positions corresponding to the most relevant subheading. Used in
 * snippet.
 * @type {?Array<!helpApp.Position>}
 */
helpApp.SearchResult.prototype.subheadingPositions;

/**
 * Response from calling findInSearchIndex.
 * @record
 * @struct
 */
helpApp.FindResponse = function() {};
/** @type {?Array<!helpApp.SearchResult>} */
helpApp.FindResponse.prototype.results;

/**
 * The delegate which exposes open source privileged WebUi functions to
 * HelpApp.
 * @record
 * @struct
 */
helpApp.ClientApiDelegate = function() {};

/**
 * Opens up the built-in chrome feedback dialog.
 * @return {!Promise<?string>} Promise which resolves when the request has been
 *     acknowledged, if the dialog could not be opened the promise resolves with
 *     an error message, resolves with null otherwise.
 */
helpApp.ClientApiDelegate.prototype.openFeedbackDialog = function() {};

/**
 * Opens up the parental controls section of OS settings.
 * @return {!Promise<undefined>}
 */
helpApp.ClientApiDelegate.prototype.showParentalControls = function() {};

/**
 * Add or update the content that is stored in the Search Index.
 * @param {!Array<!helpApp.SearchableItem>} data
 * @return {!Promise<undefined>}
 */
helpApp.ClientApiDelegate.prototype.addOrUpdateSearchIndex = function(data) {};

/**
 * Clear the content that is stored in the Search Index.
 * @return {!Promise<undefined>}
 */
helpApp.ClientApiDelegate.prototype.clearSearchIndex = function() {};

/**
 * Search the search index for content that matches the given query.
 * @param {string} query
 * @param {number=} maxResults Maximum number of search results. Default 50.
 * @return {!Promise<!helpApp.FindResponse>}
 */
helpApp.ClientApiDelegate.prototype.findInSearchIndex = function(
    query, maxResults) {};

/**
 * Close the app. Works if the app is open in the background page.
 * @return {undefined}
 */
helpApp.ClientApiDelegate.prototype.closeBackgroundPage = function() {};

/**
 * Replace the content that is stored in the launcher search index.
 * @param {!Array<!helpApp.LauncherSearchableItem>} data
 * @return {!Promise<undefined>} Promise which resolves after the update is
 *     complete.
 */
helpApp.ClientApiDelegate.prototype.updateLauncherSearchIndex
    = function(data) {};

/**
 * Request for the discover page notification to be shown to the user. The
 * notification will only be shown if the relevant heuristics are true, i.e.
 * user is a child, is using a supported language etc.
 * @return {!Promise<undefined>}
 */
helpApp.ClientApiDelegate.prototype.maybeShowDiscoverNotification =
    function() {};

/**
 * Request for the release notes notification to be shown to the user. The
 * notification will only be shown if a notification for the help app has not
 * yet been shown in the current milestone.
 * @return {!Promise<undefined>}
 */
helpApp.ClientApiDelegate.prototype.maybeShowReleaseNotesNotification =
    function() {};

/**
 * Launch data that can be read by the app when it first loads.
 * @type {{
 *     delegate: !helpApp.ClientApiDelegate,
 * }}
 */
window.customLaunchData;
