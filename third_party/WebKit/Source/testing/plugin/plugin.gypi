# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
{
  'variables': {
    'test_plugin_files': [
      'PluginObject.cpp',
      'PluginObject.h',
      'PluginObjectMac.mm',
      'PluginTest.cpp',
      'PluginTest.h',
      'TestObject.cpp',
      'TestObject.h',
      'Tests/DocumentOpenInDestroyStream.cpp',
      'Tests/EvaluateJSAfterRemovingPluginElement.cpp',
      'Tests/FormValue.cpp',
      'Tests/GetURLNotifyWithURLThatFailsToLoad.cpp',
      'Tests/GetURLWithJavaScriptURL.cpp',
      'Tests/GetURLWithJavaScriptURLDestroyingPlugin.cpp',
      'Tests/GetUserAgentWithNullNPPFromNPPNew.cpp',
      'Tests/LeakWindowScriptableObject.cpp',
      'Tests/LogNPPSetWindow.cpp',
      'Tests/NPDeallocateCalledBeforeNPShutdown.cpp',
      'Tests/NPPNewFails.cpp',
      'Tests/NPRuntimeCallsWithNullNPP.cpp',
      'Tests/NPRuntimeObjectFromDestroyedPlugin.cpp',
      'Tests/NPRuntimeRemoveProperty.cpp',
      'Tests/NullNPPGetValuePointer.cpp',
      'Tests/PassDifferentNPPStruct.cpp',
      'Tests/PluginScriptableNPObjectInvokeDefault.cpp',
      'Tests/PluginScriptableObjectOverridesAllProperties.cpp',
      'main.cpp',
    ]
  }
}
