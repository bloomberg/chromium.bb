/*
 * Copyright (C) 2018 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <blpwtk2_contentclient.h>

#include <blpwtk2_contentbrowserclientimpl.h>
#include <blpwtk2_contentrendererclientimpl.h>
#include <blpwtk2_contentutilityclientimpl.h>
#include <blpwtk2_products.h>
#include <blpwtk2_statics.h>

#include <base/command_line.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/path_service.h>
#include <content/public/common/content_switches.h>
#include <content/public/common/user_agent.h>
#include <ui/base/resource/resource_bundle.h>
#include <ui/base/resource/resource_bundle_win.h>
#include <ui/base/ui_base_switches.h>

namespace blpwtk2 {
static ContentClient *g_contentClientInstance;

                        // -------------------
                        // class ContentClient
                        // -------------------

ContentClient *ContentClient::Instance()
{
    return g_contentClientInstance;
}

ContentClient::ContentClient()
{
    DCHECK(nullptr == g_contentClientInstance);
    g_contentClientInstance = this;
}

ContentClient::~ContentClient()
{
    DCHECK(nullptr != g_contentClientInstance);
    g_contentClientInstance = nullptr;
}

base::StringPiece ContentClient::GetDataResource(
    int             resource_id,
    ui::ScaleFactor scale_factor) const
{
    return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
        resource_id, scale_factor);
}

}  // close namespace blpwtk2

// vim: ts=4 et

