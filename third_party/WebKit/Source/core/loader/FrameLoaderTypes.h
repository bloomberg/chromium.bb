/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FrameLoaderTypes_h
#define FrameLoaderTypes_h

namespace blink {

// See WebFrameLoadType in public/web/WebFrameLoadType.h for details.
enum FrameLoadType {
  kFrameLoadTypeStandard,
  kFrameLoadTypeBackForward,
  kFrameLoadTypeReload,
  kFrameLoadTypeReplaceCurrentItem,
  kFrameLoadTypeInitialInChildFrame,
  kFrameLoadTypeInitialHistoryLoad,
  kFrameLoadTypeReloadBypassingCache,
};

enum NavigationType {
  kNavigationTypeLinkClicked,
  kNavigationTypeFormSubmitted,
  kNavigationTypeBackForward,
  kNavigationTypeReload,
  kNavigationTypeFormResubmitted,
  kNavigationTypeOther
};

enum ShouldSendReferrer { kMaybeSendReferrer, kNeverSendReferrer };

enum ShouldSetOpener { kMaybeSetOpener, kNeverSetOpener };

enum ReasonForCallingAllowPlugins {
  kAboutToInstantiatePlugin,
  kNotAboutToInstantiatePlugin
};

enum LoadStartType {
  kNavigationToDifferentDocument,
  kNavigationWithinSameDocument
};

enum SameDocumentNavigationSource {
  kSameDocumentNavigationDefault,
  kSameDocumentNavigationHistoryApi,
};

enum HistoryLoadType {
  kHistorySameDocumentLoad,
  kHistoryDifferentDocumentLoad
};

enum HistoryCommitType {
  kStandardCommit,
  kBackForwardCommit,
  kInitialCommitInChildFrame,
  kHistoryInertCommit
};

enum HistoryScrollRestorationType {
  kScrollRestorationAuto,
  kScrollRestorationManual
};

enum class ProgressBarCompletion {
  kLoadEvent,
  kResourcesBeforeDCL,
  kDOMContentLoaded,
  kResourcesBeforeDCLAndSameOriginIFrames
};

// This enum is used to index different kinds of single-page-application
// navigations for UMA enum histogram. New enum values can be added, but
// existing enums must never be renumbered or deleted and reused.
// This enum should be consistent with SinglePageAppNavigationType in
// tools/metrics/histograms/enums.xml.
enum SinglePageAppNavigationType {
  kSPANavTypeHistoryPushStateOrReplaceState = 0,
  kSPANavTypeSameDocumentBackwardOrForward = 1,
  kSPANavTypeOtherFragmentNavigation = 2,
  kSPANavTypeCount
};
}  // namespace blink

#endif
