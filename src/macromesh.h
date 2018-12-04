#ifndef _MACROMESH_H
#define _MACROMESH_H

#include <stdbool.h>
#include "global.h"


#include <igraph.h>


//! \brief structure for managing the mesh obtained from gmsh.
//! It is called a macromesh, because it will be refined
//! afterwards.
typedef struct MacroMesh{
  int nbelems; //!< number of macro elems
  int nbnodes; //!< number of nodes in the macromesh
  int nbfaces; //!< number of macrofaces
  int nboundaryfaces; //!< number of macrocell faces which are boundaries
  int nmacrointerfaces; //!< number of macrocell-to-macrocell interfaces
  int *boundaryface; //!< list of boundary faces
  int *macrointerface; //<! List of macrocell interfaces

  bool is_read;
  bool is_build;

  // connectivity
  int *elem2node; //!< elems to nodes connectivity (20 nodes/elem)
  int *elem2elem; //!< elems to elems connectivity (along 6 faces)
  int *face2elem; //!< faces to elems connectivity (Left and Right)

  //! record of the graph edges -> faces association
  int *edge2face;
  //! edge order:"0"= L to R ; "1" = R to L
  int* edge_dir;

  //! topological order of the macrocells
  int* topo_order;

  //! graph data for upwind implicit resolution
  igraph_t connect_graph;

  //! max numbers of elems that touch a node +1
  int max_node2elem;
  //! nodes to elems connectivity (size = max_node2elem * nbelems)
  //! the list for a given node ends with -1's (it explains the +1)
  int* node2elem;

  schnaps_real *node; //!< nodes coordinates array

  //! Activate or not 2D computations
  bool is2d;
  //! Activate or not 1D computations
  bool is1d;


  //! a value for checking that connectivity is finished
  bool connec_ok;


  // mesh boundaries
  schnaps_real xmin[3],xmax[3];

  //! period in each direction
  //! if negative: non-periodic computation (default)
  schnaps_real period[3];

} MacroMesh;

//! \brief a simple struct for modelling a four
//! corner face with a left and a right element.
//! Used only by the connectivity builder.
typedef struct Face4Sort{
  //! 4 nodes
  int node[4];
  //! left elem index
  int left;
  //! right elem index
  //int right;  // FIXME: this is never referenced
  //! index of this face in the left elem
  int locfaceleft;
  //! index of this face in the right elem
  //int locfaceright;  // FIXME: this is never referenced
} Face4Sort;

//!\brief  sort the node list of the face.
//! Used only by the connectivity builder.
//! \param[in] f face whose nodes has to be sorted
void OrderFace4Sort(Face4Sort* f);

//! \brief compare two integers (used by quicksort).
//! \param[in] a first integer
//! \param[in] b second integer
//! \returns a value v, v<0 if a<b, v=0 if a==b, v>0 if a>b
int CompareInt(const void* a,const void* b);

//! \brief compare two ordered four-corner faces (used by quicksort).
//! Lexicographic order on the four nodes indices.
//! \param[in] a first face
//! \param[in] b second face
//! \returns a value v, v<0 if a<b, v=0 if a==b, v>0 if a>b
int CompareFace4Sort(const void *a, const void *b);

//! \brief get the mesh from a gmsh file.
//! \param[inout] m pointer to a macromesh
//! \param[in] filename location of the gmsh file
void ReadMacroMesh(MacroMesh *m, char *filename);

//! \brief Free the macro mesh structure.
//! \param[inout] m pointer to a macromesh
void FreeMacroMesh(MacroMesh *m);

//! \brief compute additional connectivity arrays from
//! a basic connectivity given by gmsh.
//! \param[inout] m pointer to a macromesh
void BuildConnectivity(MacroMesh *m);

//! \brief compute the upwind connectivity graph
//! associated to a constant velocity
//! \param[inout] m a macromesh
//! \param[in] vit a velocity vector (3 components)
//! \param[in] deg degree approximation in each direction
//! \param[in] raf refinement in each direction
void BuildMacroMeshGraph(MacroMesh *m, schnaps_real vit[], int deg[], int raf[]);

//! \brief affine transformation
//! \param[inout] x the transformed point
//! \param[in] x0 the initial point
//! \param[in] A the transformation
void AffineMap(schnaps_real* x,schnaps_real A[3][3], schnaps_real x0[3]);
//! \brief simple transformations of the mesh
//! \param[inout] m the macromesh
//! \param[in] x0 the initial point
//! \param[in] A the transformation
void AffineMapMacroMesh(MacroMesh *m,schnaps_real A[3][3], schnaps_real x0[3]);

//! \brief detects if the mesh is 1D and then permuts the nodes so
//! that the y,z directions coincide in the reference or physical
//! frame.
//! \param[inout] m a macromesh with is1d modified.
void Detect1DMacroMesh(MacroMesh* m);

//! \brief detects if the mesh is 2D and then permuts the nodes so
//! that the z direction coincides in the reference or physical frame.
//! \param[inout] m a macromesh with is2d modified.
void Detect2DMacroMesh(MacroMesh *m);

//! \brief verify the validity and orientation of the mesh
//! for  given interpolation parameters.
//! The function simply aborts if the mesh is bad because
//! going on with computations has no meaning.
//! \param[in] m a macromesh
//! \param[in] deg degrees parameters
//! \param[in] raf refinement parameters
void CheckMacroMesh(MacroMesh *m, int *deg, int *raf);
//! \brief list the mesh data
//! \param[in] m a macromesh
void PrintMacroMesh(MacroMesh *m);

//! \brief test if a physical point is in a given element
//! \param[in] m a macromesh
//! \param[in] ie a macrocell index
//! \param[in] xphy a point in physical space
//! \param[out] xref the corresponding ref coordinates (optional if NULL)
//! \returns true or false
bool IsInElem(MacroMesh *m,int ie, schnaps_real* xphy, schnaps_real* xref);

//! \brief find the nearest node to xphy in the mesh
//! \param[in] m a macromesh
//! \param[in] xphy a point in physical space
//! \returns the index of the nearest node
int NearestNode(MacroMesh *m,schnaps_real* xphy);

//! \brief find the cell containing a physical point
//! \param[in] m a macromesh
//! \param[in] xphy a point in physical space
//! \param[out] xref the corresponding ref coordinates (optional if NULL)
//! \returns the index of the macrocell containing xphy or -1 if none found
int NumElemFromPoint(MacroMesh *m,schnaps_real* xphy, schnaps_real* xref);


#endif
