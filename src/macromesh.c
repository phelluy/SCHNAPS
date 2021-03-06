#include "macromesh.h"
#ifndef _GNU_SOURCE
#define _GNU_SOURCE  // for avoiding a compiler warning
#endif
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "geometry.h"
#include "interpolation.h"
#include <math.h>

// macro for activating flann (fast finding of neighbours)
//#define _WITH_FLANN
#undef _WITH_FLANN

#ifdef _WITH_FLANN
#include <flann/flann.h>
#endif

void ReadMacroMesh(MacroMesh *m, char *filename)
{

  m->is_read = false;
  m->is_build = false;

  // First, set default values
  m->is2d = false;
  m->is1d = false;
  m->period[0] = -1;
  m->period[1] = -1;
  m->period[2] = -1;

  m->connec_ok = false;

  FILE *f = NULL;
  char *line = NULL;
  size_t linesize = 0;
  size_t ret;

  printf("Read mesh file %s\n", filename);

  f = fopen(filename,"r");
  assert(f != NULL);

  do {
    ret = getline(&line, &linesize, f);
  } while(strcmp(line, "$Nodes\n") != 0);

  // Read the nodes data
  ret = getline(&line, &linesize, f);
  m->nbnodes = atoi(line);
  printf("nbnodes=%d\n", m->nbnodes);

  m->node = malloc(3 * m->nbnodes * sizeof(schnaps_real));
  assert(m->node);

  for(int i = 0; i < m->nbnodes; i++) {
    ret = getdelim(&line, &linesize, (int) ' ',f); // node number
    ret = getdelim(&line, &linesize, (int) ' ',f); // x
    m->node[3 * i + 0] = atof(line);
    ret = getdelim(&line, &linesize, (int) ' ',f); // y
    m->node[3 * i + 1] = atof(line);
    ret = getline(&line, &linesize,f); // z (end of the line)
    m->node[3 * i + 2] = atof(line);
    /* printf("Node %d x=%f y=%f z=%f\n",i, */
    /* 	   m->node[3*i+0], */
    /* 	   m->node[3*i+1], */
    /* 	   m->node[3*i+2]); */
  }
  ret = getline(&line, &linesize, f);
  //printf("%s",line);
  // check that we have reached the end of nodes
  assert(strcmp(line, "$EndNodes\n") == 0);

  // Now read all the elements of the mesh
  do {
    ret = getline(&line, &linesize, f);
  } while(strcmp(line,"$Elements\n") != 0);

  // size of the gmsh elems list
  ret = getline(&line, &linesize, f);
  int nball = atoi(line);
  // allocate to a too big size
  m->elem2node = malloc(20 * sizeof(int) * nball);
  assert(m->elem2node);

  // Now count only the H20 elems (code=17)
  m->nbelems = 0;
  int countnode = 0;
  for(int i = 0; i < nball; i++) {
    ret = getdelim(&line, &linesize, (int) ' ', f); // elem number
    ret = getdelim(&line, &linesize, (int) ' ', f); // elem type
    int elemtype = atoi(line);
    if(elemtype != 17) {
      ret = getline(&line, &linesize, f);
    } else {
      m->nbelems++;
      ret = getdelim(&line, &linesize, (int) ' ', f); //useless code
      ret = getdelim(&line, &linesize, (int) ' ', f); //useless code
      ret = getdelim(&line, &linesize, (int) ' ', f); //useless code
      for(int j = 0; j < 19; j++) {
	ret = getdelim(&line, &linesize,(int) ' ', f);
	//printf("%d ",atoi(line));
	m->elem2node[countnode] = atoi(line) - 1;
	countnode++;
      }
      ret = getline(&line, &linesize, f);
      //printf("%d\n",atoi(line));
      m->elem2node[countnode] = atoi(line) - 1;
      countnode++;
    }
  }
  ret = getline(&line, &linesize, f);
  //printf("%s",line);

  // Check that we have reached the end of nodes
  assert(strcmp(line,"$EndElements\n") == 0);
  printf("nbelems=%d\n", m->nbelems);
  m->elem2node = realloc(m->elem2node, 20 * sizeof(int) * m->nbelems);
  assert(m->elem2node);

  m->is_read = true;

  m->elem2elem = NULL;

  free(line);

}

void AffineMap(schnaps_real *x, schnaps_real A[3][3], schnaps_real x0[3])
{

  schnaps_real newx[3];

  for(int i = 0; i < 3; i++)
    newx[i] = x0[i] + dot_product(A[i], x);
  x[0] = newx[0];
  x[1] = newx[1];
  x[2] = newx[2];
}

void AffineMapMacroMesh(MacroMesh *m, schnaps_real A[3][3], schnaps_real x0[3])
{
  for(int ino = 0; ino < m->nbnodes; ino++) {
    AffineMap(&(m->node[ino * 3]),A,x0);
  }
}

// Display macromesh data on standard output
void PrintMacroMesh(MacroMesh *m) {
  printf("Print macromesh...\n");
  int start = 1;
  printf("nbnodes=%d\n", m->nbnodes);
  for(int i = 0; i < m->nbnodes; i++) {
    printf("node %d x=%f y=%f z=%f\n",
	   i + start,
	   m->node[3 * i + 0],
	   m->node[3 * i + 1],
	   m->node[3 * i + 2]);
  }

  printf("nbelems=%d\n", m->nbelems);
  for(int i = 0; i < m->nbelems; i++) {
    printf("elem %d -> ", i + start);
    for(int j = 0; j < 20; j++) {
      printf("%d ", m->elem2node[20 * i + j] + start);
    }
    printf("\n");
  }

  if(m->elem2elem != 0) {
    for(int i = 0; i < m->nbelems; i++) {
      printf("elem %d neighbours: ",
	     i + start);
      for(int j = 0; j < 6; j++) {
	printf("%d ",
	       m->elem2elem[6 * i + j] + start);
      }
      printf("\n");
    }
  }

  // Output to command-line
  if(m->face2elem !=0) {
    for(int ifa = 0; ifa < m->nbfaces; ifa++) {
      printf("face %d, left: %d loc%d, right: %d loc%d\n",
	     ifa,
             m->face2elem[4 * ifa + 0] + start,
             m->face2elem[4 * ifa + 1] ,
             m->face2elem[4 * ifa + 2] + start,
             m->face2elem[4 * ifa + 3]
             );
    }
  }

  // list of nodes neighbours
  for(int ino=0;ino<m->nbnodes;ino++){
    printf("node %d touches elems ",ino+start);
    int ii=0;
    while(m->node2elem[ii+ m->max_node2elem * ino] != -1){
      printf("%d ",m->node2elem[ii+ m->max_node2elem * ino]+start);
      ii++;
      assert(ii < m->max_node2elem);
    }
    printf("\n");
  }
}

// Fill array of faces of subcells
void build_face(MacroMesh *m, Face4Sort *face)
{
  assert(face != NULL);

  int face2locnode[6][4] = { {0,1,5,4},
			     {1,2,6,5},
			     {2,3,7,6},
			     {0,4,7,3},
			     {5,6,7,4},
			     {0,3,2,1} };


  // Loop over macroelements and their six faces
  for(int ie = 0; ie < m->nbelems; ie++) {
    for(int ifa = 0; ifa < 6; ifa++) {
      Face4Sort *f = face + ifa + 6 * ie;
      for(int ino = 0; ino < 4; ino++) {
	f->node[ino] = m->elem2node[face2locnode[ifa][ino] + 20 * ie];
      }
      f->left = ie;
      f->locfaceleft = ifa;
      //f->right = -1; // FIXME: this is never referenced
      //f->locfaceright = -1; // FIXME: this is never referenced.

      OrderFace4Sort(f);
      /* printf("elem=%d ifa=%d left=%d nodes %d %d %d %d\n",ie,ifa, */
      /* 	     f->left,f->node[0], */
      /* 	     f->node[1],f->node[2],f->node[3]); */
    }
  }

  // Sort the list of faces
  qsort(face, 6 * m->nbelems, sizeof(Face4Sort), CompareFace4Sort);

  // check
  /* for(int ie = 0;ie<m->nbelems;ie++) { */
  /*   for(int ifa = 0;ifa<6;ifa++) { */
  /*     Face4Sort *f=face+ifa+6*ie; */
  /*     printf("left=%d right=%d, nodes %d %d %d %d\n", */
  /* 	     f->left,-100,f->node[0], */
  /* 	     f->node[1],f->node[2],f->node[3]); */
  /*   } */
  /* } */
  /* assert(1==2); */
}

// Find the coordinates of the minimal bounding box for the MacroMesh
void macromesh_bounds(MacroMesh *m, schnaps_real *bounds)
{
  schnaps_real xmin = m->node[0];
  schnaps_real xmax = xmin;
  schnaps_real ymin = m->node[1];
  schnaps_real ymax = ymin;
  schnaps_real zmin = m->node[2];
  schnaps_real zmax = zmin;

  // Loop over all the points in all the subcells of the macrocell
  const int nbelems = m->nbelems;
  for(int i = 0; i < m->nbnodes; i++) {
    schnaps_real x = m->node[3 * i];
    schnaps_real y = m->node[3 * i + 1];
    schnaps_real z = m->node[3 * i + 2];
    //printf("xyz %f %f %f \n",x,y,z);
    if(x < xmin) xmin = x;
    if(x > xmax) xmax = x;
    if(y < ymin) ymin = y;
    if(y > ymax) ymax = y;
    if(z < xmin) zmin = z;
    if(z > zmax) zmax = z;
  }

  bounds[0] = xmin;
  bounds[1] = xmax;

  bounds[2] = ymin;
  bounds[3] = ymax;

  bounds[4] = zmin;
  bounds[5] = zmax;

  m->xmin[0] = xmin;
  m->xmax[0] = xmax;
  m->xmin[1] = ymin;
  m->xmax[1] = ymax;
  m->xmin[2] = zmin;
  m->xmax[2] = zmax;

}

int* build_boundarylist(MacroMesh *m)
{
  int nbound = 0;
  for(int ifa = 0; ifa < m->nbfaces; ++ifa) {
    int ieR = m->face2elem[4 * ifa + 2];
    if(ieR == -1)
      nbound++;
  }
  m->nboundaryfaces = nbound;

  printf("m->nboundaryfaces: %d\n", m->nboundaryfaces);
  if(m->nboundaryfaces > 0) {
    int *bf = malloc(sizeof(int) * m->nboundaryfaces);

    int ibound = 0;
    for(int ifa = 0; ifa < m->nbfaces; ++ifa) {
      int ieR = m->face2elem[4 * ifa + 2];
      if(ieR == -1)
  	bf[ibound++] = ifa;
    }
    return bf;
  } else {
    return NULL;
  }
}

int* build_interfacelist(MacroMesh *m)
{
  int ninter = 0;
  for(int ifa = 0; ifa < m->nbfaces; ++ifa) {
    int ieR = m->face2elem[4 * ifa + 2];
    if(ieR != -1)
      ninter++;
  }
  m->nmacrointerfaces = ninter;

  printf("m->nmacrointerfaces: %d\n", m->nmacrointerfaces);
  if(m->nmacrointerfaces > 0) {
    int *mif = calloc(m->nmacrointerfaces,sizeof(int));

    int iinter = 0;
    for(int ifa = 0; ifa < m->nbfaces; ++ifa) {
      int ieR = m->face2elem[4 * ifa + 2];
      if(ieR != -1)
  	mif[iinter++] = ifa;
    }
    return mif;
  } else {
    return NULL;
  }
}

// Allocate and fill the elem2elem array, which provides macrocell
// interface connectivity.
void build_elem2elem(MacroMesh *m, Face4Sort *face)
{
  // Allocate element connectivity array
  assert(m->elem2elem == NULL);
  m->elem2elem = calloc(6 * m->nbelems,sizeof(int));
  assert(m->elem2elem);

  // Initialize to -1 (value for faces on a boundary)
  for(int i = 0; i < 6 * m->nbelems; i++)
    m->elem2elem[i] = -1;

  // Two successive equal faces correspond to two neighbours in the
  // element list
  m->nbfaces = 0;
  int stop = 6 * m->nbelems;
  for(int ifa = 0; ifa < stop; ifa++) {
    Face4Sort* f1 = face + ifa;
    Face4Sort* f2 = face + ifa + 1;
    if(ifa != (stop - 1) && CompareFace4Sort(f1, f2) == 0) {
      ifa++;
      int ie1 = f1->left;
      int if1 = f1->locfaceleft;
      int ie2 = f2->left;
      int if2 = f2->locfaceleft;
      m->elem2elem[if1 + 6 * ie1] = ie2;
      m->elem2elem[if2 + 6 * ie2] = ie1;
    }
    m->nbfaces++;
  }

  m->face2elem = calloc(4 * m->nbfaces,sizeof(int));
  printf("nfaces=%d\n", m->nbfaces);

  /* m->nbfaces = 0; */
  /* for(int ifa = 0;ifa < stop;ifa++) { */
  /*   Face4Sort* f1=face+ifa; */
  /*   Face4Sort* f2=face+ifa+1; */
  /*   if(ifa!=stop-1 && CompareFace4Sort(f1,f2)= = 0) { */
  /*     ifa++; */
  /*     int ie1=f1->left; */
  /*     int if1=f1->locfaceleft; */
  /*     int ie2=f2->left; */
  /*     int if2=f2->locfaceleft; */
  /*     m->face2elem[4*m->nbfaces+0]=ie1; */
  /*     m->face2elem[4*m->nbfaces+1]=if1; */
  /*     m->face2elem[4*m->nbfaces+2]=ie2; */
  /*     m->face2elem[4*m->nbfaces+3]=if2; */
  /*   } else { */
  /*     int ie1=f1->left; */
  /*     int if1=f1->locfaceleft; */
  /*     m->face2elem[4*m->nbfaces+0]=ie1; */
  /*     m->face2elem[4*m->nbfaces+1]=if1; */
  /*     m->face2elem[4*m->nbfaces+2]=-1; */
  /*     m->face2elem[4*m->nbfaces+3]=-1; */
  /*   } */
  /*   m->nbfaces++; */
  /* } */

  // Loop over the face of the macro elements
  int facecount = 0;
  for(int ie = 0; ie < m->nbelems; ie++) {
    for(int ifa = 0; ifa < 6; ifa++) {

      // Find the element that is connected to face ifa of element ie.
      int ie2 = m->elem2elem[6 * ie + ifa];
      if(ie2 < ie) {
        m->face2elem[4 * facecount + 0] = ie;
        m->face2elem[4 * facecount + 1] = ifa;
        m->face2elem[4 * facecount + 2] = ie2;

        if(ie2 >= 0) {
	  m->face2elem[4 * facecount + 3] = -1;
	  for(int ifa2 = 0; ifa2 < 6; ++ifa2) {
	    if(m->elem2elem[6 * ie2 + ifa2] == ie) {
	      m->face2elem[4 * facecount + 3] = ifa2;
	      break;
	    }
	  }
	  assert(m->face2elem[4 * facecount + 3] != -1);

          /* while(m->elem2elem[6 * ie2 + ifa2] != ie) { */
          /*   ifa2++; /// bug here !!!!!! */
          /*   /\* printf("ie2=%d ifa2=%d iep=%d ie=%d\n",ie2,ifa2, *\/ */
          /*   /\*        m->elem2elem[6*ie2+ifa2],ie); *\/ */
          /*   assert(ifa2 < 6); */
          /*   assert(ie2 >= 0 && ie2 < m->nbelems); */
          /* } */
        } else {
          m->face2elem[4 * facecount + 3] = -1;
        }

        facecount++;
      }
    }
  }
  assert(facecount == m->nbfaces);

  m->boundaryface = build_boundarylist(m);

  m->macrointerface = build_interfacelist(m);
}

// Remove the faces in the third z dimension from the list.
void suppress_zfaces(MacroMesh *m)
{
  printf("Suppress 3d faces...\n");
  int newfacecount = 0;
  for(int ifa = 0; ifa < m->nbfaces; ifa++) {
    if(m->face2elem[4 * ifa + 1] < 4)
      newfacecount++;
  }

  printf("Old num faces=%d, new num faces=%d\n", m->nbfaces, newfacecount);

  int* oldf = m->face2elem;
  m->face2elem = calloc(4 * newfacecount, sizeof(int));

  newfacecount = 0;
  for(int ifa = 0; ifa < m->nbfaces; ifa++) {
    if(oldf[4 * ifa + 1] < 4) {
      m->face2elem[4 * newfacecount + 0] = oldf[4 * ifa + 0];
      m->face2elem[4 * newfacecount + 1] = oldf[4 * ifa + 1];
      m->face2elem[4 * newfacecount + 2] = oldf[4 * ifa + 2];
      m->face2elem[4 * newfacecount + 3] = oldf[4 * ifa + 3];
      newfacecount++;
    }
  }

  m->nbfaces = newfacecount;
  free(oldf);

  if(m->nboundaryfaces > 0)
    free(m->boundaryface);
  m->boundaryface = build_boundarylist(m);

  if(m->nmacrointerfaces > 0)
    free(m->macrointerface);
  m->macrointerface = build_interfacelist(m);
}

// Remove the faces in the y direction from the list.
// must be called after suppressing z faces...
void suppress_yfaces(MacroMesh *m)
{
  printf("Suppress y faces...\n");
  int newfacecount = 0;
  for(int ifa = 0; ifa < m->nbfaces; ifa++) {
    if(m->face2elem[4 * ifa + 1] % 2 != 0) newfacecount++;
  }

  printf("Old num faces=%d, new num faces=%d\n", m->nbfaces, newfacecount);

  int* oldf = m->face2elem;
  m->face2elem = calloc(4 * newfacecount,sizeof(int));

  newfacecount = 0;
  for(int ifa = 0; ifa < m->nbfaces; ifa++) {
    if(oldf[4 * ifa + 1] % 2 != 0) {
      m->face2elem[4 * newfacecount + 0] = oldf[4 * ifa + 0];
      m->face2elem[4 * newfacecount + 1] = oldf[4 * ifa + 1];
      m->face2elem[4 * newfacecount + 2] = oldf[4  *ifa + 2];
      m->face2elem[4 * newfacecount + 3] = oldf[4  *ifa + 3];
      newfacecount++;
    }
  }

  m->nbfaces = newfacecount;
  free(oldf);

  if(m->nboundaryfaces > 0)
    free(m->boundaryface);
  m->boundaryface = build_boundarylist(m);

  if(m->nmacrointerfaces > 0)
    free(m->macrointerface);
  m->macrointerface = build_interfacelist(m);
}

// Remove the doubled faces from the list.
void suppress_double_faces(MacroMesh *m)
{
  printf("Suppress doubled faces...\n");
  int newfacecount = 0;
  for(int ifa = 0; ifa < m->nbfaces; ifa++) {
    if(m->face2elem[4 * ifa + 0]  != -1) newfacecount++;
  }

  printf("Old num faces=%d, new num faces=%d\n", m->nbfaces, newfacecount);

  int* oldf = m->face2elem;
  m->face2elem = calloc(4 * newfacecount, sizeof(int));

  newfacecount = 0;
  for(int ifa = 0; ifa < m->nbfaces; ifa++) {
    if(oldf[4 * ifa + 0] != -1) {
      m->face2elem[4 * newfacecount + 0] = oldf[4 * ifa + 0];
      m->face2elem[4 * newfacecount + 1] = oldf[4 * ifa + 1];
      m->face2elem[4 * newfacecount + 2] = oldf[4  *ifa + 2];
      m->face2elem[4 * newfacecount + 3] = oldf[4  *ifa + 3];
      newfacecount++;
    }
  }

  m->nbfaces=newfacecount;
  free(oldf);

  if(m->nboundaryfaces > 0)
    free(m->boundaryface);
  m->boundaryface = build_boundarylist(m);

  if(m->nmacrointerfaces > 0)
    free(m->macrointerface);
  m->macrointerface = build_interfacelist(m);
}

// Build the node 2 elems connectivity
void build_node2elem(MacroMesh *m)
{
  // first pass: count the maximal number of elems attached to a given
  // node
  {
    int count[m->nbnodes];
    for(int ino = 0; ino < m->nbnodes; ino++){
      count[ino] = 0;
    }
    for(int ie = 0; ie < m->nbelems; ie++) {
      for(int iloc = 0; iloc < 20; iloc++){
	count[m->elem2node[iloc + 20 * ie]]++;
      }
    }
    m->max_node2elem = 0;
    for(int ino = 0; ino < m->nbnodes; ino++){
      m->max_node2elem = m->max_node2elem > count[ino] ?
	m->max_node2elem : count[ino];
    }
  } // end of block: count is deallocated...

  // add a column of -1's for marking the ends of neighbours list
  (m->max_node2elem)++;
  printf("max number of elems touching a node: %d\n", m->max_node2elem - 1);

  m->node2elem = calloc((m->max_node2elem + 1) * m->nbnodes,sizeof(int));
  assert(m->node2elem);

  // fill the array with -1's for marking the end of neighbours
  for(int i = 0; i < m->max_node2elem * m->nbnodes; i++)
    m->node2elem[i] = -1;

  // second pass: fill the neighbours list
  for(int ie = 0; ie < m->nbelems; ie++) {
    for(int iloc = 0; iloc < 20; iloc++){
      int ino = m->elem2node[iloc + 20 * ie];
      int ii = 0;
      while(m->node2elem[ii + m->max_node2elem * ino] != -1)
	ii++;
      assert(ii < m->max_node2elem);
      if (ii < m->max_node2elem - 1)
	m->node2elem[ii + m->max_node2elem * ino] = ie;
    }
  }

  // send to infinity nodes that does not belong to any element
  for(int ino = 0; ino < m->nbnodes; ino++){
    if (m->node2elem[0 + m->max_node2elem * ino] == -1) m->node[0+3*ino]=1e10;
  }

}

// Build other connectivity arrays
void BuildConnectivity(MacroMesh* m)
{
  assert(m->is_read);
  assert(!m->is_build);
  printf("Build connectivity...\n");

  schnaps_real *bounds = calloc(6,sizeof(schnaps_real));
  macromesh_bounds(m, bounds);

  printf("bounds: %.5e, %.5e, %.5e, %.5e, %.5e, %.5e\n",
	 bounds[0], bounds[1], bounds[2], bounds[3], bounds[4], bounds[5]);

  printf("bounds:  %.5e, %.5e, %.5e, %.5e, %.5e, %.5e\n",
	 m->xmin[0],m->xmax[0],
	 m->xmin[1],m->xmax[1],
	 m->xmin[2],m->xmax[2]
	 );
  // Build a list of faces each face is made of four corners of the
  // hexaedron mesh
  //Face4Sort *face = malloc(6 * sizeof(Face4Sort) * m->nbelems);
  Face4Sort *face = calloc(6 *  m->nbelems, sizeof(Face4Sort));
  build_face(m, face);
  build_elem2elem(m, face);
  free(face);

  build_node2elem(m);

  // check
  /* for(int ie = 0;ie<m->nbelems;ie++) { */
  /*   for(int ifa = 0;ifa<6;ifa++) { */
  /*     printf("elem=%d face=%d, voisin=%d\n", */
  /* 	     ie,ifa,m->elem2elem[ifa+6*ie]); */
  /*   } */
  /* } */

  if(m->is2d) suppress_zfaces(m);
  if(m->is1d) {
    suppress_zfaces(m);
    suppress_yfaces(m);
  }


  // update connectivity if the mesh is periodic
  // in some directions

  schnaps_real diag[3][3]={1,0,0,
		   0,1,0,
		   0,0,1};

  for (int ie = 0; ie < m->nbelems; ie++) {
    schnaps_real physnode[20][3];
    for(int inoloc = 0; inoloc < 20; inoloc++) {
      int ino = m->elem2node[20 * ie + inoloc];
      physnode[inoloc][0] = m->node[3 * ino + 0];
      physnode[inoloc][1] = m->node[3 * ino + 1];
      physnode[inoloc][2] = m->node[3 * ino + 2];
    }
    for(int ifa = 0; ifa < 6; ifa++) {
      if (m->elem2elem[6 * ie + ifa] < 0){
	schnaps_real xpgref[3],xpgref_in[3];
	int ipgf=0;
	int deg[3] = {0,0,0};
	int raf[3] = {1,1,1};
	ref_pg_face(deg, raf, ifa, ipgf, xpgref, NULL, xpgref_in);
	schnaps_real dtau[3][3],xpg_in[3];
	schnaps_real codtau[3][3],vnds[3]={0,0,0};
	schnaps_ref2phy(physnode,
		xpgref_in,
		NULL, ifa, // dpsiref,ifa
		xpg_in, dtau,
		codtau, NULL, vnds); // codtau,dpsi,vnds
	Normalize(vnds);
	vnds[0]=fabs(vnds[0]);
	vnds[1]=fabs(vnds[1]);
	vnds[2]=fabs(vnds[2]);
	int dim=0;
	while(dim<3 && Dist(vnds,diag[dim]) > 1e-2) dim++;
	//assert(dim < 3);
	//printf("xpg_in_before=%f\n",xpg_in[dim]);
	if (dim < 3 && m->period[dim]  > 0){
	  //if (xpg_in[dim] > m->period[dim]){
          if (xpg_in[dim] > m->xmax[dim]){
	    xpg_in[dim] -= m->period[dim];
	    //printf("xpg_in_after=%f\n",xpg_in[dim]);
	  }
	  //else if (xpg_in[dim] < 0){
          else if (xpg_in[dim] < m->xmin[dim]){
	    xpg_in[dim] += m->period[dim];
	    //printf("xpg_in_after=%f\n",xpg_in[dim]);
	  }
	  else {
            //printf("xpg_in=%f\n",xpg_in[dim]);
	    assert(1==2);
	  }
	  m->elem2elem[6 * ie + ifa] = NumElemFromPoint(m,xpg_in,NULL);
	  /* printf("ie=%d ifa=%d numelem=%d vnds=%f %f %f xpg_in=%f %f %f \n", */
	  /* 	 ie,ifa,NumElemFromPoint(m,xpg_in,NULL), */
	  /* 	 vnds[0],vnds[1],vnds[2], */
	  /* 	 xpg_in[0],xpg_in[1],xpg_in[2]); */
	}
      }
    }
  }

  // now, update the face2elem connectivity (because elem2elem has changed)
  for(int ifa = 0; ifa < m->nbfaces; ifa++) {
    int ieL = m->face2elem[4 * ifa + 0];
    int locfaL = m->face2elem[4 * ifa + 1];
    int ieR = m->face2elem[4 * ifa + 2];
    int locfaR = m->face2elem[4 * ifa + 3];

    int ieR2=m->elem2elem[6 * ieL + locfaL];

    if (ieR != ieR2){
      assert(ieR == -1);
      int opp[6]={2,3,0,1,5,4};
      if (locfaL == 0 || locfaL == 1 || locfaL == 4) {
	m->face2elem[4 * ifa + 2] = ieR2;
	m->face2elem[4 * ifa + 3] = opp[locfaL];
      } else {
	// mark the face for suppression
	m->face2elem[4 * ifa + 0] = -1;
      }
    }
  }

  suppress_double_faces(m);



  //assert(1==5);
  free(bounds);

  m->connec_ok = true;
  m->is_build = true;

/* #ifdef _PERIOD */
/*   assert(m->is1d); // TODO : generalize to 2D */
/*   assert(m->nbelems==1);  */
/*   // faces 1 and 3 point to the same unique macrocell */
/*   m->elem2elem[1+6*0]=0; */
/*   m->elem2elem[3+6*0]=0; */
/* #endif */

}

void print_vector(igraph_vector_t *v, FILE *f) {
  long int i;
  for (i=0; i<igraph_vector_size(v); i++) {
    fprintf(f, " %d", (int)VECTOR(*v)[i]);
  }
  fprintf(f, "\n");
}

void BuildMacroMeshGraph(MacroMesh *m, schnaps_real vit[], int deg[], int raf[]){

  igraph_t* graph = &(m->connect_graph);

  assert(m->is_build = true);

  m->edge2face = malloc((m->nbfaces + 2) * sizeof(int));
  m->edge_dir = malloc((m->nbfaces + 2) * sizeof(int));

  //igraph_empty(graph, m->nbelems+2, IGRAPH_DIRECTED);
  igraph_empty(graph, m->nbelems+2, IGRAPH_DIRECTED);

  int count_edge = 0;

  for(int ifa = 0; ifa < m->nbfaces; ifa++){
    bool edgeLtoR = false;
    bool edgeRtoL = false;
    bool upwind = false;
    bool downwind = false;

    int ieL = m->face2elem[4 * ifa + 0];
    int locfaL = m->face2elem[4 * ifa + 1];
    int ieR = m->face2elem[4 * ifa + 2];
    int locfaR = m->face2elem[4 * ifa + 3];
    for(int ipgf = 0; ipgf < NPGF(deg, raf, locfaL); ipgf++){
      schnaps_real xpgref[3];
      int ipgL = ref_pg_face(deg, raf, locfaL, ipgf, xpgref, NULL, NULL);
      schnaps_real physnode[20][3];
      for(int inoloc = 0; inoloc < 20; inoloc++) {
	int ino = m->elem2node[20 * ieL + inoloc];
	physnode[inoloc][0] = m->node[3 * ino + 0];
	physnode[inoloc][1] = m->node[3 * ino + 1];
	physnode[inoloc][2] = m->node[3 * ino + 2];
      }
      schnaps_real vnds[3];
      {
	schnaps_real dtau[3][3], codtau[3][3];
	schnaps_ref2phy(physnode,
		xpgref,
		NULL, locfaL, // dpsiref, ifa
		NULL, dtau,
		codtau, NULL, vnds); // codtau, dpsi, vnds
      }
      schnaps_real v_dot_n = vnds[0] * vit[0] + vnds[1] * vit[1] + vnds[2] * vit[2];
      // TODO: normalize n and v
      if (v_dot_n > _SMALL && ieR >= 0) edgeLtoR = true;
      if (v_dot_n < -_SMALL && ieR >= 0) edgeRtoL = true;
      if (v_dot_n > _SMALL && ieR < 0) downwind = true;
      if (v_dot_n < -_SMALL && ieR < 0) upwind = true;
    }

    if (edgeLtoR) {
      igraph_add_edge(graph, ieL, ieR);
      m->edge2face[count_edge] = ifa;
      m->edge_dir[count_edge++] = 0;
    }
    if (edgeRtoL) {
      igraph_add_edge(graph, ieR, ieL);
      m->edge2face[count_edge] = ifa;
      m->edge_dir[count_edge++] = 1;
    }
    if (upwind) {
      igraph_add_edge(graph, m->nbelems, ieL);
      m->edge2face[count_edge] = ifa;
      m->edge_dir[count_edge++] = 1;
    }
    if (downwind) {
      igraph_add_edge(graph, ieL, m->nbelems + 1);
      m->edge2face[count_edge] = ifa;
      m->edge_dir[count_edge++] = 0;
    }

  }


  printf("Found %d edges and igraph says : %d\n", count_edge,
	 igraph_ecount(graph));
  m->edge2face = realloc(m->edge2face, count_edge * sizeof(int));
  m->edge_dir = realloc(m->edge_dir, count_edge * sizeof(int));

  for(int eid = 0; eid < count_edge; eid++){
    int ifa =  m->edge2face[eid];
    int ieL = m->face2elem[4 * ifa + 0];
    int ieR = m->face2elem[4 * ifa + 2];
    /* printf("Edge %d is face %d with ieL=%d ieR=%d dir=%d\n", */
    /* 	   eid,ifa,ieL,ieR,m->edge_dir[eid]);     */
  }

  // topological sorting
  igraph_bool_t is_dag;
  igraph_is_dag(graph, &is_dag);
  igraph_vector_t sorting;
  if (is_dag) {
    printf("The graph is a DAG. Topological sorting...\n");
    igraph_vector_init(&sorting, m->nbelems + 2);
    igraph_topological_sorting(graph, &sorting,
                  IGRAPH_OUT);
    print_vector(&sorting, stdout);
  }

  m->topo_order = malloc((m->nbelems + 2) * sizeof(int));

  for(int nid = 0; nid < m->nbelems + 2; nid++){
    m->topo_order[nid] = (int)VECTOR(sorting)[nid];
  }

  igraph_vector_destroy(&sorting);

}



// Compare two integers
int CompareInt(const void* a, const void* b) {
  return(*(int*)a - *(int*)b);
}

// Sort the nodes list of the face
void OrderFace4Sort(Face4Sort* f) {
  qsort(f->node, 4, sizeof(int), CompareInt);
}

// Compare two ordered four-corner faces lexicographical order
int CompareFace4Sort(const void* a,const void* b) {
  Face4Sort *f1 = (Face4Sort*)a;
  Face4Sort *f2 = (Face4Sort*)b;

  int r = f1->node[0]-f2->node[0];
  if(r == 0)
    r = f1->node[1] - f2->node[1];
  if(r == 0)
    r = f1->node[2] - f2->node[2];
  if(r == 0)
    r = f1->node[3] - f2->node[3];
  return r;
};

void CheckMacroMesh(MacroMesh *m, int *deg, int *raf) {
  Geom g;
  g.nbrefnodes = 20;

  schnaps_real physnode[g.nbrefnodes][3];

  schnaps_real face_centers[6][3]={ {0.5,0.0,0.5},
			    {1.0,0.5,0.5},
			    {0.5,1.0,0.5},
			    {0.0,0.5,0.5},
			    {0.5,0.5,1.0},
			    {0.5,0.5,0.0} };

  //real *bounds = malloc(6 * sizeof(real));
  //macromesh_bounds(m, bounds);

  /* real refnormal[6][3]={{0,-1,0},{1,0,0}, */
  /* 			  {0,1,0},{-1,0,0}, */
  /* 			  {0,0,1},{0,0,-1}}; */

  assert(m->connec_ok);

  for(int ie = 0; ie < m->nbelems; ie++) {
    // Load geometry for macro element ie:
    for(int inoloc = 0; inoloc < g.nbrefnodes; inoloc++) {
      int ino = m->elem2node[g.nbrefnodes * ie + inoloc];
      physnode[inoloc][0] = m->node[3 * ino + 0];
      physnode[inoloc][1] = m->node[3 * ino + 1];
      physnode[inoloc][2] = m->node[3 * ino + 2];
    }

    // Test that the ref_ipg function is compatible with ref_pg_vol
    //int param[7]={_DEGX,_DEGY,_DEGZ,_RAFX,_RAFY,_RAFZ,0};
    for(int ipg = 0; ipg < NPG(deg,raf); ipg++) {
      schnaps_real xref1[3], xref_in[3];
      schnaps_real wpg;
      ref_pg_vol(deg, raf, ipg, xref1, &wpg, xref_in);
      //memcpy(g.xref, xref1, sizeof(g.xref));

      /* g.ifa = 0; */
      /* GeomRef2Phy(&g); */
      /* GeomPhy2Ref(&g); */

      // if(param[4]==1 && param[5]==1 && param[6]==1) {
      //printf("ipg %d ipg2 %d xref %f %f %f\n",ipg,
      //	     ref_ipg(param,xref_in),xref_in[0],xref_in[1],xref_in[2]);

      // Ensure that the physical coordinates give the same point:
      assert(ipg == ref_ipg(deg, raf, xref_in));
      //}
    }

    // middle of the element
    schnaps_real xref[3];

    xref[0] = 0.5;
    xref[1] = 0.5;
    xref[2] = 0.5;

    schnaps_real xphym[3];
    schnaps_ref2phy(physnode, xref, NULL, 0, xphym, NULL, NULL, NULL, NULL);
    /* GeomRef2Phy(&g); */
    /* memcpy(xphym, g.xphy, sizeof(xphym)); */

    for(int ifa = 0; ifa < 6; ifa++) {
      // Middle of the face
      /* memcpy(g.xref, face_centers[ifa], sizeof(g.xref)); */
      /* g.ifa = ifa; */
      /* GeomRef2Phy(&g); */

      schnaps_real xphy[3];
      schnaps_real dtau[3][3], codtau[3][3], vnds[3];
      schnaps_ref2phy(physnode, face_centers[ifa], NULL, ifa, xphy, dtau, codtau, NULL, vnds);
      schnaps_real det = dot_product(dtau[0], codtau[0]);

      // Check volume  orientation
      assert(det > 0);

      schnaps_real vec[3] = {xphy[0] - xphym[0],
		     xphy[1] - xphym[1],
		     xphy[2] - xphym[2]};

      // Check face orientation
      assert(0 < dot_product(vnds, vec));

      // Check compatibility between face and volume numbering
      for(int ipgf = 0; ipgf < NPGF(deg, raf, ifa); ipgf++) {

        // Get the coordinates of the Gauss point
        schnaps_real xpgref[3];

	  schnaps_real wpg;

	// Recover the volume gauss point from the face index
	  int ipgv = ref_pg_face(deg, raf, ifa, ipgf, xpgref, &wpg, NULL);


	// Recover the volume gauss point from the face index
	schnaps_real xpgref2[3];
	{
	  schnaps_real wpg2;
	  ref_pg_vol(deg, raf, ipgv, xpgref2, &wpg2, NULL);
	}

        if(m->is2d) { // in 2D do not check upper and lower face
          if(ifa < 4)
          assert(Dist(xpgref, xpgref2) < _SMALL);
        }
	else if (m->is1d){
	  if (ifa==1 || ifa==3) {
	    assert(Dist(xpgref,xpgref2)< _SMALL);
	  }
	}
	// in 3D check all faces
	else { // in 3D check all faces
	  if(Dist(xpgref, xpgref2) >= _SMALL) {
	    printf("ERROR: face and vol indices give different rev points:\n");
	    printf("ipgv: %d\n", ipgv);
	    printf("ipgf: %d\n", ipgf);
	    printf("ifa: %d\n", ifa);
	    printf("xpgref:%f %f %f\n", xpgref[0], xpgref[1], xpgref[2]);
	    printf("xpgref2:%f %f %f\n", xpgref2[0], xpgref2[1], xpgref2[2]);
	  }
          assert(Dist(xpgref, xpgref2) < _SMALL);
        }

      }
    }
  }

  // Check that the faces are defined by the same mapping with
  // opposite normals
  printf("checking macro elem");
  for (int ie = 0; ie < m->nbelems; ie++) {
    printf(" %d",ie);
    // Get the geometry for the macro element ie
    schnaps_real physnode[20][3];
    for(int inoloc = 0; inoloc < 20; inoloc++) {
      int ino = m->elem2node[20 * ie + inoloc];
      physnode[inoloc][0] = m->node[3 * ino + 0];
      physnode[inoloc][1] = m->node[3 * ino + 1];
      physnode[inoloc][2] = m->node[3 * ino + 2];
    }

    // Loop on the 6 faces
    for(int ifa = 0; ifa < 6; ifa++) {
      //printf("checking face %d...\n",ifa);
     // Loop on the glops (numerical integration) of the face ifa
      for(int ipgf = 0; ipgf < NPGF(deg, raf, ifa); ipgf++) {

	// Get the right elem or the boundary id
	int ieR = m->elem2elem[6 * ie + ifa];
	// If the right element exists and is not
	// the left element (may arrive in periodic cases)
  	if(ieR >= 0 && ieR != ie) {
	  // Get the coordinates of the Gauss point from the
	  // face-local point index and the point slightly inside the
	  // macrocell.
	  schnaps_real xpgref[3], xpgref_in[3];
	  int ipg = ref_pg_face(deg, raf, ifa, ipgf, xpgref, NULL, xpgref_in);

	  // Compute the position of the point and the face normal.
	  schnaps_real xpg[3], vnds[3];
	  {
	    schnaps_real dtau[3][3];
	    schnaps_real codtau[3][3];
	    schnaps_ref2phy(physnode,
		    xpgref,
		    NULL, ifa, // dpsiref,ifa
		    xpg, dtau,
		    codtau, NULL, vnds); // codtau,dpsi,vnds
	  }

	  // Compute the "slightly inside" position
	  schnaps_real xpg_in[3];
	  schnaps_ref2phy(physnode,
		  xpgref_in,
		  NULL, ifa, // dpsiref,ifa
		  xpg_in, NULL,
		  NULL, NULL, NULL); // codtau,dpsi,vnds
	  PeriodicCorrection(xpg_in,m->period);

	  // Load the geometry of the right macrocell
	  schnaps_real physnodeR[20][3];
	  for(int inoloc = 0; inoloc < 20; inoloc++) {
	    int ino = m->elem2node[20 * ieR + inoloc];
	    physnodeR[inoloc][0] = m->node[3 * ino + 0];
	    physnodeR[inoloc][1] = m->node[3 * ino + 1];
	    physnodeR[inoloc][2] = m->node[3 * ino + 2];
	  }

  	  // Find the corresponding point in the right elem
  	  schnaps_real xpgrefR_in[3];//,xpgrefR[3];
	  schnaps_phy2ref(physnodeR, xpg_in, xpgrefR_in);
	  //Phy2Ref(physnodeR, xpg, xpgrefR);
	  int ipgR = ref_ipg(deg, raf, xpgrefR_in);
	  // search the id of the face in the right elem
	  // special treatment if the mesh is periodic
	  // and contains only one elem (then ie==ieR)
	  int neighb_count=0;
	  for(int ifaR=0;ifaR<6;ifaR++){
	    if (m->elem2elem[6*ieR+ifaR] == ie) {
	      for(int ipgfR = 0; ipgfR < NPGF(deg, raf, ifaR); ipgfR++) {
		schnaps_real xpgrefR[3];
		int numvol = ref_pg_face(deg, raf, ifaR, ipgfR,
					 xpgrefR, NULL, NULL);
		if (numvol == ipgR){
		  schnaps_real xpgR[3];
		  schnaps_real vndsR[3];
		  {
		    ref_pg_vol(deg, raf, ipgR, xpgrefR, NULL, NULL);
		    schnaps_real dtauR[3][3], codtauR[3][3];
		    schnaps_ref2phy(physnodeR,
			    xpgrefR,
			    NULL, ifaR, // dphiref, ifa
			    xpgR, dtauR,
			    codtauR, NULL, vndsR); // codtau, dphi, vnds
		  }
		  // Ensure that the normals are opposite
		  // if xpg and xpgR are close
		  /* printf("xpg:%f %f %f\n", xpg_in[0], xpg_in[1], xpg_in[2]); */
		  /* printf("vnds: %f %f %f vndsR: %f %f %f \n", */
		  /* 	 vnds[0],vnds[1],vnds[2], */
		  /* 	 vndsR[0],vndsR[1],vndsR[2]); */
		  /* printf("xpgR:%f %f %f\n", xpgR[0], xpgR[1], xpgR[2]); */
		  assert(fabs(vnds[0] + vndsR[0]) < _SMALL*10);
		  assert(fabs(vnds[1] + vndsR[1]) < _SMALL*10);
		  assert(fabs(vnds[2] + vndsR[2]) < _SMALL*10);
		  neighb_count++;
		}

	      }
	    }
	  }
	  //printf("neighb=%d\n",neighb_count);
	  assert(neighb_count == 1);
	}
      }
    }
  }
  printf("\n");
  //free(bounds);
}

// Detect if the mesh is 2D and then permut the nodes so that the z
// direction coincides in the reference or physical frame
void Detect2DMacroMesh(MacroMesh *m)
{
  m->is2d = true;

  // Do not permut the node if the connectivity is already built
  if(m->elem2elem != NULL)
    printf("Cannot permute nodes before building connectivity\n");
  assert(m->elem2elem == 0);

  for(int ie = 0; ie < m->nbelems; ie++) {
    // get the physical nodes of element ie
    schnaps_real physnode[20][3];
    for(int inoloc = 0; inoloc < 20; inoloc++) {
      int ino = m->elem2node[20 * ie + inoloc];
      physnode[inoloc][0] = m->node[3 * ino + 0];
      physnode[inoloc][1] = m->node[3 * ino + 1];
      physnode[inoloc][2] = m->node[3 * ino + 2];
    }

    // We decide that the mesh is 2D if the middles of the elements
    // have a constant z coordinate equal to 0.5
    schnaps_real zmil = 0;
    for(int inoloc = 0; inoloc < 20; inoloc++) {
      zmil += physnode[inoloc][2];
    }
    zmil /= 20;
    //printf("zmil: %f\n", zmil);

    if(fabs(zmil-0.5) > _SMALL * 10) {
      // The mesh is not 2d
      m->is2d = false;
      return;
    }
  }

  // TODO: if the mesh is not 2D, then assert constraints on nraf[2]
  // and deg[2].

  printf("Detection of a 2D mesh\n");
  for(int ie = 0; ie < m->nbelems; ie++) {
    // get the physical nodes of element ie
    schnaps_real physnode[20][3];
    for(int inoloc = 0; inoloc < 20; inoloc++) {
      int ino=m->elem2node[20 * ie + inoloc];
      physnode[inoloc][0] = m->node[3 * ino + 0];
      physnode[inoloc][1] = m->node[3 * ino + 1];
      physnode[inoloc][2] = m->node[3 * ino + 2];
    }
    // If the mesh is 2d permut the nodes in order that the z^ and z
    // axis are the same

    schnaps_real face_centers[6][3] = { {0.5, 0.0, 0.5},
				{1.0, 0.5, 0.5},
				{0.5, 1.0, 0.5},
				{0.0, 0.5, 0.5},
				{0.5, 0.5, 1.0},
				{0.5, 0.5, 0.0} };

    // Rotation of the cube around the origin at most two rotations
    // are needed to put the cube in a correct position
    for(int irot = 0; irot < 2; irot++) {
      // compute the normal to face 4
      schnaps_real vnds[3], dtau[3][3], codtau[3][3];
      schnaps_ref2phy(physnode,
	      face_centers[4],
	      NULL, 4, // dphiref,ifa
	      NULL, dtau,
	      codtau, NULL, vnds); // codtau,dphi,vnds

      schnaps_real d = norm(vnds);
      // If the normal is not up or down we have to permut the nodes
      if(fabs(vnds[2] / d) < 0.9) {
	printf("irot=%d rotating the element %d\n", irot, ie);
	int oldnum[20];
	int newnum[20] = {1, 5, 6, 2, 4, 8, 7, 3, 11, 9,
			  10, 17, 18, 13, 19, 12, 16, 14, 20, 15};
	for(int inoloc = 0; inoloc < 20; inoloc++) {
	  newnum[inoloc]--;
	  oldnum[inoloc] = m->elem2node[20 * ie + inoloc];
	}
	// Rotate the node numbering
	for(int inoloc = 0; inoloc < 20; inoloc++) {
	  m->elem2node[20 * ie + inoloc] = oldnum[newnum[inoloc]];
	}
	// Get the rotated node coordinates
	for(int inoloc = 0; inoloc < 20; inoloc++) {
	  int ino = m->elem2node[20 * ie + inoloc];
	  physnode[inoloc][0] = m->node[3 * ino + 0];
	  physnode[inoloc][1] = m->node[3 * ino + 1];
	  physnode[inoloc][2] = m->node[3 * ino + 2];
	}

      }
    }

  }

}

bool IsInElem(MacroMesh *m,int ie, schnaps_real* xphy, schnaps_real* xref0)
{
  schnaps_real physnode[20][3];
  for(int inoloc = 0; inoloc < 20; inoloc++) {
    int ino = m->elem2node[20 * ie + inoloc];
    physnode[inoloc][0] = m->node[3 * ino + 0];
    physnode[inoloc][1] = m->node[3 * ino + 1];
    physnode[inoloc][2] = m->node[3 * ino + 2];
  }

  schnaps_real xref[3];

  RobustPhy2Ref(physnode,xphy,xref);

  bool is_in_elem = (xref[0] >=0) && (xref[0]<= 1)
    && (xref[1] >=0) && (xref[1]<= 1)
    && (xref[2] >=0) && (xref[2]<= 1);

  if (xref0 != NULL){
    xref0[0]=xref[0];
    xref0[1]=xref[1];
    xref0[2]=xref[2];
  }

  return is_in_elem;

}

int NumElemFromPoint(MacroMesh *m, schnaps_real *xphy, schnaps_real *xref0)
{
  int num = -1;
  int ino = NearestNode(m, xphy);
  schnaps_real xref[3];

  // TO DO: remove nodes that do not belong to any element
  assert(m->node2elem[0 + m->max_node2elem * ino] != -1);

  int ii = 0;
  while(m->node2elem[ii + m->max_node2elem * ino] != -1) {
    int ie = m->node2elem[ii + m->max_node2elem * ino];
    if(IsInElem(m, ie, xphy, xref)) {
      if(xref0 != NULL){
	xref0[0] = xref[0];
	xref0[1] = xref[1];
	xref0[2] = xref[2];
      }
      return ie;
    }
    ii++;
  }

  return num;
}

int NearestNode(MacroMesh *m, schnaps_real *xphy) {
  int nearest = -1;

#ifdef _WITH_FLANN
  // Use of flann library: faster  ???
  static bool is_ready = false;
  static struct FLANNParameters p;

  static float speedup;
  static flann_index_t findex;

  // at first call: construct the index
  // TO DO free the index when finished
  if (!is_ready) {
    printf("Using flann: build search index...\n");
    p = DEFAULT_FLANN_PARAMETERS;
    p.algorithm = FLANN_INDEX_AUTOTUNED;
    p.target_precision = 0.9; /* want 90% target precision */

#ifdef _DOUBLE_PRECISION
      findex = flann_build_index_double(m->node, m->nbnodes, 3, &speedup, &p);
#else
      findex = flann_build_index_float(m->node, m->nbnodes, 3, &speedup, &p);
#endif
    is_ready = true;
  }

  // number of nearest neighbors to search
  int nn = 1;
  int result[nn];
  schnaps_real dists[nn];

  // compute the nn nearest-neighbors of each point in xphy
  // with index construction
  // flann_find_nearest_neighbors_real(m->node,     // nodes list
  // 				      m->nbnodes,  // number of nodes
  // 				      3,           // space dim
  // 				      xphy,
  // 				      1,           // number of points in xphy
  // 				      result,      // nearest points indices
  // 				      dists,       // distances
  // 				      nn,
  // 				      &p);         // flan struct


#if real == double
  flann_find_nearest_neighbors_index_double(findex,// index
					    xphy,
					    1,      // number of points in xphy
					    result, // nearest points indices
					    dists,  // distances
					    nn,
					    &p);     // flan struct
#else
  flann_find_nearest_neighbors_index_float(findex,// index
					   xphy,
					   1,      // number of points in xphy
					   result, // nearest points indices
					   dists,  // distances
					   nn,
					   &p);     // flan struct
#endif

  nearest = result[0];
  // printf("xphy=%f %f %f nearest=%d %f %f %f \n",
  // 	 xphy[0],xphy[1],xphy[2],nearest+1,
  // 	 m->node[0+nearest*3],m->node[1+nearest*3],m->node[2+nearest*3]);

#else // Do not use FLANN library.

  // slow version: loops on all the points
  schnaps_real d = 1e20;

  for(int ino = 0; ino < m->nbnodes; ino++){
    schnaps_real d2 = Dist(xphy, m->node + 3 * ino);
    if (d2 < d) {
      nearest = ino;
      d = d2;
    }
  }
#endif

  return nearest;
}

// Detect if the mesh is 1D and then permut the nodes so that the y,z
// direction coincides in the reference or physical frame
void Detect1DMacroMesh(MacroMesh* m){
  m->is1d = true;

  // do not permut the node if the connectivity
  // is already built
  if (m->elem2elem != NULL)
    printf("Cannot permut nodes before building connectivity\n");
  assert(m->elem2elem == 0);

  for(int ie = 0; ie < m->nbelems; ie++) {
    // get the physical nodes of element ie
    schnaps_real physnode[20][3];
    for(int inoloc = 0; inoloc < 20; inoloc++){
      int ino = m->elem2node[20 * ie + inoloc];
      physnode[inoloc][0] = m->node[3 * ino + 0];
      physnode[inoloc][1] = m->node[3 * ino + 1];
      physnode[inoloc][2] = m->node[3 * ino + 2];
    }

    // we decide that the mesh is 1D if the
    // middles of the elements have a constant y,z
    // coordinate equal to 0.5
    schnaps_real zmil = 0;
    schnaps_real ymil = 0;
    for(int inoloc = 0; inoloc < 20; inoloc++){
      zmil += physnode[inoloc][2];
      ymil += physnode[inoloc][1];
    }
    zmil /= 20;
    ymil /= 20;
    // the mesh is not 1d
    if (fabs(zmil-0.5) > _SMALL * 10 || fabs(ymil-0.5)> _SMALL * 10) {
      printf("The mesh is not 1D zmil=%f ymil=%f\n",zmil,ymil);
      m->is1d=false;
      return;
    }
  }

  printf("Detection of a 1D mesh\n");

  printf("Check now hexahedrons orientation\n");
  for(int ie = 0; ie < m->nbelems; ++ie){
    // get the physical nodes of element ie
    schnaps_real physnode[20][3];
    for(int inoloc = 0; inoloc < 20; inoloc++){
      int ino = m->elem2node[20 * ie + inoloc];
      physnode[inoloc][0] = m->node[3 * ino + 0];
      physnode[inoloc][1] = m->node[3 * ino + 1];
      physnode[inoloc][2] = m->node[3 * ino + 2];
    }

    // face centers coordinates in the ref frame
    schnaps_real face_centers[6][3]={
      {0.5,0.0,0.5},
      {1.0,0.5,0.5},
      {0.5,1.0,0.5},
      {0.0,0.5,0.5},
      {0.5,0.5,1.0},
      {0.5,0.5,0.0},
    };

    // compute the normal to face 1
    schnaps_real vnds[3], dtau[3][3], codtau[3][3];
    schnaps_ref2phy(physnode,
	    face_centers[1],
	    NULL, 1, // dphiref,ifa
	    NULL, dtau,
	    codtau, NULL, vnds); // codtau,dphi,vnds

    schnaps_real d = sqrt((vnds[0] - 1) * (vnds[0] - 1)
		+ vnds[1] * vnds[1]
		+ vnds[2] * vnds[2]);

    // if the mesh is not 1D exit
    assert(d < _SMALL * 10);
  }
}




void FreeMacroMesh(MacroMesh *m){

  if(m->boundaryface !=NULL){
    free(m->boundaryface);
  }
  if(m->macrointerface != NULL){
    free(m->macrointerface);
  }

  if(m->elem2node !=NULL){
    free(m->elem2node);
  }

  if(m->elem2elem !=NULL){
  free(m->elem2elem);
  }

  if(m->face2elem !=NULL){
    free(m->face2elem);
  }

  if(m->node2elem !=NULL){
    free(m->node2elem);
  }

  if(m->node !=NULL){
    free(m->node);
  }



}
