// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="../../../../webui/resources/js/cr.js"/>
<include src="../../../../webui/resources/js/cr/event_target.js"/>
<include src="../../../../webui/resources/js/cr/ui/array_data_model.js"/>

// Hack for polymer, notifying that CSP is enabled here.
// TODO(yoshiki): Find a way to remove the hack.
if (!('securityPolicy' in document))
  document['securityPolicy'] = {};
if (!('allowsEval' in document.securityPolicy))
  document.securityPolicy['allowsEval'] = false;

// Force Polymer into dirty-checking mode, see http://crbug.com/351967
Object['observe'] = undefined;

<include src="../../../../../third_party/polymer_legacy/platform/platform.js">
<include src="../../../../../third_party/polymer_legacy/polymer/polymer.js">

(function() {

// 'strict mode' is invoked for this scope.
'use strict';

<include src="../../common/js/async_util.js"/>
<include src="../../common/js/util.js"/>
<include src="../../common/js/volume_manager_common.js"/>
<include src="../../foreground/js/file_type.js"/>
<include src="../../foreground/js/volume_manager_wrapper.js">
<include src="../../foreground/js/metadata/metadata_cache.js"/>

<include src="audio_player.js"/>
<include src="audio_player_model.js"/>

<include src="../elements/track_list.js"/>
<include src="../elements/control_panel.js"/>
<include src="../elements/volume_controller.js"/>
<include src="../elements/audio_player.js"/>

window.reload = reload;
window.unload = unload;
window.AudioPlayer = AudioPlayer;

})();
