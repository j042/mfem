// Copyright (c) 2010-2025, Lawrence Livermore National Security, LLC. Produced
// at the Lawrence Livermore National Laboratory. All Rights reserved. See files
// LICENSE and NOTICE for details. LLNL-CODE-806117.
//
// This file is part of the MFEM library. For more information and source code
// availability visit https://mfem.org.
//
// MFEM is free software; you can redistribute it and/or modify it under the
// terms of the BSD-3 license. We welcome feedback and contributions, see file
// CONTRIBUTING.md for details.

#include "mfem.hpp"
#include "unit_tests.hpp"

using namespace mfem;

namespace bilininteg_1d
{

double f1(const Vector & x) { return 2.345 * x[0]; }
double df1(const Vector & x) { return 2.345; }

double v1(const Vector & x) { return 1.231 * x[0] + 3.57; }
double dv1(const Vector & x) { return 1.231; }

double vf1(const Vector & x) { return v1(x) * f1(x); }
double vdf1(const Vector & x) { return v1(x) * df1(x); }
double dvf1(const Vector & x) { return dv1(x) * f1(x) + v1(x) * df1(x); }

double ddf1(const Vector & x) { return 0.0; }
double dvdf1(const Vector & x) { return - dv1(x) * df1(x); }

const std::string MapTypeName(FiniteElement::MapType map_type)
{
   switch (map_type)
   {
      case FiniteElement::VALUE:
         return "VALUE";
      case FiniteElement::INTEGRAL:
         return "INTEGRAL";
      case FiniteElement::H_CURL:
         return "H_CURL";
      case FiniteElement::H_DIV:
         return "H_DIV";
      default:
         return "UNKNOWN";
   }
}

TEST_CASE("1D Bilinear Mass Integrators",
          "[MixedScalarMassIntegrator]"
          "[MixedScalarIntegrator]"
          "[BilinearFormIntegrator]"
          "[NonlinearFormIntegrator]")
{
   int order = 2, n = 1, dim = 1;
   double cg_rtol = 1e-14;
   double tol = 1e-9;

   Mesh mesh = Mesh::MakeCartesian1D(n, 2.0);

   FunctionCoefficient f1_coef(f1);
   FunctionCoefficient v1_coef(v1);
   FunctionCoefficient vf1_coef(vf1);

   SECTION("Operators on H1")
   {
      H1_FECollection    fec_h1(order, dim);
      FiniteElementSpace fespace_h1(&mesh, &fec_h1);

      GridFunction f_h1(&fespace_h1); f_h1.ProjectCoefficient(f1_coef);

      for (int map_type = (int)FiniteElement::VALUE;
           map_type <= (int)FiniteElement::INTEGRAL; map_type++)
      {
         SECTION("Mapping H1 to L2 (" +
                 MapTypeName((FiniteElement::MapType)map_type) + ")")
         {
            L2_FECollection    fec_l2(order, dim,
                                      BasisType::GaussLegendre,
                                      (FiniteElement::MapType)map_type);
            FiniteElementSpace fespace_l2(&mesh, &fec_l2);

            BilinearForm m_l2(&fespace_l2);
            m_l2.AddDomainIntegrator(new MassIntegrator());
            m_l2.Assemble();
            m_l2.Finalize();

            GridFunction g_l2(&fespace_l2);

            Vector tmp_l2(fespace_l2.GetNDofs());

            SECTION("Without Coefficient")
            {
               MixedBilinearForm blf(&fespace_h1, &fespace_l2);
               blf.AddDomainIntegrator(new MixedScalarMassIntegrator());
               blf.Assemble();
               blf.Finalize();

               blf.Mult(f_h1, tmp_l2); g_l2 = 0.0;
               CG(m_l2, tmp_l2, g_l2, 0, 200, cg_rtol * cg_rtol, 0.0);

               REQUIRE( g_l2.ComputeL2Error(f1_coef) < tol );

               MixedBilinearForm blfw(&fespace_l2, &fespace_h1);
               blfw.AddDomainIntegrator(new MixedScalarMassIntegrator());
               blfw.Assemble();
               blfw.Finalize();

               SparseMatrix * blfT = Transpose(blfw.SpMat());
               SparseMatrix * diff = Add(1.0, blf.SpMat(), -1.0, *blfT);
               REQUIRE( diff->MaxNorm() < tol );
               delete diff;
               delete blfT;
            }
            SECTION("With Coefficient")
            {
               MixedBilinearForm blf(&fespace_h1, &fespace_l2);
               blf.AddDomainIntegrator(new MixedScalarMassIntegrator(v1_coef));
               blf.Assemble();
               blf.Finalize();

               blf.Mult(f_h1, tmp_l2); g_l2 = 0.0;
               CG(m_l2, tmp_l2, g_l2, 0, 200, cg_rtol * cg_rtol, 0.0);

               REQUIRE( g_l2.ComputeL2Error(vf1_coef) < tol );

               MixedBilinearForm blfw(&fespace_l2, &fespace_h1);
               blfw.AddDomainIntegrator(new MixedScalarMassIntegrator(v1_coef));
               blfw.Assemble();
               blfw.Finalize();

               SparseMatrix * blfT = Transpose(blfw.SpMat());
               SparseMatrix * diff = Add(1.0, blf.SpMat(), -1.0, *blfT);
               REQUIRE( diff->MaxNorm() < tol );
               delete diff;
               delete blfT;
            }
         }
      }
      SECTION("Mapping H1 to H1")
      {
         BilinearForm m_h1(&fespace_h1);
         m_h1.AddDomainIntegrator(new MassIntegrator());
         m_h1.Assemble();
         m_h1.Finalize();

         GridFunction g_h1(&fespace_h1);

         Vector tmp_h1(fespace_h1.GetNDofs());

         SECTION("Without Coefficient")
         {
            MixedBilinearForm blf(&fespace_h1, &fespace_h1);
            blf.AddDomainIntegrator(new MixedScalarMassIntegrator());
            blf.Assemble();
            blf.Finalize();

            blf.Mult(f_h1, tmp_h1); g_h1 = 0.0;
            CG(m_h1, tmp_h1, g_h1, 0, 200, cg_rtol * cg_rtol, 0.0);

            REQUIRE( g_h1.ComputeL2Error(f1_coef) < tol );
         }
         SECTION("With Coefficient")
         {
            MixedBilinearForm blf(&fespace_h1, &fespace_h1);
            blf.AddDomainIntegrator(new MixedScalarMassIntegrator(v1_coef));
            blf.Assemble();
            blf.Finalize();

            blf.Mult(f_h1, tmp_h1); g_h1 = 0.0;
            CG(m_h1, tmp_h1, g_h1, 0, 200, cg_rtol * cg_rtol, 0.0);

            REQUIRE( g_h1.ComputeL2Error(vf1_coef) < tol );
         }
      }
   }
   for (int map_type_d = (int)FiniteElement::VALUE;
        map_type_d <= (int)FiniteElement::INTEGRAL; map_type_d++)
   {
      SECTION("Operators on L2 (" +
              MapTypeName((FiniteElement::MapType)map_type_d) + ")")
      {
         L2_FECollection    fec_l2_d(order, dim,
                                     BasisType::GaussLegendre,
                                     (FiniteElement::MapType)map_type_d);
         FiniteElementSpace fespace_l2_d(&mesh, &fec_l2_d);

         GridFunction f_l2(&fespace_l2_d); f_l2.ProjectCoefficient(f1_coef);

         for (int map_type_r = (int)FiniteElement::VALUE;
              map_type_r <= (int)FiniteElement::INTEGRAL; map_type_r++)
         {
            SECTION("Mapping L2 (" +
                    MapTypeName((FiniteElement::MapType)map_type_d) +
                    ") to L2 (" +
                    MapTypeName((FiniteElement::MapType)map_type_r) + ")")
            {
               L2_FECollection    fec_l2_r(order, dim,
                                           BasisType::GaussLegendre,
                                           (FiniteElement::MapType)map_type_r);
               FiniteElementSpace fespace_l2_r(&mesh, &fec_l2_r);

               BilinearForm m_l2(&fespace_l2_r);
               m_l2.AddDomainIntegrator(new MassIntegrator());
               m_l2.Assemble();
               m_l2.Finalize();

               GridFunction g_l2(&fespace_l2_r);

               Vector tmp_l2(fespace_l2_r.GetNDofs());

               SECTION("Without Coefficient")
               {
                  MixedBilinearForm blf(&fespace_l2_d, &fespace_l2_r);
                  blf.AddDomainIntegrator(new MixedScalarMassIntegrator());
                  blf.Assemble();
                  blf.Finalize();

                  blf.Mult(f_l2, tmp_l2); g_l2 = 0.0;
                  CG(m_l2, tmp_l2, g_l2, 0, 200, cg_rtol * cg_rtol, 0.0);

                  REQUIRE( g_l2.ComputeL2Error(f1_coef) < tol );
               }
               SECTION("With Coefficient")
               {
                  MixedBilinearForm blf(&fespace_l2_d, &fespace_l2_r);
                  blf.AddDomainIntegrator(new MixedScalarMassIntegrator(v1_coef));
                  blf.Assemble();
                  blf.Finalize();

                  blf.Mult(f_l2, tmp_l2); g_l2 = 0.0;
                  CG(m_l2, tmp_l2, g_l2, 0, 200, cg_rtol * cg_rtol, 0.0);

                  REQUIRE( g_l2.ComputeL2Error(vf1_coef) < tol );
               }
            }
         }
         SECTION("Mapping L2 (" +
                 MapTypeName((FiniteElement::MapType)map_type_d) +
                 ") to H1")
         {
            H1_FECollection    fec_h1(order, dim);
            FiniteElementSpace fespace_h1(&mesh, &fec_h1);

            BilinearForm m_h1(&fespace_h1);
            m_h1.AddDomainIntegrator(new MassIntegrator());
            m_h1.Assemble();
            m_h1.Finalize();

            GridFunction g_h1(&fespace_h1);

            Vector tmp_h1(fespace_h1.GetNDofs());

            SECTION("Without Coefficient")
            {
               MixedBilinearForm blf(&fespace_l2_d, &fespace_h1);
               blf.AddDomainIntegrator(new MixedScalarMassIntegrator());
               blf.Assemble();
               blf.Finalize();

               blf.Mult(f_l2, tmp_h1); g_h1 = 0.0;
               CG(m_h1, tmp_h1, g_h1, 0, 200, cg_rtol * cg_rtol, 0.0);

               REQUIRE( g_h1.ComputeL2Error(f1_coef) < tol );

               MixedBilinearForm blfw(&fespace_h1, &fespace_l2_d);
               blfw.AddDomainIntegrator(new MixedScalarMassIntegrator());
               blfw.Assemble();
               blfw.Finalize();

               SparseMatrix * blfT = Transpose(blfw.SpMat());
               SparseMatrix * diff = Add(1.0, blf.SpMat(), -1.0, *blfT);
               REQUIRE( diff->MaxNorm() < tol );
               delete diff;
               delete blfT;
            }
            SECTION("With Coefficient")
            {
               MixedBilinearForm blf(&fespace_l2_d, &fespace_h1);
               blf.AddDomainIntegrator(new MixedScalarMassIntegrator(v1_coef));
               blf.Assemble();
               blf.Finalize();

               blf.Mult(f_l2, tmp_h1); g_h1 = 0.0;
               CG(m_h1, tmp_h1, g_h1, 0, 200, cg_rtol * cg_rtol, 0.0);

               REQUIRE( g_h1.ComputeL2Error(vf1_coef) < tol );

               MixedBilinearForm blfw(&fespace_h1, &fespace_l2_d);
               blfw.AddDomainIntegrator(new MixedScalarMassIntegrator(v1_coef));
               blfw.Assemble();
               blfw.Finalize();

               SparseMatrix * blfT = Transpose(blfw.SpMat());
               SparseMatrix * diff = Add(1.0, blf.SpMat(), -1.0, *blfT);
               REQUIRE( diff->MaxNorm() < tol );
               delete diff;
               delete blfT;
            }
         }
      }
   }
}

TEST_CASE("1D Bilinear Derivative Integrator",
          "[MixedScalarDerivativeIntegrator]"
          "[MixedScalarIntegrator]"
          "[BilinearFormIntegrator]"
          "[NonlinearFormIntegrator]")
{
   int order = 2, n = 1, dim = 1;
   double cg_rtol = 1e-14;
   double tol = 1e-9;

   Mesh mesh = Mesh::MakeCartesian1D(n, 2.0);

   FunctionCoefficient f1_coef(f1);
   FunctionCoefficient df1_coef(df1);
   FunctionCoefficient v1_coef(v1);
   FunctionCoefficient vdf1_coef(vdf1);

   SECTION("Operators on H1")
   {
      H1_FECollection    fec_h1(order, dim);
      FiniteElementSpace fespace_h1(&mesh, &fec_h1);

      GridFunction f_h1(&fespace_h1); f_h1.ProjectCoefficient(f1_coef);

      for (int map_type = (int)FiniteElement::VALUE;
           map_type <= (int)FiniteElement::INTEGRAL; map_type++)
      {
         SECTION("Mapping H1 to L2 (" +
                 MapTypeName((FiniteElement::MapType)map_type) + ")")
         {
            L2_FECollection    fec_l2(order - 1, dim,
                                      BasisType::GaussLegendre,
                                      (FiniteElement::MapType)map_type);
            FiniteElementSpace fespace_l2(&mesh, &fec_l2);

            BilinearForm m_l2(&fespace_l2);
            m_l2.AddDomainIntegrator(new MassIntegrator());
            m_l2.Assemble();
            m_l2.Finalize();

            GridFunction g_l2(&fespace_l2);

            Vector tmp_l2(fespace_l2.GetNDofs());

            SECTION("Without Coefficient")
            {
               MixedBilinearForm blf(&fespace_h1, &fespace_l2);
               blf.AddDomainIntegrator(new MixedScalarDerivativeIntegrator());
               blf.Assemble();
               blf.Finalize();

               blf.Mult(f_h1, tmp_l2); g_l2 = 0.0;
               CG(m_l2, tmp_l2, g_l2, 0, 200, cg_rtol * cg_rtol, 0.0);

               REQUIRE( g_l2.ComputeL2Error(df1_coef) < tol );
            }
            SECTION("With Coefficient")
            {
               MixedBilinearForm blf(&fespace_h1, &fespace_l2);
               blf.AddDomainIntegrator(
                  new MixedScalarDerivativeIntegrator(v1_coef));
               blf.Assemble();
               blf.Finalize();

               blf.Mult(f_h1, tmp_l2); g_l2 = 0.0;
               CG(m_l2, tmp_l2, g_l2, 0, 200, cg_rtol * cg_rtol, 0.0);

               REQUIRE( g_l2.ComputeL2Error(vdf1_coef) < tol );
            }
         }
      }
      SECTION("Mapping H1 to H1")
      {
         BilinearForm m_h1(&fespace_h1);
         m_h1.AddDomainIntegrator(new MassIntegrator());
         m_h1.Assemble();
         m_h1.Finalize();

         GridFunction g_h1(&fespace_h1);

         Vector tmp_h1(fespace_h1.GetNDofs());

         SECTION("Without Coefficient")
         {
            MixedBilinearForm blf(&fespace_h1, &fespace_h1);
            blf.AddDomainIntegrator(new MixedScalarDerivativeIntegrator());
            blf.Assemble();
            blf.Finalize();

            blf.Mult(f_h1, tmp_h1); g_h1 = 0.0;
            CG(m_h1, tmp_h1, g_h1, 0, 200, cg_rtol * cg_rtol, 0.0);

            REQUIRE( g_h1.ComputeL2Error(df1_coef) < tol );
         }
         SECTION("With Coefficient")
         {
            MixedBilinearForm blf(&fespace_h1, &fespace_h1);
            blf.AddDomainIntegrator(
               new MixedScalarDerivativeIntegrator(v1_coef));
            blf.Assemble();
            blf.Finalize();

            blf.Mult(f_h1, tmp_h1); g_h1 = 0.0;
            CG(m_h1, tmp_h1, g_h1, 0, 200, cg_rtol * cg_rtol, 0.0);

            REQUIRE( g_h1.ComputeL2Error(vdf1_coef) < tol );
         }
      }
   }
}

TEST_CASE("1D Bilinear Weak Derivative Integrator",
          "[MixedScalarWeakDerivativeIntegrator]"
          "[MixedScalarIntegrator]"
          "[BilinearFormIntegrator]"
          "[NonlinearFormIntegrator]")
{
   int order = 2, n = 1, dim = 1;
   double cg_rtol = 1e-14;
   double tol = 1e-9;

   Mesh mesh = Mesh::MakeCartesian1D(n, 2.0);

   FunctionCoefficient f1_coef(f1);
   FunctionCoefficient v1_coef(v1);
   FunctionCoefficient vf1_coef(vf1);
   FunctionCoefficient df1_coef(df1);
   FunctionCoefficient dvf1_coef(dvf1);

   SECTION("Operators on H1")
   {
      H1_FECollection    fec_h1(order, dim);
      FiniteElementSpace fespace_h1(&mesh, &fec_h1);

      GridFunction f_h1(&fespace_h1); f_h1.ProjectCoefficient(f1_coef);

      SECTION("Mapping H1 to H1")
      {
         BilinearForm m_h1(&fespace_h1);
         m_h1.AddDomainIntegrator(new MassIntegrator());
         m_h1.Assemble();
         m_h1.Finalize();

         GridFunction g_h1(&fespace_h1);

         Vector tmp_h1(fespace_h1.GetNDofs());

         SECTION("Without Coefficient")
         {
            MixedBilinearForm blf(&fespace_h1, &fespace_h1);
            blf.AddDomainIntegrator(new MixedScalarWeakDerivativeIntegrator());
            blf.Assemble();
            blf.Finalize();

            LinearForm b(&fespace_h1);
            b.AddBoundaryIntegrator(new BoundaryLFIntegrator(f1_coef));
            b.Assemble();

            blf.Mult(f_h1, tmp_h1); tmp_h1 += b; g_h1 = 0.0;
            CG(m_h1, tmp_h1, g_h1, 0, 200, cg_rtol * cg_rtol, 0.0);

            REQUIRE( g_h1.ComputeL2Error(df1_coef) < tol );

            MixedBilinearForm blfw(&fespace_h1, &fespace_h1);
            blfw.AddDomainIntegrator(new MixedScalarDerivativeIntegrator());
            blfw.Assemble();
            blfw.Finalize();

            SparseMatrix * blfT = Transpose(blfw.SpMat());
            SparseMatrix * diff = Add(1.0, blf.SpMat(), 1.0, *blfT);
            REQUIRE( diff->MaxNorm() < tol );
            delete diff;
            delete blfT;
         }
         SECTION("With Coefficient")
         {
            MixedBilinearForm blf(&fespace_h1, &fespace_h1);
            blf.AddDomainIntegrator(
               new MixedScalarWeakDerivativeIntegrator(v1_coef));
            blf.Assemble();
            blf.Finalize();

            LinearForm b(&fespace_h1);
            b.AddBoundaryIntegrator(new BoundaryLFIntegrator(vf1_coef));
            b.Assemble();

            blf.Mult(f_h1, tmp_h1); tmp_h1 += b; g_h1 = 0.0;
            CG(m_h1, tmp_h1, g_h1, 0, 200, cg_rtol * cg_rtol, 0.0);

            REQUIRE( g_h1.ComputeL2Error(dvf1_coef) < tol );

            MixedBilinearForm blfw(&fespace_h1, &fespace_h1);
            blfw.AddDomainIntegrator(
               new MixedScalarDerivativeIntegrator(v1_coef));
            blfw.Assemble();
            blfw.Finalize();

            SparseMatrix * blfT = Transpose(blfw.SpMat());
            SparseMatrix * diff = Add(1.0, blf.SpMat(), 1.0, *blfT);
            REQUIRE( diff->MaxNorm() < tol );
            delete diff;
            delete blfT;
         }
      }
   }
   for (int map_type = (int)FiniteElement::VALUE;
        map_type <= (int)FiniteElement::INTEGRAL; map_type++)
   {
      SECTION("Operators on L2 (" +
              MapTypeName((FiniteElement::MapType)map_type) + ")")
      {
         L2_FECollection    fec_l2(order, dim,
                                   BasisType::GaussLegendre,
                                   (FiniteElement::MapType)map_type);
         FiniteElementSpace fespace_l2(&mesh, &fec_l2);

         GridFunction f_l2(&fespace_l2); f_l2.ProjectCoefficient(f1_coef);

         SECTION("Mapping L2 (" +
                 MapTypeName((FiniteElement::MapType)map_type) + ") to H1")
         {
            H1_FECollection    fec_h1(order, dim);
            FiniteElementSpace fespace_h1(&mesh, &fec_h1);

            BilinearForm m_h1(&fespace_h1);
            m_h1.AddDomainIntegrator(new MassIntegrator());
            m_h1.Assemble();
            m_h1.Finalize();

            GridFunction g_h1(&fespace_h1);

            Vector tmp_h1(fespace_h1.GetNDofs());

            SECTION("Without Coefficient")
            {
               MixedBilinearForm blf(&fespace_l2, &fespace_h1);
               blf.AddDomainIntegrator(new MixedScalarWeakDerivativeIntegrator());
               blf.Assemble();
               blf.Finalize();

               LinearForm b(&fespace_h1);
               b.AddBoundaryIntegrator(new BoundaryLFIntegrator(f1_coef));
               b.Assemble();

               blf.Mult(f_l2, tmp_h1); tmp_h1 += b; g_h1 = 0.0;
               CG(m_h1, tmp_h1, g_h1, 0, 200, cg_rtol * cg_rtol, 0.0);

               REQUIRE( g_h1.ComputeL2Error(df1_coef) < tol );

               MixedBilinearForm blfw(&fespace_h1, &fespace_l2);
               blfw.AddDomainIntegrator(new MixedScalarDerivativeIntegrator());
               blfw.Assemble();
               blfw.Finalize();

               SparseMatrix * blfT = Transpose(blfw.SpMat());
               SparseMatrix * diff = Add(1.0, blf.SpMat(), 1.0, *blfT);
               REQUIRE( diff->MaxNorm() < tol );
               delete diff;
               delete blfT;
            }
            SECTION("With Coefficient")
            {
               MixedBilinearForm blf(&fespace_l2, &fespace_h1);
               blf.AddDomainIntegrator(
                  new MixedScalarWeakDerivativeIntegrator(v1_coef));
               blf.Assemble();
               blf.Finalize();

               LinearForm b(&fespace_h1);
               b.AddBoundaryIntegrator(new BoundaryLFIntegrator(vf1_coef));
               b.Assemble();

               blf.Mult(f_l2, tmp_h1); tmp_h1 += b; g_h1 = 0.0;
               CG(m_h1, tmp_h1, g_h1, 0, 200, cg_rtol * cg_rtol, 0.0);

               REQUIRE( g_h1.ComputeL2Error(dvf1_coef) < tol );

               MixedBilinearForm blfw(&fespace_h1, &fespace_l2);
               blfw.AddDomainIntegrator(
                  new MixedScalarDerivativeIntegrator(v1_coef));
               blfw.Assemble();
               blfw.Finalize();

               SparseMatrix * blfT = Transpose(blfw.SpMat());
               SparseMatrix * diff = Add(1.0, blf.SpMat(), 1.0, *blfT);
               REQUIRE( diff->MaxNorm() < tol );
               delete diff;
               delete blfT;
            }
         }
      }
   }
}

void f_2(const Vector &x, Vector &f)
{
   f(0) = f(1) = 2.345 * x[0];
}
void v_2(const Vector &x, Vector &v)
{
   v(0) = v(1) = 1.231 * x[0] + 3.57;
}
void v_df_2(const Vector &x, Vector &r)
{
   r(0) = r(1) = v1(x) * df1(x);
}
void dv_df_2(const Vector &x, Vector &d)
{
   d(0) = d(1) = - dv1(x) * df1(x);
}

TEST_CASE("1D Bilinear Diffusion Integrator",
          "[DiffusionIntegrator]"
          "[VectorDiffusionIntegrator]"
          "[BilinearFormIntegrator]"
          "[NonlinearFormIntegrator]")
{
   int order = 2, n = 1, dim = 1;
   double cg_rtol = 1e-14;
   double tol = 1e-9;

   Mesh mesh = Mesh::MakeCartesian1D(n, 2.0);

   H1_FECollection    fec_h1(order, dim);
   FiniteElementSpace fespace_h1(&mesh, &fec_h1);

   BilinearForm m_h1(&fespace_h1);
   m_h1.AddDomainIntegrator(new MassIntegrator());
   m_h1.Assemble();
   m_h1.Finalize();

   FunctionCoefficient f1_coef(f1);
   FunctionCoefficient v1_coef(v1);
   FunctionCoefficient df1_coef(df1);
   FunctionCoefficient vdf1_coef(vdf1);
   FunctionCoefficient ddf1_coef(ddf1);
   FunctionCoefficient dvdf1_coef(dvdf1);

   GridFunction f_h1(&fespace_h1); f_h1.ProjectCoefficient(f1_coef);
   GridFunction g_h1(&fespace_h1);

   Vector tmp_h1(fespace_h1.GetNDofs());

   SECTION("DiffusionIntegrator Without Coefficient")
   {
      BilinearForm blf(&fespace_h1);
      blf.AddDomainIntegrator(new DiffusionIntegrator());
      blf.Assemble();
      blf.Finalize();

      LinearForm b(&fespace_h1);
      b.AddBoundaryIntegrator(new BoundaryLFIntegrator(df1_coef));
      b.Assemble();
      b[0] *=-1.0;

      blf.Mult(f_h1, tmp_h1); tmp_h1 -= b; g_h1 = 0.0;
      CG(m_h1, tmp_h1, g_h1, 0, 200, cg_rtol * cg_rtol, 0.0);

      REQUIRE( g_h1.ComputeL2Error(ddf1_coef) < tol );
   }
   SECTION("DiffusionIntegrator With Coefficient")
   {
      BilinearForm blf(&fespace_h1);
      blf.AddDomainIntegrator(new DiffusionIntegrator(v1_coef));
      blf.Assemble();
      blf.Finalize();

      LinearForm b(&fespace_h1);
      b.AddBoundaryIntegrator(new BoundaryLFIntegrator(vdf1_coef));
      b.Assemble();
      b[0] *= -1.0;

      blf.Mult(f_h1, tmp_h1); tmp_h1 -= b; g_h1 = 0.0;
      CG(m_h1, tmp_h1, g_h1, 0, 200, cg_rtol * cg_rtol, 0.0);

      REQUIRE( g_h1.ComputeL2Error(dvdf1_coef) < tol );
   }
   SECTION("VectorDiffusionIntegrator With VectorCoefficient")
   {
      FiniteElementSpace fespace_h1_2(&mesh, &fec_h1, 2);
      VectorFunctionCoefficient f_coef_2(2, f_2);
      VectorFunctionCoefficient v_coeff_2(2, v_2);
      VectorFunctionCoefficient v_df_coef_2(2, v_df_2);
      VectorFunctionCoefficient dvdf_coef_2(2, dv_df_2);

      BilinearForm B(&fespace_h1_2);
      B.AddDomainIntegrator(new VectorDiffusionIntegrator(v_coeff_2));
      B.Assemble();
      B.Finalize();

      // b_i = (v grad(f).n phi_i).
      LinearForm b(&fespace_h1_2);
      b.AddBoundaryIntegrator(new VectorBoundaryLFIntegrator(v_df_coef_2));
      b.Assemble();
      // The normal on the left is -1 (it's not part of the integrator).
      b[0] *= -1.0;
      b[3] *= -1.0;

      // tmp_i = (grad(v grad(f)) phi_i).
      GridFunction f_2(&fespace_h1_2); f_2.ProjectCoefficient(f_coef_2);
      Vector tmp(b.Size());
      B.Mult(f_2, tmp);
      // Check AssembleElementVector (assumes 1-element mesh).
      {
         VectorDiffusionIntegrator vdi(v_coeff_2);
         const FiniteElement &fe = *fespace_h1_2.GetFE(0);
         Vector res(b.Size());
         f_2.HostRead();
         vdi.AssembleElementVector(fe, *mesh.GetElementTransformation(0),
                                   f_2, res);
         res -= tmp;
         REQUIRE(res.Norml1() < tol);
      }
      tmp -= b;

      Vector one(2); one = 1.0;
      VectorConstantCoefficient coeff_one(one);
      BilinearForm M(&fespace_h1_2);
      M.AddDomainIntegrator(new VectorMassIntegrator(coeff_one));
      M.Assemble();
      M.Finalize();

      // g = grad(v grad(f)).
      GridFunction g(&fespace_h1_2); g = 0.0;
      CG(M, tmp, g, 0, 200, cg_rtol * cg_rtol, 0.0);

      // Assumes grad^2(f) = 0.
      REQUIRE(g.ComputeL2Error(dvdf_coef_2) < tol);
   }
}

} // namespace bilininteg_1d
