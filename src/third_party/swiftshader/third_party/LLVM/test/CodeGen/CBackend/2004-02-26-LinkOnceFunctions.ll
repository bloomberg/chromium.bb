; RUN: llc < %s -march=c | grep func1 | grep WEAK

define linkonce i32 @func1() {
        ret i32 5
}

