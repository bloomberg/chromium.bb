/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (c) 2000 Daniel Molkentin (molkentin@kde.org)
 *  Copyright (c) 2000 Stefan Schimanski (schimmi@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
 *  Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#include "modules/mediastream/NavigatorMediaStream.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "core/frame/Settings.h"
#include "core/page/Page.h"
#include "modules/mediastream/MediaDevicesRequest.h"
#include "modules/mediastream/MediaErrorState.h"
#include "modules/mediastream/MediaStreamConstraints.h"
#include "modules/mediastream/NavigatorUserMediaErrorCallback.h"
#include "modules/mediastream/NavigatorUserMediaSuccessCallback.h"
#include "modules/mediastream/UserMediaController.h"
#include "modules/mediastream/UserMediaRequest.h"

namespace blink {

void NavigatorMediaStream::getUserMedia(
    Navigator& navigator,
    const MediaStreamConstraints& options,
    NavigatorUserMediaSuccessCallback* success_callback,
    NavigatorUserMediaErrorCallback* error_callback,
    ExceptionState& exception_state) {
  if (!success_callback)
    return;

  UserMediaController* user_media =
      UserMediaController::From(navigator.GetFrame());
  if (!user_media) {
    exception_state.ThrowDOMException(
        kNotSupportedError,
        "No user media controller available; is this a detached window?");
    return;
  }

  MediaErrorState error_state;
  UserMediaRequest* request = UserMediaRequest::Create(
      navigator.GetFrame()->GetDocument(), user_media, options,
      success_callback, error_callback, error_state);
  if (!request) {
    DCHECK(error_state.HadException());
    if (error_state.CanGenerateException()) {
      error_state.RaiseException(exception_state);
    } else {
      error_callback->handleEvent(error_state.CreateError());
    }
    return;
  }

  String error_message;
  if (!request->IsSecureContextUse(error_message)) {
    request->Fail(WebUserMediaRequest::Error::kSecurityError, error_message);
    return;
  }

  request->Start();
}

}  // namespace blink
