/*
 * Copyright (C) 2014 Bloomberg Finance L.P.
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

#include <blpwtk2_channelinfo.h>

#include <blpwtk2_stringref.h>

#include <base/base64.h>
#include <base/command_line.h>
#include <base/pickle.h>
#include <base/strings/utf_string_conversions.h>
#include "services/service_manager/embedder/switches.h"

namespace blpwtk2 {

const uint16_t PICKLE_VERSION = 2;  // Increment this version whenever the
                                    // serialization format changes.

                        // -----------------
                        // class ChannelInfo
                        // -----------------

// CREATORS
ChannelInfo::ChannelInfo()
    : d_mojoControllerHandle(-1)
{
}

ChannelInfo::~ChannelInfo() = default;

// MANIPULATORS
void ChannelInfo::loadSwitchesFromCommandLine(const base::CommandLine& commandLine)
{
    d_switches.clear();
    typedef base::CommandLine::SwitchMap::const_iterator SwitchIterator;
    const base::CommandLine::SwitchMap& switches = commandLine.GetSwitches();
    for (SwitchIterator it = switches.begin(); it != switches.end(); ++it) {
        d_switches.push_back(
            std::make_pair<std::string, std::string>(
                std::string(it->first),
                base::WideToUTF8(it->second)));
    }
}

void ChannelInfo::setMojoControllerHandle(int fd)
{
    d_mojoControllerHandle = fd;
}

bool ChannelInfo::deserialize(const std::string& data)
{
    base::StringPiece sp((const char*)data.data(), data.length());
    std::string decoded;
    if (!Base64Decode(sp, &decoded)) {
        return false;
    }

    base::Pickle pickle(decoded.data(), decoded.size());
    base::PickleIterator it(pickle);
    uint16_t versionCheck;
    if (!it.ReadUInt16(&versionCheck) || versionCheck != PICKLE_VERSION) {
        return false;
    }

    if (!it.ReadInt(&d_mojoControllerHandle)) {
        return false;
    }

    int length;
    if (!it.ReadLength(&length)) {
        return false;
    }
    d_switches.resize(length);
    for (int i = 0; i < length; ++i) {
        if (!it.ReadString(&d_switches[i].first)) {
            return false;
        }
        if (!it.ReadString(&d_switches[i].second)) {
            return false;
        }
    }
    return true;
}
// ACCESSORS
const ChannelInfo::SwitchMap& ChannelInfo::switches() const
{
    return d_switches;
}

int ChannelInfo::getMojoControllerHandle() const
{
    return d_mojoControllerHandle;
}

std::string ChannelInfo::getMojoServiceToken() const
{
    for (auto it : d_switches) {
        if (it.first == service_manager::switches::kServiceRequestChannelToken) {
            return it.second;
        }
    }

    return std::string();
}

std::string ChannelInfo::serialize() const
{
    base::Pickle pickle(sizeof(base::Pickle::Header));
    pickle.WriteUInt16(PICKLE_VERSION);
    pickle.WriteInt(d_mojoControllerHandle);
    pickle.WriteInt(d_switches.size());
    for (size_t i = 0; i < d_switches.size(); ++i) {
        pickle.WriteString(d_switches[i].first);
        pickle.WriteString(d_switches[i].second);
    }
    base::StringPiece sp((const char*)pickle.data(), pickle.size());
    std::string result;
    Base64Encode(sp, &result);

    // The pickle size is already approching to 1KB.  The maximum character
    // limit for Windows command line is 8KB.  We want to make sure we don't
    // come close to the limit with lots of room to spare.
    DCHECK(result.length() < 4096);
    return result;
}


}  // close namespace blpwtk2

// vim: ts=4 et

