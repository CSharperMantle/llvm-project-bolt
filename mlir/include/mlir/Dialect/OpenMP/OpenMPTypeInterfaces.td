//===-- OpenMPTypeInterfaces.td - OpenMP type interfaces ---*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef OPENMP_TYPE_INTERFACES
#define OPENMP_TYPE_INTERFACES

include "mlir/IR/OpBase.td"

def OpenMP_PointerLikeTypeInterface : TypeInterface<"PointerLikeType"> {
  let cppNamespace = "::mlir::omp";

  let description = [{
    An interface for pointer-like types suitable to contain a value that OpenMP
    specification refers to as variable.
  }];

  let methods = [
    InterfaceMethod<
      /*description=*/[{
        Returns the pointee type or null if the pointer has no pointee type
      }],
      /*retTy=*/"::mlir::Type",
      /*methodName=*/"getElementType"
    >,
  ];
}

def OpenMP_IntLikeTypeInterface : TypeInterface<"IntLikeType"> {
  let cppNamespace = "::mlir::omp";

  let description = [{
    An interface for integer-like types suitable for use as loop bounds,
    steps, and other integer operands in OpenMP operations.
  }];

  let methods = [InterfaceMethod<
                     /*description=*/[{
        Returns the bit width of the integer type.
      }],
                     /*retTy=*/"unsigned",
                     /*methodName=*/"getWidth">,
  ];
}

#endif // OPENMP_TYPE_INTERFACES
