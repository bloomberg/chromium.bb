// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @enum {string} */ var AudioNodeType = {
  HEADPHONE: 'HEADPHONE',
  MIC: 'MIC',
  USB: 'USB',
  BLUETOOTH: 'BLUETOOTH',
  HDMI: 'HDMI',
  INTERNAL_SPEAKER: 'INTERNAL_SPEAKER',
  INTERNAL_MIC: 'INTERNAL_MIC',
  KEYBOARD_MIC: 'KEYBOARD_MIC',
  AOKR: 'AOKR',
  POST_MIX_LOOPBACK: 'POST_MIX_LOOPBACK',
  POST_DSP_LOOPBACK: 'POST_DSP_LOOPBACK',
  OTHER: 'OTHER',
};

/**
 * An audio node. Based on the struct AudioNode found in audio_node.h.
 * @constructor
 */
var AudioNode = function() {
  // Whether node will input or output audio.
  this.isInput = false;

  // Node ID. Set to 3000 because predefined output and input
  // nodes use 10000's and 20000's respectively and |nodeCount| will append it.
  this.id = '3000';

  // Display name of the node. When this is empty, cras will automatically
  // use |this.deviceName| as the display name.
  this.name = '';

  // The text label of the selected node name.
  this.deviceName = 'New Device';

  // Based on the AudioNodeType enum.
  this.type = AudioNodeType.OTHER;

  // Whether the node is active or not.
  this.active = false;

  // The time the node was plugged in (in seconds).
  this.pluggedTime = 0;
};

Polymer({
  is: 'audio-settings',

  behaviors: [Polymer.NeonAnimatableBehavior],

  properties: {
    /**
     * An AudioNode which is currently being edited.
     * @type {AudioNode}
     */
    currentEditableObject: {
      type: Object,
      value: function() {
        return {};
      }
    },

    /**
     * The index of the audio node which is currently being edited.
     * This is initially set to -1 (i.e. no node selected) becuase no devices
     * have been copied.
     */
    currentEditIndex: {
      type: Number,
      value: function() {
        return -1;
      }
    },

    /**
     * A counter that will auto increment everytime a new node is added
     * or copied and used to set a new id. This allows the |AudioNode.id|
     * to allows be unique.
     */
    nodeCount: {
      type: Number,
      value: function() {
        return 0;
      }
    },

    /**
     * A set of audio nodes.
     * @type !Array<!AudioNode>
     */
    nodes: {
      type: Array,
      value: function() {
        return [];
      }
    },

    /**
     * A set of options for the possible audio node types.
     * AudioNodeType |type| is based on the AudioType emumation.
     * @type {!Array<!{name: string, type: string}>}
     */
    nodeTypeOptions: {
      type: Array,
      value: function() {
        return [
          {name: 'Headphones', type: AudioNodeType.HEADPHONE},
          {name: 'Mic', type: AudioNodeType.MIC},
          {name: 'Usb', type: AudioNodeType.USB},
          {name: 'Bluetooth', type: AudioNodeType.BLUETOOTH},
          {name: 'HDMI', type: AudioNodeType.HDMI},
          {name: 'Internal Speaker', type: AudioNodeType.INTERNAL_SPEAKER},
          {name: 'Internal Mic', type: AudioNodeType.INTERNAL_MIC},
          {name: 'Keyboard Mic', type: AudioNodeType.KEYBOARD_MIC},
          {name: 'Aokr', type: AudioNodeType.AOKR},
          {name: 'Post Mix Loopback', type: AudioNodeType.POST_MIX_LOOPBACK},
          {name: 'Post Dsp Loopback', type: AudioNodeType.POST_DSP_LOOPBACK},
          {name: 'Other', type: AudioNodeType.OTHER}
        ];
      }
    },
  },

  ready: function() {
    chrome.send('requestAudioNodes');
  },

  /**
   * Adds a new node with default settings to the list of nodes.
   */
  appendNewNode: function() {
    var newNode = new AudioNode();
    newNode.id += this.nodeCount;
    this.nodeCount++;
    this.push('nodes', newNode);
  },

  /**
   * This adds or modifies an audio node to the AudioNodeList.
   * @param {model: {index: number}} e Event with a model containing
   *     the index in |nodes| to add.
   */
  insertAudioNode: function(e) {
    // Create a new audio node and add all the properties from |nodes[i]|.
    var info = this.nodes[e.model.index];
    chrome.send('insertAudioNode', [info]);
  },

  /**
   * This adds/modifies the audio node |nodes[currentEditIndex]| to/from the
   * AudioNodeList.
   * @param {model: {index: number}} e Event with a model containing
   *     the index in |nodes| to add.
   */
  insertEditedAudioNode: function(e) {
    // Insert a new node or update an existing node using all the properties
    // in |node|.
    var node = this.nodes[this.currentEditIndex];
    chrome.send('insertAudioNode', [node]);
    this.$.editDialog.close();
  },

  /**
   * Removes the audio node with id |id|.
   * @param {model: {index: number}} e Event with a model containing
   *     the index in |nodes| to add.
   */
  removeAudioNode: function(e) {
    var info = this.nodes[e.model.index];
    chrome.send('removeAudioNode', [info.id]);
  },

  /**
   * Called on "copy" button from the device list clicked. Creates a copy of
   * the selected node.
   * @param {Event} event Contains event data. |event.model.index| is the index
   *     of the item which the target is contained in.
   */
  copyDevice: function(event) {
    // Create a shallow copy of the selected device.
    var newNode = new AudioNode();
    Object.assign(newNode, this.nodes[event.model.index]);
    newNode.name += ' (Copy)';
    newNode.deviceName += ' (Copy)';
    newNode.id += this.nodeCount;
    this.nodeCount++;

    this.push('nodes', newNode);
  },

  /**
   * Shows a dialog to edit the selected node's properties.
   * @param {Event} event Contains event data. |event.model.index| is the index
   *     of the item which the target is contained in.
   */
  showEditDialog: function(event) {
    var index = event.model.index;
    this.currentEditIndex = index;
    this.currentEditableObject = this.nodes[index];
    this.$.editDialog.showModal();
  },

  /**
   * Called by the WebUI which provides a list of nodes.
   * @param {!Array<!AudioNode>} nodeList A list of audio nodes.
   */
  updateAudioNodes: function(nodeList) {
    /** @type {!Array<!AudioNode>} */ var newNodeList = [];
    for (var i = 0; i < nodeList.length; ++i) {
      // Create a new audio node and add all the properties from |nodeList[i]|.
      var node = new AudioNode();
      Object.assign(node, nodeList[i]);
      newNodeList.push(node);
    }
    this.nodes = newNodeList;
  }
});
