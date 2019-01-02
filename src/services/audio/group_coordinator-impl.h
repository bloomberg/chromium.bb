// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_GROUP_COORDINATOR_IMPL_H_
#define SERVICES_AUDIO_GROUP_COORDINATOR_IMPL_H_

namespace audio {

template <typename Member>
GroupCoordinator<Member>::GroupCoordinator() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

template <typename Member>
GroupCoordinator<Member>::~GroupCoordinator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(groups_.empty());
}

template <typename Member>
void GroupCoordinator<Member>::RegisterMember(
    const base::UnguessableToken& group_id,
    Member* member) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(member);

  const auto it = FindGroup(group_id);
  std::vector<Member*>& members = it->second.members;
  DCHECK(!base::ContainsValue(members, member));
  members.push_back(member);

  for (Observer* observer : it->second.observers) {
    observer->OnMemberJoinedGroup(member);
  }
}

template <typename Member>
void GroupCoordinator<Member>::UnregisterMember(
    const base::UnguessableToken& group_id,
    Member* member) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(member);

  const auto group_it = FindGroup(group_id);
  std::vector<Member*>& members = group_it->second.members;
  const auto member_it = std::find(members.begin(), members.end(), member);
  DCHECK(member_it != members.end());
  members.erase(member_it);

  for (Observer* observer : group_it->second.observers) {
    observer->OnMemberLeftGroup(member);
  }

  MaybePruneGroupMapEntry(group_it);
}

template <typename Member>
void GroupCoordinator<Member>::AddObserver(
    const base::UnguessableToken& group_id,
    Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);

  std::vector<Observer*>& observers = FindGroup(group_id)->second.observers;
  DCHECK(!base::ContainsValue(observers, observer));
  observers.push_back(observer);
}

template <typename Member>
void GroupCoordinator<Member>::RemoveObserver(
    const base::UnguessableToken& group_id,
    Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);

  const auto group_it = FindGroup(group_id);
  std::vector<Observer*>& observers = group_it->second.observers;
  const auto it = std::find(observers.begin(), observers.end(), observer);
  DCHECK(it != observers.end());
  observers.erase(it);

  MaybePruneGroupMapEntry(group_it);
}

template <typename Member>
const std::vector<Member*>& GroupCoordinator<Member>::GetCurrentMembers(
    const base::UnguessableToken& group_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const auto& entry : groups_) {
    if (entry.first == group_id) {
      return entry.second.members;
    }
  }

  static const std::vector<Member*> empty_set;
  return empty_set;
}

template <typename Member>
typename GroupCoordinator<Member>::GroupMap::iterator
GroupCoordinator<Member>::FindGroup(const base::UnguessableToken& group_id) {
  for (auto it = groups_.begin(); it != groups_.end(); ++it) {
    if (it->first == group_id) {
      return it;
    }
  }

  // Group does not exist. Create a new entry.
  groups_.emplace_back();
  const auto new_it = groups_.end() - 1;
  new_it->first = group_id;
  return new_it;
}

template <typename Member>
void GroupCoordinator<Member>::MaybePruneGroupMapEntry(
    typename GroupMap::iterator it) {
  if (it->second.members.empty() && it->second.observers.empty()) {
    groups_.erase(it);
  }
}

template <typename Member>
GroupCoordinator<Member>::Observer::~Observer() = default;

template <typename Member>
GroupCoordinator<Member>::Group::Group() = default;
template <typename Member>
GroupCoordinator<Member>::Group::~Group() = default;
template <typename Member>
GroupCoordinator<Member>::Group::Group(
    GroupCoordinator<Member>::Group&& other) = default;
template <typename Member>
typename GroupCoordinator<Member>::Group& GroupCoordinator<Member>::Group::
operator=(GroupCoordinator::Group&& other) = default;

}  // namespace audio

#endif  // SERVICES_AUDIO_GROUP_COORDINATOR_IMPL_H_
