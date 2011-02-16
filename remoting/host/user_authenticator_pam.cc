// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/user_authenticator_pam.h"

#include <stdlib.h>

#include <string>

#include <security/pam_appl.h>

namespace remoting {

static const char kPamServiceName[] = "chromoting";

UserAuthenticatorPam::UserAuthenticatorPam() {
}

UserAuthenticatorPam::~UserAuthenticatorPam() {
}

bool UserAuthenticatorPam::Authenticate(const std::string& username,
                                        const std::string& password) {
  username_ = username;
  password_ = password;
  pam_conv conversation;
  conversation.conv = ConvFunction;
  conversation.appdata_ptr = static_cast<void*>(this);
  // TODO(lambroslambrou): Allow PAM service name to be configurable.
  pam_handle_t* pam_handle;
  if (pam_start(kPamServiceName, username_.c_str(),
                &conversation, &pam_handle) != PAM_SUCCESS) {
    return false;
  }

  // TODO(lambroslambrou): Move to separate thread.
  int pam_status = pam_authenticate(pam_handle, 0);
  pam_end(pam_handle, pam_status);
  return pam_status == PAM_SUCCESS;
}

// static
int UserAuthenticatorPam::ConvFunction(int num_msg,
                                       const pam_message** msg,
                                       pam_response** resp,
                                       void* appdata_ptr) {
  if (num_msg <= 0)
    return PAM_CONV_ERR;
  UserAuthenticatorPam* user_auth =
      static_cast<UserAuthenticatorPam*>(appdata_ptr);
  // Must allocate with malloc(), as the calling PAM module will
  // release the memory with free().
  pam_response* resp_tmp = static_cast<pam_response*>(
      malloc(num_msg * sizeof(pam_response)));
  if (resp_tmp == NULL)
    return PAM_CONV_ERR;

  bool raise_error = false;
  // On exit from the loop, 'count' will hold the number of initialised items
  // that the cleanup code needs to look at, in case of error.
  int count;
  for (count = 0; count < num_msg; count++) {
    // Alias for readability.
    pam_response* resp_item = &resp_tmp[count];
    resp_item->resp_retcode = 0;
    resp_item->resp = NULL;
    switch (msg[count]->msg_style) {
      case PAM_PROMPT_ECHO_ON:
        resp_item->resp = strdup(user_auth->username_.c_str());
        if (resp_item->resp == NULL)
          raise_error = true;
        break;
      case PAM_PROMPT_ECHO_OFF:
        resp_item->resp = strdup(user_auth->password_.c_str());
        if (resp_item->resp == NULL)
          raise_error = true;
        break;
      case PAM_TEXT_INFO:
        // No response needed, as this instructs the PAM client to display
        // text to the user.  Leave as NULL and continue with next prompt.
        break;
      default:
        // Unexpected style code, so abort.
        raise_error = true;
    }
    if (raise_error)
      break;
  }

  if (raise_error) {
    // Not passing the response back, so free up any memory used.
    for (int n = 0; n < count; n++) {
      if (resp_tmp[n].resp) {
        free(resp_tmp[n].resp);
      }
    }
    free(resp_tmp);
    return PAM_CONV_ERR;
  } else {
    *resp = resp_tmp;
    return PAM_SUCCESS;
  }
}

}  // namespace remoting
