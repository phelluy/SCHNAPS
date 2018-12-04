#include "schnaps.h"
#include <stdio.h>
#include <assert.h>
#include "../test/test.h"
#include "getopt.h"
#include <stdlib.h>     /* atoi */
#include "mhd.h"
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#define _XOPEN_SOURCE 700

//real seconds()
//{
//  struct timespec ts;
//  clock_gettime(CLOCK_MONOTONIC, &ts);
//  return (real)ts.tv_sec + 1e-9 * (real)ts.tv_nsec;
//}
int TestDoubleTearing(int argc, char *argv[]);



int main(int argc, char *argv[]) {
  int resu = TestDoubleTearing(argc,argv);
  if (resu)
    printf("DoubleTearing test OK !\n");
  else 
    printf("DoubleTearing test failed !\n");
  return !resu;
}

int TestDoubleTearing(int argc, char *argv[]) {

  int test = true;

  if(!cldevice_is_acceptable(nplatform_cl, ndevice_cl)) {
    printf("OpenCL device not acceptable.\n");
    return true;
  }

  Simulation simu;
  EmptySimulation(&simu);


  MacroMesh mesh;
  char *mshname =  "../test/testdoubletearinggrid.msh";
  
  ReadMacroMesh(&mesh, mshname);
  Detect2DMacroMesh(&mesh);
  bool is2d=mesh.is2d; 
  assert(is2d);  

  schnaps_real periodsize = 4.0;
  mesh.period[1]=periodsize;

  BuildConnectivity(&mesh);
  int deg[]={1, 1, 0};
  int raf[]={20, 20, 1};
  CheckMacroMesh(&mesh, deg, raf);

  Model model;

  schnaps_real cfl = 0.2;

  simu.cfl = cfl;
  model.m = 9;

  strcpy(model.name,"MHD");

  model.NumFlux=MHDNumFluxRusanov;
  model.BoundaryFlux=MHDBoundaryFluxDoubleTearing;
  model.InitData=MHDInitDataDoubleTearing;
  model.ImposedData=MHDImposedDataDoubleTearing;
  model.Source = NULL;
  
  char buf[1000];
  sprintf(buf, "-D _M=%d -D _PERIODX=%f -D _PERIODY=%f",
          model.m,
          periodsize,
          periodsize);
  strcat(cl_buildoptions, buf);

  sprintf(numflux_cl_name, "%s", "MHDNumFluxRusanov");
  sprintf(buf," -D NUMFLUX=");
  strcat(buf, numflux_cl_name);
  strcat(cl_buildoptions, buf);

  sprintf(buf, " -D BOUNDARYFLUX=%s -cl-fast-relaxed-math", "MHDBoundaryFluxDoubleTearing");
  strcat(cl_buildoptions, buf);



  InitSimulation(&simu, &mesh, deg, raf, &model);
 
  schnaps_real tmax = 1.0;
  simu.vmax = 6.0;
  schnaps_real dt = 0;
  RK4_CL(&simu, tmax, dt,  0, NULL, NULL);
  //RK4(&simu, tmax);
  
  CopyfieldtoCPU(&simu);
 
  show_cl_timing(&simu);
  PlotFields(2, false, &simu, NULL, "dgvisu.msh");

  return test;


}

  /*
  real cfl = 0.1;
  real tmax = 0.1;
  bool writemsh = false;
  real vmax = 6.0;
  bool usegpu = false;
  real dt = 0.0;
  real periodsize = 4.;
  
  for (;;) {
    int cc = getopt(argc, argv, "c:t:w:D:P:g:s:");
    if (cc == -1) break;
    switch (cc) {
    case 0:
      break;
    case 'c':
      cfl = atof(optarg);
      break;
    case 'g':
      usegpu = atoi(optarg);
      break;
    case 't':
      tmax = atof(optarg);
      break;
    case 'w':
      writemsh = true;
      break;
    case 'D':
       ndevice_cl= atoi(optarg);
      break;
    case 'P':
      nplatform_cl = atoi(optarg);
      break;
    default:
      printf("Error: invalid option.\n");
      printf("Usage:\n");
      printf("./testmanyv -c <cfl> -d <deg> -n <nraf> -t <tmax> -C\n -P <cl platform number> -D <cl device number> FIXME");
      exit(1);
    }
  }

  bool test = true;
  field f;
  init_empty_field(&f);  

  f.varindex = GenericVarindex;
  f.model.m = 9;
  f.model.cfl = cfl;

  strcpy(f.model.name,"MHD");

  f.model.NumFlux=MHDNumFluxRusanov;
  f.model.BoundaryFlux=MHDBoundaryFluxDoubleTearing;
  f.model.InitData=MHDInitDataDoubleTearing;
  f.model.ImposedData=MHDImposedDataDoubleTearing;
  
  char buf[1000];
  sprintf(buf, "-D _M=%d -D _PERIODX=%f -D _PERIODY=%f",
          f.model.m,
          periodsize,
          periodsize);
  strcat(cl_buildoptions, buf);

  sprintf(numflux_cl_name, "%s", "MHDNumFluxRusanov");
  sprintf(buf," -D NUMFLUX=");
  strcat(buf, numflux_cl_name);
  strcat(cl_buildoptions, buf);

  sprintf(buf, " -D BOUNDARYFLUX=%s", "MHDBoundaryFluxDoubleTearing");
  strcat(cl_buildoptions, buf);
  
  // Set the global parameters for the Vlasov equation
  f.interp.interp_param[0] = f.model.m; // _M
  f.interp.interp_param[1] = 1; // x direction degree
  f.interp.interp_param[2] = 1; // y direction degree
  f.interp.interp_param[3] = 0; // z direction degree
  f.interp.interp_param[4] = 10; // x direction refinement
  f.interp.interp_param[5] = 10; // y direction refinement
  f.interp.interp_param[6] = 1; // z direction refinement


  //set_vlasov_params(&(f.model));

  // Read the gmsh file
  ReadMacroMesh(&(f.macromesh), "../test/testdoubletearinggrid.msh");
  // Try to detect a 2d mesh
  Detect2DMacroMesh(&(f.macromesh)); 
  bool is2d=f.macromesh.is2d; 
  assert(is2d);  

  f.macromesh.period[1]=periodsize;
  
  // Mesh preparation
  BuildConnectivity(&(f.macromesh));

  // Prepare the initial fields
  Initfield(&f);

  
  // Prudence...
  CheckMacroMesh(&(f.macromesh), f.interp.interp_param + 1);

  //Plotfield(5, false, &f, "By", "dginit.msh");

  f.vmax=vmax;

  real executiontime;
  if(usegpu) {
    printf("Using OpenCL:\n");

    RK4_CL(&f, 0.873, dt, 0, NULL, NULL);
    CopyfieldtoCPU(&f);
    show_cl_timing(&f);
    }
  else { 
    printf("Using C:\n");
    RK4(&f, tmax, dt);
  }

  Plotfield(2,false,&f, "P", "dgvisu.msh");
  //Gnuplot(&f,0,0.0,"data1D.dat");


  return test; */

