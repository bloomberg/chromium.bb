; RUN: opt < %s -globalopt -S | not grep internal

@G = internal global i32 123            ; <i32*> [#uses=1]

define void @foo() {
        store i32 1, i32* @G
        ret void
}

