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
 * Creates BoundingBox from minExtent and maxExtent
 * @param {!o3d.math.Point3} minExtent minimum extent of the box.
 * @param {!o3d.math.Point3} maxExtent maximum extent of the box.
 * @constructor
 */
o3d.BoundingBox =
    function(minExtent, maxExtent) {
  o3d.ParamObject.call(this);
  this.minExtent = [minExtent[0], minExtent[1], minExtent[2]];
  this.minExtent = [maxExtent[0], maxExtent[1], maxExtent[2]];
};
o3d.inherit('BoundingBox', 'ParamObject');


/**
 * True if this boundingbox has been initialized.
 * @type {boolean}
 */
o3d.BoundingBox.prototype.valid_ = false;


/**
 * The min extent of the box.
 * @type {!o3d.math.Point3}
 */
o3d.BoundingBox.prototype.minExtent = [0, 0, 0];



/**
 * The max extent of the box.
 * @type {!o3d.math.Point3}
 */
o3d.BoundingBox.prototype.maxExtent = [0, 0, 0];



/**
 * Multiplies the bounding box by the given matrix returning a new bounding
 * box.
 * @param {!o3d.math.Matrix4} matrix The matrix to multiply by.
 * @return {!o3d.BoundingBox}  The new bounding box.
 */
o3d.BoundingBox.prototype.mul =
    function(matrix) {
  o3d.notImplemented();
};


/**
 * Adds a bounding box to this bounding box returning a bounding box that
 * encompases both.
 * @param {!o3d.BoundingBox} box BoundingBox to add to this BoundingBox.
 * @return {!o3d.BoundingBox}  The new bounding box.
 */
o3d.BoundingBox.prototype.add =
    function(box) {
  o3d.notImplemented();
};


/**
 * Checks if a ray defined in same coordinate system as this box intersects
 * this bounding box.
 * TODO(petersont): this can also take six coordinates as input.
 * @param {!o3d.math.Point3} start position of start of ray in local space.
 * @param {!o3d.math.Point3} end position of end of ray in local space.
 * @return {!o3d.RayIntersectionInfo}  RayIntersectionInfo. If result.value
 *     is false then something was wrong like using this function with an
 *     uninitialized bounding box. If result.intersected is true then the ray
 *     intersected the box and result.position is the exact point of
 *     intersection.
 */
o3d.BoundingBox.prototype.intersectRay =
    function(start, end) {
  o3d.notImplemented();
};


/**
 * Returns true if the bounding box is inside the frustum.
 * @param {!o3d.math.Matrix4} matrix Matrix to transform the box from its
 *     local space to view frustum space.
 * @return {boolean}  True if the box is in the frustum.
 */
o3d.BoundingBox.prototype.inFrustum =
    function(matrix) {
  o3d.notImplemented();
};


