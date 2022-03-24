// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotations_web_contents_observer.h"

#include "base/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "components/google/core/common/google_util.h"
#include "components/optimization_guide/content/browser/optimization_guide_decider.h"
#include "components/optimization_guide/content/browser/page_content_annotations_service.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/page_entities_metadata.pb.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_user_data.h"

namespace optimization_guide {

namespace {

// Returns the search query if |url| is a valid Search URL according to
// |template_url_service|.
absl::optional<std::u16string> ExtractSearchTerms(
    const TemplateURLService* template_url_service,
    const GURL& url) {
  const TemplateURL* default_search_provider =
      template_url_service->GetDefaultSearchProvider();
  const SearchTermsData& search_terms_data =
      template_url_service->search_terms_data();

  std::u16string search_terms;
  if (default_search_provider &&
      default_search_provider->ExtractSearchTermsFromURL(url, search_terms_data,
                                                         &search_terms) &&
      !search_terms.empty()) {
    return search_terms;
  }
  return absl::nullopt;
}

// Data scoped to a single page. PageData has the same lifetime as the page's
// main document. Contains information for whether we annotated the title for
// the page yet.
class PageData : public content::PageUserData<PageData> {
 public:
  explicit PageData(content::Page& page) : PageUserData(page) {}
  PageData(const PageData&) = delete;
  PageData& operator=(const PageData&) = delete;
  ~PageData() override = default;

  int64_t navigation_id() const { return navigation_id_; }
  void set_navigation_id(int64_t navigation_id) {
    navigation_id_ = navigation_id;
  }

  bool annotation_was_requested() const { return annotation_was_requested_; }
  void set_annotation_was_requested() { annotation_was_requested_ = true; }

  PAGE_USER_DATA_KEY_DECL();

 private:
  int64_t navigation_id_ = 0;
  bool annotation_was_requested_ = false;
};

PAGE_USER_DATA_KEY_IMPL(PageData);

}  // namespace

PageContentAnnotationsWebContentsObserver::
    PageContentAnnotationsWebContentsObserver(
        content::WebContents* web_contents,
        PageContentAnnotationsService* page_content_annotations_service,
        TemplateURLService* template_url_service,
        OptimizationGuideDecider* optimization_guide_decider)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<PageContentAnnotationsWebContentsObserver>(
          *web_contents),
      page_content_annotations_service_(page_content_annotations_service),
      template_url_service_(template_url_service),
      optimization_guide_decider_(optimization_guide_decider),
      max_size_for_text_dump_(features::MaxSizeForPageContentTextDump()) {
  DCHECK(page_content_annotations_service_);

  if (!features::ShouldAnnotateTitleInsteadOfPageContent()) {
    // Make sure we always attach ourselves to a PageTextObserver if we are
    // annotating page content.
    PageTextObserver* observer =
        PageTextObserver::GetOrCreateForWebContents(web_contents);
    observer->AddConsumer(this);
  }

  if (features::RemotePageEntitiesEnabled() && optimization_guide_decider_) {
    optimization_guide_decider_->RegisterOptimizationTypes(
        {proto::PAGE_ENTITIES});
  }
}

PageContentAnnotationsWebContentsObserver::
    ~PageContentAnnotationsWebContentsObserver() {
  // Only detach ourselves if |web_contents()| as well as PageTextObserver for
  // |web_contents()| are still alive.
  if (!web_contents())
    return;

  PageTextObserver* observer =
      PageTextObserver::FromWebContents(web_contents());
  if (observer)
    observer->RemoveConsumer(this);
}

void PageContentAnnotationsWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  if (!navigation_handle->GetURL().SchemeIsHTTPOrHTTPS())
    return;

  PageData* page_data = nullptr;
  if (features::ShouldAnnotateTitleInsteadOfPageContent()) {
    page_data = PageData::GetOrCreateForPage(web_contents()->GetPrimaryPage());
    page_data->set_navigation_id(navigation_handle->GetNavigationId());
  }

  optimization_guide::HistoryVisit history_visit = optimization_guide::
      PageContentAnnotationsService::CreateHistoryVisitFromWebContents(
          web_contents(), navigation_handle->GetNavigationId());

  if (features::RemotePageEntitiesEnabled() && optimization_guide_decider_) {
    optimization_guide_decider_->CanApplyOptimizationAsync(
        navigation_handle, proto::PAGE_ENTITIES,
        base::BindOnce(&PageContentAnnotationsWebContentsObserver::
                           OnRemotePageEntitiesReceived,
                       weak_ptr_factory_.GetWeakPtr(), history_visit));
  }

  if (google_util::IsGoogleSearchUrl(navigation_handle->GetURL())) {
    // Extract related searches.
    if (optimization_guide::features::ShouldExtractRelatedSearches()) {
      page_content_annotations_service_->ExtractRelatedSearches(history_visit,
                                                                web_contents());
    }

    absl::optional<std::u16string> search_terms =
        ExtractSearchTerms(template_url_service_, navigation_handle->GetURL());
    if (search_terms) {
      if (page_data) {
        page_data->set_annotation_was_requested();
      }
      const std::u16string& normalized_search_query =
          base::i18n::ToLower(base::CollapseWhitespace(*search_terms, false));
      page_content_annotations_service_->Annotate(
          history_visit, base::UTF16ToUTF8(normalized_search_query));
      return;
    }
  }

  // TODO(crbug/1177102): Remove this title hack once the PageTextObserver works
  // for same-document navigations.
  if (navigation_handle->IsSameDocument()) {
    if (page_data) {
      page_data->set_annotation_was_requested();
    }
    // Annotate the title instead.
    page_content_annotations_service_->Annotate(
        history_visit, base::UTF16ToUTF8(web_contents()->GetTitle()));
  }
}

// This triggers the annotation of titles for pages that are not SRP or
// same document and will only request annotation if the flag to annotate titles
// instead of content is enabled.
void PageContentAnnotationsWebContentsObserver::TitleWasSet(
    content::NavigationEntry* entry) {
  if (!entry)
    return;

  PageData* page_data = PageData::GetForPage(web_contents()->GetPrimaryPage());
  if (!page_data)
    return;
  if (page_data->annotation_was_requested())
    return;

  page_data->set_annotation_was_requested();
  optimization_guide::HistoryVisit history_visit = optimization_guide::
      PageContentAnnotationsService::CreateHistoryVisitFromWebContents(
          web_contents(), page_data->navigation_id());
  page_content_annotations_service_->Annotate(
      history_visit, base::UTF16ToUTF8(entry->GetTitleForDisplay()));
}

std::unique_ptr<PageTextObserver::ConsumerTextDumpRequest>
PageContentAnnotationsWebContentsObserver::MaybeRequestFrameTextDump(
    content::NavigationHandle* navigation_handle) {
  DCHECK(!features::ShouldAnnotateTitleInsteadOfPageContent());

  DCHECK(navigation_handle->HasCommitted());
  // TODO(https://crbug.com/1218946): With MPArch there may be multiple main
  // frames. This caller was converted automatically to the primary main frame
  // to preserve its semantics. Follow up to confirm correctness.
  DCHECK(navigation_handle->IsInPrimaryMainFrame());

  if (!navigation_handle->GetURL().SchemeIsHTTPOrHTTPS())
    return nullptr;

  if (navigation_handle->IsSameDocument())
    return nullptr;

  if (google_util::IsGoogleSearchUrl(navigation_handle->GetURL()))
    return nullptr;

  std::unique_ptr<PageTextObserver::ConsumerTextDumpRequest> request =
      std::make_unique<PageTextObserver::ConsumerTextDumpRequest>();
  request->max_size = max_size_for_text_dump_;
  request->events = {mojom::TextDumpEvent::kFirstLayout};
  request->dump_amp_subframes = true;
  request->callback = base::BindOnce(
      &PageContentAnnotationsWebContentsObserver::OnTextDumpReceived,
      weak_ptr_factory_.GetWeakPtr(),
      PageContentAnnotationsService::CreateHistoryVisitFromWebContents(
          navigation_handle->GetWebContents(),
          navigation_handle->GetNavigationId()));
  return request;
}

void PageContentAnnotationsWebContentsObserver::OnTextDumpReceived(
    const HistoryVisit& visit,
    const PageTextDumpResult& result) {
  DCHECK(!features::ShouldAnnotateTitleInsteadOfPageContent());

  if (result.empty()) {
    return;
  }

  // If the page had AMP frames, then only use that content. Otherwise, use the
  // mainframe.
  if (result.GetAMPTextContent()) {
    page_content_annotations_service_->Annotate(visit,
                                                *result.GetAMPTextContent());
    return;
  }
  page_content_annotations_service_->Annotate(
      visit, *result.GetMainFrameTextContent());
}

void PageContentAnnotationsWebContentsObserver::OnRemotePageEntitiesReceived(
    const HistoryVisit& history_visit,
    OptimizationGuideDecision decision,
    const OptimizationMetadata& metadata) {
  if (decision != OptimizationGuideDecision::kTrue)
    return;

  absl::optional<proto::PageEntitiesMetadata> page_entities_metadata =
      metadata.ParsedMetadata<proto::PageEntitiesMetadata>();
  if (!page_entities_metadata || page_entities_metadata->entities().size() == 0)
    return;

  std::vector<history::VisitContentModelAnnotations::Category> entities;
  for (const auto& entity : page_entities_metadata->entities()) {
    if (entity.entity_id().empty())
      continue;

    if (entity.score() < 0 || entity.score() > 100)
      continue;

    entities.emplace_back(history::VisitContentModelAnnotations::Category(
        entity.entity_id(), entity.score()));
  }
  page_content_annotations_service_->PersistRemotePageEntities(history_visit,
                                                               entities);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PageContentAnnotationsWebContentsObserver);

}  // namespace optimization_guide
