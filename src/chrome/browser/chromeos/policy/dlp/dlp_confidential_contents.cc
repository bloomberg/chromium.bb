// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_contents.h"

#include <memory>
#include <vector>

#include "base/containers/cxx20_erase_vector.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"

namespace policy {

namespace {

gfx::ImageSkia GetWindowIcon(aura::Window* window) {
  gfx::ImageSkia* image = window->GetProperty(aura::client::kWindowIconKey);
  if (!image)
    image = window->GetProperty(aura::client::kAppIconKey);
  return image ? *image : gfx::ImageSkia();
}

}  // namespace

// The maximum number of entries that can be kept in the
// DlpConfidentialContentsCache.
// TODO(crbug.com/1275926): determine the value to use
static constexpr size_t kDefaultCacheSizeLimit = 100;

// The default timeout after which the entries are evicted from the
// DlpConfidentialContentsCache.
static constexpr base::TimeDelta kDefaultCacheTimeout = base::Days(7);

DlpConfidentialContent::DlpConfidentialContent() = default;

DlpConfidentialContent::DlpConfidentialContent(
    content::WebContents* web_contents)
    : icon(favicon::TabFaviconFromWebContents(web_contents).AsImageSkia()),
      title(web_contents->GetTitle()),
      url(web_contents->GetLastCommittedURL()) {}

DlpConfidentialContent::DlpConfidentialContent(aura::Window* window,
                                               const GURL& url)
    : icon(GetWindowIcon(window)), title(window->GetTitle()), url(url) {}

DlpConfidentialContent::DlpConfidentialContent(
    const DlpConfidentialContent& other) = default;
DlpConfidentialContent& DlpConfidentialContent::operator=(
    const DlpConfidentialContent& other) = default;

bool DlpConfidentialContent::operator==(
    const DlpConfidentialContent& other) const {
  return url.EqualsIgnoringRef(other.url);
}

bool DlpConfidentialContent::operator!=(
    const DlpConfidentialContent& other) const {
  return !(*this == other);
}

bool DlpConfidentialContent::operator<(
    const DlpConfidentialContent& other) const {
  return title < other.title;
}

bool DlpConfidentialContent::operator<=(
    const DlpConfidentialContent& other) const {
  return *this == other || *this < other;
}

bool DlpConfidentialContent::operator>(
    const DlpConfidentialContent& other) const {
  return !(*this <= other);
}

bool DlpConfidentialContent::operator>=(
    const DlpConfidentialContent& other) const {
  return !(*this < other);
}

DlpConfidentialContents::DlpConfidentialContents() = default;

DlpConfidentialContents::DlpConfidentialContents(
    const std::vector<content::WebContents*>& web_contents) {
  for (auto* content : web_contents)
    Add(content);
}

DlpConfidentialContents::DlpConfidentialContents(
    const DlpConfidentialContents& other) = default;

DlpConfidentialContents& DlpConfidentialContents::operator=(
    const DlpConfidentialContents& other) = default;

DlpConfidentialContents::~DlpConfidentialContents() = default;

const base::flat_set<DlpConfidentialContent>&
DlpConfidentialContents::GetContents() const {
  return contents_;
}

base::flat_set<DlpConfidentialContent>& DlpConfidentialContents::GetContents() {
  return contents_;
}

void DlpConfidentialContents::Add(content::WebContents* web_contents) {
  contents_.insert(DlpConfidentialContent(web_contents));
}

void DlpConfidentialContents::Add(aura::Window* window, const GURL& url) {
  contents_.insert(DlpConfidentialContent(window, url));
}

void DlpConfidentialContents::Add(const DlpConfidentialContent& content) {
  contents_.insert(content);
}

void DlpConfidentialContents::ClearAndAdd(content::WebContents* web_contents) {
  contents_.clear();
  Add(web_contents);
}

void DlpConfidentialContents::ClearAndAdd(aura::Window* window,
                                          const GURL& url) {
  contents_.clear();
  Add(window, url);
}

bool DlpConfidentialContents::IsEmpty() const {
  return contents_.empty();
}

void DlpConfidentialContents::UnionWith(const DlpConfidentialContents& other) {
  contents_.insert(other.contents_.begin(), other.contents_.end());
}

DlpConfidentialContentsCache::Entry::Entry(
    const DlpConfidentialContent& content,
    DlpRulesManager::Restriction restriction,
    base::TimeTicks timestamp)
    : content(content), restriction(restriction), created_at(timestamp) {}

DlpConfidentialContentsCache::Entry::~Entry() = default;

DlpConfidentialContentsCache::DlpConfidentialContentsCache()
    : cache_size_limit_(kDefaultCacheSizeLimit),
      task_runner_(
          content::GetUIThreadTaskRunner(content::BrowserTaskTraits())) {}

DlpConfidentialContentsCache::~DlpConfidentialContentsCache() = default;

void DlpConfidentialContentsCache::Cache(
    const DlpConfidentialContent& content,
    DlpRulesManager::Restriction restriction) {
  if (Contains(content, restriction)) {
    return;
  }

  auto entry =
      std::make_unique<Entry>(content, restriction, base::TimeTicks::Now());
  StartEvictionTimer(entry.get());
  entries_.push_front(std::move(entry));

  if (entries_.size() > cache_size_limit_) {
    entries_.pop_back();
  }
}

bool DlpConfidentialContentsCache::Contains(
    content::WebContents* web_contents,
    DlpRulesManager::Restriction restriction) const {
  const GURL url = web_contents->GetLastCommittedURL();
  return std::find_if(entries_.begin(), entries_.end(),
                      [&](const std::unique_ptr<Entry>& entry) {
                        return entry->restriction == restriction &&
                               entry->content.url.EqualsIgnoringRef(url);
                      }) != entries_.end();
}

bool DlpConfidentialContentsCache::Contains(
    const DlpConfidentialContent& content,
    DlpRulesManager::Restriction restriction) const {
  return std::find_if(
             entries_.begin(), entries_.end(),
             [&](const std::unique_ptr<Entry>& entry) {
               return entry->restriction == restriction &&
                      entry->content.url.EqualsIgnoringRef(content.url);
             }) != entries_.end();
}

size_t DlpConfidentialContentsCache::GetSizeForTesting() const {
  return entries_.size();
}

// static
base::TimeDelta DlpConfidentialContentsCache::GetCacheTimeout() {
  return kDefaultCacheTimeout;
}

void DlpConfidentialContentsCache::SetCacheSizeLimitForTesting(int limit) {
  cache_size_limit_ = limit;
}

void DlpConfidentialContentsCache::SetTaskRunnerForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  task_runner_ = task_runner;
}

void DlpConfidentialContentsCache::StartEvictionTimer(Entry* entry) {
  entry->eviction_timer.SetTaskRunner(task_runner_);
  entry->eviction_timer.Start(
      FROM_HERE, GetCacheTimeout(),
      base::BindOnce(&DlpConfidentialContentsCache::OnEvictionTimerUp,
                     base::Unretained(this), entry->content));
}

void DlpConfidentialContentsCache::OnEvictionTimerUp(
    const DlpConfidentialContent& content) {
  entries_.remove_if([&](const std::unique_ptr<Entry>& entry) {
    return entry.get()->content == content;
  });
}

}  // namespace policy
