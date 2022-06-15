// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

@available(iOS 15, *)
struct OverflowMenuView: View {
  enum Dimensions {
    static let destinationListHeight: CGFloat = 123
  }

  var model: OverflowMenuModel

  weak var metricsHandler: PopupMenuMetricsHandler?

  weak var carouselMetricsDelegate: PopupMenuCarouselMetricsDelegate?

  var body: some View {
    VStack(
      alignment: .leading,
      // Leave no spaces above or below Divider, the two other sections will
      // include proper spacing.
      spacing: 0
    ) {
      OverflowMenuDestinationList(
        destinations: model.destinations, metricsHandler: metricsHandler
      ).onPreferenceChange(
        DestinationVisibilityPreferenceKey.self
      ) {
        (value: DestinationVisibilityPreferenceKey.Value) in
        carouselMetricsDelegate?.visibleDestinationCountDidChange(value)
      }.frame(height: Dimensions.destinationListHeight)
      Divider()
      OverflowMenuActionList(actionGroups: model.actionGroups, metricsHandler: metricsHandler)
    }.background(Color(.systemGroupedBackground).edgesIgnoringSafeArea(.all))
  }
}
