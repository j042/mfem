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

if (MFEM_USE_PARELAG AND MFEM_USE_MPI)

  configure_file(${PROJECT_SOURCE_DIR}/miniapps/parelag/MultilevelHcurlSolver_cube_example_parameters.xml
                 ${PROJECT_BINARY_DIR}/miniapps/parelag/MultilevelHcurlSolver_cube_example_parameters.xml
                 COPYONLY)
  configure_file(${PROJECT_SOURCE_DIR}/miniapps/parelag/MultilevelHcurlSolver_pipe_example_parameters.xml
                 ${PROJECT_BINARY_DIR}/miniapps/parelag/MultilevelHcurlSolver_pipe_example_parameters.xml
                 COPYONLY)

  configure_file(${PROJECT_SOURCE_DIR}/miniapps/parelag/MultilevelHdivSolver_cube_example_parameters.xml
                 ${PROJECT_BINARY_DIR}/miniapps/parelag/MultilevelHdivSolver_cube_example_parameters.xml
                 COPYONLY)
  configure_file(${PROJECT_SOURCE_DIR}/miniapps/parelag/MultilevelHdivSolver_pipe_example_parameters.xml
                 ${PROJECT_BINARY_DIR}/miniapps/parelag/MultilevelHdivSolver_pipe_example_parameters.xml
                 COPYONLY)

  add_mfem_miniapp(MultilevelHcurlHdivSolver
    MAIN MultilevelHcurlHdivSolver.cpp
    LIBRARIES mfem)

# The way the libraries are included, by default, in the command line is based
# on the idea that MFEM depends on the other libraries. Here, the setting is
# such that ParELAG, in fact, depends on MFEM, while MFEM does not depend on
# ParELAG. This addresses that issue.
  target_link_libraries(MultilevelHcurlHdivSolver PUBLIC ${TPL_LIBRARIES} mfem)

endif()
