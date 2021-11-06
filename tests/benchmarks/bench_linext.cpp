// Copyright (c) 2010-2021, Lawrence Livermore National Security, LLC. Produced
// at the Lawrence Livermore National Laboratory. All Rights reserved. See files
// LICENSE and NOTICE for details. LLNL-CODE-806117.
//
// This file is part of the MFEM library. For more information and source code
// availability visit https://mfem.org.
//
// MFEM is free software; you can redistribute it and/or modify it under the
// terms of the BSD-3 license. We welcome feedback and contributions, see file
// CONTRIBUTING.md for details.

#include "bench.hpp"

#define MFEM_DEBUG_COLOR 226
#include "general/debug.hpp"

#ifdef MFEM_USE_BENCHMARK

////////////////////////////////////////////////////////////////////////////////
constexpr int _2D = 2;
constexpr int _3D = 3;
constexpr int _GL = false; // Gauss-Legendre, q=p+2
constexpr int _GLL = true; // Gauss-Legendre-Lobatto, q=p+1

////////////////////////////////////////////////////////////////////////////////
constexpr int VDIM = 24;
constexpr int SEED = 0x100001b3;

////////////////////////////////////////////////////////////////////////////////
/// \brief vdim_vector_function
/// Used for the VectorFunctionCoefficient in the VectorDomainLFIntegrator
/// benchmark.
void vdim_vector_function(const Vector&, Vector &y)
{
   y.SetSize(VDIM);
   y.Randomize(SEED);
}

////////////////////////////////////////////////////////////////////////////////
/// Base class for the LinearForm extension test and the bench
template<int DIM, int VDIM, bool GLL>
struct LinExt
{
   const int problem, N, p, q;
   const bool test;
   const Element::Type type;
   Mesh mesh;
   H1_FECollection fec;
   FiniteElementSpace vfes; // vdim finite element space
   FiniteElementSpace *mfes; // mesh finite elemente space
   QuadratureSpace qspace;
   GridFunction x;
   const Geometry::Type geom_type;
   IntegrationRules IntRulesGLL;
   const IntegrationRule *irGLL;
   const IntegrationRule *ir;
   const IntegrationRule qir;
   const GeometricFactors *geom_factors;
   ConstantCoefficient one;
   const int dofs;
   double mdofs;

   Vector one_vec, dim_vec, vdim_vec;
   ConstantCoefficient constant_coeff;
   VectorConstantCoefficient dim_constant_coeff;
   VectorConstantCoefficient vdim_constant_coeff;
   std::function<void(const Vector&, Vector&)> vector_f{vdim_vector_function};
   VectorFunctionCoefficient vector_function_coeff;
   QuadratureFunction qfunc, vqfunc;
   QuadratureFunctionCoefficient *qfc;
   VectorQuadratureFunctionCoefficient *vqfc;

   LinearForm *lf[2];

   LinExt(int problem, int order, bool test):
      problem(problem),
      N(Device::IsEnabled() ? test?8:32 : test?4:8),
      p(order),
      q(2*p + (GLL?-1:3)),
      test(test),
      type(DIM==3 ? Element::HEXAHEDRON : Element::QUADRILATERAL),
      mesh(DIM==3 ?
           Mesh::MakeCartesian3D(N,N,N,type) : Mesh::MakeCartesian2D(N,N,type)),
      fec(p, DIM),
      vfes(&mesh, &fec, VDIM),
      mfes(new FiniteElementSpace(&mesh, &fec, DIM)),
      qspace(&mesh, q),
      x(mfes),
      geom_type(vfes.GetFE(0)->GetGeomType()),
      IntRulesGLL(0, Quadrature1D::GaussLobatto),
      irGLL(&IntRulesGLL.Get(geom_type, q)), // Gauss-Legendre-Lobatto
      ir(&IntRules.Get(geom_type, q)), // Gauss-Legendre
      qir(GLL?*irGLL:*ir),//qspace.GetElementIntRule(0)),
      geom_factors(mesh.GetGeometricFactors(qir,GeometricFactors::COORDINATES)),
      one(1.0),
      dofs(vfes.GetTrueVSize()),
      mdofs(0.0),
      one_vec(1),
      dim_vec(DIM),
      vdim_vec(VDIM),
      constant_coeff((one_vec.Randomize(SEED),one_vec(0))),
      dim_constant_coeff((dim_vec.Randomize(SEED),dim_vec)),
      vdim_constant_coeff((vdim_vec.Randomize(SEED),vdim_vec)),
      vector_function_coeff(VDIM,vector_f),
      qfunc(&qspace, 1),
      vqfunc(&qspace, VDIM),
      qfc(nullptr),
      vqfc(nullptr),
      lf{new LinearForm(&vfes), new LinearForm(&vfes)}
   {
      MFEM_VERIFY(DIM==2||DIM==3, "Only 2D and 3D tests are supported!");
      SetupRandomMesh();
      //SetupQuadratureFunctions();
      SetupLinearForms();
      lf[0]->SetAssemblyLevel(LinearAssemblyLevel::LEGACY);
      lf[1]->SetAssemblyLevel(LinearAssemblyLevel::FULL);
   }

   void SetupQuadratureFunctions()
   {
      const int nqpts = qir.GetNPoints();
      const int sdim = mesh.SpaceDimension();

      {
         int nelems = qfunc.Size() / qfunc.GetVDim() / nqpts;
         for (int e = 0; e < nelems; e++)
         {
            for (int q = 0; q < nqpts; q++)
            {
               // X has dims NQ x SDIM x NE
               qfunc((e * nqpts) + q) =
                  geom_factors->X((e * nqpts * sdim) + /*(nqpts * 2)*/ + q );
            }
         }
      }

      {
         int nelems = vqfunc.Size() / vqfunc.GetVDim() / nqpts;

         for (int i = 0; i < nelems; i++)
         {
            for (int j = 0; j < sdim; j++)
            {
               for (int k = 0; k < nqpts; k++)
               {
                  // X has dims NQ x SDIM x NE
                  vqfunc((i * nqpts * sdim) + (k * sdim ) + j) =
                     geom_factors->X((i * nqpts * sdim) + (j * nqpts) + k);
               }
            }
         }
      }

      qfc = new QuadratureFunctionCoefficient(qfunc);
      vqfc = new VectorQuadratureFunctionCoefficient(vqfunc);
      vqfc->SetComponent(0,VDIM);
   }

   ~LinExt()
   {
      delete lf[0];
      delete lf[1];
      delete qfc;
      delete vqfc;
   }

   void SetupRandomMesh() noexcept
   {
      mesh.SetNodalFESpace(mfes);
      mesh.SetNodalGridFunction(&x);
      const double jitter = 1./(M_PI*M_PI);
      const double h0 = mesh.GetElementSize(0);
      GridFunction rdm(mfes);
      rdm.Randomize(SEED);
      rdm -= 0.5; // Shift to random values in [-0.5,0.5]
      rdm *= jitter * h0; // Scale the random values to be of same order
      x -= rdm;
   }

   virtual void benchmark() = 0;

   double SumMdofs() const { return mdofs; }

   double MDofs() const { return 1e-6 * dofs; }

   void SetupLinearForms() noexcept
   {
      for (int i=0; i<2; i++)
      {
         if (problem==1) // DomainLFIntegrator
         {
            DomainLFIntegrator *dlfi;
            dlfi = new DomainLFIntegrator(constant_coeff);
            dlfi->SetIntRule(GLL?irGLL:ir);
            lf[i]->AddDomainIntegrator(dlfi);
         }
         else if (problem==2) // VectorDomainLFIntegrator
         {
            VectorDomainLFIntegrator *vdlfi;
            if (test)
            {
               vdlfi = new VectorDomainLFIntegrator(vector_function_coeff);
            }
            else
            {
               vdlfi = new VectorDomainLFIntegrator(vdim_constant_coeff);
            }
            vdlfi->SetIntRule(GLL?irGLL:ir);
            lf[i]->AddDomainIntegrator(vdlfi);
         }
         else if (problem==3) // DomainLFGradIntegrator
         {
            DomainLFGradIntegrator *dlfgi;
            dlfgi = new DomainLFGradIntegrator(dim_constant_coeff);
            dlfgi->SetIntRule(GLL?irGLL:ir);
            lf[i]->AddDomainIntegrator(dlfgi);
         }
         else if (problem==4) // VectorDomainLFGradIntegrator
         {
            LinearFormIntegrator *vdlfgi;
            /*if (test)
            {
               vdlfgi = new VectorQuadratureLFIntegrator(*vqfc, nullptr);
            }
            else*/
            {
               vdlfgi = new VectorDomainLFGradIntegrator(vdim_constant_coeff);
            }
            vdlfgi->SetIntRule(GLL?irGLL:ir);
            lf[i]->AddDomainIntegrator(vdlfgi);
         }
         else { MFEM_ABORT("Problem not specified!"); }
      }
   }
};

////////////////////////////////////////////////////////////////////////////////
/// TEST for LinearFormExtension
template<int dim, int vdim, bool gll>
struct Test: public LinExt<dim,vdim,gll>
{
   using LinExt<dim,vdim,gll>::lf;
   static constexpr bool testing = true;

   Test(int problem, int order):LinExt<dim,vdim,gll>(problem,order,testing) { }

   void benchmark()
   {
      lf[0]->Assemble();
      lf[1]->Assemble();
      const double dtd = (*lf[1]) * (*lf[1]);
      const double rtr = (*lf[0]) * (*lf[0]);
      const bool almost_eq = almost_equal(dtd,rtr);
      if (!almost_eq) { dbg("%.15e %.15e",dtd,rtr); }
      MFEM_VERIFY(almost_eq, "almost_equal test error!");
      MFEM_DEVICE_SYNC;
      this->mdofs += this->MDofs();
   }
};

/// Scalar Linear Form Extension Tests
#define LinExtTest(Problem,Kernel,dim,vdim,gll)\
static void TEST_##Kernel##dim##gll(bm::State &state){\
   const int order = state.range(0);\
   Test<dim,vdim,gll> ker(Problem,order);\
   while(state.KeepRunning()) { ker.benchmark();}\
   state.counters["MDof/s"] = bm::Counter(ker.SumMdofs(),bm::Counter::kIsRate);}\
BENCHMARK(TEST_##Kernel##dim##gll)->DenseRange(1,6)->Unit(bm::kMillisecond);

/// Scalar linear form tests & Gauss-Legendre-Lobatto, q=p+1
LinExtTest(1,DomainLF,_2D,1,_GLL)
LinExtTest(1,DomainLF,_3D,1,_GLL)

/// Vector linear form tests & Gauss-Legendre-Lobatto, q=p+1
LinExtTest(2,VectorDomainLF,_2D,VDIM,_GLL)
LinExtTest(2,VectorDomainLF,_3D,VDIM,_GLL)

/// Grad linear form tests & Gauss-Legendre-Lobatto, q=p+1
LinExtTest(3,DomainLFGrad,_2D,1,_GLL)
LinExtTest(3,DomainLFGrad,_3D,1,_GLL)

/// Vector Grad linear form tests & Gauss-Legendre-Lobatto, q=p+1
LinExtTest(4,VectorDomainLFGrad,_2D,VDIM,_GLL)
LinExtTest(4,VectorDomainLFGrad,_3D,VDIM,_GLL)

/// Scalar linear form tests & Gauss-Legendre, q=p+2
LinExtTest(1,DomainLF,_2D,1,_GL)
LinExtTest(1,DomainLF,_3D,1,_GL)

/// Vector linear form tests & Gauss-Legendre, q=p+2
LinExtTest(2,VectorDomainLF,_2D,VDIM,_GL)
LinExtTest(2,VectorDomainLF,_3D,VDIM,_GL)

/// Grad linear form tests & Gauss-Legendre, q=p+2
LinExtTest(3,DomainLFGrad,_2D,1,_GL)
LinExtTest(3,DomainLFGrad,_3D,1,_GL)

/// Vector Grad linear form tests & Gauss-Legendre, q=p+2
LinExtTest(4,VectorDomainLFGrad,_2D,VDIM,_GL)
LinExtTest(4,VectorDomainLFGrad,_3D,VDIM,_GL)


////////////////////////////////////////////////////////////////////////////////
/// BENCH for LinearFormExtension
template<int dim, int vdim, enum LinearAssemblyLevel lal, bool gll>
struct Bench: public LinExt<dim,vdim,gll>
{
   LinearForm &lf;
   static constexpr bool bench = false;

   Bench(int problem, int order): LinExt<dim,vdim,gll>(problem,order,bench),
      lf(*LinExt<dim,vdim,gll>::lf[lal==LinearAssemblyLevel::LEGACY?0:1]) { }

   void benchmark()
   {
      lf.Assemble();
      MFEM_DEVICE_SYNC;
      this->mdofs += this->MDofs();
   }
};

/// Linear Form Extension Scalar Benchs
#define LinExtBench(Problem,Kernel,lal,dim,vdim,gll)\
static void BENCH_##lal##_##Kernel##dim##gll(bm::State &state){\
   const int order = state.range(0);\
   Bench<dim,vdim,LinearAssemblyLevel::lal,gll> ker(Problem, order);\
   while (state.KeepRunning()) { ker.benchmark(); }\
   state.counters["MDof/s"] = bm::Counter(ker.SumMdofs(), bm::Counter::kIsRate);}\
BENCHMARK(BENCH_##lal##_##Kernel##dim##gll)->DenseRange(1,6)->Unit(bm::kMicrosecond);

/// Scalar linear form bench & Gauss-Legendre-Lobatto, q=p+1
LinExtBench(1,DomainLF,LEGACY,_2D,1,_GLL)
LinExtBench(1,DomainLF,  FULL,_2D,1,_GLL)
LinExtBench(1,DomainLF,LEGACY,_3D,1,_GLL)
LinExtBench(1,DomainLF,  FULL,_3D,1,_GLL)

/// Vector linear form bench & Gauss-Legendre-Lobatto, q=p+1
LinExtBench(2,VectorDomainLF,LEGACY,_2D,VDIM,_GLL)
LinExtBench(2,VectorDomainLF,  FULL,_2D,VDIM,_GLL)
LinExtBench(2,VectorDomainLF,LEGACY,_3D,VDIM,_GLL)
LinExtBench(2,VectorDomainLF,  FULL,_3D,VDIM,_GLL)

/// Grad Scalar linear form bench & Gauss-Legendre-Lobatto, q=p+1
LinExtBench(3,DomainLFGrad,LEGACY,_2D,1,_GLL)
LinExtBench(3,DomainLFGrad,  FULL,_2D,1,_GLL)
LinExtBench(3,DomainLFGrad,LEGACY,_3D,1,_GLL)
LinExtBench(3,DomainLFGrad,  FULL,_3D,1,_GLL)

/// Vector Grad linear form bench & Gauss-Legendre-Lobatto, q=p+1
LinExtBench(4,VectorDomainLFGrad,LEGACY,_2D,VDIM,_GLL)
LinExtBench(4,VectorDomainLFGrad,  FULL,_2D,VDIM,_GLL)
LinExtBench(4,VectorDomainLFGrad,LEGACY,_3D,VDIM,_GLL)
LinExtBench(4,VectorDomainLFGrad,  FULL,_3D,VDIM,_GLL)

/// Scalar linear form bench & Gauss-Legendre, q=p+2
LinExtBench(1,DomainLF,LEGACY,_2D,1,_GL)
LinExtBench(1,DomainLF,  FULL,_2D,1,_GL)
LinExtBench(1,DomainLF,LEGACY,_3D,1,_GL)
LinExtBench(1,DomainLF,  FULL,_3D,1,_GL)

/// Vector linear form bench & Gauss-Legendre, q=p+2
LinExtBench(2,VectorDomainLF,LEGACY,_2D,VDIM,_GL)
LinExtBench(2,VectorDomainLF,  FULL,_2D,VDIM,_GL)
LinExtBench(2,VectorDomainLF,LEGACY,_3D,VDIM,_GL)
LinExtBench(2,VectorDomainLF,  FULL,_3D,VDIM,_GL)

/// Grad Scalar linear form bench & Gauss-Legendre, q=p+2
LinExtBench(3,DomainLFGrad,LEGACY,_2D,1,_GL)
LinExtBench(3,DomainLFGrad,  FULL,_2D,1,_GL)
LinExtBench(3,DomainLFGrad,LEGACY,_3D,1,_GL)
LinExtBench(3,DomainLFGrad,  FULL,_3D,1,_GL)

/// Vector Grad linear form bench & Gauss-Legendre, q=p+2
LinExtBench(4,VectorDomainLFGrad,LEGACY,_2D,VDIM,_GL)
LinExtBench(4,VectorDomainLFGrad,  FULL,_2D,VDIM,_GL)
LinExtBench(4,VectorDomainLFGrad,LEGACY,_3D,VDIM,_GL)
LinExtBench(4,VectorDomainLFGrad,  FULL,_3D,VDIM,_GL)

/** ****************************************************************************
 * @brief main entry point
 * --benchmark_filter=TEST
 * --benchmark_filter=BENCH_FULL
 * --benchmark_context=device=cuda
 */
int main(int argc, char *argv[])
{
   bm::ConsoleReporter CR;
   bm::Initialize(&argc, argv);

   // Device setup, cpu by default
   std::string device_config = "cpu";
   if (bmi::global_context != nullptr)
   {
      const auto device = bmi::global_context->find("device");
      if (device != bmi::global_context->end())
      {
         mfem::out << device->first << " : " << device->second << std::endl;
         device_config = device->second;
      }
   }
   Device device(device_config.c_str());
   device.Print();

   if (bm::ReportUnrecognizedArguments(argc, argv)) { return 1; }
   bm::RunSpecifiedBenchmarks(&CR);
   return 0;
}

#endif // MFEM_USE_BENCHMARK