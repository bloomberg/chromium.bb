// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXJS_CJS_EVENTRECORDER_H_
#define FXJS_CJS_EVENTRECORDER_H_

#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"
#include "core/fxcrt/unowned_ptr.h"
#include "fpdfsdk/cpdfsdk_annot.h"
#include "fpdfsdk/cpdfsdk_formfillenvironment.h"

class CPDF_Bookmark;
class CPDF_FormField;

enum JS_EVENT_T {
  JET_UNKNOWN,
  JET_APP_INIT,
  JET_DOC_OPEN,
  JET_DOC_WILLPRINT,
  JET_DOC_DIDPRINT,
  JET_DOC_WILLSAVE,
  JET_DOC_DIDSAVE,
  JET_DOC_WILLCLOSE,
  JET_PAGE_OPEN,
  JET_PAGE_CLOSE,
  JET_PAGE_INVIEW,
  JET_PAGE_OUTVIEW,
  JET_FIELD_MOUSEDOWN,
  JET_FIELD_MOUSEUP,
  JET_FIELD_MOUSEENTER,
  JET_FIELD_MOUSEEXIT,
  JET_FIELD_FOCUS,
  JET_FIELD_BLUR,
  JET_FIELD_KEYSTROKE,
  JET_FIELD_VALIDATE,
  JET_FIELD_CALCULATE,
  JET_FIELD_FORMAT,
  JET_SCREEN_FOCUS,
  JET_SCREEN_BLUR,
  JET_SCREEN_OPEN,
  JET_SCREEN_CLOSE,
  JET_SCREEN_MOUSEDOWN,
  JET_SCREEN_MOUSEUP,
  JET_SCREEN_MOUSEENTER,
  JET_SCREEN_MOUSEEXIT,
  JET_SCREEN_INVIEW,
  JET_SCREEN_OUTVIEW,
  JET_BATCH_EXEC,
  JET_MENU_EXEC,
  JET_CONSOLE_EXEC,
  JET_EXTERNAL_EXEC,
  JET_BOOKMARK_MOUSEUP,
  JET_LINK_MOUSEUP
};

class CJS_EventRecorder {
 public:
  CJS_EventRecorder();
  ~CJS_EventRecorder();

  void OnApp_Init();

  void OnDoc_Open(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                  const WideString& strTargetName);
  void OnDoc_WillPrint(CPDFSDK_FormFillEnvironment* pFormFillEnv);
  void OnDoc_DidPrint(CPDFSDK_FormFillEnvironment* pFormFillEnv);
  void OnDoc_WillSave(CPDFSDK_FormFillEnvironment* pFormFillEnv);
  void OnDoc_DidSave(CPDFSDK_FormFillEnvironment* pFormFillEnv);
  void OnDoc_WillClose(CPDFSDK_FormFillEnvironment* pFormFillEnv);

  void OnPage_Open(CPDFSDK_FormFillEnvironment* pFormFillEnv);
  void OnPage_Close(CPDFSDK_FormFillEnvironment* pFormFillEnv);
  void OnPage_InView(CPDFSDK_FormFillEnvironment* pFormFillEnv);
  void OnPage_OutView(CPDFSDK_FormFillEnvironment* pFormFillEnv);

  void OnField_Calculate(CPDF_FormField* pSource,
                         CPDF_FormField* pTarget,
                         WideString* Value,
                         bool* pbRc);
  void OnField_Format(CPDF_FormField* pTarget, WideString* Value);
  void OnField_Keystroke(WideString* strChange,
                         const WideString& strChangeEx,
                         bool KeyDown,
                         bool bModifier,
                         int* nSelEnd,
                         int* nSelStart,
                         bool bShift,
                         CPDF_FormField* pTarget,
                         WideString* Value,
                         bool bWillCommit,
                         bool bFieldFull,
                         bool* bRc);
  void OnField_Validate(WideString* strChange,
                        const WideString& strChangeEx,
                        bool bKeyDown,
                        bool bModifier,
                        bool bShift,
                        CPDF_FormField* pTarget,
                        WideString* Value,
                        bool* bRc);
  void OnField_MouseDown(bool bModifier, bool bShift, CPDF_FormField* pTarget);
  void OnField_MouseEnter(bool bModifier, bool bShift, CPDF_FormField* pTarget);
  void OnField_MouseExit(bool bModifier, bool bShift, CPDF_FormField* pTarget);
  void OnField_MouseUp(bool bModifier, bool bShift, CPDF_FormField* pTarget);
  void OnField_Blur(bool bModifier,
                    bool bShift,
                    CPDF_FormField* pTarget,
                    WideString* Value);
  void OnField_Focus(bool bModifier,
                     bool bShift,
                     CPDF_FormField* pTarget,
                     WideString* Value);

  void OnScreen_Focus(bool bModifier, bool bShift, CPDFSDK_Annot* pScreen);
  void OnScreen_Blur(bool bModifier, bool bShift, CPDFSDK_Annot* pScreen);
  void OnScreen_Open(bool bModifier, bool bShift, CPDFSDK_Annot* pScreen);
  void OnScreen_Close(bool bModifier, bool bShift, CPDFSDK_Annot* pScreen);
  void OnScreen_MouseDown(bool bModifier, bool bShift, CPDFSDK_Annot* pScreen);
  void OnScreen_MouseUp(bool bModifier, bool bShift, CPDFSDK_Annot* pScreen);
  void OnScreen_MouseEnter(bool bModifier, bool bShift, CPDFSDK_Annot* pScreen);
  void OnScreen_MouseExit(bool bModifier, bool bShift, CPDFSDK_Annot* pScreen);
  void OnScreen_InView(bool bModifier, bool bShift, CPDFSDK_Annot* pScreen);
  void OnScreen_OutView(bool bModifier, bool bShift, CPDFSDK_Annot* pScreen);

  void OnBookmark_MouseUp(CPDF_Bookmark* pBookMark);
  void OnLink_MouseUp(CPDFSDK_FormFillEnvironment* pFormFillEnv);

  void OnMenu_Exec(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                   const WideString& strTargetName);
  void OnBatchExec(CPDFSDK_FormFillEnvironment* pFormFillEnv);
  void OnConsole_Exec();
  void OnExternal_Exec();

  void Destroy();

  JS_EVENT_T EventType() const { return m_eEventType; }
  bool IsValid() const { return m_bValid; }
  bool IsUserGesture() const;
  WideString& Change();
  WideString ChangeEx() const { return m_WideStrChangeEx; }
  WideString SourceName() const { return m_strSourceName; }
  WideString TargetName() const { return m_strTargetName; }
  int CommitKey() const { return m_nCommitKey; }
  bool FieldFull() const { return m_bFieldFull; }
  bool KeyDown() const { return m_bKeyDown; }
  bool Modifier() const { return m_bModifier; }
  ByteStringView Name() const;
  ByteStringView Type() const;
  bool& Rc();
  int SelEnd() const;
  int SelStart() const;
  void SetSelEnd(int value);
  void SetSelStart(int value);
  bool Shift() const { return m_bShift; }
  bool HasValue() const { return !!m_pValue; }
  WideString& Value() { return *m_pValue; }
  bool WillCommit() const { return m_bWillCommit; }
  CPDFSDK_FormFillEnvironment* GetFormFillEnvironment() const {
    return m_pTargetFormFillEnv.Get();
  }

  void SetValueForTest(WideString* pStr) { m_pValue = pStr; }
  void SetRCForTest(bool* pRC) { m_pbRc = pRC; }
  void SetStrChangeForTest(WideString* pStrChange) {
    m_pWideStrChange = pStrChange;
  }
  void ResetWillCommitForTest() { m_bWillCommit = false; }

 private:
  void Initialize(JS_EVENT_T type);

  JS_EVENT_T m_eEventType = JET_UNKNOWN;
  bool m_bValid = false;
  UnownedPtr<WideString> m_pValue;
  WideString m_strSourceName;
  WideString m_strTargetName;
  WideString m_WideStrChangeDu;
  WideString m_WideStrChangeEx;
  UnownedPtr<WideString> m_pWideStrChange;
  int m_nCommitKey = -1;
  bool m_bKeyDown = false;
  bool m_bModifier = false;
  bool m_bShift = false;
  int m_nSelEndDu = 0;
  int m_nSelStartDu = 0;
  UnownedPtr<int> m_pISelEnd;
  UnownedPtr<int> m_pISelStart;
  bool m_bWillCommit = false;
  bool m_bFieldFull = false;
  bool m_bRcDu = false;
  UnownedPtr<bool> m_pbRc;
  UnownedPtr<CPDF_Bookmark> m_pTargetBookMark;
  ObservedPtr<CPDFSDK_FormFillEnvironment> m_pTargetFormFillEnv;
  ObservedPtr<CPDFSDK_Annot> m_pTargetAnnot;
};

#endif  // FXJS_CJS_EVENTRECORDER_H_
