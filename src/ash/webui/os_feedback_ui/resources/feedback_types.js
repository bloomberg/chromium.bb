// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Type aliases for the mojo API.
 */

import '//resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import '//resources/mojo/url/mojom/url.mojom-lite.js';
import './mojom/os_feedback_ui.mojom-lite.js';

/**
 * Type alias for HelpContentType.
 * @typedef {ash.osFeedbackUi.mojom.HelpContentType}
 */
export const HelpContentType = ash.osFeedbackUi.mojom.HelpContentType;

/**
 * Type alias for HelpContent.
 * @typedef {ash.osFeedbackUi.mojom.HelpContent}
 */
export const HelpContent = ash.osFeedbackUi.mojom.HelpContent;

/**
 * Type alias for an array of HelpContent.
 * @typedef !Array<!HelpContent>
 */
export let HelpContentList;

/**
 * Type alias for SearchRequest.
 * @typedef {ash.osFeedbackUi.mojom.SearchRequest}
 */
export const SearchRequest = ash.osFeedbackUi.mojom.SearchRequest;

/**
 * Type alias for SearchResponse.
 * @typedef {ash.osFeedbackUi.mojom.SearchResponse}
 */
export const SearchResponse = ash.osFeedbackUi.mojom.SearchResponse;

/**
 * Type alias for search result. When isPopularContent is true, the contentList
 * contains top popular help contents, i.e. returned where the search query is
 * empty. The isQueryEmpty is true when the current query is empty. The
 * isPopularContent is true when the current query is not empty and no matches
 * are found.
 * @typedef {{
 *   contentList: HelpContentList,
 *   isQueryEmpty: boolean,
 *   isPopularContent: boolean
 * }}
 */
export let SearchResult;

/**
 * Type alias for the HelpContentProviderInterface.
 * @typedef {ash.osFeedbackUi.mojom.HelpContentProviderInterface}
 */
export const HelpContentProviderInterface =
    ash.osFeedbackUi.mojom.HelpContentProviderInterface;

/**
 * Type alias for the HelpContentProvider.
 * @typedef {ash.osFeedbackUi.mojom.HelpContentProvider}
 */
export const HelpContentProvider = ash.osFeedbackUi.mojom.HelpContentProvider;

/**
 * Type alias for FeedbackContext.
 * @typedef {ash.osFeedbackUi.mojom.FeedbackContext}
 */
export const FeedbackContext = ash.osFeedbackUi.mojom.FeedbackContext;

/**
 * Type alias for SendReportStatus.
 * @typedef {ash.osFeedbackUi.mojom.SendReportStatus}
 */
export const SendReportStatus = ash.osFeedbackUi.mojom.SendReportStatus;

/**
 * Type alias for Report.
 * @typedef {ash.osFeedbackUi.mojom.Report}
 */
export const Report = ash.osFeedbackUi.mojom.Report;

/**
 * Type alias for the FeedbackServiceProviderInterface.
 * @typedef {ash.osFeedbackUi.mojom.FeedbackServiceProviderInterface}
 */
export const FeedbackServiceProviderInterface =
    ash.osFeedbackUi.mojom.FeedbackServiceProviderInterface;

/**
 * Type alias for the FeedbackServiceProvider.
 * @typedef {ash.osFeedbackUi.mojom.FeedbackServiceProvider}
 */
export const FeedbackServiceProvider =
    ash.osFeedbackUi.mojom.FeedbackServiceProvider;
