// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_notes/user_note_service_factory.h"

#include <utility>

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/user_notes/user_note_service_delegate_impl.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/user_notes/browser/user_note_service.h"
#include "components/user_notes/user_notes_features.h"
#include "content/public/browser/browser_context.h"

namespace user_notes {

// static
UserNoteService* UserNoteServiceFactory::GetForContext(
    content::BrowserContext* context) {
  auto* instance = GetInstance();
  return instance->service_for_testing_
             ? instance->service_for_testing_.get()
             : static_cast<UserNoteService*>(
                   instance->GetServiceForBrowserContext(context,
                                                         /*create=*/true));
}

// static
UserNoteServiceFactory* UserNoteServiceFactory::GetInstance() {
  return base::Singleton<UserNoteServiceFactory>::get();
}

// static
void UserNoteServiceFactory::SetServiceForTesting(
    std::unique_ptr<UserNoteService> service) {
  GetInstance()->service_for_testing_ = std::move(service);
}

UserNoteServiceFactory::UserNoteServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "UserNoteService",
          BrowserContextDependencyManager::GetInstance()) {}

UserNoteServiceFactory::~UserNoteServiceFactory() = default;

KeyedService* UserNoteServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK(IsUserNotesEnabled());
  return new UserNoteService(std::make_unique<UserNoteServiceDelegateImpl>(
      Profile::FromBrowserContext(context)));
}

content::BrowserContext* UserNoteServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // For now, the feature is not supported in Incognito mode.
  // TODO(crbug.com/1313967): This will need to be changed if User Notes are to
  // be available in Incognito.
  if (context->IsOffTheRecord()) {
    return nullptr;
  }

  return context;
}

}  // namespace user_notes
