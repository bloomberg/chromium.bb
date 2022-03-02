// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Basic generate bid script that offers a bid of 1 using the first ad's
// `renderUrl` and, if present, the first adComponent's `renderUrl`.
function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                     trustedBiddingSignals, browserSignals) {
  const ad = interestGroup.ads[0];
  let result = {'ad': ad, 'bid': 1, 'render': ad.renderUrl};
  if (interestGroup.adComponents && interestGroup.adComponents[0])
    result.adComponents = [interestGroup.adComponents[0].renderUrl];
  return result;
}

function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                   browserSignals) {
  sendReportTo(browserSignals.interestGroupOwner + '/echoall?report_bidder');
}
