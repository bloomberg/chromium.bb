define weak i8* @memset(i8* %s, i32 %c, i32 %n) nounwind {
entry:
  %s_addr = alloca i8*, align 4
  %c_addr = alloca i32, align 4
  %n_addr = alloca i32, align 4
  %retval = alloca i8*
  %0 = alloca i8*
  %i = alloca i32
  %"alloca point" = bitcast i32 0 to i32
  store i8* %s, i8** %s_addr
  store i32 %c, i32* %c_addr
  store i32 %n, i32* %n_addr
  store i32 0, i32* %i, align 4
  br label %bb1

bb:                                               ; preds = %bb1
  %1 = load i8** %s_addr, align 4
  %2 = load i32* %c_addr, align 4
  %3 = trunc i32 %2 to i8
  %4 = load i32* %i, align 4
  %5 = getelementptr inbounds i8* %1, i32 %4
  store i8 %3, i8* %5, align 1
  %6 = load i32* %i, align 4
  %7 = add i32 %6, 1
  store i32 %7, i32* %i, align 4
  br label %bb1

bb1:                                              ; preds = %bb, %entry
  %8 = load i32* %i, align 4
  %9 = load i32* %n_addr, align 4
  %10 = icmp ult i32 %8, %9
  br i1 %10, label %bb, label %bb2

bb2:                                              ; preds = %bb1
  %11 = load i8** %s_addr, align 4
  store i8* %11, i8** %0, align 4
  %12 = load i8** %0, align 4
  store i8* %12, i8** %retval, align 4
  br label %return

return:                                           ; preds = %bb2
  %retval3 = load i8** %retval
  ret i8* %retval3
}

define weak i8* @memcpy(i8* %dest, i8* %src, i32 %n) nounwind {
entry:
  %dest_addr = alloca i8*, align 4
  %src_addr = alloca i8*, align 4
  %n_addr = alloca i32, align 4
  %retval = alloca i8*
  %0 = alloca i8*
  %i = alloca i32
  %"alloca point" = bitcast i32 0 to i32
  store i8* %dest, i8** %dest_addr
  store i8* %src, i8** %src_addr
  store i32 %n, i32* %n_addr
  store i32 0, i32* %i, align 4
  br label %bb1

bb:                                               ; preds = %bb1
  %1 = load i8** %dest_addr, align 4
  %2 = load i8** %src_addr, align 4
  %3 = load i32* %i, align 4
  %4 = getelementptr inbounds i8* %2, i32 %3
  %5 = load i8* %4, align 1
  %6 = load i32* %i, align 4
  %7 = getelementptr inbounds i8* %1, i32 %6
  store i8 %5, i8* %7, align 1
  %8 = load i32* %i, align 4
  %9 = add i32 %8, 1
  store i32 %9, i32* %i, align 4
  br label %bb1

bb1:                                              ; preds = %bb, %entry
  %10 = load i32* %i, align 4
  %11 = load i32* %n_addr, align 4
  %12 = icmp ult i32 %10, %11
  br i1 %12, label %bb, label %bb2

bb2:                                              ; preds = %bb1
  %13 = load i8** %dest_addr, align 4
  store i8* %13, i8** %0, align 4
  %14 = load i8** %0, align 4
  store i8* %14, i8** %retval, align 4
  br label %return

return:                                           ; preds = %bb2
  %retval3 = load i8** %retval
  ret i8* %retval3
}
