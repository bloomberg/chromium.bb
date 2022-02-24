// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  validateInterestGroup(interestGroup);
  validateAuctionSignals(auctionSignals);
  validatePerBuyerSignals(perBuyerSignals);
  validateTrustedBiddingSignals(trustedBiddingSignals);
  validateBrowserSignals(browserSignals, /*isGenerateBid=*/true);

  // Bid 2 to outbid other parties bidding 1 at auction.
  const ad = interestGroup.ads[0];
  return {
      'ad': ad,
      'bid': 2,
      'render': ad.renderUrl,
      'adComponents': [interestGroup.adComponents[0].renderUrl]
  };
}

function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                   browserSignals) {
  validateAuctionSignals(auctionSignals);
  validatePerBuyerSignals(perBuyerSignals);
  validateSellerSignals(sellerSignals);
  validateBrowserSignals(browserSignals, /*isGenerateBid=*/false);

  sendReportTo(browserSignals.interestGroupOwner + '/echo?report_bidder');
}

function validateInterestGroup(interestGroup) {
  if (!interestGroup)
    throw 'No interest group';
  if (interestGroup.name !== 'cars')
    throw 'Wrong interestGroup.name ' + interestGroup.name;
  if (!interestGroup.owner.startsWith('https://a.test'))
    throw 'Missing a.test in owner ' + interestGroup.owner;
  // TODO(crbug.com/1186444): Consider validating URL fields like
  // interestGroup.biddingLogicUrl once we decide what to do about URL
  // normalization.

  // If userBiddingSignals is passed as a JSON string instead of an object,
  // stringify() will wrap it in another layer of quotes, causing the test to
  // fail. The order of properties produced by stringify() isn't guaranteed by
  // the ECMAScript standard, but some sites depend on the V8 behavior of
  // serializing in declaration order.
  const userBiddingSignalsJSON =
      JSON.stringify(interestGroup.userBiddingSignals);
  if (userBiddingSignalsJSON !== '{"some":"json","data":{"here":[1,2,3]}}')
    throw 'Wrong userBiddingSignals ' + userBiddingSignalsJSON;

  if (interestGroup.ads.length !== 1)
    throw 'Wrong ads.length ' + interestGroup.ads.length;
  if (interestGroup.ads[0].renderUrl !== 'https://example.com/render')
    throw 'Wrong ads[0].renderUrl ' + interestGroup.ads[0].renderUrl;
  const adMetadataJson = JSON.stringify(interestGroup.ads[0].metadata);
  if (adMetadataJson !== '{"ad":"metadata","here":[1,2,3]}')
    throw 'Wrong ad[0].metadata ' + adMetadataJson;

  if (interestGroup.adComponents.length !== 1)
    throw 'Wrong adComponents.length ' + interestGroup.adComponents.length;
  if (interestGroup.adComponents[0].renderUrl !==
        'https://example.com/render-component') {
    throw 'Wrong adComponents[0].renderUrl ' +
        interestGroup.adComponents[0].renderUrl;
  }
  if (interestGroup.adComponents[0].metadata !== undefined) {
    throw 'interestGroup.adComponents[0].metadata ' +
        adMetadataJinterestGroup.adComponents[0].metadataSON;
  }
}

function validateAuctionSignals(auctionSignals) {
  const auctionSignalsJson = JSON.stringify(auctionSignals);
  if (auctionSignalsJson !== '["component auction signals"]')
    throw 'Wrong auctionSignals ' + auctionSignalsJson;
}

function validatePerBuyerSignals(perBuyerSignals) {
  const perBuyerSignalsJson = JSON.stringify(perBuyerSignals);
  if (perBuyerSignalsJson !== '["component buyer signals"]')
    throw 'Wrong perBuyerSignals ' + perBuyerSignalsJson;
}

function validateTrustedBiddingSignals(trustedBiddingSignals) {
  const trustedBiddingSignalsJson = JSON.stringify(trustedBiddingSignals);
  if (trustedBiddingSignalsJson !== '{"key1":"1"}')
    throw 'Wrong trustedBiddingSignals ' + trustedBiddingSignalsJson;
}

function validateBrowserSignals(browserSignals, isGenerateBid) {
  // Common fields for generateBid() and reportWin().
  if (browserSignals.topWindowHostname !== 'c.test')
    throw 'Wrong topWindowHostname ' + browserSignals.topWindowHostname;
  if (!browserSignals.seller.startsWith('https://d.test'))
    throw 'Wrong seller ' + browserSignals.seller;

  if (isGenerateBid) {
    if (Object.keys(browserSignals).length !== 5) {
      throw 'Wrong number of browser signals fields ' +
          JSON.stringify(browserSignals);
    }
    if (browserSignals.joinCount !== 1)
      throw 'Wrong joinCount ' + browserSignals.joinCount;
    if (browserSignals.bidCount !== 0)
      throw 'Wrong bidCount ' + bidCount;
    if (browserSignals.prevWins.length !== 0)
      throw 'Wrong prevWins ' + JSON.stringify(browserSignals.prevWins);
  } else {
    if (Object.keys(browserSignals).length !== 6) {
      throw 'Wrong number of browser signals fields ' +
          JSON.stringify(browserSignals);
    }
    if (!browserSignals.interestGroupOwner.startsWith('https://a.test'))
      throw 'Wrong interestGroupOwner ' + browserSignals.interestGroupOwner;
    if (browserSignals.interestGroupName !== 'cars')
      throw 'Wrong interestGroupName ' + browserSignals.interestGroupName;
    if (browserSignals.renderUrl !== "https://example.com/render")
      throw 'Wrong renderUrl ' + browserSignals.renderUrl;
    if (browserSignals.bid !== 2)
      throw 'Wrong bid ' + browserSignals.bid;
  }
}

function validateSellerSignals(sellerSignals) {
  const sellerSignalsJson = JSON.stringify(sellerSignals);
  if (sellerSignalsJson !== '["component seller signals for winner"]')
    throw 'Wrong sellerSignals ' + sellerSignals;
}
