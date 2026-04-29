! RUN: bbc -emit-fir -hlfir -o - %s | FileCheck %s

module m
  implicit none

  ! Array Globals that should get alignment 64 by default.
  integer :: int_array(10)
  real :: real_array(5, 5)
  complex :: complex_array(3)
  logical :: logical_array(4)
  character(len=10) :: char_array(2)

  integer, target :: target_array(8)

  ! Currently not align 64
  integer, allocatable :: alloc_array(:)

  ! Non-array and Bind(C) globals should not get alignment 
  integer :: scalar_var

  integer, bind(c, name="c_int_array") :: bind_c_int_array(10)
  real, bind(c, name="c_real_array") :: bind_c_real_array(5)

  ! BIND(C) arrays with initializers (exercises tryCreatingDenseGlobal path)
  integer, bind(c, name="c_init_array") :: bind_c_init_array(5) = [1,2,3,4,5]
  real, bind(c, name="c_real_init") :: bind_c_real_init(3) = [1.0, 2.0, 3.0]

  integer, bind(c, name="c_scalar") :: bind_c_scalar
end module

subroutine sub_with_common()
  implicit none
  ! Common block (alignment from semantics, not 64)
  integer :: cb_int(10)
  real :: cb_real
  common /myblock/ cb_int, cb_real
end subroutine

block data
  implicit none
  integer :: bd_array(5)
  common /initblock/ bd_array
  data bd_array /1, 2, 3, 4, 5/
end block data

! CHECK: fir.global @initblock_ {alignment = 4 : i64} : tuple<!fir.array<5xi32>>
! CHECK-NOT: alignment = 64
! CHECK: fir.global common @myblock_(dense<0> : vector<44xi8>) {alignment = 4 : i64} : !fir.array<44xi8>
! CHECK-NOT: alignment = 64

! CHECK: fir.global @_QMmEalloc_array : !fir.box<!fir.heap<!fir.array<?xi32>>>
! CHECK-NOT: alignment

! CHECK: fir.global @c_init_array(dense<[1, 2, 3, 4, 5]> : tensor<5xi32>) : !fir.array<5xi32>
! CHECK-NOT: alignment
! CHECK: fir.global common @c_int_array : !fir.array<10xi32>
! CHECK-NOT: alignment
! CHECK: fir.global common @c_real_array : !fir.array<5xf32>
! CHECK-NOT: alignment
! CHECK: fir.global @c_real_init(dense<[1.000000e+00, 2.000000e+00, 3.000000e+00]> : tensor<3xf32>) : !fir.array<3xf32>
! CHECK-NOT: alignment
! CHECK: fir.global common @c_scalar : i32
! CHECK-NOT: alignment

! CHECK: fir.global @_QMmEchar_array {alignment = 64 : i64} : !fir.array<2x!fir.char<1,10>>
! CHECK: fir.global @_QMmEcomplex_array {alignment = 64 : i64} : !fir.array<3xcomplex<f32>>
! CHECK: fir.global @_QMmEint_array {alignment = 64 : i64} : !fir.array<10xi32>
! CHECK: fir.global @_QMmElogical_array {alignment = 64 : i64} : !fir.array<4x!fir.logical<4>>
! CHECK: fir.global @_QMmEreal_array {alignment = 64 : i64} : !fir.array<5x5xf32>

! CHECK: fir.global @_QMmEscalar_var : i32
! CHECK-NOT: alignment

! CHECK: fir.global @_QMmEtarget_array {alignment = 64 : i64} target : !fir.array<8xi32>
