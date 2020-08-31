// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_CPDFSDK_ACTIONHANDLER_H_
#define FPDFSDK_CPDFSDK_ACTIONHANDLER_H_

#include <set>
#include <utility>

#include "core/fpdfdoc/cpdf_aaction.h"
#include "core/fpdfdoc/cpdf_action.h"
#include "core/fxcrt/fx_string.h"
#include "fpdfsdk/cpdfsdk_fieldaction.h"

class CPDFSDK_Annot;
class CPDFSDK_FormFillEnvironment;
class CPDF_Dictionary;
class CPDF_FormField;
class IJS_EventContext;

class CPDFSDK_ActionHandler {
 public:
  bool DoAction_DocOpen(const CPDF_Action& action,
                        CPDFSDK_FormFillEnvironment* pFormFillEnv);
  bool DoAction_JavaScript(const CPDF_Action& JsAction,
                           WideString csJSName,
                           CPDFSDK_FormFillEnvironment* pFormFillEnv);
  bool DoAction_Page(const CPDF_Action& action,
                     enum CPDF_AAction::AActionType eType,
                     CPDFSDK_FormFillEnvironment* pFormFillEnv);
  bool DoAction_Document(const CPDF_Action& action,
                         enum CPDF_AAction::AActionType eType,
                         CPDFSDK_FormFillEnvironment* pFormFillEnv);
  bool DoAction_Field(const CPDF_Action& action,
                      CPDF_AAction::AActionType type,
                      CPDFSDK_FormFillEnvironment* pFormFillEnv,
                      CPDF_FormField* pFormField,
                      CPDFSDK_FieldAction* data);
  bool DoAction_FieldJavaScript(const CPDF_Action& JsAction,
                                CPDF_AAction::AActionType type,
                                CPDFSDK_FormFillEnvironment* pFormFillEnv,
                                CPDF_FormField* pFormField,
                                CPDFSDK_FieldAction* data);
  bool DoAction_Link(const CPDF_Action& action,
                     CPDF_AAction::AActionType type,
                     CPDFSDK_FormFillEnvironment* form_fill_env,
                     int modifiers);
  bool DoAction_Destination(const CPDF_Dest& dest,
                            CPDFSDK_FormFillEnvironment* form_fill_env);

 private:
  using RunScriptCallback = std::function<void(IJS_EventContext* context)>;

  void RunScript(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                 const WideString& script,
                 const RunScriptCallback& cb);

  bool ExecuteDocumentOpenAction(const CPDF_Action& action,
                                 CPDFSDK_FormFillEnvironment* pFormFillEnv,
                                 std::set<const CPDF_Dictionary*>* visited);
  bool ExecuteDocumentPageAction(const CPDF_Action& action,
                                 CPDF_AAction::AActionType type,
                                 CPDFSDK_FormFillEnvironment* pFormFillEnv,
                                 std::set<const CPDF_Dictionary*>* visited);
  bool ExecuteFieldAction(const CPDF_Action& action,
                          CPDF_AAction::AActionType type,
                          CPDFSDK_FormFillEnvironment* pFormFillEnv,
                          CPDF_FormField* pFormField,
                          CPDFSDK_FieldAction* data,
                          std::set<const CPDF_Dictionary*>* visited);

  void DoAction_NoJs(const CPDF_Action& action,
                     CPDF_AAction::AActionType type,
                     CPDFSDK_FormFillEnvironment* pFormFillEnv,
                     int modifiers);
  void RunDocumentPageJavaScript(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                                 CPDF_AAction::AActionType type,
                                 const WideString& script);
  void RunDocumentOpenJavaScript(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                                 const WideString& sScriptName,
                                 const WideString& script);
  void RunFieldJavaScript(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                          CPDF_FormField* pFormField,
                          CPDF_AAction::AActionType type,
                          CPDFSDK_FieldAction* data,
                          const WideString& script);

  bool IsValidField(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                    CPDF_Dictionary* pFieldDict);

  void DoAction_GoTo(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                     const CPDF_Action& action);
  void DoAction_Launch(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                       const CPDF_Action& action);
  void DoAction_URI(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                    const CPDF_Action& action,
                    int modifiers);
  void DoAction_Named(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                      const CPDF_Action& action);

  bool DoAction_Hide(const CPDF_Action& action,
                     CPDFSDK_FormFillEnvironment* pFormFillEnv);
  bool DoAction_SubmitForm(const CPDF_Action& action,
                           CPDFSDK_FormFillEnvironment* pFormFillEnv);
  void DoAction_ResetForm(const CPDF_Action& action,
                          CPDFSDK_FormFillEnvironment* pFormFillEnv);
};

#endif  // FPDFSDK_CPDFSDK_ACTIONHANDLER_H_
