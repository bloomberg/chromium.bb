// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
                 browserSignals) {
  validateAdMetadata(adMetadata);
  validateBid(bid);
  validateAuctionConfig(auctionConfig);
  validateTrustedScoringSignals(trustedScoringSignals);
  validateBrowserSignals(browserSignals, /*isScoreAd=*/true);
  return {desirability: 13, allowComponentAuction: true,
          bid:42, ad:['Replaced metadata']};
}

function reportResult(auctionConfig, browserSignals) {
  validateAuctionConfig(auctionConfig);
  validateBrowserSignals(browserSignals, /*isScoreAd=*/false);

  sendReportTo(auctionConfig.seller + '/echo?report_component_seller');
  return ['component seller signals for winner'];
}

function validateAdMetadata(adMetadata) {
  const adMetadataJSON = JSON.stringify(adMetadata);
  if (adMetadataJSON !==
      '{"renderUrl":"https://example.com/render",' +
      '"metadata":{"ad":"metadata","here":[1,2,3]}}') {
    throw 'Wrong adMetadata ' + adMetadataJSON;
  }
}

function validateBid(bid) {
  if (bid !== 2)
    throw 'Wrong bid ' + bid;
}

function validateAuctionConfig(auctionConfig) {
  if (!auctionConfig.seller.includes('d.test'))
    throw 'Wrong seller ' + auctionConfig.seller;

  if (auctionConfig.decisionLogicUrl !==
      auctionConfig.seller + '/interest_group' +
          '/component_auction_component_decision_argument_validator.js') {
    throw 'Wrong decisionLogicUrl ' + auctionConfig.decisionLogicUrl;
  }

  if (auctionConfig.trustedScoringSignalsUrl !==
      auctionConfig.seller + '/interest_group/trusted_scoring_signals2.json') {
      throw 'Wrong trustedScoringSignalsUrl ' +
          auctionConfig.trustedScoringSignalsUrl;
  }

  if (auctionConfig.interestGroupBuyers.length !== 1 ||
      !auctionConfig.interestGroupBuyers[0].startsWith('https://a.test')) {
    throw 'Wrong interestGroupBuyers ' + auctionConfig.interestGroupBuyers;
  }
  // If auctionSignals is passed as a JSON string instead of an object,
  // stringify() will wrap it in another layer of quotes, causing the test to
  // fail.
  const auctionSignalsJson = JSON.stringify(auctionConfig.auctionSignals);
  if (auctionSignalsJson !== '["component auction signals"]')
    throw 'Wrong auctionSignals ' + auctionConfig.auctionSignals;
  const sellerSignalsJson = JSON.stringify(auctionConfig.sellerSignals);
  if (sellerSignalsJson !== '["component seller signals"]')
    throw 'Wrong sellerSignals ' + auctionConfig.sellerSignalsJson;
  if (auctionConfig.sellerTimeout !== 200)
    throw 'Wrong sellerTimeout ' + auctionConfig.sellerTimeout;
  const perBuyerSignalsJson = JSON.stringify(auctionConfig.perBuyerSignals);
  if (!perBuyerSignalsJson.includes('a.test') ||
      !perBuyerSignalsJson.includes('["component buyer signals"]')) {
    throw 'Wrong perBuyerSignals ' + perBuyerSignalsJson;
  }
  const perBuyerTimeoutsJson = JSON.stringify(auctionConfig.perBuyerTimeouts);
  if (!perBuyerTimeoutsJson.includes('a.test') ||
      !perBuyerTimeoutsJson.includes('200')) {
    throw 'Wrong perBuyerTimeouts ' + perBuyerTimeoutsJson;
  }

  if ('componentAuctions' in auctionConfig) {
    throw 'Unexpected componentAuctions ' +
        JSON.stringify(auctionConfig.componentAuctions);
  }
}

function validateTrustedScoringSignals(signals) {
  if (signals.renderUrl["https://example.com/render"] !== "bar") {
    throw 'Wrong trustedScoringSignals.renderUrl ' +
        signals.renderUrl["https://example.com/render"];
  }
  if (signals.adComponentRenderUrls["https://example.com/render-component"] !==
      2) {
    throw 'Wrong trustedScoringSignals.adComponentRenderUrls ' +
        signals.adComponentRenderUrls["https://example.com/render-component"];
  }
}

// Used for both scoreAd() and reportResult().
function validateBrowserSignals(browserSignals, isScoreAd) {
  // Fields common to scoreAd() and reportResult().
  if (browserSignals.topWindowHostname !== 'c.test')
    throw 'Wrong topWindowHostname ' + browserSignals.topWindowHostname;
  if (!browserSignals.topLevelSeller.startsWith('https://b.test'))
    throw 'Wrong topLevelSeller ' + browserSignals.topLevelSeller;
  if ('componentSeller' in browserSignals)
    throw 'Wrong componentSeller ' + browserSignals.componentSeller;
  if (!browserSignals.interestGroupOwner.startsWith('https://a.test'))
    throw 'Wrong interestGroupOwner ' + browserSignals.interestGroupOwner;
  if (browserSignals.renderUrl !== "https://example.com/render")
    throw 'Wrong renderUrl ' + browserSignals.renderUrl;

  // Fields that vary by method.
  if (isScoreAd) {
    if (Object.keys(browserSignals).length !== 7) {
      throw 'Wrong number of browser signals fields ' +
          JSON.stringify(browserSignals);
    }
    const adComponentsJson = JSON.stringify(browserSignals.adComponents);
    if (adComponentsJson !== '["https://example.com/render-component"]')
      throw 'Wrong adComponents ' + adComponentsJson;
    if (browserSignals.biddingDurationMsec < 0)
      throw 'Wrong biddingDurationMsec ' + browserSignals.biddingDurationMsec;
    if (browserSignals.dataVersion !== 5678)
      throw 'Wrong dataVersion ' + browserSignals.dataVersion;
  } else {
    if (Object.keys(browserSignals).length !== 10) {
      throw 'Wrong number of browser signals fields ' +
          JSON.stringify(browserSignals);
    }
    validateBid(browserSignals.bid);
    if (browserSignals.desirability !== 13)
      throw 'Wrong desireability ' + browserSignals.desirability;
    if (browserSignals.highestScoringOtherBid !== 0) {
      throw 'Wrong highestScoringOtherBid ' +
          browserSignals.highestScoringOtherBid;
    }
    if (browserSignals.dataVersion !== 5678)
      throw 'Wrong dataVersion ' + browserSignals.dataVersion;
    if (browserSignals.modifiedBid !== 42)
      throw 'Wrong modifiedBid ' + browserSignals.modifiedBid;
    const topLevelSellerSignals =
        JSON.stringify(browserSignals.topLevelSellerSignals);
    if (topLevelSellerSignals !== '["top-level seller signals for winner"]')
      throw 'Wrong topLevelSellerSignals ' + topLevelSellerSignals;
  }
}
