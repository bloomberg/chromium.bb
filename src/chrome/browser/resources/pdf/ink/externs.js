// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview We don't rely on any of these types but they are missing
 * from the Ink extern file currently and need to be defined somewhere.
 * @externs
 */

var ink = {};  // eslint-disable-line no-var
ink.proto.scene_change = {};

/** @constructor */
ink.proto.Snapshot = function() {};

/** @constructor */
ink.proto.Command = function() {};

/** @constructor */
ink.proto.MutationPacket = function() {};

/** @constructor */
ink.proto.SetCallbackFlags = function() {};

/** @constructor */
ink.proto.ToolEvent = function() {};

/** @constructor */
ink.Box = function() {};

/** @constructor */
ink.Model = function() {};

/** @constructor */
ink.ElementListener = function() {};

/** @constructor */
ink.proto.scene_change.SceneChangeEvent = function() {};

const goog = {};
goog.events = {};
goog.disposable = {};
goog.math = {};
goog.ui = {};
goog.html = {};
goog.proto2 = {};

/** @constructor */
goog.events.EventTarget = function() {};

/** @interface */
goog.disposable.IDisposable = function() {};

/** @interface */
goog.events.Listenable = function() {};

/** @constructor */
goog.math.Size = function() {};

/** @constructor */
goog.events.Event = function() {};

/** @constructor */
goog.proto2.Message = function() {};

/**
 * @extends {goog.events.EventTarget}
 * @constructor
 */
goog.ui.Component = function() {};

/** @constructor */
goog.html.SafeUrl = function() {};
