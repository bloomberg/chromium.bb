// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/credentialmanager/PasswordCredential.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/URLSearchParams.h"
#include "core/html/FormData.h"
#include "modules/credentialmanager/FormDataOptions.h"
#include "modules/credentialmanager/PasswordCredentialData.h"
#include "platform/credentialmanager/PlatformPasswordCredential.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebCredential.h"
#include "public/platform/WebPasswordCredential.h"

namespace blink {

PasswordCredential* PasswordCredential::create(WebPasswordCredential* webPasswordCredential)
{
    return new PasswordCredential(webPasswordCredential);
}

PasswordCredential* PasswordCredential::create(const PasswordCredentialData& data, ExceptionState& exceptionState)
{
    KURL iconURL = parseStringAsURL(data.iconURL(), exceptionState);
    if (exceptionState.hadException())
        return nullptr;
    return new PasswordCredential(data.id(), data.password(), data.name(), iconURL);
}

PasswordCredential::PasswordCredential(WebPasswordCredential* webPasswordCredential)
    : Credential(webPasswordCredential->platformCredential())
    , m_idName("username")
    , m_passwordName("password")
{
}

PasswordCredential::PasswordCredential(const String& id, const String& password, const String& name, const KURL& icon)
    : Credential(PlatformPasswordCredential::create(id, password, name, icon))
    , m_idName("username")
    , m_passwordName("password")
{
}

PassRefPtr<EncodedFormData> PasswordCredential::encodeFormData(String& contentType) const
{
    if (m_additionalData.isURLSearchParams()) {
        // If |additionalData| is a 'URLSearchParams' object, build a urlencoded response.
        URLSearchParams* params = URLSearchParams::create(URLSearchParamsInit());
        URLSearchParams* additionalData = m_additionalData.getAsURLSearchParams();
        for (const auto& param : additionalData->params()) {
            const String& name = param.first;
            if (name != idName() && name != passwordName())
                params->append(name, param.second);
        }
        params->append(idName(), id());
        params->append(passwordName(), password());

        contentType = AtomicString("application/x-www-form-urlencoded;charset=UTF-8", AtomicString::ConstructFromLiteral);

        return params->encodeFormData();
    }

    // Otherwise, we'll build a multipart response.
    FormData* formData = FormData::create(nullptr);
    if (m_additionalData.isFormData()) {
        FormData* additionalData = m_additionalData.getAsFormData();
        for (const FormData::Entry* entry : additionalData->entries()) {
            const String& name = formData->decode(entry->name());
            if (name == idName() || name == passwordName())
                continue;

            if (entry->blob())
                formData->append(name, entry->blob(), entry->filename());
            else
                formData->append(name, formData->decode(entry->value()));
        }
    }
    formData->append(idName(), id());
    formData->append(passwordName(), password());

    RefPtr<EncodedFormData> encodedData = formData->encodeMultiPartFormData();
    contentType = AtomicString("multipart/form-data; boundary=", AtomicString::ConstructFromLiteral) + encodedData->boundary().data();
    return encodedData.release();
}

const String& PasswordCredential::password() const
{
    return static_cast<PlatformPasswordCredential*>(m_platformCredential.get())->password();
}

DEFINE_TRACE(PasswordCredential)
{
    Credential::trace(visitor);
    visitor->trace(m_additionalData);
}

} // namespace blink
