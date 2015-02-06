// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package bindings

func align(size, alignment int) int {
	return ((size - 1) | (alignment - 1)) + 1
}

// bytesForBits returns minimum number of bytes required to store provided
// number of bits.
func bytesForBits(bits uint64) int {
	return int((bits + 7) / 8)
}

// StringPointer converts provided string to *string.
func StringPointer(s string) *string {
	return &s
}
