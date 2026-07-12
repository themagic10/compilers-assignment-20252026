; ModuleID = 'foo.m2r.ll'
source_filename = "foo.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: noinline nounwind sspstrong uwtable
define dso_local void @test(i32 noundef %0, i32 noundef %1, i32 noundef %2, i32 noundef %3) #0 {
  %5 = mul i32 %0, 23
  %6 = lshr i32 %0, 3
  %7 = lshr i32 %0, 4
  %8 = add i32 %7, %6
  %9 = add i32 %8, 1
  call void (...) @print()
  %10 = mul i32 %1, 24
  %11 = lshr i32 %1, 3
  %12 = lshr i32 %1, 4
  %13 = add i32 %12, %11
  call void (...) @print()
  %14 = mul i32 %0, 64
  %15 = lshr i32 %0, 6
  call void (...) @print()
  %16 = udiv i32 %2, 16
  %17 = lshr i32 16, 4
  call void (...) @print()
  %18 = sdiv i32 %3, 16
  %19 = ashr i32 16, 4
  ret void
}

declare void @print(...) #1

attributes #0 = { noinline nounwind sspstrong uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"clang version 19.1.7"}
