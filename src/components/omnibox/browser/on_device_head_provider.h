// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_HEAD_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_HEAD_PROVIDER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/task/post_task.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/on_device_head_model.h"
#include "components/omnibox/browser/on_device_model_update_listener.h"

class AutocompleteProviderListener;

// An asynchronous autocomplete provider which receives input string and tries
// to find the matches in an on device head model. This provider is designed to
// help users get suggestions when they are in poor network.
// By default, all matches provided by this provider will have a relevance no
// greater than 99, such that its matches will not show before any other
// providers; However the relevance can be changed to any arbitrary value by
// Finch when the input is not classified as a URL.
// TODO(crbug.com/925072): make some cleanups after removing |model_| and |this|
// in task postings from this class.
class OnDeviceHeadProvider : public AutocompleteProvider {
 public:
  static OnDeviceHeadProvider* Create(AutocompleteProviderClient* client,
                                      AutocompleteProviderListener* listener);

  // Adds a callback to on device head model updater listener which will update
  // |model_filename_| once the model is ready on disk.
  void AddModelUpdateCallback();

  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(bool clear_cached_results, bool due_to_user_inactivity) override;
  void AddProviderInfo(ProvidersInfo* provider_info) const override;

  AutocompleteProviderClient* client() { return client_; }

 private:
  friend class OnDeviceHeadProviderTest;

  // A useful data structure to store Autocomplete input and suggestions fetched
  // from the on device head model for a search request to the model.
  struct OnDeviceHeadProviderParams;

  OnDeviceHeadProvider(AutocompleteProviderClient* client,
                       AutocompleteProviderListener* listener);
  ~OnDeviceHeadProvider() override;

  bool IsOnDeviceHeadProviderAllowed(const AutocompleteInput& input,
                                     const std::string& incognito_serve_mode);

  // Helper functions used for asynchronous search to the on device head model.
  // The Autocomplete input and output from the model will be passed from
  // DoSearch to SearchDone via the OnDeviceHeadProviderParams object.
  // DoSearch: searches the on device model and returns the tops suggestions
  // matches the given AutocompleteInput.
  void DoSearch(std::unique_ptr<OnDeviceHeadProviderParams> params);
  // SearchDone: called after DoSearch, fills |matches_| with the suggestions
  // fetches by DoSearch and then calls OnProviderUpdate.
  void SearchDone(std::unique_ptr<OnDeviceHeadProviderParams> params);

  // Used by OnDeviceModelUpdateListener to notify this provider when new model
  // is available.
  void OnModelUpdate(const std::string& new_model_filename);

  // Fetches suggestions matching the params from the given on device head
  // model.
  static std::unique_ptr<OnDeviceHeadProviderParams> GetSuggestionsFromModel(
      const std::string& model_filename,
      const size_t provider_max_matches,
      std::unique_ptr<OnDeviceHeadProviderParams> params);

  AutocompleteProviderClient* client_;
  AutocompleteProviderListener* listener_;

  // The task runner dedicated for on device head model operations which is
  // added to offload expensive operations out of the UI sequence.
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner_;

  // Sequence checker that ensure utocomplete request handling will only happen
  // main thread.
  SEQUENCE_CHECKER(main_sequence_checker_);

  // The filename points to the on device head model on the disk.
  std::string model_filename_;

  // The request id used to trace current request to the on device head model.
  // The id will be increased whenever a new request is received from the
  // AutocompleteController.
  size_t on_device_search_request_id_;

  // Owns the subscription after adding the model update callback to the
  // listener such that the callback can be removed automatically from the
  // listener on provider's deconstruction.
  std::unique_ptr<OnDeviceModelUpdateListener::UpdateSubscription>
      model_update_subscription_;

  base::WeakPtrFactory<OnDeviceHeadProvider> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(OnDeviceHeadProvider);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_HEAD_PROVIDER_H_
