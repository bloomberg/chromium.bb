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

#ifndef INCLUDED_BLPWTK2_CHANNELINFO_H
#define INCLUDED_BLPWTK2_CHANNELINFO_H

#include <blpwtk2_config.h>

#include <string>
#include <vector>

namespace base {
class CommandLine;
}  // close namespace base

namespace blpwtk2 {
class StringRef;

                        // =================
                        // class ChannelInfo
                        // =================

// This class contains the necessary information for setting up a channel
// between a blpwtk2 host process and blpwtk2 client process.  It contains
// methods to serialize and deserialize itself into a string so that it can be
// easily passed between processes.
class ChannelInfo
{
  public:
    // TYPES
    typedef std::vector<std::pair<std::string, std::string> > SwitchMap;

  private:
    // DATA
    SwitchMap d_switches;
    int d_mojoControllerHandle;

  public:

    // CREATORS
    ChannelInfo();
    ~ChannelInfo();

    // MANIPULATORS
    void loadSwitchesFromCommandLine(const base::CommandLine& commandLine);
        // Load switches from the specified 'commandLine'.

    void setMojoControllerHandle(int fd);
        // Sets the file descriptor for Mojo channels.

    bool deserialize(const std::string& data);
        // Import the channel info from the serialized 'data'.
        // Return true if successful, and false otherwise.

    // ACCESSORS
    const SwitchMap& switches() const;
        // Returns a non-modifiable reference to the vector of pairs of each
        // switch and its value.

    int getMojoControllerHandle() const;
        // Returns the file descriptor for Mojo channels.

    std::string getMojoServiceToken() const;
        // Returns the Mojo token for service requests.

    std::string serialize() const;
        // Export the channel info in a serialized string.  This string can
        // be imported to another instance of ChannelInfo.
};
}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_CHANNELINFO_H

// vim: ts=4 et

