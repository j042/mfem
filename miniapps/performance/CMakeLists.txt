# Copyright (c) 2010-2025, Lawrence Livermore National Security, LLC. Produced
# at the Lawrence Livermore National Laboratory. All Rights reserved. See files
# LICENSE and NOTICE for details. LLNL-CODE-806117.
#
# This file is part of the MFEM library. For more information and source code
# availability visit https://mfem.org.
#
# MFEM is free software; you can redistribute it and/or modify it under the
# terms of the BSD-3 license. We welcome feedback and contributions, see file
# CONTRIBUTING.md for details.

# Rosetta will have x86 as a physical CPU, so we need CMAKE_OSX_ARCHITECTURES case too.
if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "ppc|ppc64" OR (APPLE AND "${CMAKE_OSX_ARCHITECTURES}" MATCHES "ppc|ppc64"))
   set(MFEM_PERF_CXX_ARCH_FLAGS "-mcpu=native" "-mtune=native")
elseif (APPLE AND ${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm64")
   set(MFEM_PERF_CXX_ARCH_FLAGS "-mcpu=apple-m1")
elseif (${CMAKE_SYSTEM_PROCESSOR} MATCHES "riscv64")
   set(MFEM_PERF_CXX_ARCH_FLAGS "-march=rv64gc")
else()
   set(MFEM_PERF_CXX_ARCH_FLAGS "-march=native")
endif()

set(PERFORMANCE_CXX_OPTIONS)
if (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
  list(APPEND PERFORMANCE_CXX_OPTIONS
    ${MFEM_PERF_CXX_ARCH_FLAGS}
    "-fcolor-diagnostics"
    "-fvectorize"
    "-fslp-vectorize"
    "-ffp-contract=fast")
elseif (${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
  list(APPEND PERFORMANCE_CXX_OPTIONS
    ${MFEM_PERF_CXX_ARCH_FLAGS}
    "-Wall")
  if (NOT MFEM_USE_CUDA)
     list(APPEND PERFORMANCE_CXX_OPTIONS "-pedantic")
  endif()
  if (NOT MFEM_USE_SIMD)
    list(APPEND PERFORMANCE_CXX_OPTIONS "--param" "max-completely-peel-times=3")
  endif()
elseif (${CMAKE_CXX_COMPILER_ID} MATCHES "Intel")
  list(APPEND PERFORMANCE_CXX_OPTIONS "-xHost")
endif()

add_mfem_miniapp(performance_ex1
  MAIN ex1.cpp
  LIBRARIES mfem
  EXTRA_OPTIONS ${PERFORMANCE_CXX_OPTIONS})

if (MFEM_ENABLE_TESTING)
  add_test(NAME performance_ex1_ser
    COMMAND performance_ex1 -no-vis -r 2)
endif()

if (MFEM_USE_MPI)
  add_mfem_miniapp(performance_ex1p
    MAIN ex1p.cpp
    LIBRARIES mfem
    EXTRA_OPTIONS ${PERFORMANCE_CXX_OPTIONS})

  if (MFEM_ENABLE_TESTING)
    add_test(NAME performance_ex1p_np=${MFEM_MPI_NP}
      COMMAND ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} ${MFEM_MPI_NP}
      ${MPIEXEC_PREFLAGS} $<TARGET_FILE:performance_ex1p> -no-vis -rs 2
      ${MPIEXEC_POSTFLAGS})
  endif()
endif()
