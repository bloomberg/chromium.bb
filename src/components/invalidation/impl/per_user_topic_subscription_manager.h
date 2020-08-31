// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_PER_USER_TOPIC_SUBSCRIPTION_MANAGER_H_
#define COMPONENTS_INVALIDATION_IMPL_PER_USER_TOPIC_SUBSCRIPTION_MANAGER_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/invalidation/impl/channels_states.h"
#include "components/invalidation/impl/per_user_topic_subscription_request.h"
#include "components/invalidation/public/identity_provider.h"
#include "components/invalidation/public/invalidation_export.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"
#include "net/base/backoff_entry.h"
#include "net/url_request/url_request_context_getter.h"

class PrefRegistrySimple;
class PrefService;

namespace invalidation {
class ActiveAccountAccessTokenFetcher;
class IdentityProvider;
}  // namespace invalidation

namespace syncer {

// A class that manages the subscription to topics for server-issued
// notifications.
// Manages the details of subscribing to topics for invalidations. For example,
// Chrome Sync uses the ModelTypes (bookmarks, passwords, autofill data) as
// topics.
class INVALIDATION_EXPORT PerUserTopicSubscriptionManager {
 public:
  class Observer {
   public:
    virtual void OnSubscriptionChannelStateChanged(
        SubscriptionChannelState state) = 0;
  };

  PerUserTopicSubscriptionManager(
      invalidation::IdentityProvider* identity_provider,
      PrefService* pref_service,
      network::mojom::URLLoaderFactory* url_loader_factory,
      const std::string& project_id,
      bool migrate_prefs);

  virtual ~PerUserTopicSubscriptionManager();

  // Just calls std::make_unique. For ease of base::Bind'ing
  static std::unique_ptr<PerUserTopicSubscriptionManager> Create(
      invalidation::IdentityProvider* identity_provider,
      PrefService* pref_service,
      network::mojom::URLLoaderFactory* url_loader_factory,
      const std::string& project_id,
      bool migrate_prefs);

  // RegisterProfilePrefs and RegisterPrefs register the same prefs, because on
  // device level (sign in screen, device local account) we spin up separate
  // InvalidationService and on profile level (when user signed in) we have
  // another InvalidationService, and we want to keep profile data in an
  // encrypted area of disk. While device data which is public can be kept in an
  // unencrypted area.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  static void RegisterPrefs(PrefRegistrySimple* registry);

  virtual void Init();

  // Triggers subscription and/or unsubscription requests so that the set of
  // subscribed topics matches |topics|. If the |instance_id_token| has changed,
  // triggers re-subscription for all topics.
  virtual void UpdateSubscribedTopics(const Topics& topics,
                                      const std::string& instance_id_token);

  // Called when the InstanceID token (previously passed to
  // UpdateSubscribedTopics()) is deleted or revoked. Clears the cached token
  // and any subscribed topics, since the subscriptions will not be valid
  // anymore.
  void ClearInstanceIDToken();

  // Classes interested in subscription channel state changes should implement
  // PerUserTopicSubscriptionManager::Observer and register here.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  base::DictionaryValue CollectDebugData() const;

  virtual base::Optional<Topic> LookupSubscribedPublicTopicByPrivateTopic(
      const std::string& private_topic) const;

  TopicSet GetSubscribedTopicsForTest() const;

  bool HaveAllRequestsFinishedForTest() const {
    return pending_subscriptions_.empty();
  }

 private:
  struct SubscriptionEntry;
  enum class TokenStateOnSubscriptionRequest;

  void StartPendingSubscriptions();

  // Tries to (un)subscribe to |topic|. No retry in case of failure.
  // Effectively no-op if (un)subscription request is backed off or already in
  // flight with the same access token.
  void StartPendingSubscriptionRequest(const Topic& topic);

  void ActOnSuccessfulSubscription(
      const Topic& topic,
      const std::string& private_topic_name,
      PerUserTopicSubscriptionRequest::RequestType type);
  void ScheduleRequestForRepetition(const Topic& topic);
  void SubscriptionFinishedForTopic(
      Topic topic,
      Status code,
      std::string private_topic_name,
      PerUserTopicSubscriptionRequest::RequestType type);

  void RequestAccessToken();

  void OnAccessTokenRequestCompleted(GoogleServiceAuthError error,
                                     std::string access_token);
  void OnAccessTokenRequestSucceeded(const std::string& access_token);
  void OnAccessTokenRequestFailed(GoogleServiceAuthError error);

  void DropAllSavedSubscriptionsOnTokenChange();
  TokenStateOnSubscriptionRequest DropAllSavedSubscriptionsOnTokenChangeImpl();

  void NotifySubscriptionChannelStateChange(
      SubscriptionChannelState invalidator_state);

  PrefService* const pref_service_;
  invalidation::IdentityProvider* const identity_provider_;
  network::mojom::URLLoaderFactory* const url_loader_factory_;

  const std::string project_id_;

  const bool migrate_prefs_;

  // Subscription or unsubscription requests that are either scheduled or
  // started, but not finished yet.
  std::map<Topic, std::unique_ptr<SubscriptionEntry>> pending_subscriptions_;

  // For subscribed topics, these map from the topic to the private topic name
  // and vice versa.
  std::map<Topic, std::string> topic_to_private_topic_;
  std::map<std::string, Topic> private_topic_to_topic_;

  // Token derived from GCM IID.
  std::string instance_id_token_;

  // Cached OAuth2 access token, and/or pending request to fetch one.
  std::string access_token_;
  std::unique_ptr<invalidation::ActiveAccountAccessTokenFetcher>
      access_token_fetcher_;
  base::OneShotTimer request_access_token_retry_timer_;
  net::BackoffEntry request_access_token_backoff_;

  base::ObserverList<Observer>::Unchecked observers_;
  SubscriptionChannelState last_issued_state_ =
      SubscriptionChannelState::NOT_STARTED;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(PerUserTopicSubscriptionManager);
};

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_IMPL_PER_USER_TOPIC_SUBSCRIPTION_MANAGER_H_
