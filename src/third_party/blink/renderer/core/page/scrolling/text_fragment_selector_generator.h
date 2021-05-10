// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_SELECTOR_GENERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_SELECTOR_GENERATOR_H_

#include "base/optional.h"
#include "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#include "third_party/blink/public/mojom/link_to_text/link_to_text.mojom-blink.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_finder.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_selector.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"

namespace blink {

class LocalFrame;

// TextFragmentSelectorGenerator is responsible for generating text fragment
// selectors for the user selected text according to spec in
// https://github.com/WICG/scroll-to-text-fragment#proposed-solution.
// Generated selectors would be later used to highlight the same
// text if successfully parsed by |TextFragmentAnchor |. Generation will be
// triggered when users request "link to text" for the selected text.
//
// TextFragmentSelectorGenerator generates candidate selectors and tries it
// against the page content to ensure the correct and unique match. Repeats the
// process adding context/range to the selector as necessary until the correct
// match is uniquely identified or no new context/range can be added.
class CORE_EXPORT TextFragmentSelectorGenerator final
    : public GarbageCollected<TextFragmentSelectorGenerator>,
      public TextFragmentFinder::Client,
      public blink::mojom::blink::TextFragmentSelectorProducer {
 public:
  explicit TextFragmentSelectorGenerator() = default;

  void BindTextFragmentSelectorProducer(
      mojo::PendingReceiver<mojom::blink::TextFragmentSelectorProducer>
          producer);

  // Sets the frame and range of the current selection.
  void UpdateSelection(LocalFrame* selection_frame,
                       const EphemeralRangeInFlatTree& selection_range);

  // Adjust the selection start/end to a valid position. That includes skipping
  // non text start/end nodes and extending selection from start and end to
  // contain full words.
  void AdjustSelection();

  // blink::mojom::blink::TextFragmentSelectorProducer interface
  void Cancel() override;

  // Requests selector for current selection.
  void RequestSelector(RequestSelectorCallback callback) override;

  // TextFragmentFinder::Client interface
  void DidFindMatch(const EphemeralRangeInFlatTree& match,
                    const TextFragmentAnchorMetrics::Match match_metrics,
                    bool is_unique) override;

  void NoMatchFound() override;

  // Wrappers for tests.
  String GetPreviousTextBlockForTesting(const Position& position) {
    return GetPreviousTextBlock(position);
  }
  String GetNextTextBlockForTesting(const Position& position) {
    return GetNextTextBlock(position);
  }
  void SetCallbackForTesting(RequestSelectorCallback callback) {
    pending_generate_selector_callback_ = std::move(callback);
  }

  // Releases members if necessary.
  void ClearSelection();

  void Trace(Visitor*) const;

 private:
  // Used for determining the next step of selector generation.
  enum GenerationStep { kExact, kRange, kContext };

  // Used for determining the current state of |selector_|.
  enum SelectorState {
    // Sreach for candidate selector didn't start.
    kNotStarted,

    // Candidate selector should be generated or extended.
    kNeedsNewCandidate,

    // Candidate selector generation was successful and selector is ready to be
    // tested for uniqueness and accuracy by running against the page's content.
    kTestCandidate,

    // Candidate selector generation was unsuccessful. No further attempts are
    // necessary.
    kFailure,

    // Selector is found. No further attempts are necessary.
    kSuccess
  };

  // Generates selector for current selection.
  void GenerateSelector();

  void GenerateSelectorCandidate();

  void ResolveSelectorState();
  void RunTextFinder();

  // Returns max text preceding given position that doesn't cross block
  // boundaries.
  String GetPreviousTextBlock(const Position& position);

  // Returns max text following given position that doesn't cross block
  // boundaries.
  String GetNextTextBlock(const Position& position);

  void GenerateExactSelector();
  void ExtendRangeSelector();
  void ExtendContext();

  void Reset();

  void RecordAllMetrics(const TextFragmentSelector& selector);
  void RecordPreemptiveGenerationMetrics(const TextFragmentSelector& selector);

  // Called when selector generation is complete.
  void OnSelectorReady(const TextFragmentSelector& selector);

  // Called to notify clients of the result of |GenerateSelector|.
  void NotifyClientSelectorReady(const TextFragmentSelector& selector);

  Member<LocalFrame> selection_frame_;
  Member<Range> selection_range_;
  std::unique_ptr<TextFragmentSelector> selector_;

  // Used for communication between |TextFragmentSelectorGenerator| in renderer
  // and |TextFragmentSelectorClientImpl| in browser.
  HeapMojoReceiver<blink::mojom::blink::TextFragmentSelectorProducer,
                   TextFragmentSelectorGenerator>
      selector_producer_{this, nullptr};
  RequestSelectorCallback pending_generate_selector_callback_;

  GenerationStep step_ = kExact;
  SelectorState state_ = kNeedsNewCandidate;

  // Used when preemptive link generation is enabled to report
  // whether |RequestSelector| was called before or after selector was ready.
  base::Optional<bool> selector_requested_before_ready_;

  base::Optional<shared_highlighting::LinkGenerationError> error_;

  // Fields used for keeping track of context.

  // Strings available for gradually forming prefix and suffix.
  String max_available_prefix_;
  String max_available_suffix_;

  String max_available_range_start_;
  String max_available_range_end_;

  // Indicates a number of words used from |max_available_prefix_| and
  // |max_available_suffix_| for the current |selector_|.
  int num_context_words_ = 0;

  int num_range_words_ = 0;

  int iteration_ = 0;
  base::TimeTicks generation_start_time_;

  Member<TextFragmentFinder> finder_;

  DISALLOW_COPY_AND_ASSIGN(TextFragmentSelectorGenerator);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_SELECTOR_GENERATOR_H_
