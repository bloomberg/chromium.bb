// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_BACKGROUND_PRINTING_MANAGER_H_
#define CHROME_BROWSER_PRINTING_BACKGROUND_PRINTING_MANAGER_H_

#include <map>
#include <memory>
#include <set>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace content {
class WebContents;
class BrowserContext;
}

namespace printing {

// Manages hidden WebContents that prints documents in the background.
// The hidden WebContents are no longer part of any Browser / TabStripModel.
// The WebContents started life as a ConstrainedWebDialog.
// They get deleted when the printing finishes.
class BackgroundPrintingManager : public content::NotificationObserver {
 public:
  class Observer;

  BackgroundPrintingManager();
  ~BackgroundPrintingManager() override;

  // Takes ownership of |preview_dialog| and deletes it when |preview_dialog|
  // finishes printing. This removes |preview_dialog| from its ConstrainedDialog
  // and hides it from the user.
  void OwnPrintPreviewDialog(
      std::unique_ptr<content::WebContents> preview_dialog);

  // Returns true if |printing_contents_map_| contains |preview_dialog|.
  bool HasPrintPreviewDialog(content::WebContents* preview_dialog);

  // Let others see the list of background printing contents.
  std::set<content::WebContents*> CurrentContentSet();

  // Delete all preview contents that are associated with |browser_context|.
  void DeletePreviewContentsForBrowserContext(
      content::BrowserContext* browser_context);

  void OnPrintRequestCancelled(content::WebContents* preview_dialog);

 private:
  // content::NotificationObserver overrides:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Schedule deletion of |preview_contents|.
  void DeletePreviewContents(content::WebContents* preview_contents);

  // A map from print preview WebContentses (managed by
  // BackgroundPrintingManager) to the Observers that observe them and the owned
  // version of the WebContents.
  struct PrintingContents {
    PrintingContents();
    ~PrintingContents();
    PrintingContents(PrintingContents&&);
    PrintingContents& operator=(PrintingContents&&);

    std::unique_ptr<content::WebContents> contents;
    std::unique_ptr<Observer> observer;

   private:
    DISALLOW_COPY_AND_ASSIGN(PrintingContents);
  };
  std::map<content::WebContents*, PrintingContents> printing_contents_map_;

  content::NotificationRegistrar registrar_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(BackgroundPrintingManager);
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_BACKGROUND_PRINTING_MANAGER_H_
