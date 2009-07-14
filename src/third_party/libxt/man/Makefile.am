libmandir = $(LIB_MAN_DIR)

LIB_MAN_DIR_SUFFIX = $(LIB_MAN_DIR:@mandir@/man%=%)

libman_PRE = \
	XtAddActions.man \
	XtAddCallback.man \
	XtAddEventHandler.man \
	XtAddExposureToRegion.man \
	XtAddGrab.man \
	XtAddInput.man \
	XtAllocateGC.man \
	XtAppAddActionHook.man \
	XtAppAddActions.man \
	XtAppAddBlockHook.man \
	XtAppAddConverter.man \
	XtAppAddInput.man \
	XtAppAddSignal.man \
	XtAppAddTimeOut.man \
	XtAppAddWorkProc.man \
	XtAppCreateShell.man \
	XtAppError.man \
	XtAppErrorMsg.man \
	XtAppGetErrorDatabase.man \
	XtAppGetSelectionTimeout.man \
	XtAppInitialize.man \
	XtAppLock.man \
	XtAppNextEvent.man \
	XtAppReleaseCacheRefs.man \
	XtAppSetExitFlag.man \
	XtAppSetFallbackResources.man \
	XtAppSetTypeConverter.man \
	XtBuildEventMask.man \
	XtCallAcceptFocus.man \
	XtCallActionProc.man \
	XtCallCallbacks.man \
	XtClass.man \
	XtConfigureWidget.man \
	XtConvert.man \
	XtConvertAndStore.man \
	XtCreateApplicationContext.man \
	XtCreateApplicationShell.man \
	XtCreatePopupShell.man \
	XtCreateSelectionRequest.man \
	XtCreateWidget.man \
	XtCreateWindow.man \
	XtDisplay.man \
	XtDisplayInitialize.man \
	XtDisplayStringConversionWarning.man \
	XtDisplayToApplicationContext.man \
	XtError.man \
	XtErrorMsg.man \
	XtFindFile.man \
	XtGetActionKeysym.man \
	XtGetActionList.man \
	XtGetApplicationNameAndClass.man \
	XtGetApplicationResources.man \
	XtGetClassExtension.man \
	XtGetDisplays.man \
	XtGetErrorDatabase.man \
	XtGetGC.man \
	XtGetKeyboardFocusWidget.man \
	XtGetKeysymTable.man \
	XtGetResourceList.man \
	XtGetSelectionParameters.man \
	XtGetSelectionRequest.man \
	XtGetSelectionTimeout.man \
	XtGetSelectionValue.man \
	XtGetSelectionValueIncremental.man \
	XtGetSubresources.man \
	XtGrabKey.man \
	XtHooksOfDisplay.man \
	XtInitialize.man \
	XtInitializeWidgetClass.man \
	XtInsertEventTypeHandler.man \
	XtLastEventProcessed.man \
	XtMakeGeometryRequest.man \
	XtMalloc.man \
	XtManageChildren.man \
	XtMapWidget.man \
	XtName.man \
	XtNameToWidget.man \
	XtNextEvent.man \
	XtOffset.man \
	XtOpenApplication.man \
	XtOwnSelection.man \
	XtParent.man \
	XtParseAcceleratorTable.man \
	XtParseTranslationTable.man \
	XtPopdown.man \
	XtPopup.man \
	XtProcessLock.man \
	XtQueryGeometry.man \
	XtRealizeWidget.man \
	XtRegisterDrawable.man \
	XtRegisterGrabAction.man \
	XtReservePropertyAtom.man \
	XtResolvePathname.man \
	XtSessionGetToken.man \
	XtSetArg.man \
	XtSetKeyTranslator.man \
	XtSetKeyboardFocus.man \
	XtSetLanguageProc.man \
	XtSetMultiClickTime.man \
	XtSetSelectionParameters.man \
	XtSetSensitive.man \
	XtSetValues.man \
	XtSetWMColormapWindows.man \
	XtStringConversionWarning.man \
	XtToolkitThreadInitialize.man \
	XtTranslateCoords.man \
	XtVaCreateArgsList.man

libman_DATA = $(libman_PRE:man=@LIB_MAN_SUFFIX@) \
	      $(all_shadows:=.@LIB_MAN_SUFFIX@)

BUILT_SOURCES = shadows.DONE

EXTRA_DIST = $(libman_PRE)

CLEANFILES = $(libman_DATA)

# Rules for generating files using the C pre-processor
# (Replaces CppFileTarget from Imake)

SED = sed

SUFFIXES = .$(LIB_MAN_SUFFIX) .man

# Translate XCOMM into pound sign with sed, rather than passing -DXCOMM=XCOMM
# to cpp, because that trick does not work on all ANSI C preprocessors.
# Delete line numbers from the cpp output (-P is not portable, I guess).
# Allow XCOMM to be preceded by whitespace and provide a means of generating
# output lines with trailing backslashes.
# Allow XHASH to always be substituted, even in cases where XCOMM isn't.

CPP_SED_MAGIC = $(SED) -e '/^\#  *[0-9][0-9]*  *.*$$/d' \
                       -e '/^\#line  *[0-9][0-9]*  *.*$$/d' \
                       -e '/^[         ]*XCOMM$$/s/XCOMM/\#/' \
                       -e '/^[         ]*XCOMM[^a-zA-Z0-9_]/s/XCOMM/\#/' \
                       -e '/^[         ]*XHASH/s/XHASH/\#/' \
                       -e '/\@\@$$/s/\@\@$$/\\/'

# Strings to replace in man pages
XORGRELSTRING = @PACKAGE_STRING@
  XORGMANNAME = X Version 11
  XSERVERNAME = Xorg

MANDEFS = \
        -D__vendorversion__="\"$(XORGRELSTRING)\" \"$(XORGMANNAME)\"" \
        -D__xorgversion__="\"$(XORGRELSTRING)\" \"$(XORGMANNAME)\"" \
	-D__appmansuffix__=$(APP_MAN_SUFFIX) \
	-D__filemansuffix__=$(FILE_MAN_SUFFIX) \
	-D__libmansuffix__=$(LIB_MAN_SUFFIX) \
	-D__miscmansuffix__=$(MISC_MAN_SUFFIX) \
	-D__drivermansuffix__=$(DRIVER_MAN_SUFFIX) \
	-D__adminmansuffix__=$(ADMIN_MAN_SUFFIX) \
	-D__projectroot__=$(prefix)

.man.$(LIB_MAN_SUFFIX):
	$(RAWCPP) $(RAWCPPFLAGS) $(MANDEFS) $(EXTRAMANDEFS) < $< | $(CPP_SED_MAGIC) > $@


# Generate man page shadow files (Replaces InstallManPageAliases from Imake)

all_shadows =                                \
    $(XtAddCallback_shadows)                 \
    $(XtAddEventHandler_shadows)             \
    $(XtAddGrab_shadows)                     \
    $(XtAppAddConverter_shadows)             \
    $(XtAppAddInput_shadows)                 \
    $(XtAppAddTimeOut_shadows)               \
    $(XtAppAddWorkProc_shadows)              \
    $(XtAppCreateShell_shadows)              \
    $(XtAppError_shadows)                    \
    $(XtAppErrorMsg_shadows)                 \
    $(XtAppGetErrorDatabase_shadows)         \
    $(XtAppGetSelectionTimeout_shadows)      \
    $(XtAppNextEvent_shadows)                \
    $(XtNextEvent_shadows)                   \
    $(XtCallCallbacks_shadows)               \
    $(XtClass_shadows)                       \
    $(XtConfigureWidget_shadows)             \
    $(XtConvert_shadows)                     \
    $(XtConvertAndStore_shadows)             \
    $(XtCreateApplicationContext_shadows)    \
    $(XtCreatePopupShell_shadows)            \
    $(XtCreateWidget_shadows)                \
    $(XtDisplay_shadows)                     \
    $(XtDisplayInitialize_shadows)           \
    $(XtGetGC_shadows)                       \
    $(XtGetResourceList_shadows)             \
    $(XtGetSelectionValue_shadows)           \
    $(XtGetSubresources_shadows)             \
    $(XtGetApplicationResources_shadows)     \
    $(XtMakeGeometryRequest_shadows)         \
    $(XtMalloc_shadows)                      \
    $(XtManageChildren_shadows)              \
    $(XtMapWidget_shadows)                   \
    $(XtNameToWidget_shadows)                \
    $(XtOffset_shadows)                      \
    $(XtOwnSelection_shadows)                \
    $(XtParseAcceleratorTable_shadows)       \
    $(XtParseTranslationTable_shadows)       \
    $(XtPopdown_shadows)                     \
    $(XtPopup_shadows)                       \
    $(XtRealizeWidget_shadows)               \
    $(XtSetArg_shadows)                      \
    $(XtSetKeyTranslator_shadows)            \
    $(XtSetSensitive_shadows)                \
    $(XtSetValues_shadows)                   \
    $(XtGetKeysymTable_shadows)              \
    $(XtAppSetTypeConverter_shadows)         \
    $(XtAppAddActionHook_shadows)            \
    $(XtSetMultiClickTime_shadows)           \
    $(XtInsertEventTypeHandler_shadows)      \
    $(XtLastEventProcessed_shadows)          \
    $(XtAppAddSignal_shadows)                \
    $(XtAddInput_shadows)                    \
    $(XtAppAddBlockHook_shadows)             \
    $(XtAppInitialize_shadows)               \
    $(XtSessionGetToken_shadows)             \
    $(XtErrorMsg_shadows)                    \
    $(XtError_shadows)                       \
    $(XtGetErrorDatabase_shadows)            \
    $(XtGetSelectionValueIncremental_shadows) \
    $(XtGetSelectionTimeout_shadows)         \
    $(XtCreateSelectionRequest_shadows)      \
    $(XtReservePropertyAtom_shadows)         \
    $(XtGrabKey_shadows)                     \
    $(XtRegisterDrawable_shadows)            \
    $(XtAppSetExitFlag_shadows)              \
    $(XtAppLock_shadows)                     \
    $(XtProcessLock_shadows)                 \
    $(XtOpenApplication_shadows)



XtAddCallback_shadows =                     \
    XtAddCallbacks                          \
    XtRemoveCallback                        \
    XtRemoveCallbacks                       \
    XtRemoveAllCallbacks

XtAddEventHandler_shadows =                 \
    XtAddRawEventHandler                    \
    XtRemoveEventHandler                    \
    XtRemoveRawEventHandler                 \
    XtInsertEventHandler                    \
    XtInsertRawEventHandler

XtAddGrab_shadows =                         \
    XtRemoveGrab

XtAppAddConverter_shadows =                 \
    XtAddConverter

XtAppAddInput_shadows =                     \
    XtRemoveInput

XtAppAddTimeOut_shadows =                   \
    XtRemoveTimeOut

XtAppAddWorkProc_shadows =                  \
    XtRemoveWorkProc

XtAppCreateShell_shadows =                  \
    XtVaAppCreateShell

XtAppError_shadows =                        \
    XtAppSetErrorHandler                    \
    XtAppSetWarningHandler                  \
    XtAppWarning

XtAppErrorMsg_shadows =                     \
    XtAppSetErrorMsgHandler                 \
    XtAppSetWarningMsgHandler               \
    XtAppWarningMsg

XtAppGetErrorDatabase_shadows =             \
    XtAppGetErrorDatabaseText

XtAppGetSelectionTimeout_shadows =          \
    XtAppSetSelectionTimeout

XtAppNextEvent_shadows =                    \
    XtAppPending                            \
    XtAppPeekEvent                          \
    XtAppProcessEvent                       \
    XtDispatchEvent                         \
    XtAppMainLoop

XtNextEvent_shadows =                       \
    XtPending                               \
    XtPeekEvent                             \
    XtProcessEvent                          \
    XtMainLoop

XtCallCallbacks_shadows =                   \
    XtCallCallbackList                      \
    XtHasCallbacks

XtClass_shadows =                           \
    XtSuperclass                            \
    XtIsSubclass                            \
    XtCheckSubclass                         \
    XtIsObject                              \
    XtIsRectObj                             \
    XtIsWidget                              \
    XtIsComposite                           \
    XtIsConstraint                          \
    XtIsShell                               \
    XtIsOverrideShell                       \
    XtIsWMShell                             \
    XtIsVendorShell                         \
    XtIsTransientShell                      \
    XtIsTopLevelShell                       \
    XtIsApplicationShell                    \
    XtIsSessionShell

XtConfigureWidget_shadows =                 \
    XtMoveWidget                            \
    XtResizeWidget

XtConvert_shadows =                         \
    XtDirectConvert

XtConvertAndStore_shadows =                 \
    XtCallConverter

XtCreateApplicationContext_shadows =        \
    XtDestroyApplicationContext             \
    XtWidgetToApplicationContext            \
    XtToolkitInitialize

XtCreatePopupShell_shadows =                \
    XtVaCreatePopupShell

XtCreateWidget_shadows =                    \
    XtVaCreateWidget                        \
    XtCreateManagedWidget                   \
    XtVaCreateManagedWidget                 \
    XtDestroyWidget

XtDisplay_shadows =                         \
                                            \
    XtDisplayOfObject                       \
    XtScreen                                \
    XtScreenOfObject                        \
    XtWindow                                \
    XtWindowOfObject

XtDisplayInitialize_shadows =               \
    XtOpenDisplay                           \
    XtCloseDisplay                          \
    XtDatabase                              \
    XtScreenDatabase

XtGetGC_shadows =                           \
    XtReleaseGC

XtGetResourceList_shadows =                 \
    XtGetConstraintResourceList

XtGetSelectionValue_shadows =               \
    XtGetSelectionValues

XtGetSubresources_shadows =                 \
    XtVaGetSubresources

XtGetApplicationResources_shadows =         \
    XtVaGetApplicationResources

XtMakeGeometryRequest_shadows =             \
    XtMakeResizeRequest

XtMalloc_shadows =                          \
    XtCalloc                                \
    XtRealloc                               \
    XtFree                                  \
    XtNew                                   \
    XtNewString

XtManageChildren_shadows =                  \
    XtManageChild                           \
    XtUnmanageChildren                      \
    XtUnmanageChild                         \
    XtIsManaged                             \
    XtChangeManagedSet

XtMapWidget_shadows =                       \
    XtSetMappedWhenManaged                  \
    XtUnmapWidget

XtNameToWidget_shadows =                    \
    XtWindowToWidget

XtOffset_shadows =                          \
    XtOffsetOf                              \
    XtNumber

XtOwnSelection_shadows =                    \
    XtOwnSelectionIncremental               \
    XtDisownSelection

XtParseAcceleratorTable_shadows =           \
    XtInstallAccelerators                   \
    XtInstallAllAccelerators

XtParseTranslationTable_shadows =           \
    XtAugmentTranslations                   \
    XtOverrideTranslations                  \
    XtUninstallTranslations

XtPopdown_shadows =                         \
    XtCallbackPopdown                       \
    MenuPopdown

XtPopup_shadows =                           \
    XtPopupSpringLoaded                     \
    XtCallbackNone                          \
    XtCallbackNonexclusive                  \
    XtCallbackExclusive                     \
    MenuPopup

XtRealizeWidget_shadows =                   \
    XtIsRealized                            \
    XtUnrealizeWidget

XtSetArg_shadows =                          \
    XtMergeArgLists

XtSetKeyTranslator_shadows =                \
    XtTranslateKeycode                      \
    XtRegisterCaseConverter                 \
    XtConvertCase

XtSetSensitive_shadows =                    \
    XtIsSensitive

XtSetValues_shadows =                       \
    XtVaSetValues                           \
    XtSetSubvalues                          \
    XtGetValues                             \
    XtVaGetValues                           \
    XtGetSubvalues                          \
    XtVaGetSubvalues                        \
    XtVaSetSubvalues

XtGetKeysymTable_shadows =                  \
    XtKeysymToKeycodeList

XtAppSetTypeConverter_shadows =             \
    XtSetTypeConverter

XtAppAddActionHook_shadows =                \
    XtRemoveActionHook

XtSetMultiClickTime_shadows =               \
    XtGetMultiClickTime

XtInsertEventTypeHandler_shadows =          \
    XtRemoveEventTypeHandler                \
    XtRegisterExtensionSelector             \
    XtSetEventDispatcher                    \
    XtDispatchEventToWidget

XtLastEventProcessed_shadows =              \
    XtLastTimestampProcessed

XtAppAddSignal_shadows =                    \
    XtNoticeSignal                          \
    XtRemoveSignal

XtAddInput_shadows =                        \
    XtAddTimeOut                            \
    XtAddWorkProc

XtAppAddBlockHook_shadows =                 \
    XtRemoveBlockHook

XtAppInitialize_shadows =                   \
    XtVaAppInitialize

XtSessionGetToken_shadows =                 \
    XtSessionReturnToken

XtErrorMsg_shadows =                        \
    XtSetErrorMsgHandler                    \
    XtSetWarningMsgHandler                  \
    XtWarningMsg

XtError_shadows =                           \
    XtSetErrorHandler                       \
    XtSetWarningHandler                     \
    XtWarning

XtGetErrorDatabase_shadows =                \
    XtGetErrorDatabaseText

XtGetSelectionValueIncremental_shadows =    \
    XtGetSelectionValuesIncremental

XtGetSelectionTimeout_shadows =             \
    XtSetSelectionTimeout

XtCreateSelectionRequest_shadows =          \
    XtSendSelectionRequest                  \
    XtCancelSelectionRequest

XtReservePropertyAtom_shadows =             \
    XtReleasePropertyAtom

XtGrabKey_shadows =                         \
    XtUngrabKey                             \
    XtGrabKeyboard                          \
    XtUngrabKeyboard                        \
    XtGrabButton                            \
    XtUngrabButton                          \
    XtGrabPointer                           \
    XtUngrabPointer

XtRegisterDrawable_shadows =                \
    XtUnregisterDrawable

XtAppSetExitFlag_shadows =                  \
    XtAppGetExitFlag

XtAppLock_shadows =                         \
    XtAppUnlock

XtProcessLock_shadows =                     \
    XtProcessUnlock

XtOpenApplication_shadows =                 \
    XtVaOpenApplication

shadows.DONE:
	-rm -f $(all_aliases:=.@LIB_MAN_SUFFIX@)
	(for i in $(XtAddCallback_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtAddCallback.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtAddEventHandler_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtAddEventHandler.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtAddGrab_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtAddGrab.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtAppAddConverter_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtAppAddConverter.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtAppAddInput_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtAppAddInput.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtAppAddTimeOut_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtAppAddTimeOut.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtAppAddTimeOut_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtAppAddTimeOut.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtAppAddWorkProc_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtAppAddWorkProc.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtAppCreateShell_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtAppCreateShell.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtAppError_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtAppError.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtAppErrorMsg_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtAppErrorMsg.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtAppGetErrorDatabase_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtAppGetErrorDatabase.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtAppGetSelectionTimeout_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtAppGetSelectionTimeout.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtAppNextEvent_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtAppNextEvent.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtNextEvent_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtNextEvent.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtCallCallbacks_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtCallCallbacks.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtClass_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtClass.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtConfigureWidget_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtConfigureWidget.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtConvert_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtConvert.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtConvertAndStore_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtConvertAndStore.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtCreateApplicationContext_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtCreateApplicationContext.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtCreatePopupShell_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtCreatePopupShell.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtCreateWidget_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtCreateWidget.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtDisplay_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtDisplay.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtDisplayInitialize_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtDisplayInitialize.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtGetGC_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtGetGC.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtGetResourceList_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtGetResourceList.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtGetSelectionValue_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtGetSelectionValue.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtGetSubresources_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtGetSubresources.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtGetApplicationResources_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtGetApplicationResources.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtMakeGeometryRequest_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtMakeGeometryRequest.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtMalloc_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtMalloc.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtManageChildren_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtManageChildren.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtMapWidget_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtMapWidget.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtNameToWidget_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtNameToWidget.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtOffset_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtOffset.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtOwnSelection_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtOwnSelection.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtParseAcceleratorTable_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtParseAcceleratorTable.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtParseTranslationTable_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtParseTranslationTable.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtPopdown_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtPopdown.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtPopup_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtPopup.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtRealizeWidget_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtRealizeWidget.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtSetArg_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtSetArg.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtSetKeyTranslator_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtSetKeyTranslator.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtSetSensitive_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtSetSensitive.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtSetValues_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtSetValues.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtGetKeysymTable_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtGetKeysymTable.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtAppSetTypeConverter_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtAppSetTypeConverter.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtAppAddActionHook_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtAppAddActionHook.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtSetMultiClickTime_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtSetMultiClickTime.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtInsertEventTypeHandler_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtInsertEventTypeHandler.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtLastEventProcessed_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtLastEventProcessed.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtAppAddSignal_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtAppAddSignal.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtAddInput_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtAddInput.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtAddInput_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtAddInput.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtAddInput_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtAddInput.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtAppAddBlockHook_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtAppAddBlockHook.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtAppInitialize_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtAppInitialize.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtSessionGetToken_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtSessionGetToken.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtErrorMsg_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtErrorMsg.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtError_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtError.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtGetErrorDatabase_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtGetErrorDatabase.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtGetSelectionValueIncremental_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtGetSelectionValueIncremental.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtGetSelectionTimeout_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtGetSelectionTimeout.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtCreateSelectionRequest_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtCreateSelectionRequest.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtReservePropertyAtom_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtReservePropertyAtom.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtGrabKey_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtGrabKey.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtRegisterDrawable_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtRegisterDrawable.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtAppSetExitFlag_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtAppSetExitFlag.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtAppLock_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtAppLock.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtProcessLock_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtProcessLock.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XtOpenApplication_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XtOpenApplication.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
