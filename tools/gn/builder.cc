// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/builder.h"

#include "tools/gn/config.h"
#include "tools/gn/err.h"
#include "tools/gn/loader.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/settings.h"
#include "tools/gn/target.h"
#include "tools/gn/trace.h"

namespace {

typedef BuilderRecord::BuilderRecordSet BuilderRecordSet;

// Recursively looks in the tree for a given node, returning true if it
// was found in the dependecy graph. This is used to see if a given node
// participates in a cycle.
//
// If this returns true, the cycle will be in *path. This should point to an
// empty vector for the first call. During computation, the path will contain
// the full dependency path to the current node.
//
// Return false means no cycle was found.
bool RecursiveFindCycle(const BuilderRecord* search_in,
                        std::vector<const BuilderRecord*>* path) {
  path->push_back(search_in);

  const BuilderRecord::BuilderRecordSet& unresolved =
      search_in->unresolved_deps();
  for (BuilderRecord::BuilderRecordSet::const_iterator i = unresolved.begin();
       i != unresolved.end(); ++i) {
    const BuilderRecord* cur = *i;

    std::vector<const BuilderRecord*>::iterator found =
        std::find(path->begin(), path->end(), cur);
    if (found != path->end()) {
      // This item is already in the set, we found the cycle. Everything before
      // the first definition of cur is irrelevant to the cycle.
      path->erase(path->begin(), found);
      path->push_back(cur);
      return true;
    }

    if (RecursiveFindCycle(cur, path))
      return true;  // Found cycle.
  }
  path->pop_back();
  return false;
}

}  // namespace

Builder::Builder(Loader* loader) : loader_(loader) {
}

Builder::~Builder() {
}

void Builder::ItemDefined(scoped_ptr<Item> item) {
  ScopedTrace trace(TraceItem::TRACE_DEFINE_TARGET, item->label());
  trace.SetToolchain(item->settings()->toolchain_label());

  BuilderRecord::ItemType type = BuilderRecord::TypeOfItem(item.get());

  Err err;
  BuilderRecord* record =
      GetOrCreateRecordOfType(item->label(), item->defined_from(), type, &err);
  if (!record) {
    g_scheduler->FailWithError(err);
    return;
  }

  // Check that it's not been already defined.
  if (record->item()) {
    err = Err(item->defined_from(), "Duplicate definition.",
        "The item\n  " + item->label().GetUserVisibleName(false) +
        "\nwas already defined.");
    err.AppendSubErr(Err(record->item()->defined_from(),
                         "Previous definition:"));
    g_scheduler->FailWithError(err);
    return;
  }

  record->set_item(item.Pass());

  // Do target-specific dependency setup. This will also schedule dependency
  // loads for targets that are required.
  switch (type) {
    case BuilderRecord::ITEM_TARGET:
      TargetDefined(record, &err);
      break;
    case BuilderRecord::ITEM_TOOLCHAIN:
      ToolchainDefined(record, &err);
      break;
    default:
      break;
  }
  if (err.has_error()) {
    g_scheduler->FailWithError(err);
    return;
  }

  if (record->can_resolve()) {
    if (!ResolveItem(record, &err)) {
      g_scheduler->FailWithError(err);
      return;
    }
  }
}

const Item* Builder::GetItem(const Label& label) const {
  const BuilderRecord* record = GetRecord(label);
  if (!record)
    return NULL;
  return record->item();
}

const Toolchain* Builder::GetToolchain(const Label& label) const {
  const BuilderRecord* record = GetRecord(label);
  if (!record)
    return NULL;
  if (!record->item())
    return NULL;
  return record->item()->AsToolchain();
}

std::vector<const BuilderRecord*> Builder::GetAllRecords() const {
  std::vector<const BuilderRecord*> result;
  result.reserve(records_.size());
  for (RecordMap::const_iterator i = records_.begin();
       i != records_.end(); ++i)
    result.push_back(i->second);
  return result;
}

std::vector<const Target*> Builder::GetAllResolvedTargets() const {
  std::vector<const Target*> result;
  result.reserve(records_.size());
  for (RecordMap::const_iterator i = records_.begin();
       i != records_.end(); ++i) {
    if (i->second->type() == BuilderRecord::ITEM_TARGET &&
        i->second->should_generate() && i->second->item())
      result.push_back(i->second->item()->AsTarget());
  }
  return result;
}

const BuilderRecord* Builder::GetRecord(const Label& label) const {
  // Forward to the non-const version.
  return const_cast<Builder*>(this)->GetRecord(label);
}

BuilderRecord* Builder::GetRecord(const Label& label) {
  RecordMap::iterator found = records_.find(label);
  if (found == records_.end())
    return NULL;
  return found->second;
}

bool Builder::CheckForBadItems(Err* err) const {
  // Look for errors where we find a defined node with an item that refers to
  // an undefined one with no item. There may be other nodes in turn depending
  // on our defined one, but listing those isn't helpful: we want to find the
  // broken link.
  //
  // This finds normal "missing dependency" errors but does not find circular
  // dependencies because in this case all items in the cycle will be GENERATED
  // but none will be resolved. If this happens, we'll check explicitly for
  // that below.
  std::vector<const BuilderRecord*> bad_records;
  std::string depstring;
  for (RecordMap::const_iterator i = records_.begin();
       i != records_.end(); ++i) {
    const BuilderRecord* src = i->second;
    if (!src->should_generate())
      continue;  // Skip ungenerated nodes.

    if (!src->resolved()) {
      bad_records.push_back(src);

      // Check dependencies.
      for (BuilderRecord::BuilderRecordSet::const_iterator dest_iter =
               src->unresolved_deps().begin();
          dest_iter != src->unresolved_deps().end();
          ++dest_iter) {
        const BuilderRecord* dest = *dest_iter;
        if (!dest->item()) {
          depstring += src->label().GetUserVisibleName(true) +
              "\n  needs " + dest->label().GetUserVisibleName(true) + "\n";
        }
      }
    }
  }

  if (!depstring.empty()) {
    *err = Err(Location(), "Unresolved dependencies.", depstring);
    return false;
  }

  if (!bad_records.empty()) {
    // Our logic above found a bad node but didn't identify the problem. This
    // normally means a circular dependency.
    depstring = CheckForCircularDependencies(bad_records);
    if (depstring.empty()) {
      // Something's very wrong, just dump out the bad nodes.
      depstring = "I have no idea what went wrong, but these are unresolved, "
          "possible due to an\ninternal error:";
      for (size_t i = 0; i < bad_records.size(); i++) {
        depstring += "\n\"" +
            bad_records[i]->label().GetUserVisibleName(false) + "\"";
      }
      *err = Err(Location(), "", depstring);
    } else {
      *err = Err(Location(), "Dependency cycle:", depstring);
    }
    return false;
  }

  return true;
}

bool Builder::TargetDefined(BuilderRecord* record, Err* err) {
  Target* target = record->item()->AsTarget();

  if (!AddDeps(record, target->deps(), err) ||
      !AddDeps(record, target->datadeps(), err) ||
      !AddDeps(record, target->configs().vector(), err) ||
      !AddDeps(record, target->all_dependent_configs(), err) ||
      !AddDeps(record, target->direct_dependent_configs(), err) ||
      !AddToolchainDep(record, target, err))
    return false;

  // All targets in the default toolchain get generated by default. We also
  // check if this target was previously marked as "required" and force setting
  // the bit again so the target's dependencies (which we now know) get the
  // required bit pushed to them.
  if (record->should_generate() || target->settings()->is_default())
    RecursiveSetShouldGenerate(record, true);

  return true;
}

bool Builder::ToolchainDefined(BuilderRecord* record, Err* err) {
  Toolchain* toolchain = record->item()->AsToolchain();

  if (!AddDeps(record, toolchain->deps(), err))
    return false;

  // The default toolchain gets generated by default. Also propogate the
  // generate flag if it depends on items in a non-default toolchain.
  if (record->should_generate() ||
      toolchain->settings()->default_toolchain_label() == toolchain->label())
    RecursiveSetShouldGenerate(record, true);

  loader_->ToolchainLoaded(toolchain);
  return true;
}

BuilderRecord* Builder::GetOrCreateRecordOfType(const Label& label,
                                                const ParseNode* request_from,
                                                BuilderRecord::ItemType type,
                                                Err* err) {
  BuilderRecord* record = GetRecord(label);
  if (!record) {
    // Not seen this record yet, create a new one.
    record = new BuilderRecord(type, label);
    record->set_originally_referenced_from(request_from);
    records_[label] = record;
    return record;
  }

  // Check types.
  if (record->type() != type) {
    std::string msg =
        "The type of " + label.GetUserVisibleName(false) +
        "\nhere is a " + BuilderRecord::GetNameForType(type) +
        " but was previously seen as a " +
        BuilderRecord::GetNameForType(record->type()) + ".\n\n"
        "The most common cause is that the label of a config was put in the\n"
        "in the deps section of a target (or vice-versa).";
    *err = Err(request_from, "Item type does not match.", msg);
    if (record->originally_referenced_from()) {
      err->AppendSubErr(Err(record->originally_referenced_from(),
                            std::string()));
    }
    return NULL;
  }

  return record;
}

BuilderRecord* Builder::GetResolvedRecordOfType(const Label& label,
                                                const ParseNode* origin,
                                                BuilderRecord::ItemType type,
                                                Err* err) {
  BuilderRecord* record = GetRecord(label);
  if (!record) {
    *err = Err(origin, "Item not found",
        "\"" + label.GetUserVisibleName(false) + "\" doesn't\n"
        "refer to an existent thing.");
    return NULL;
  }

  const Item* item = record->item();
  if (!item) {
    *err = Err(origin, "Item not resolved.",
        "\"" + label.GetUserVisibleName(false) + "\" hasn't been resolved.\n");
    return NULL;
  }

  if (!BuilderRecord::IsItemOfType(item, type)) {
    *err = Err(origin,
        std::string("This is not a ") + BuilderRecord::GetNameForType(type),
        "\"" + label.GetUserVisibleName(false) + "\" refers to a " +
        item->GetItemTypeName() + " instead of a " +
        BuilderRecord::GetNameForType(type) + ".");
    return NULL;
  }
  return record;
}

bool Builder::AddDeps(BuilderRecord* record,
                      const LabelConfigVector& configs,
                      Err* err) {
  for (size_t i = 0; i < configs.size(); i++) {
    BuilderRecord* dep_record = GetOrCreateRecordOfType(
        configs[i].label, configs[i].origin, BuilderRecord::ITEM_CONFIG, err);
    if (!dep_record)
      return false;
    record->AddDep(dep_record);
  }
  return true;
}

bool Builder::AddDeps(BuilderRecord* record,
                      const UniqueVector<LabelConfigPair>& configs,
                      Err* err) {
  for (size_t i = 0; i < configs.size(); i++) {
    BuilderRecord* dep_record = GetOrCreateRecordOfType(
        configs[i].label, configs[i].origin, BuilderRecord::ITEM_CONFIG, err);
    if (!dep_record)
      return false;
    record->AddDep(dep_record);
  }
  return true;
}

bool Builder::AddDeps(BuilderRecord* record,
                      const LabelTargetVector& targets,
                      Err* err) {
  for (size_t i = 0; i < targets.size(); i++) {
    BuilderRecord* dep_record = GetOrCreateRecordOfType(
        targets[i].label, targets[i].origin, BuilderRecord::ITEM_TARGET, err);
    if (!dep_record)
      return false;
    record->AddDep(dep_record);
  }
  return true;
}

bool Builder::AddToolchainDep(BuilderRecord* record,
                              const Target* target,
                              Err* err) {
  BuilderRecord* toolchain_record = GetOrCreateRecordOfType(
      target->settings()->toolchain_label(), target->defined_from(),
      BuilderRecord::ITEM_TOOLCHAIN, err);
  if (!toolchain_record)
    return false;
  record->AddDep(toolchain_record);

  return true;
}

void Builder::RecursiveSetShouldGenerate(BuilderRecord* record,
                                         bool force) {
  if (!force && record->should_generate())
    return;  // Already set.
  record->set_should_generate(true);

  const BuilderRecordSet& deps = record->all_deps();
  for (BuilderRecordSet::const_iterator i = deps.begin();
       i != deps.end(); i++) {
    BuilderRecord* cur = *i;
    if (!cur->should_generate()) {
      ScheduleItemLoadIfNecessary(cur);
      RecursiveSetShouldGenerate(cur, false);
    }
  }
}

void Builder::ScheduleItemLoadIfNecessary(BuilderRecord* record) {
  const ParseNode* origin = record->originally_referenced_from();
  loader_->Load(record->label(),
                origin ? origin->GetRange() : LocationRange());
}

bool Builder::ResolveItem(BuilderRecord* record, Err* err) {
  DCHECK(record->can_resolve() && !record->resolved());

  if (record->type() == BuilderRecord::ITEM_TARGET) {
    Target* target = record->item()->AsTarget();
    if (!ResolveDeps(&target->deps(), err) ||
        !ResolveDeps(&target->datadeps(), err) ||
        !ResolveConfigs(&target->configs(), err) ||
        !ResolveConfigs(&target->all_dependent_configs(), err) ||
        !ResolveConfigs(&target->direct_dependent_configs(), err) ||
        !ResolveForwardDependentConfigs(target, err))
      return false;
  } else if (record->type() == BuilderRecord::ITEM_TOOLCHAIN) {
    Toolchain* toolchain = record->item()->AsToolchain();
    if (!ResolveDeps(&toolchain->deps(), err))
      return false;
  }

  record->set_resolved(true);
  record->item()->OnResolved();
  if (!resolved_callback_.is_null())
    resolved_callback_.Run(record);

  // Recursively update everybody waiting on this item to be resolved.
  BuilderRecordSet& waiting_set = record->waiting_on_resolution();
  for (BuilderRecordSet::iterator i = waiting_set.begin();
       i != waiting_set.end(); ++i) {
    BuilderRecord* waiting = *i;
    DCHECK(waiting->unresolved_deps().find(record) !=
           waiting->unresolved_deps().end());
    waiting->unresolved_deps().erase(record);

    if (waiting->can_resolve()) {
      if (!ResolveItem(waiting, err))
        return false;
    }
  }
  waiting_set.clear();
  return true;
}

bool Builder::ResolveDeps(LabelTargetVector* deps, Err* err) {
  for (size_t i = 0; i < deps->size(); i++) {
    LabelTargetPair& cur = (*deps)[i];
    DCHECK(!cur.ptr);

    BuilderRecord* record = GetResolvedRecordOfType(
        cur.label, cur.origin, BuilderRecord::ITEM_TARGET, err);
    if (!record)
      return false;
    cur.ptr = record->item()->AsTarget();
  }
  return true;
}

bool Builder::ResolveConfigs(UniqueVector<LabelConfigPair>* configs, Err* err) {
  for (size_t i = 0; i < configs->size(); i++) {
    const LabelConfigPair& cur = (*configs)[i];
    DCHECK(!cur.ptr);

    BuilderRecord* record = GetResolvedRecordOfType(
        cur.label, cur.origin, BuilderRecord::ITEM_CONFIG, err);
    if (!record)
      return false;
    const_cast<LabelConfigPair&>(cur).ptr = record->item()->AsConfig();
  }
  return true;
}

// "Forward dependent configs" should refer to targets in the deps that should
// have their configs forwarded.
bool Builder::ResolveForwardDependentConfigs(Target* target, Err* err) {
  const LabelTargetVector& deps = target->deps();
  const UniqueVector<LabelTargetPair>& configs =
      target->forward_dependent_configs();

  // Assume that the lists are small so that brute-force n^2 is appropriate.
  for (size_t config_i = 0; config_i < configs.size(); config_i++) {
    for (size_t dep_i = 0; dep_i < deps.size(); dep_i++) {
      if (configs[config_i].label == deps[dep_i].label) {
        DCHECK(deps[dep_i].ptr);  // Should already be resolved.
        // UniqueVector's contents are constant so uniqueness is preserved, but
        // we want to update this pointer which doesn't change uniqueness
        // (uniqueness in this vector is determined by the label only).
        const_cast<LabelTargetPair&>(configs[config_i]).ptr = deps[dep_i].ptr;
        break;
      }
    }
    if (!configs[config_i].ptr) {
      *err = Err(target->defined_from(),
          "Target in forward_dependent_configs_from was not listed in the deps",
          "This target has a forward_dependent_configs_from entry that was "
          "not present in\nthe deps. A target can only forward things it "
          "depends on. It was forwarding:\n  " +
          configs[config_i].label.GetUserVisibleName(false));
      return false;
    }
  }
  return true;
}

std::string Builder::CheckForCircularDependencies(
    const std::vector<const BuilderRecord*>& bad_records) const {
  std::vector<const BuilderRecord*> cycle;
  if (!RecursiveFindCycle(bad_records[0], &cycle))
    return std::string();  // Didn't find a cycle, something else is wrong.

  // Walk backwards since the dependency arrows point in the reverse direction.
  std::string ret;
  for (int i = static_cast<int>(cycle.size()) - 1; i >= 0; i--) {
    ret += "  " + cycle[i]->label().GetUserVisibleName(false);
    if (i != 0)
      ret += " ->\n";
  }

  return ret;
}
