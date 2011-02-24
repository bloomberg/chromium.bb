; List of stdlib functions that match llvm instrinsics.
; Used for the static linking scenerio to prevent stripping, constant
; merging, and deletion of these symbols.
; See: http://llvm.org/docs/LangRef.html#intg_used
declare i8* @memset(i8*, i32, i32) nounwind
declare i8* @memcpy(i8*, i8*, i32) nounwind
declare i8* @memmove(i8*, i8*, i32) nounwind
declare double @sqrt(double) nounwind
declare float @sqrtf(float) nounwind
declare double @cos(double) nounwind
declare float @cosf(float) nounwind
declare double @sin(double) nounwind
declare float @sinf(float) nounwind
declare double @pow(double, double) nounwind
declare float @powf(float, float) nounwind

@llvm.used = appending global [11 x i8*] [
    i8* bitcast (i8* (i8*,i32,i32)* @memset to i8*),
    i8* bitcast (i8* (i8*,i8*,i32)* @memcpy to i8*),
    i8* bitcast (i8* (i8*,i8*,i32)* @memmove to i8*),
    i8* bitcast (double (double)* @sqrt to i8*),
    i8* bitcast (float (float)* @sqrtf to i8*),
    i8* bitcast (double (double)* @cos to i8*),
    i8* bitcast (float (float)* @cosf to i8*),
    i8* bitcast (double (double)* @sin to i8*),
    i8* bitcast (float (float)* @sinf to i8*),
    i8* bitcast (double (double, double)* @pow to i8*),
    i8* bitcast (float (float, float)* @powf to i8*)
], section "llvm.metadata"
