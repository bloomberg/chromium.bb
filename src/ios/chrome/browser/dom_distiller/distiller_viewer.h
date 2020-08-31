// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOM_DISTILLER_DISTILLER_VIEWER_H_
#define IOS_CHROME_BROWSER_DOM_DISTILLER_DISTILLER_VIEWER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "components/dom_distiller/core/dom_distiller_request_view_base.h"
#include "components/dom_distiller/core/task_tracker.h"

class GURL;

namespace dom_distiller {

class DistilledPagePrefs;
class Distiller;

// An interface for a dom_distiller ViewRequestDelegate that distills a URL and
// calls the given callback with the distilled HTML string and the images it
// contains.
class DistillerViewerInterface : public DomDistillerRequestViewBase {
 public:
  struct ImageInfo {
    // The url of the image.
    GURL url;
    // The image data as a string.
    std::string data;
  };
  typedef base::Callback<void(const GURL& url,
                              const std::string& html,
                              const std::vector<ImageInfo>& images,
                              const std::string& title)>
      DistillationFinishedCallback;

  DistillerViewerInterface(PrefService* prefs)
      : DomDistillerRequestViewBase(new DistilledPagePrefs(prefs)) {}
  ~DistillerViewerInterface() override {}

  void OnArticleReady(
      const dom_distiller::DistilledArticleProto* article_proto) override = 0;

  void SendJavaScript(const std::string& buffer) override = 0;

  DISALLOW_COPY_AND_ASSIGN(DistillerViewerInterface);
};

// A very simple and naive implementation of the DistillerViewer.
class DistillerViewer : public DistillerViewerInterface {
 public:
  // Creates a |DistillerView| that will be used to distill |url|.
  // |callback| is called when distillation is finished with the protobuf
  // containing the distilled page.
  DistillerViewer(dom_distiller::DomDistillerService* distillerService,
                  PrefService* prefs,
                  const GURL& url,
                  const DistillationFinishedCallback& callback);

  // Creates a |DistillerView| without depending on the DomDistillerService.
  // Caller must provide |distiller_factory| and |page| which cannot be null.
  // |callback| is called when distillation is finished with the protobuf
  // containing the distilled page.
  DistillerViewer(dom_distiller::DistillerFactory* distiller_factory,
                  std::unique_ptr<dom_distiller::DistillerPage> page,
                  PrefService* prefs,
                  const GURL& url,
                  const DistillationFinishedCallback& callback);
  ~DistillerViewer() override;

  // DistillerViewerInterface implementation
  // Called by the distiller service when article is ready.
  void OnArticleReady(
      const dom_distiller::DistilledArticleProto* article_proto) override;

  void SendJavaScript(const std::string& buffer) override;

 private:
  // Called by the distiller when article is ready.
  void OnDistillerFinished(
      std::unique_ptr<dom_distiller::DistilledArticleProto> distilled_article);

  // Called by the distiller when article is updated.
  void OnArticleDistillationUpdated(
      const dom_distiller::ArticleDistillationUpdate& article_update);

  // The url of the distilled page.
  const GURL url_;
  // JavaScript buffer.
  std::string js_buffer_;
  // Callback to run once distillation is complete.
  const DistillationFinishedCallback callback_;
  // Keep reference of the distiller_ during distillation.
  std::unique_ptr<Distiller> distiller_;

  DISALLOW_COPY_AND_ASSIGN(DistillerViewer);
};

}  // namespace dom_distiller

#endif  // IOS_CHROME_BROWSER_DOM_DISTILLER_DISTILLER_VIEWER_H_
