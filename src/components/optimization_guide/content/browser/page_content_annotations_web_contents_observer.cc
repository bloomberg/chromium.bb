// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotations_web_contents_observer.h"

#include "base/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/google/core/common/google_util.h"
#include "components/optimization_guide/content/browser/optimization_guide_decider.h"
#include "components/optimization_guide/content/browser/page_content_annotations_service.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/proto/page_entities_metadata.pb.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_user_data.h"

namespace optimization_guide {

namespace {

// Return whether or not we should fetch remote metadata.
bool FetchRemoteMetadataEnabled() {
  return features::RemotePageEntitiesEnabled() ||
         features::RemotePageMetadataEnabled();
}

// Returns search metadata if |url| is a valid Search URL according to
// |template_url_service|.
absl::optional<SearchMetadata> ExtractSearchMetadata(
    TemplateURLService* template_url_service,
    const GURL& url) {
  if (!template_url_service)
    return absl::nullopt;

  if (!template_url_service->loaded()) {
    if (switches::ShouldLogPageContentAnnotationsInput()) {
      LOG(ERROR) << "Template URL Service not loaded";
    }
    return absl::nullopt;
  }

  const TemplateURL* template_url =
      template_url_service->GetTemplateURLForHost(url.host());
  const SearchTermsData& search_terms_data =
      template_url_service->search_terms_data();

  std::u16string search_terms;
  bool is_valid_search_url = template_url &&
                             template_url->ExtractSearchTermsFromURL(
                                 url, search_terms_data, &search_terms) &&
                             !search_terms.empty();
  if (!is_valid_search_url) {
    if (switches::ShouldLogPageContentAnnotationsInput()) {
      LOG(ERROR) << "Url " << url << " is not a valid search URL";
      LOG(ERROR) << "Matching TemplateURL instances for host: "
                 << template_url_service->GetTemplateURLCountForHostForLogging(
                        url.host());
    }
    return absl::nullopt;
  }

  const std::u16string& normalized_search_query =
      base::i18n::ToLower(base::CollapseWhitespace(search_terms, false));
  TemplateURLRef::SearchTermsArgs search_terms_args(normalized_search_query);
  const TemplateURLRef& search_url_ref = template_url->url_ref();
  if (!search_url_ref.SupportsReplacement(search_terms_data)) {
    if (switches::ShouldLogPageContentAnnotationsInput()) {
      LOG(ERROR) << "Url " << url << " does not support replacement";
    }
    return absl::nullopt;
  }

  if (switches::ShouldLogPageContentAnnotationsInput()) {
    LOG(ERROR) << "Url " << url << " is a valid search URL";
  }
  return SearchMetadata{
      GURL(search_url_ref.ReplaceSearchTerms(search_terms_args,
                                             search_terms_data)),
      base::i18n::ToLower(base::CollapseWhitespace(search_terms, false))};
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
      optimization_guide_decider_(optimization_guide_decider) {
  DCHECK(page_content_annotations_service_);

  if (FetchRemoteMetadataEnabled() && optimization_guide_decider_) {
    optimization_guide_decider_->RegisterOptimizationTypes(
        {proto::PAGE_ENTITIES});
  }
}

PageContentAnnotationsWebContentsObserver::
    ~PageContentAnnotationsWebContentsObserver() = default;

void PageContentAnnotationsWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  if (!navigation_handle->GetURL().SchemeIsHTTPOrHTTPS())
    return;

  PageData* page_data =
      PageData::GetOrCreateForPage(web_contents()->GetPrimaryPage());
  page_data->set_navigation_id(navigation_handle->GetNavigationId());

  optimization_guide::HistoryVisit history_visit = optimization_guide::
      PageContentAnnotationsService::CreateHistoryVisitFromWebContents(
          web_contents(), navigation_handle->GetNavigationId());
  if (FetchRemoteMetadataEnabled() && optimization_guide_decider_) {
    optimization_guide_decider_->CanApplyOptimizationAsync(
        navigation_handle, proto::PAGE_ENTITIES,
        base::BindOnce(&PageContentAnnotationsWebContentsObserver::
                           OnRemotePageMetadataReceived,
                       weak_ptr_factory_.GetWeakPtr(), history_visit));
  }

  bool is_google_search_url =
      google_util::IsGoogleSearchUrl(navigation_handle->GetURL());
  // Extract related searches.
  if (is_google_search_url &&
      optimization_guide::features::ShouldExtractRelatedSearches()) {
    page_content_annotations_service_->ExtractRelatedSearches(history_visit,
                                                              web_contents());
  }

  // Persist search metadata, if applicable if it's a Google search URL or if
  // it's a search-y URL as determined by the TemplateURLService if the flag is
  // enabled.
  if (is_google_search_url ||
      optimization_guide::features::
          ShouldPersistSearchMetadataForNonGoogleSearches()) {
    base::UmaHistogramBoolean(
        "OptimizationGuide.PageContentAnnotations."
        "TemplateURLServiceLoadedAtNavigationFinish",
        template_url_service_ && template_url_service_->loaded());

    absl::optional<SearchMetadata> search_metadata = ExtractSearchMetadata(
        template_url_service_, navigation_handle->GetURL());
    if (search_metadata) {
      if (page_data) {
        page_data->set_annotation_was_requested();
      }
      history_visit.text_to_annotate =
          base::UTF16ToUTF8(search_metadata->search_terms);
      page_content_annotations_service_->Annotate(history_visit);
      page_content_annotations_service_->PersistSearchMetadata(
          history_visit, *search_metadata);

      if (switches::ShouldLogPageContentAnnotationsInput()) {
        LOG(ERROR) << "Annotating search terms: \n"
                   << "URL: " << navigation_handle->GetURL() << "\n"
                   << "Text: " << *(history_visit.text_to_annotate);
      }

      return;
    }
  }

  // Same-document navigations and reloads do not trigger |TitleWasSet|, so we
  // need to capture the title text here for these cases.
  if (navigation_handle->IsSameDocument() ||
      navigation_handle->GetReloadType() != content::ReloadType::NONE) {
    if (page_data) {
      page_data->set_annotation_was_requested();
    }
    // Annotate the title instead.
    history_visit.text_to_annotate =
        base::UTF16ToUTF8(web_contents()->GetTitle());
    page_content_annotations_service_->Annotate(history_visit);

    if (switches::ShouldLogPageContentAnnotationsInput()) {
      LOG(ERROR) << "Annotating same document navigation: \n"
                 << "URL: " << navigation_handle->GetURL() << "\n"
                 << "Text: " << *(history_visit.text_to_annotate);
    }
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
  history_visit.text_to_annotate =
      base::UTF16ToUTF8(entry->GetTitleForDisplay());
  page_content_annotations_service_->Annotate(history_visit);

  if (switches::ShouldLogPageContentAnnotationsInput()) {
    LOG(ERROR) << "Annotating main frame navigation: \n"
               << "URL: " << entry->GetURL() << "\n"
               << "Text: " << *(history_visit.text_to_annotate);
  }
}

void PageContentAnnotationsWebContentsObserver::OnRemotePageMetadataReceived(
    const HistoryVisit& history_visit,
    OptimizationGuideDecision decision,
    const OptimizationMetadata& metadata) {
  if (decision != OptimizationGuideDecision::kTrue)
    return;

  absl::optional<proto::PageEntitiesMetadata> page_entities_metadata =
      metadata.ParsedMetadata<proto::PageEntitiesMetadata>();
  if (!page_entities_metadata)
    return;

  // Persist entities to VisitContentModelAnnotations if that feature is
  // enabled.
  if (page_entities_metadata->entities().size() != 0 &&
      features::RemotePageEntitiesEnabled()) {
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
  if (!features::RemotePageMetadataEnabled()) {
    return;
  }
  // Persist any other metadata to VisitContentAnnotations.
  page_entities_metadata->clear_entities();
  if (page_entities_metadata->has_alternative_title()) {
    page_content_annotations_service_->PersistRemotePageMetadata(
        history_visit, *page_entities_metadata);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PageContentAnnotationsWebContentsObserver);

}  // namespace optimization_guide
