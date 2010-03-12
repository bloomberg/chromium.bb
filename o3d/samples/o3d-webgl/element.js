/*
 * Copyright 2010, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/**
 * An Element manages DrawElements for classes inherited from Element.
 * 
 * @param {!o3d.Material} opt_material The Material used by this Element.
 * @param {!o3d.BoundingBox} opt_boundingBox The BoundingBox used by this
 *     Element for culling.
 * @param {!o3d.Point3} opt_zSortPoint The point to sort by when rendering
 *     this Element in a z ordered DrawPass.
 * @param {boolean} opt_cull Whether or not to attempt to cull this
 *     Element based on whether or not its bounding box is in the view
 *     frustum.
 * @constructor
 */
o3d.Element =
    function(opt_material, opt_boundingBox, opt_zSortPoint, opt_cull) {
  o3d.ParamObject.call(this);
  this.material = opt_material;
  this.boundingBox = opt_boundingBox ||
      new o3d.BoundingBox([-1, -1, -1], [1, 1, 1]);
  this.zSortPoint = opt_zSortPoint || [0, 0, 0];
  this.cull = opt_cull || false;
  this.drawElements = [];
};
o3d.inherit('Element', 'ParamObject');


/**
 * The Material for this element.
 * @type {o3d.Material}
 */
o3d.Element.prototype.material = null;



/**
 * The BoundingBox for this element. If culling is on this bounding box will be
 * tested against the view frustum of any draw context used to render this
 * Element.
 * @type {o3d.BoundingBox}
 */
o3d.Element.prototype.boundingBox = null;



/**
 * The z sort point for this element. If this Element is drawn by a DrawPass
 * that is set to sort by z order this value will be multiplied by the
 * worldViewProjection matrix to compute a z value to sort by.
 * @type {o3d.Point3}
 */
o3d.Element.prototype.zSortPoint = [0, 0, 0];



/**
 * The priority for this element. Used to sort if this Element is drawn by a
 * DrawPass that is set to sort by priority.
 * @type {number}
 */
o3d.Element.prototype.priority = 0;



/**
 * The cull settings for this element. If true this Element will be culled
 * by the bounding box above compared to the view frustum it is being rendered
 * with.
 * 
 * @type {boolean}
 */
o3d.Element.prototype.cull = false;



/**
 * The current owner of this Draw Element. Pass in null to stop being owned.
 * 
 * Note: Elements are referenced by the Pack they are created in and their
 * owner. If the Element is removed from its Pack, then setting the owner
 * to null will free the Element. Or, visa versa, if you set the
 * Element's owner to null then removing it from its Pack will free the
 * Element.
 * 
 * @type {o3d.Element}
 */
o3d.Element.prototype.owner_ = null;

o3d.Element.prototype.__defineSetter__('owner',
    function(o) {
      this.owner_ = o;
      o.addElement(this);
    }
);

o3d.Element.prototype.__defineGetter__('owner',
    function(o) {
      return this.owner_;
    }
);



/**
 * Gets all the DrawElements under this Element.
 * 
 * Each access to this field gets the entire list so it is best to get it
 * just once. For example:
 * 
 * var drawElements = element.drawElements;
 * for (var i = 0; i < drawElements.length; i++) {
 *   var drawElement = drawElements[i];
 * }
 * 
 * 
 * Note that modifications to this array [e.g. push()] will not affect
 * the underlying Element, while modifications to the members of the array.
 * will affect them.
 * 
 * @type {!Array.<!o3d.DrawElement>}
 */
o3d.Element.prototype.drawElements = [];



/**
 * Creates a DrawElement for this Element. Note that unlike
 * Shape.createDrawElements and Transform.createDrawElements this one will
 * create more than one element for the same material.
 * 
 * @param {!o3d.Pack} pack pack used to manage created DrawElement.
 * @param {!o3d.Material} material material to use for DrawElement. If you
 *     pass null it will use the material on this Element. This allows you
 *     to easily setup the default (just draw as is) by passing null or
 *     setup a shadow pass by passing in a shadow material.
 * @return {!o3d.DrawElement} The created draw element.
 */
o3d.Element.prototype.createDrawElement =
    function(pack, material) {
  drawElement = new o3d.DrawElement();
  drawElement.owner = this;
  drawElement.material = material || this.material;
  this.drawElements.push(drawElement);
  return drawElement;
};


/**
 * Computes the intersection of a ray in the same coordinate system as
 * the specified POSITION stream.
 * @param {number} position_stream_index Index of POSITION stream.
 * @param {o3d.Cull} cull which side of the triangles to ignore.
 * @param {!o3d.math.Point3} start position of start of ray in local space.
 * @param {!o3d.math.Point3} end position of end of ray. in local space.
 * @return {!o3d.RayIntersectionInfo}  RayIntersectionInfo class. If valid()
 *     is false then something was wrong, Check GetLastError(). If
 *     intersected() is true then the ray intersected a something. position()
 *     is the exact point of intersection.
 */
o3d.Element.prototype.intersectRay =
    function(position_stream_index, cull, start, end) {
  o3d.notImplemented();
};


/**
 * Computes the bounding box in same coordinate system as the specified
 * POSITION stream.
 * @param {number} position_stream_index Index of POSITION stream.
 * @return {!o3d.BoundingBox}  The boundingbox for this element in local space.
 */
o3d.Element.prototype.getBoundingBox =
    function(position_stream_index) {
  return this.boundingBox;
};


/**
 * Virtual function that renders the element.
 */
o3d.Element.prototype.render = function() { };


