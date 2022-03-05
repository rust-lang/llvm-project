; RUN: opt -S -passes='cgscc(inline,instcombine)' < %s | FileCheck %s

; We use call to a dummy function to avoid inlining test1 into test2 or vice
; versa, such that we aren't left with a trivial cycle, as trivial cycles are
; special-cased to never be inlined.
; However, InstCombine will eliminate these calls after inlining, and thus
; make the functions eligible for inlining in their callers.
declare void @dummy() readnone nounwind willreturn

define void @test1() {
; CHECK-LABEL: define void @test1(
; CHECK-NEXT:    call void @test2()
; CHECK-NEXT:    call void @test2()
; CHECK-NEXT:    ret void
;
  call void @test2()
  call void @test2()
  call void @dummy()
  call void @dummy()
  call void @dummy()
  call void @dummy()
  call void @dummy()
  call void @dummy()
  call void @dummy()
  call void @dummy()
  call void @dummy()
  call void @dummy()
  call void @dummy()
  ret void
}

define void @test2() {
; CHECK-LABEL: define void @test2(
; CHECK-NEXT:    call void @test1()
; CHECK-NEXT:    call void @test1()
; CHECK-NEXT:    ret void
;
  call void @test1()
  call void @test1()
  call void @dummy()
  call void @dummy()
  call void @dummy()
  call void @dummy()
  call void @dummy()
  call void @dummy()
  call void @dummy()
  call void @dummy()
  call void @dummy()
  call void @dummy()
  call void @dummy()
  ret void
}

; We should inline the @test2 calls and mark the inlined @test1 calls as noinline
define void @test3() {
; CHECK-LABEL: define void @test3(
; CHECK-NEXT:    call void @test1() #[[NOINLINE:[0-9]+]]
; CHECK-NEXT:    call void @test1() #[[NOINLINE]]
; CHECK-NEXT:    call void @test1() #[[NOINLINE]]
; CHECK-NEXT:    call void @test1() #[[NOINLINE]]
; CHECK-NEXT:    ret void
;
  call void @test2()
  call void @test2()
  ret void
}

; CHECK: [[NOINLINE]] = { noinline }
