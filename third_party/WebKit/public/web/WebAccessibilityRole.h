/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebAccessibilityRole_h
#define WebAccessibilityRole_h

#include "../platform/WebCommon.h"

namespace WebKit {

// These values must match WebCore::AccessibilityRole values
// DEPRECATED: these will be replaced with the enums defined in
// WebAXEnums.h (http://crbug.com/269034).
enum WebAccessibilityRole {
    WebAccessibilityRoleApplicationAlertDialog = 1,
    WebAccessibilityRoleApplicationAlert,
    WebAccessibilityRoleAnnotation,
    WebAccessibilityRoleLandmarkApplication,
    WebAccessibilityRoleDocumentArticle,
    WebAccessibilityRoleLandmarkBanner,
    WebAccessibilityRoleBrowser,
    WebAccessibilityRoleBusyIndicator,
    WebAccessibilityRoleButton,
    WebAccessibilityRoleCanvas,
    WebAccessibilityRoleCell,
    WebAccessibilityRoleCheckBox,
    WebAccessibilityRoleColorWell,
    WebAccessibilityRoleColumnHeader,
    WebAccessibilityRoleColumn,
    WebAccessibilityRoleComboBox,
    WebAccessibilityRoleLandmarkComplementary,
    WebAccessibilityRoleLandmarkContentInfo,
    WebAccessibilityRoleDefinition,
    WebAccessibilityRoleDescriptionListDetail,
    WebAccessibilityRoleDescriptionListTerm,
    WebAccessibilityRoleApplicationDialog,
    WebAccessibilityRoleDirectory,
    WebAccessibilityRoleDisclosureTriangle,
    WebAccessibilityRoleDiv,
    WebAccessibilityRoleDocument,
    WebAccessibilityRoleDrawer,
    WebAccessibilityRoleEditableText,
    WebAccessibilityRoleFooter,
    WebAccessibilityRoleForm,
    WebAccessibilityRoleGrid,
    WebAccessibilityRoleGroup,
    WebAccessibilityRoleGrowArea,
    WebAccessibilityRoleHeading,
    WebAccessibilityRoleHelpTag,
    WebAccessibilityRoleHorizontalRule,
    WebAccessibilityRoleIgnored,
    WebAccessibilityRoleImageMapLink,
    WebAccessibilityRoleImageMap,
    WebAccessibilityRoleImage,
    WebAccessibilityRoleIncrementor,
    WebAccessibilityRoleLabel,
    WebAccessibilityRoleLegend,
    WebAccessibilityRoleLink,
    WebAccessibilityRoleListBoxOption,
    WebAccessibilityRoleListBox,
    WebAccessibilityRoleListItem,
    WebAccessibilityRoleListMarker,
    WebAccessibilityRoleList,
    WebAccessibilityRoleApplicationLog,
    WebAccessibilityRoleLandmarkMain,
    WebAccessibilityRoleApplicationMarquee,
    WebAccessibilityRoleMathElement,
    WebAccessibilityRoleDocumentMath,
    WebAccessibilityRoleMatte,
    WebAccessibilityRoleMenuBar,
    WebAccessibilityRoleMenuButton,
    WebAccessibilityRoleMenuItem,
    WebAccessibilityRoleMenuListOption,
    WebAccessibilityRoleMenuListPopup,
    WebAccessibilityRoleMenu,
    WebAccessibilityRoleLandmarkNavigation,
    WebAccessibilityRoleDocumentNote,
    WebAccessibilityRoleOutline,
    WebAccessibilityRoleParagraph,
    WebAccessibilityRolePopUpButton,
    WebAccessibilityRolePresentational,
    WebAccessibilityRoleProgressIndicator,
    WebAccessibilityRoleRadioButton,
    WebAccessibilityRoleRadioGroup,
    WebAccessibilityRoleDocumentRegion,
    WebAccessibilityRoleRootWebArea,
    WebAccessibilityRoleRowHeader,
    WebAccessibilityRoleRow,
    WebAccessibilityRoleRulerMarker,
    WebAccessibilityRoleRuler,
    WebAccessibilityRoleSVGRoot,
    WebAccessibilityRoleScrollArea,
    WebAccessibilityRoleScrollBar,
    WebAccessibilityRoleSeamlessWebArea,
    WebAccessibilityRoleLandmarkSearch,
    WebAccessibilityRoleSheet,
    WebAccessibilityRoleSlider,
    WebAccessibilityRoleSliderThumb,
    WebAccessibilityRoleSpinButtonPart,
    WebAccessibilityRoleSpinButton,
    WebAccessibilityRoleSplitGroup,
    WebAccessibilityRoleSplitter,
    WebAccessibilityRoleStaticText,
    WebAccessibilityRoleApplicationStatus,
    WebAccessibilityRoleSystemWide,
    WebAccessibilityRoleTabGroup,
    WebAccessibilityRoleTabList,
    WebAccessibilityRoleTabPanel,
    WebAccessibilityRoleTab,
    WebAccessibilityRoleTableHeaderContainer,
    WebAccessibilityRoleTable,
    WebAccessibilityRoleTextArea,
    WebAccessibilityRoleTextField,
    WebAccessibilityRoleApplicationTimer,
    WebAccessibilityRoleToggleButton,
    WebAccessibilityRoleToolbar,
    WebAccessibilityRoleTreeGrid,
    WebAccessibilityRoleTreeItemRole,
    WebAccessibilityRoleTreeRole,
    WebAccessibilityRoleUnknown,
    WebAccessibilityRoleUserInterfaceTooltip,
    WebAccessibilityRoleValueIndicator,
    WebAccessibilityRoleWebArea,
    WebAccessibilityRoleWindow,

    // Deprecated.
    WebAccessibilityRoleApplication,
    WebAccessibilityRoleWebCoreLink
};

} // namespace WebKit

#endif
