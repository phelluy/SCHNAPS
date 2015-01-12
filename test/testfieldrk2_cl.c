#include "test.h"
#include "schnaps.h"
#include<stdio.h>
#include <assert.h>
#include <math.h>

int main(void) {
  // Unit tests
  int resu = TestFieldRK2_CL();
  if(resu) 
    printf("Field RK2_CL test OK !\n");
  else 
    printf("Field RK2_CL test failed !\n");
  return !resu;
} 

int TestFieldRK2_CL(void){
  int test = true;

  Field f;
  f.model.cfl = 0.05;
  f.model.m = 1; // only one conservative variable
  f.model.NumFlux = TransNumFlux;
  f.model.BoundaryFlux = TestTransBoundaryFlux;
  f.model.InitData = TestTransInitData;
  f.model.ImposedData = TestTransImposedData;
  f.varindex = GenericVarindex;

  f.interp.interp_param[0] = 1; // _M
  f.interp.interp_param[1] = 3; // x direction degree
  f.interp.interp_param[2] = 3; // y direction degree
  f.interp.interp_param[3] = 3; // z direction degree
  f.interp.interp_param[4] = 1; // x direction refinement
  f.interp.interp_param[5] = 1; // y direction refinement
  f.interp.interp_param[6] = 1; // z direction refinement

  ReadMacroMesh(&(f.macromesh), "test/testdisque.msh");
  BuildConnectivity(&(f.macromesh));

  //AffineMapMacroMesh(&(f.macromesh));
  InitField(&f);

  CheckMacroMesh(&(f.macromesh), f.interp.interp_param + 1);
 
  printf("cfl param =%f\n", f.hmin);
  double tmax = 0.1;

  RK2_CL(&f, tmax);
 
  PlotField(0, false, &f, NULL, "dgvisu.msh");
  PlotField(0, true , &f, "error", "dgerror.msh");

  double dd = L2error(&f);

  printf("erreur L2=%f\n", dd);

  test = test && (dd < 0.0009);
  
  return test;
};



