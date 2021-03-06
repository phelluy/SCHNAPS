#include "schnaps.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "test.h"
#include "collision.h"
#include "waterwave2d.h"
#include "implicit.h"

const  schnaps_real a[6]={1,1,1,1,1,1};
const  schnaps_real b[6]={1,1,1,1,1,1};
const  schnaps_real c[6]={1,1,1,1,1,1};

const schnaps_real TestSteady_Transport_v2[3] = {0.7071067811865476, 0.7071067811865476, 0};

int Test_Local_Implicit(void);


void TestSteady_Transport_NumFlux(schnaps_real *wL, schnaps_real *wR, schnaps_real *vnorm, schnaps_real *flux);
void TestSteady_Transport_ImposedData(const schnaps_real *x, const schnaps_real t, schnaps_real *w);
void TestSteady_Transport_InitData(schnaps_real *x, schnaps_real *w);
void TestSteady_Transport_Source(const schnaps_real *xy, const schnaps_real t, const schnaps_real *w, schnaps_real *S);
void Transport_Upwind_BoundaryFlux(schnaps_real *x, schnaps_real t, schnaps_real *wL, schnaps_real *vnorm,
                              schnaps_real *flux);



void TestSteady_Wave_ImposedData(const schnaps_real *x, const schnaps_real t, schnaps_real *w);
void TestSteady_Wave_InitData(schnaps_real *x, schnaps_real *w);
void TestSteady_Wave_Source(const schnaps_real *xy, const schnaps_real t, const schnaps_real *w, schnaps_real *S);
void Wave_Upwind_BoundaryFlux(schnaps_real *x, schnaps_real t, schnaps_real *wL, schnaps_real *vnorm,
			      schnaps_real *flux);





int main(void) {
  
  // unit tests
    
  int resu = Test_Local_Implicit();
	 
  if (resu) printf("locally implicit  test OK !\n");
  else printf("locally implicit  test failed !\n");

  return !resu;
} 

int Test_Local_Implicit(void) {

  bool test = true;

  MacroMesh mesh;
  ReadMacroMesh(&mesh,"../test/testcube2.msh");
  //ReadMacroMesh(&mesh,"cubegros.msh");
  //ReadMacroMesh(&mesh,"../test/testdisque2d.msh");
  //ReadMacroMesh(&mesh,"../test/testmacromesh.msh");
  Detect2DMacroMesh(&mesh);
  
  schnaps_real A[3][3] = {{_LENGTH_DOMAIN, 0, 0}, {0, _LENGTH_DOMAIN, 0}, {0, 0,1}};
  schnaps_real x0[3] = {0, 0, 0};
  AffineMapMacroMesh(&mesh,A,x0);

  BuildConnectivity(&mesh);

  Model model;

  model.m=1;
  model.NumFlux = TestSteady_Transport_NumFlux;
  model.BoundaryFlux = Transport_Upwind_BoundaryFlux;
  model.InitData = TestSteady_Transport_InitData;
  model.ImposedData = TestSteady_Transport_ImposedData;
  model.Source = TestSteady_Transport_Source;

  /* model.m = 7; */
  /* model.NumFlux = Maxwell2DNumFlux_upwind; */
  /* model.BoundaryFlux = Maxwell2DBoundaryFlux_upwind; */
  /* model.InitData = Maxwell2DInitData; */
  /* model.ImposedData = Maxwell2DImposedData; */
  /* model.Source = Maxwell2DSource; */
  //model.Source = NULL;

  /* model.m=3; */
  /* model.NumFlux=Wave_Upwind_NumFlux; */
  /* model.InitData = TestSteady_Wave_InitData; */
  /* model.ImposedData = TestSteady_Wave_ImposedData; */
  /* model.BoundaryFlux = Wave_Upwind_BoundaryFlux; */
  /* model.Source = TestSteady_Wave_Source; */

  int deg[]={3, 3, 0};
  int raf[]={4, 4, 1};
  
  CheckMacroMesh(&mesh, deg, raf);

  Simulation simu;
  EmptySimulation(&simu);

  InitSimulation(&simu, &mesh, deg, raf, &model);
  //DisplaySimulation(&simu);

  field* fd = simu.fd;

  schnaps_real tmax = 1;
  simu.cfl=0.2;
  simu.vmax= _SPEED_WAVE;
  simu.dt = 0.025;
  simu.dt = 0.1;
  /* InitFieldImplicitSolver(fd); */
  /* AssemblyFieldImplicitSolver(fd, 1, 1); */
  LocalThetaTimeScheme(&simu, tmax, simu.dt);
  schnaps_real dd = L2error(&simu);
  printf("erreur local implicit L2=%.12e\n", dd);
  PlotFields(0, false, &simu, NULL, "dgvisu.msh");
  PlotFields(0, true, &simu, NULL, "dgerror.msh");

  //printf("erreur local implicit L2=%.12e\n", _VERY_SMALL);
  test = test && (dd < 600 * _VERY_SMALL);
  
  return test;
}

void TestSteady_Transport_ImposedData(const schnaps_real *xy, const schnaps_real t, schnaps_real *w) {

  schnaps_real x=xy[0];
  schnaps_real y=xy[1];
  //real xx = x * v2[0] + y * v2[1] - t;
  
  w[0] = x * (1 - x) * y * (1-y) + 1;
  //w[0] = exp(-2 * xx * xx);
  //w[0] = 1;
}

void TestSteady_Transport_Source(const schnaps_real *xy, const schnaps_real t, const schnaps_real *w, schnaps_real *S){

  schnaps_real x=xy[0];
  schnaps_real y=xy[1];


  S[0] = TestSteady_Transport_v2[0] * (1 - 2 * x) * y * (1 - y) +
   TestSteady_Transport_v2[1] * (1 - 2 * y) * x * (1 - x);
  //S[0] = 0;

}

void TestSteady_Transport_InitData(schnaps_real *x, schnaps_real *w) {
  schnaps_real t = 0;
  TestSteady_Transport_ImposedData(x, t, w);
}


void Transport_Upwind_BoundaryFlux(schnaps_real *x, schnaps_real t, schnaps_real *wL, schnaps_real *vnorm,
                                       schnaps_real *flux) {
  schnaps_real wR[3];
  TestSteady_Transport_ImposedData(x , t, wR);
  TestSteady_Transport_NumFlux(wL, wR, vnorm, flux);
}
 
void TestSteady_Transport_NumFlux(schnaps_real *wL, schnaps_real *wR, schnaps_real *vnorm, schnaps_real *flux)
{
  schnaps_real vn 
    = TestSteady_Transport_v2[0] * vnorm[0]
    + TestSteady_Transport_v2[1] * vnorm[1]
    + TestSteady_Transport_v2[2] * vnorm[2];
  schnaps_real vnp = vn > 0 ? vn : 0;
  schnaps_real vnm = vn - vnp;
  flux[0] = vnp * wL[0] + vnm * wR[0];
  /* if (fabs(vnorm[2])>1e-6) { */
  /*   printf("vnds %lf %lf %lf \n", vnorm[0], vnorm[1], vnorm[2]); */
  /* } */
  // verify that 2d computations are actually
  // activated
  //assert(fabs(vnorm[2]) < 1e-8);
}

/* #undef _SPEED_WAVE */
/* #define _SPEED_WAVE 2 */

void TestSteady_Wave_ImposedData(const schnaps_real *xy, const schnaps_real t, schnaps_real *w) {

  schnaps_real x=xy[0];
  schnaps_real y=xy[1];


  w[0] = x*(1-x)*y*(1-y)+1;
  w[1] = 2*x*(1-x)*y*(1-y)+2;
  w[2] = 3*x*(1-x)*y*(1-y)+3;


}


void TestSteady_Wave_Source(const schnaps_real *xy, const schnaps_real t, const schnaps_real *w, schnaps_real *S){
  
  schnaps_real x=xy[0];
  schnaps_real y=xy[1];

  S[0] = 2*(1-2*x)*(y*(1-y))+3*(1-2*y)*(x*(1-x));
  S[1] = (1-2*x)*(y*(1-y));
  S[2] = (1-2*y)*(x*(1-x));

  S[0] *= _SPEED_WAVE;
  S[1] *= _SPEED_WAVE;
  S[2] *= _SPEED_WAVE;

}

void TestSteady_Wave_InitData(schnaps_real *x, schnaps_real *w) {
  schnaps_real t = 0;
  TestSteady_Wave_ImposedData(x, t, w);
}




void Wave_Upwind_BoundaryFlux(schnaps_real *x, schnaps_real t, schnaps_real *wL, schnaps_real *vnorm,
				       schnaps_real *flux) {
  schnaps_real wR[3];
  TestSteady_Wave_ImposedData(x , t, wR);
  Wave_Upwind_NumFlux(wL, wR, vnorm, flux);
}
