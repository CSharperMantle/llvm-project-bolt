; RUN: llc -enable-ipra -print-regusage -o /dev/null 2>&1 < %s | FileCheck %s


target triple = "x86_64-unknown-linux-gnu"

define dso_local void @bar1() {
  call void asm sideeffect "",
    "~{rax},~{rcx},~{rdx},~{rsi},~{rdi},~{r8},~{r9},~{r10},~{xmm0},~{xmm1},~{xmm2},~{xmm3},~{xmm4},~{xmm5},~{xmm6},~{xmm7},~{xmm8},~{xmm9},~{xmm10},~{xmm11},~{xmm12},~{xmm13},~{xmm14},~{xmm15}"()

  ret void
}


define preserve_allcc void @allcc_Fn()#0 {
; CHECK: allcc_Fn Clobbered Registers: $eflags $esp $hsp $rsp $sp $sph $spl $ssp $ymm0 $ymm1 $ymm2 $ymm3 $ymm4 $ymm5 $ymm6 $ymm7 $ymm8 $ymm9 $ymm10 $ymm11 $ymm12 $ymm13 $ymm14 $ymm15 $zmm0 $zmm1 $zmm2 $zmm3 $zmm4 $zmm5 $zmm6 $zmm7 $zmm8 $zmm9 $zmm10 $zmm11 $zmm12 $zmm13 $zmm14 $zmm15
  call void asm sideeffect "",
    "~{r12},~{r13}"()

  call void @bar1()
  ret void
}


define void @noattr_Fn ()#0 {
; CHECK: noattr_Fn Clobbered Registers: $ah $al $ax $ch $cl $cx $dh $di $dih $dil $dl $dx $eax $ecx $edi $edx $eflags $esi $esp $hax $hcx $hdi $hdx $hsi $hsp $rax $rcx $rdi $rdx $rsi $rsp $si $sih $sil $sp $sph $spl $ssp $r8 $r9 $r10 $xmm0 $xmm1 $xmm2 $xmm3 $xmm4 $xmm5 $xmm6 $xmm7 $xmm8 $xmm9 $xmm10 $xmm11 $xmm12 $xmm13 $xmm14 $xmm15 $r8b $r9b $r10b $r8bh $r9bh $r10bh $r8d $r9d $r10d $r8w $r9w $r10w $r8wh $r9wh $r10wh $ymm0 $ymm1 $ymm2 $ymm3 $ymm4 $ymm5 $ymm6 $ymm7 $ymm8 $ymm9 $ymm10 $ymm11 $ymm12 $ymm13 $ymm14 $ymm15 $zmm0 $zmm1 $zmm2 $zmm3 $zmm4 $zmm5 $zmm6 $zmm7 $zmm8 $zmm9 $zmm10 $zmm11 $zmm12 $zmm13 $zmm14 $zmm15

  call void asm sideeffect "",
    "~{r12},~{r13}"()

  call void @bar1()
  ret void
}


define preserve_nonecc void @nonecc_Fn()#0 {
; CHECK: nonecc_Fn Clobbered Registers: $ah $al $ax $ch $cl $cx $dh $di $dih $dil $dl $dx $eax $ecx $edi $edx $esi $esp $hax $hcx $hdi $hdx $hsi $hsp $rax $rcx $rdi $rdx $rsi $rsp $si $sih $sil $sp $sph $spl $ssp $r8 $r9 $r10 $r12 $r13 $xmm0 $xmm1 $xmm2 $xmm3 $xmm4 $xmm5 $xmm6 $xmm7 $xmm8 $xmm9 $xmm10 $xmm11 $xmm12 $xmm13 $xmm14 $xmm15 $r8b $r9b $r10b $r12b $r13b $r8bh $r9bh $r10bh $r12bh $r13bh $r8d $r9d $r10d $r12d $r13d $r8w $r9w $r10w $r12w $r13w $r8wh $r9wh $r10wh $r12wh $r13wh $ymm0 $ymm1 $ymm2 $ymm3 $ymm4 $ymm5 $ymm6 $ymm7 $ymm8 $ymm9 $ymm10 $ymm11 $ymm12 $ymm13 $ymm14 $ymm15 $zmm0 $zmm1 $zmm2 $zmm3 $zmm4 $zmm5 $zmm6 $zmm7 $zmm8 $zmm9 $zmm10 $zmm11 $zmm12 $zmm13 $zmm14 $zmm15
  call void asm sideeffect "",
    "~{r12},~{r13}"()

  call void @bar1()
  ret void
}


@llvm.used = appending global [3 x ptr] [ptr @allcc_Fn, ptr @nonecc_Fn, ptr @noattr_Fn]

attributes #0 = {nounwind}
