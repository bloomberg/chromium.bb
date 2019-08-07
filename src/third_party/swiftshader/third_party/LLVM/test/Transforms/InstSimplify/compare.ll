; RUN: opt < %s -instsimplify -S | FileCheck %s
target datalayout = "p:32:32"

define i1 @ptrtoint() {
; CHECK: @ptrtoint
  %a = alloca i8
  %tmp = ptrtoint i8* %a to i32
  %r = icmp eq i32 %tmp, 0
  ret i1 %r
; CHECK: ret i1 false
}

define i1 @zext(i32 %x) {
; CHECK: @zext
  %e1 = zext i32 %x to i64
  %e2 = zext i32 %x to i64
  %r = icmp eq i64 %e1, %e2
  ret i1 %r
; CHECK: ret i1 true
}

define i1 @zext2(i1 %x) {
; CHECK: @zext2
  %e = zext i1 %x to i32
  %c = icmp ne i32 %e, 0
  ret i1 %c
; CHECK: ret i1 %x
}

define i1 @zext3() {
; CHECK: @zext3
  %e = zext i1 1 to i32
  %c = icmp ne i32 %e, 0
  ret i1 %c
; CHECK: ret i1 true
}

define i1 @sext(i32 %x) {
; CHECK: @sext
  %e1 = sext i32 %x to i64
  %e2 = sext i32 %x to i64
  %r = icmp eq i64 %e1, %e2
  ret i1 %r
; CHECK: ret i1 true
}

define i1 @sext2(i1 %x) {
; CHECK: @sext2
  %e = sext i1 %x to i32
  %c = icmp ne i32 %e, 0
  ret i1 %c
; CHECK: ret i1 %x
}

define i1 @sext3() {
; CHECK: @sext3
  %e = sext i1 1 to i32
  %c = icmp ne i32 %e, 0
  ret i1 %c
; CHECK: ret i1 true
}

define i1 @add(i32 %x, i32 %y) {
; CHECK: @add
  %l = lshr i32 %x, 1
  %q = lshr i32 %y, 1
  %r = or i32 %q, 1
  %s = add i32 %l, %r
  %c = icmp eq i32 %s, 0
  ret i1 %c
; CHECK: ret i1 false
}

define i1 @add2(i8 %x, i8 %y) {
; CHECK: @add2
  %l = or i8 %x, 128
  %r = or i8 %y, 129
  %s = add i8 %l, %r
  %c = icmp eq i8 %s, 0
  ret i1 %c
; CHECK: ret i1 false
}

define i1 @add3(i8 %x, i8 %y) {
; CHECK: @add3
  %l = zext i8 %x to i32
  %r = zext i8 %y to i32
  %s = add i32 %l, %r
  %c = icmp eq i32 %s, 0
  ret i1 %c
; CHECK: ret i1 %c
}

define i1 @add4(i32 %x, i32 %y) {
; CHECK: @add4
  %z = add nsw i32 %y, 1
  %s1 = add nsw i32 %x, %y
  %s2 = add nsw i32 %x, %z
  %c = icmp slt i32 %s1, %s2
  ret i1 %c
; CHECK: ret i1 true
}

define i1 @add5(i32 %x, i32 %y) {
; CHECK: @add5
  %z = add nuw i32 %y, 1
  %s1 = add nuw i32 %x, %z
  %s2 = add nuw i32 %x, %y
  %c = icmp ugt i32 %s1, %s2
  ret i1 %c
; CHECK: ret i1 true
}

define i1 @addpowtwo(i32 %x, i32 %y) {
; CHECK: @addpowtwo
  %l = lshr i32 %x, 1
  %r = shl i32 1, %y
  %s = add i32 %l, %r
  %c = icmp eq i32 %s, 0
  ret i1 %c
; CHECK: ret i1 false
}

define i1 @or(i32 %x) {
; CHECK: @or
  %o = or i32 %x, 1
  %c = icmp eq i32 %o, 0
  ret i1 %c
; CHECK: ret i1 false
}

define i1 @shl(i32 %x) {
; CHECK: @shl
  %s = shl i32 1, %x
  %c = icmp eq i32 %s, 0
  ret i1 %c
; CHECK: ret i1 false
}

define i1 @lshr1(i32 %x) {
; CHECK: @lshr1
  %s = lshr i32 -1, %x
  %c = icmp eq i32 %s, 0
  ret i1 %c
; CHECK: ret i1 false
}

define i1 @lshr2(i32 %x) {
; CHECK: @lshr2
  %s = lshr i32 %x, 30
  %c = icmp ugt i32 %s, 8
  ret i1 %c
; CHECK: ret i1 false
}

define i1 @ashr1(i32 %x) {
; CHECK: @ashr1
  %s = ashr i32 -1, %x
  %c = icmp eq i32 %s, 0
  ret i1 %c
; CHECK: ret i1 false
}

define i1 @ashr2(i32 %x) {
; CHECK: @ashr2
  %s = ashr i32 %x, 30
  %c = icmp slt i32 %s, -5
  ret i1 %c
; CHECK: ret i1 false
}

define i1 @select1(i1 %cond) {
; CHECK: @select1
  %s = select i1 %cond, i32 1, i32 0
  %c = icmp eq i32 %s, 1
  ret i1 %c
; CHECK: ret i1 %cond
}

define i1 @select2(i1 %cond) {
; CHECK: @select2
  %x = zext i1 %cond to i32
  %s = select i1 %cond, i32 %x, i32 0
  %c = icmp ne i32 %s, 0
  ret i1 %c
; CHECK: ret i1 %cond
}

define i1 @select3(i1 %cond) {
; CHECK: @select3
  %x = zext i1 %cond to i32
  %s = select i1 %cond, i32 1, i32 %x
  %c = icmp ne i32 %s, 0
  ret i1 %c
; CHECK: ret i1 %cond
}

define i1 @select4(i1 %cond) {
; CHECK: @select4
  %invert = xor i1 %cond, 1
  %s = select i1 %invert, i32 0, i32 1
  %c = icmp ne i32 %s, 0
  ret i1 %c
; CHECK: ret i1 %cond
}

define i1 @urem1(i32 %X, i32 %Y) {
; CHECK: @urem1
  %A = urem i32 %X, %Y
  %B = icmp ult i32 %A, %Y
  ret i1 %B
; CHECK: ret i1 true
}

define i1 @urem2(i32 %X, i32 %Y) {
; CHECK: @urem2
  %A = urem i32 %X, %Y
  %B = icmp eq i32 %A, %Y
  ret i1 %B
; CHECK: ret i1 false
}

define i1 @urem3(i32 %X) {
; CHECK: @urem3
  %A = urem i32 %X, 10
  %B = icmp ult i32 %A, 15
  ret i1 %B
; CHECK: ret i1 true
}

define i1 @urem4(i32 %X) {
; CHECK: @urem4
  %A = urem i32 %X, 15
  %B = icmp ult i32 %A, 10
  ret i1 %B
; CHECK: ret i1 %B
}

define i1 @urem5(i16 %X, i32 %Y) {
; CHECK: @urem5
  %A = zext i16 %X to i32
  %B = urem i32 %A, %Y
  %C = icmp slt i32 %B, %Y
  ret i1 %C
; CHECK: ret i1 true
}

define i1 @urem6(i32 %X, i32 %Y) {
; CHECK: @urem6
  %A = urem i32 %X, %Y
  %B = icmp ugt i32 %Y, %A
  ret i1 %B
; CHECK: ret i1 true
}

define i1 @srem1(i32 %X) {
; CHECK: @srem1
  %A = srem i32 %X, -5
  %B = icmp sgt i32 %A, 5
  ret i1 %B
; CHECK: ret i1 false
}

; PR9343 #15
; CHECK: @srem2
; CHECK: ret i1 false
define i1 @srem2(i16 %X, i32 %Y) {
  %A = zext i16 %X to i32
  %B = add nsw i32 %A, 1
  %C = srem i32 %B, %Y
  %D = icmp slt i32 %C, 0
  ret i1 %D
}

; CHECK: @srem3
; CHECK-NEXT: ret i1 false
define i1 @srem3(i16 %X, i32 %Y) {
  %A = zext i16 %X to i32
  %B = or i32 2147483648, %A
  %C = sub nsw i32 1, %B
  %D = srem i32 %C, %Y
  %E = icmp slt i32 %D, 0
  ret i1 %E
}

define i1 @udiv1(i32 %X) {
; CHECK: @udiv1
  %A = udiv i32 %X, 1000000
  %B = icmp ult i32 %A, 5000
  ret i1 %B
; CHECK: ret i1 true
}

define i1 @udiv2(i32 %X, i32 %Y, i32 %Z) {
; CHECK: @udiv2
  %A = udiv exact i32 10, %Z
  %B = udiv exact i32 20, %Z
  %C = icmp ult i32 %A, %B
  ret i1 %C
; CHECK: ret i1 true
}

define i1 @sdiv1(i32 %X) {
; CHECK: @sdiv1
  %A = sdiv i32 %X, 1000000
  %B = icmp slt i32 %A, 3000
  ret i1 %B
; CHECK: ret i1 true
}

define i1 @or1(i32 %X) {
; CHECK: @or1
  %A = or i32 %X, 62
  %B = icmp ult i32 %A, 50
  ret i1 %B
; CHECK: ret i1 false
}

define i1 @and1(i32 %X) {
; CHECK: @and1
  %A = and i32 %X, 62
  %B = icmp ugt i32 %A, 70
  ret i1 %B
; CHECK: ret i1 false
}
