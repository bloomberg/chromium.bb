// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains forward declarations of template classes in editing/

namespace blink {

class NodeTraversal;
class FlatTreeTraversal;

template <typename Traversal>
class EditingAlgorithm;
using EditingStrategy = EditingAlgorithm<NodeTraversal>;
using EditingInFlatTreeStrategy = EditingAlgorithm<FlatTreeTraversal>;

// TODO(editing-dev): Add forward declaration of
// - PositionTemplate
// - PositionWithAffinityTemplate
// - VisiblePositionTemplate
// - EphemeralRangeTemplate
// - SelectionTemplate

template <typename Strategy>
class VisibleSelectionTemplate;
using VisibleSelection = VisibleSelectionTemplate<EditingStrategy>;
using VisibleSelectionInFlatTree =
    VisibleSelectionTemplate<EditingInFlatTreeStrategy>;

};  // namespace blink
