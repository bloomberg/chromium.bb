; RUN: llc < %s -march=x86-64 -mattr=+sse3,-avx | FileCheck %s -check-prefix=SSE3
; RUN: llc < %s -march=x86-64 -mattr=-sse3,+avx | FileCheck %s -check-prefix=AVX

; SSE3: haddpd1:
; SSE3-NOT: vhaddpd
; SSE3: haddpd
; AVX: haddpd1:
; AVX: vhaddpd
define <2 x double> @haddpd1(<2 x double> %x, <2 x double> %y) {
  %a = shufflevector <2 x double> %x, <2 x double> %y, <2 x i32> <i32 0, i32 2>
  %b = shufflevector <2 x double> %x, <2 x double> %y, <2 x i32> <i32 1, i32 3>
  %r = fadd <2 x double> %a, %b
  ret <2 x double> %r
}

; SSE3: haddpd2:
; SSE3-NOT: vhaddpd
; SSE3: haddpd
; AVX: haddpd2:
; AVX: vhaddpd
define <2 x double> @haddpd2(<2 x double> %x, <2 x double> %y) {
  %a = shufflevector <2 x double> %x, <2 x double> %y, <2 x i32> <i32 1, i32 2>
  %b = shufflevector <2 x double> %y, <2 x double> %x, <2 x i32> <i32 2, i32 1>
  %r = fadd <2 x double> %a, %b
  ret <2 x double> %r
}

; SSE3: haddpd3:
; SSE3-NOT: vhaddpd
; SSE3: haddpd
; AVX: haddpd3:
; AVX: vhaddpd
define <2 x double> @haddpd3(<2 x double> %x) {
  %a = shufflevector <2 x double> %x, <2 x double> undef, <2 x i32> <i32 0, i32 undef>
  %b = shufflevector <2 x double> %x, <2 x double> undef, <2 x i32> <i32 1, i32 undef>
  %r = fadd <2 x double> %a, %b
  ret <2 x double> %r
}

; SSE3: haddps1:
; SSE3-NOT: vhaddps
; SSE3: haddps
; AVX: haddps1:
; AVX: vhaddps
define <4 x float> @haddps1(<4 x float> %x, <4 x float> %y) {
  %a = shufflevector <4 x float> %x, <4 x float> %y, <4 x i32> <i32 0, i32 2, i32 4, i32 6>
  %b = shufflevector <4 x float> %x, <4 x float> %y, <4 x i32> <i32 1, i32 3, i32 5, i32 7>
  %r = fadd <4 x float> %a, %b
  ret <4 x float> %r
}

; SSE3: haddps2:
; SSE3-NOT: vhaddps
; SSE3: haddps
; AVX: haddps2:
; AVX: vhaddps
define <4 x float> @haddps2(<4 x float> %x, <4 x float> %y) {
  %a = shufflevector <4 x float> %x, <4 x float> %y, <4 x i32> <i32 1, i32 2, i32 5, i32 6>
  %b = shufflevector <4 x float> %y, <4 x float> %x, <4 x i32> <i32 4, i32 7, i32 0, i32 3>
  %r = fadd <4 x float> %a, %b
  ret <4 x float> %r
}

; SSE3: haddps3:
; SSE3-NOT: vhaddps
; SSE3: haddps
; AVX: haddps3:
; AVX: vhaddps
define <4 x float> @haddps3(<4 x float> %x) {
  %a = shufflevector <4 x float> %x, <4 x float> undef, <4 x i32> <i32 undef, i32 2, i32 4, i32 6>
  %b = shufflevector <4 x float> %x, <4 x float> undef, <4 x i32> <i32 undef, i32 3, i32 5, i32 7>
  %r = fadd <4 x float> %a, %b
  ret <4 x float> %r
}

; SSE3: haddps4:
; SSE3-NOT: vhaddps
; SSE3: haddps
; AVX: haddps4:
; AVX: vhaddps
define <4 x float> @haddps4(<4 x float> %x) {
  %a = shufflevector <4 x float> %x, <4 x float> undef, <4 x i32> <i32 0, i32 2, i32 undef, i32 undef>
  %b = shufflevector <4 x float> %x, <4 x float> undef, <4 x i32> <i32 1, i32 3, i32 undef, i32 undef>
  %r = fadd <4 x float> %a, %b
  ret <4 x float> %r
}

; SSE3: haddps5:
; SSE3-NOT: vhaddps
; SSE3: haddps
; AVX: haddps5:
; AVX: vhaddps
define <4 x float> @haddps5(<4 x float> %x) {
  %a = shufflevector <4 x float> %x, <4 x float> undef, <4 x i32> <i32 0, i32 3, i32 undef, i32 undef>
  %b = shufflevector <4 x float> %x, <4 x float> undef, <4 x i32> <i32 1, i32 2, i32 undef, i32 undef>
  %r = fadd <4 x float> %a, %b
  ret <4 x float> %r
}

; SSE3: haddps6:
; SSE3-NOT: vhaddps
; SSE3: haddps
; AVX: haddps6:
; AVX: vhaddps
define <4 x float> @haddps6(<4 x float> %x) {
  %a = shufflevector <4 x float> %x, <4 x float> undef, <4 x i32> <i32 0, i32 undef, i32 undef, i32 undef>
  %b = shufflevector <4 x float> %x, <4 x float> undef, <4 x i32> <i32 1, i32 undef, i32 undef, i32 undef>
  %r = fadd <4 x float> %a, %b
  ret <4 x float> %r
}

; SSE3: haddps7:
; SSE3-NOT: vhaddps
; SSE3: haddps
; AVX: haddps7:
; AVX: vhaddps
define <4 x float> @haddps7(<4 x float> %x) {
  %a = shufflevector <4 x float> %x, <4 x float> undef, <4 x i32> <i32 undef, i32 3, i32 undef, i32 undef>
  %b = shufflevector <4 x float> %x, <4 x float> undef, <4 x i32> <i32 undef, i32 2, i32 undef, i32 undef>
  %r = fadd <4 x float> %a, %b
  ret <4 x float> %r
}

; SSE3: hsubpd1:
; SSE3-NOT: vhsubpd
; SSE3: hsubpd
; AVX: hsubpd1:
; AVX: vhsubpd
define <2 x double> @hsubpd1(<2 x double> %x, <2 x double> %y) {
  %a = shufflevector <2 x double> %x, <2 x double> %y, <2 x i32> <i32 0, i32 2>
  %b = shufflevector <2 x double> %x, <2 x double> %y, <2 x i32> <i32 1, i32 3>
  %r = fsub <2 x double> %a, %b
  ret <2 x double> %r
}

; SSE3: hsubpd2:
; SSE3-NOT: vhsubpd
; SSE3: hsubpd
; AVX: hsubpd2:
; AVX: vhsubpd
define <2 x double> @hsubpd2(<2 x double> %x) {
  %a = shufflevector <2 x double> %x, <2 x double> undef, <2 x i32> <i32 0, i32 undef>
  %b = shufflevector <2 x double> %x, <2 x double> undef, <2 x i32> <i32 1, i32 undef>
  %r = fsub <2 x double> %a, %b
  ret <2 x double> %r
}

; SSE3: hsubps1:
; SSE3-NOT: vhsubps
; SSE3: hsubps
; AVX: hsubps1:
; AVX: vhsubps
define <4 x float> @hsubps1(<4 x float> %x, <4 x float> %y) {
  %a = shufflevector <4 x float> %x, <4 x float> %y, <4 x i32> <i32 0, i32 2, i32 4, i32 6>
  %b = shufflevector <4 x float> %x, <4 x float> %y, <4 x i32> <i32 1, i32 3, i32 5, i32 7>
  %r = fsub <4 x float> %a, %b
  ret <4 x float> %r
}

; SSE3: hsubps2:
; SSE3-NOT: vhsubps
; SSE3: hsubps
; AVX: hsubps2:
; AVX: vhsubps
define <4 x float> @hsubps2(<4 x float> %x) {
  %a = shufflevector <4 x float> %x, <4 x float> undef, <4 x i32> <i32 undef, i32 2, i32 4, i32 6>
  %b = shufflevector <4 x float> %x, <4 x float> undef, <4 x i32> <i32 undef, i32 3, i32 5, i32 7>
  %r = fsub <4 x float> %a, %b
  ret <4 x float> %r
}

; SSE3: hsubps3:
; SSE3-NOT: vhsubps
; SSE3: hsubps
; AVX: hsubps3:
; AVX: vhsubps
define <4 x float> @hsubps3(<4 x float> %x) {
  %a = shufflevector <4 x float> %x, <4 x float> undef, <4 x i32> <i32 0, i32 2, i32 undef, i32 undef>
  %b = shufflevector <4 x float> %x, <4 x float> undef, <4 x i32> <i32 1, i32 3, i32 undef, i32 undef>
  %r = fsub <4 x float> %a, %b
  ret <4 x float> %r
}

; SSE3: hsubps4:
; SSE3-NOT: vhsubps
; SSE3: hsubps
; AVX: hsubps4:
; AVX: vhsubps
define <4 x float> @hsubps4(<4 x float> %x) {
  %a = shufflevector <4 x float> %x, <4 x float> undef, <4 x i32> <i32 0, i32 undef, i32 undef, i32 undef>
  %b = shufflevector <4 x float> %x, <4 x float> undef, <4 x i32> <i32 1, i32 undef, i32 undef, i32 undef>
  %r = fsub <4 x float> %a, %b
  ret <4 x float> %r
}
