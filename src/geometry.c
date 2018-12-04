#include "geometry.h"
#include "global.h"
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <stdbool.h>

const int h20_refnormal[6][3] = {{0, -1, 0},
                                 {1, 0, 0},
                                 {0, 1, 0},
                                 {-1, 0, 0},
                                 {0, 0, 1},
                                 {0, 0, -1}};

// 20 nodes of the reference element
const schnaps_real h20_ref_node[20][3] = {
    {0, 0, 0},
    {1, 0, 0},
    {1, 1, 0},
    {0, 1, 0},
    {0, 0, 1},
    {1, 0, 1},
    {1, 1, 1},
    {0, 1, 1},
    {0.5, 0, 0},
    {0, 0.5, 0},
    {0, 0, 0.5},
    {1, 0.5, 0},
    {1, 0, 0.5},
    {0.5, 1, 0},
    {1, 1, 0.5},
    {0, 1, 0.5},
    {0.5, 0, 1},
    {0, 0.5, 1},
    {1, 0.5, 1},
    {0.5, 1, 1}};

// Return the dot-product of the reals a[3] and b[3]
schnaps_real dot_product(schnaps_real a[3], schnaps_real b[3])
{
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

schnaps_real norm(schnaps_real a[3])
{
  return sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
}

void Normalize(schnaps_real a[3])
{
  schnaps_real r = 1.0 / norm(a);
  a[0] *= r;
  a[1] *= r;
  a[2] *= r;
}

#pragma start_opencl
void PeriodicCorrection(schnaps_real xyz[3], schnaps_real period[3])
{
  for (int dim = 0; dim < 3; ++dim)
  {
    if (period[dim] > 0)
    {
      if (xyz[dim] > period[dim])
      {
        xyz[dim] -= period[dim];
      }
      else if (xyz[dim] < 0)
      {
        xyz[dim] += period[dim];
      }
    }
  }
}
#pragma end_opencl

void PeriodicCorrectionB(schnaps_real xyz[3], schnaps_real period[3], schnaps_real xmin[3], schnaps_real xmax[3])
{
  for (int dim = 0; dim < 3; ++dim)
  {
    if (period[dim] > 0)
    {
      //if (xyz[dim] > period[dim]){
      if (xyz[dim] > xmax[dim])
      {
        xyz[dim] -= period[dim];
      }
      //else if (xyz[dim] < 0){
      else if (xyz[dim] < xmin[dim])
      {
        xyz[dim] += period[dim];
      }
    }
  }
}

schnaps_real Dist(schnaps_real a[3], schnaps_real b[3])
{
  schnaps_real d[3] = {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
  return norm(d);
}

void PrintPoint(schnaps_real x[3])
{
  printf("%f %f %f\n", x[0], x[1], x[2]);
}

/* void GeomRef2Phy(Geom* g)  */
/* { */
/*   Ref2Phy(g->physnode, g->xref, g->dphiref, */
/*           g->ifa, g->xphy, g->dtau, g->codtau, */
/*           g->dphi, g->vnds); */
/*   g->det  */
/*     = g->codtau[0][0] * g->dtau[0][0]  */
/*     + g->codtau[0][1] * g->dtau[0][1] */
/*     + g->codtau[0][2] * g->dtau[0][2]; */
/* } */

void schnaps_ref2phy(schnaps_real physnode[20][3],
                     schnaps_real xref[3],
                     schnaps_real dphiref[3],
                     int ifa,
                     schnaps_real xphy[3],
                     schnaps_real dtau[3][3],
                     schnaps_real codtau[3][3],
                     schnaps_real dphi[3],
                     schnaps_real vnds[3])
{
  // Compute the mapping and its Jacobian
  schnaps_real x = xref[0];
  schnaps_real y = xref[1];
  schnaps_real z = xref[2];

  // Gradient of the shape functions and value (4th component) of the
  // shape functions
  schnaps_real gradphi[20][4];
#include "h20phi.h" // this file fills the values of phi and gradphi

  if (xphy != NULL)
  {
    for (int ii = 0; ii < 3; ++ii)
    {
      xphy[ii] = 0;
      for (int i = 0; i < 20; ++i)
      {
        xphy[ii] += physnode[i][ii] * gradphi[i][3];
      }
    }
  }

  if (dtau != NULL)
  {
    for (int ii = 0; ii < 3; ii++)
    {
      for (int jj = 0; jj < 3; jj++)
      {
        dtau[ii][jj] = 0;
      }
      for (int i = 0; i < 20; i++)
      {
        for (int jj = 0; jj < 3; jj++)
        {
          dtau[ii][jj] += physnode[i][ii] * gradphi[i][jj];
        }
      }
    }
  }

  if (codtau != NULL)
  {
    assert(dtau != NULL);
    /* codtau[0] =  dtau[4] * dtau[8] - dtau[5] * dtau[7]; */
    /* codtau[1] = -dtau[3] * dtau[8] + dtau[5] * dtau[6]; */
    /* codtau[2] =  dtau[3] * dtau[7] - dtau[4] * dtau[6]; */
    /* codtau[3] = -dtau[1] * dtau[8] + dtau[2] * dtau[7]; */
    /* codtau[4] =  dtau[0] * dtau[8] - dtau[2] * dtau[6]; */
    /* codtau[5] = -dtau[0] * dtau[7] + dtau[1] * dtau[6]; */
    /* codtau[6] =  dtau[1] * dtau[5] - dtau[2] * dtau[4]; */
    /* codtau[7] = -dtau[0] * dtau[5] + dtau[2] * dtau[3]; */
    /* codtau[8] =  dtau[0] * dtau[4] - dtau[1] * dtau[3]; */
    codtau[0][0] = dtau[1][1] * dtau[2][2] - dtau[1][2] * dtau[2][1];
    codtau[0][1] = -dtau[1][0] * dtau[2][2] + dtau[1][2] * dtau[2][0];
    codtau[0][2] = dtau[1][0] * dtau[2][1] - dtau[1][1] * dtau[2][0];
    codtau[1][0] = -dtau[0][1] * dtau[2][2] + dtau[0][2] * dtau[2][1];
    codtau[1][1] = dtau[0][0] * dtau[2][2] - dtau[0][2] * dtau[2][0];
    codtau[1][2] = -dtau[0][0] * dtau[2][1] + dtau[0][1] * dtau[2][0];
    codtau[2][0] = dtau[0][1] * dtau[1][2] - dtau[0][2] * dtau[1][1];
    codtau[2][1] = -dtau[0][0] * dtau[1][2] + dtau[0][2] * dtau[1][0];
    codtau[2][2] = dtau[0][0] * dtau[1][1] - dtau[0][1] * dtau[1][0];
  }

  if (dphi != NULL)
  {
    assert(codtau != NULL);
    for (int ii = 0; ii < 3; ii++)
    {
      dphi[ii] = 0;
      for (int jj = 0; jj < 3; jj++)
      {
        dphi[ii] += codtau[ii][jj] * dphiref[jj];
      }
    }
  }

  if (vnds != NULL)
  {
    assert(codtau != NULL);
    assert(ifa >= 0);
    if (ifa >= 0)
    {
      for (int ii = 0; ii < 3; ii++)
      {
        vnds[ii] = 0.0;
        for (int jj = 0; jj < 3; jj++)
        {
          vnds[ii] += codtau[ii][jj] * h20_refnormal[ifa][jj];
        }
      }
    }
  }
}

/* void GeomPhy2Ref(Geom* g)  */
/* { */
/*   Phy2Ref(g->physnode,g->xphy,g->xref); */
/* } */

void schnaps_phy2ref(schnaps_real physnode[20][3], schnaps_real xphy[3], schnaps_real xref[3])
{
#define ITERNEWTON 40

  schnaps_real dtau[3][3], codtau[3][3];
  schnaps_real dxref[3], dxphy[3];
  int ifa = -1;
  xref[0] = 0.5;
  xref[1] = 0.5;
  xref[2] = 0.5;

  schnaps_real *codtau0 = codtau[0];
  schnaps_real *codtau1 = codtau[1];
  schnaps_real *codtau2 = codtau[2];

  for (int iter = 0; iter < ITERNEWTON; ++iter)
  {
    schnaps_ref2phy(physnode, xref, 0, ifa, dxphy, dtau, codtau, 0, 0);
    dxphy[0] -= xphy[0];
    dxphy[1] -= xphy[1];
    dxphy[2] -= xphy[2];
    schnaps_real overdet = 1.0 / dot_product(dtau[0], codtau[0]);
    //assert(overdet > 0);

    for (int ii = 0; ii < 3; ii++)
    {
      dxref[ii] = 0;
      dxref[ii] += codtau0[ii] * dxphy[0] + codtau1[ii] * dxphy[1] + codtau2[ii] * dxphy[2];
      xref[ii] -= dxref[ii] * overdet;
    }
  }

  /* schnaps_real eps = 1e-5;  // may be to constraining... */
  /* assert(xref[0] < 1 + eps && xref[0] > -eps); */
  /* assert(xref[1] < 1 + eps && xref[1] > -eps); */
  /* assert(xref[2] < 1 + eps && xref[2] > -eps); */
}

void RobustPhy2Ref(schnaps_real physnode[20][3], schnaps_real xphy[3], schnaps_real xref[3])
{
#define _ITERNEWTON 5
#define _NTHETA 100

  schnaps_real dtau[3][3], codtau[3][3];
  schnaps_real dxref[3], dxphy[3], xphy0[3];
  int ifa = -1;

  // construct a point xphy0 for which we know the inverse map
  xref[0] = 0.5;
  xref[1] = 0.5;
  xref[2] = 0.5;

  schnaps_ref2phy(physnode, xref, 0, ifa, xphy0, 0, 0, 0, 0);

  // homotopy path
  // theta=0 -> xphy0
  // theta=1 -> xphy
  schnaps_real dtheta = 1. / _NTHETA;

  for (int itheta = 0; itheta <= _NTHETA; itheta++)
  {
    //printf("itheta=%d\n",itheta);
    schnaps_real theta = itheta * dtheta;
    // intermediate point to find
    schnaps_real xphy1[3];
    for (int ii = 0; ii < 3; ii++)
    {
      xphy1[ii] = theta * xphy[ii] + (1 - theta) * xphy0[ii];
    }

    for (int iter = 0; iter < _ITERNEWTON; ++iter)
    {
      schnaps_ref2phy(physnode, xref, 0, ifa, dxphy, dtau, codtau, 0, 0);
      dxphy[0] -= xphy1[0];
      dxphy[1] -= xphy1[1];
      dxphy[2] -= xphy1[2];
      schnaps_real det = dot_product(dtau[0], codtau[0]);
      //assert(det > 0);

      for (int ii = 0; ii < 3; ii++)
      {
        dxref[ii] = 0;
        for (int jj = 0; jj < 3; jj++)
        {
          dxref[ii] += codtau[jj][ii] * dxphy[jj];
        }
        xref[ii] -= dxref[ii] / det;
      }
      //printf("iter= %d dxref=%f %f %f xref=%f %f %f\n",iter,dxref[0],dxref[1],dxref[2],xref[0],xref[1],xref[2]);
    }
    bool is_in_elem = (xref[0] >= 0) && (xref[0] <= 1) && (xref[1] >= 0) && (xref[1] <= 1) && (xref[2] >= 0) && (xref[2] <= 1);
    if (!is_in_elem)
      return;
  }
}
