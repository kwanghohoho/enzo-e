// See LICENSE_CELLO file for license and copyright information

/// @file     data_FieldFace.cpp
/// @author   James Bordner (jobordner@ucsd.edu)
/// @date     2011-04-12
/// @brief    Implementation of the FieldFace class

#include "cello.hpp"
#include "data.hpp"
#include "charm_simulation.hpp"

long FieldFace::counter[CONFIG_NODE_SIZE] = {0};

// #define DEBUG_ACCUM
// #define DEBUG_ACCUM_COUNT
// #define DEBUG_BYPASS_ACCUM
// #define DEBUG_BYPASS_EXTEND

#define FORTRAN_NAME(NAME) NAME##_

extern "C" void FORTRAN_NAME(field_face_store_4)
  (float * field, float * array, int * nd3, int * n3, int * accumulate);
extern "C" void FORTRAN_NAME(field_face_store_8)
  (double * field, double * array, int * nd3, int * n3, int * accumulate);
extern "C" void FORTRAN_NAME(field_face_store_16)
  (long double * field, long double * array, int * nd3, int * n3, int * accumulate);

enum enum_op_type {
  op_unknown,
  op_load,
  op_store
};

//----------------------------------------------------------------------

FieldFace::FieldFace 
( const Field & field ) throw()
  :  refresh_type_(refresh_unknown),
     prolong_(NULL),
     restrict_(NULL),
     refresh_(NULL),
     new_refresh_(false)
{
  ++counter[cello::index_static()];

  for (int i=0; i<3; i++) {
    ghost_[i] = false;
    face_[i]  = 0;
    child_[i] = 0;
  }
}

//----------------------------------------------------------------------

FieldFace::~FieldFace() throw ()
{
  --counter[cello::index_static()];

  if (new_refresh_) {
    delete refresh_;
    refresh_ = NULL;
  }
}

//----------------------------------------------------------------------

FieldFace::FieldFace(const FieldFace & field_face) throw ()
  :  refresh_type_(refresh_unknown),
     prolong_(NULL),
     restrict_(NULL),
     refresh_(NULL),
     new_refresh_(false)

{
  ++counter[cello::index_static()];

  copy_(field_face);
}

//----------------------------------------------------------------------

FieldFace & FieldFace::operator= (const FieldFace & field_face) throw ()
/// @param     field_face  Source object of the assignment
///
/// @return    The target assigned object
{
  copy_(field_face);
  return *this;
}

//----------------------------------------------------------------------

bool FieldFace::operator== (const FieldFace & field_face) throw()
{
  for (int i=0; i<3; i++) {
    if (ghost_[i] != field_face.ghost_[i]) return false;
    if (face_[i]  != field_face.face_[i])  return false;
    if (child_[i] != field_face.child_[i]) return false;
  }
  if (refresh_type_ != field_face.refresh_type_) return false;
  if (! (*refresh_ == *field_face.refresh_)) return false;
  return true;
}

//----------------------------------------------------------------------

void FieldFace::copy_(const FieldFace & field_face)
{
  for (int i=0; i<3; i++) {
    ghost_[i] = field_face.ghost_[i];
    face_[i]  = field_face.face_[i];
    child_[i] = field_face.child_[i];
  }
  refresh_type_ = field_face.refresh_type_;
  restrict_     = field_face.restrict_;
  prolong_      = field_face.prolong_;
  refresh_      = field_face.refresh_;
  // new_refresh_ must not be true in more than one FieldFace to avoid
  // multiple deletes
  new_refresh_  = false;
}

//----------------------------------------------------------------------

void FieldFace::pup (PUP::er &p)
{

  // NOTE: change this function whenever attributes change

  TRACEPUP;

  PUParray(p,face_,3);
  PUParray(p,ghost_,3);
  PUParray(p,child_,3);
  p | refresh_type_;
  p | restrict_;
  p | prolong_;
  p | refresh_;
  p | new_refresh_;
}

//======================================================================

void FieldFace::face_to_array ( Field field, int * n, char ** array) throw()
{
  ASSERT("FieldFace::face_to_array()",
	 "field_src.size() must be > 0",
	 refresh_->any_fields());

  *n = num_bytes_array(field);
  *array = new char [*n];


  ASSERT("FieldFace::face_to_array()",
	 "array size must be > 0",
	 *n > 0);

  face_to_array (field, *array);

}

//----------------------------------------------------------------------
void FieldFace::face_to_array ( Field field,char * array) throw()
{
  size_t index_array = 0;

  std::vector <int> field_list = field_list_src_(field);

  for (size_t i_f=0; i_f < field_list.size(); i_f++) {

    size_t index_field = field_list[i_f];
  
    precision_type precision = field.precision(index_field);

    const void * field_face = field.values(index_field);

    char * array_face  = &array[index_array];

    int nd3[3],ng3[3],im3[3],n3[3];

    field.field_size(index_field,&nd3[0],&nd3[1],&nd3[2]);
    field.ghost_depth(index_field,&ng3[0],&ng3[1],&ng3[2]);

    int index_src = field_list_src_(field)[i_f];
    int index_dst = field_list_dst_(field)[i_f];
    const bool accumulate = accumulate_(index_src,index_dst);
    loop_limits (im3,n3,nd3,ng3,op_load,accumulate);
#ifdef DEBUG_ACCUM
    if (accumulate)  {
    CkPrintf ("%d %d DEBUG_ACCUM face to array %d  %d %d %d  %d %d %d\n",
	      CkMyPe(),__LINE__,
	      accumulate?1:0,im3[0],im3[1],im3[2],n3[0],n3[1],n3[2]);
    fflush(stdout);
    }
#endif    

    if (refresh_type_ == refresh_coarse) {

      // Restrict field to array

      int nc3[3] = { (n3[0]+1)/2, (n3[1]+1)/2,(n3[2]+1)/2 };

      int im3_array[3] = {0,0,0};

      Simulation * simulation = proxy_simulation.ckLocalBranch();
      Problem * problem   = simulation->problem();
      Restrict * restrict = restrict_ ? restrict_ : problem->restrict();

      index_array += restrict->apply
	(precision, 
	 array_face,nc3,im3_array,nc3, 
	 field_face,nd3,im3,      n3);

    } else {

      union { float * a4; double * a8; long double * a16; };
      union { float * f4; double * f8;long double * f16;  };
      a4 = (float *) array_face;
      f4 = (float *) field_face;
      
      // Copy field to array
      
      if (precision == precision_single) {
	index_array += load_ ( a4,  f4,  nd3,n3,im3, accumulate);
      } else if (precision == precision_double) {
	index_array += load_ ( a8,  f8,  nd3,n3,im3, accumulate);
      } else if (precision == precision_quadruple) {
	index_array += load_ ( a16, f16, nd3,n3,im3, accumulate);
      } else {
	ERROR("FieldFace::face_to_array", "Unsupported precision");
      }
    }
  }

}

//----------------------------------------------------------------------

void FieldFace::array_to_face (char * array, Field field) throw()
{
  size_t index_array = 0;

  std::vector<int> field_list = field_list_dst_(field);
  
  for (size_t i_f=0; i_f < field_list.size(); i_f++) {

    size_t index_field = field_list[i_f];

    precision_type precision = field.precision(index_field);

    char * field_ghost = field.values(index_field);
    
    char * array_ghost  = array + index_array;

    int nd3[3],ng3[3],im3[3],n3[3];

    field.field_size(index_field,&nd3[0],&nd3[1],&nd3[2]);
    field.ghost_depth(index_field,&ng3[0],&ng3[1],&ng3[2]);

    int index_src = field_list_src_(field)[i_f];
    int index_dst = field_list_dst_(field)[i_f];
    const bool accumulate = accumulate_(index_src,index_dst);

    loop_limits (im3,n3,nd3,ng3,op_store,accumulate);
#ifdef DEBUG_ACCUM
    if (accumulate)  {
    CkPrintf ("%d %d DEBUG_ACCUM array to face %d  %d %d %d  %d %d %d\n",
	      CkMyPe(),__LINE__,
	      accumulate?1:0,im3[0],im3[1],im3[2],n3[0],n3[1],n3[2]);
    fflush(stdout);
    }
#endif    

    if (refresh_type_ == refresh_fine) {

      // Prolong array to field

      bool need_padding = (ng3[0]%2==1) || (ng3[1]%2==1) || (ng3[2]%2==1);

      ASSERT("FieldFace::array_to_face()",
	     "Odd ghost zones not implemented yet: prolong needs padding",
	     ! need_padding);

      int nc3[3] = { (n3[0]+1)/2, (n3[1]+1)/2, (n3[2]+1)/2 };

      int im3_array[3] = {0,0,0};

      Simulation * simulation = proxy_simulation.ckLocalBranch();
      Problem * problem   = simulation->problem();

      Prolong * prolong = prolong_ ? prolong_ : problem->prolong();

      index_array += prolong->apply
	(precision, 
	 field_ghost,nd3,im3,       n3,
	 array_ghost,nc3,im3_array, nc3);

    } else {

      // Copy array to field
      union { float * as4; double * as8; long double * as16; };
      union { float * fd4; double * fd8;long double * fd16;  };
      as4 = (float *) array_ghost;
      fd4 = (float *) field_ghost;
      
      // Copy field to array
      
      if (precision == precision_single) {
	index_array += store_ ( fd4,  as4,  nd3,n3,im3, accumulate);
      } else if (precision == precision_double) {
	index_array += store_ ( fd8,  as8,  nd3,n3,im3, accumulate);
      } else if (precision == precision_quadruple) {
	index_array += store_ ( fd16, as16, nd3,n3,im3, accumulate);
      } else {
	ERROR("FieldFace::array_to_face()", "Unsupported precision");
      }
    }
  }
}

//----------------------------------------------------------------------

void FieldFace::face_to_face (Field field_src, Field field_dst)
{
  
  std::vector<int> field_list_src = field_list_src_(field_src);
  std::vector<int> field_list_dst = field_list_dst_(field_dst);
  
  for (size_t i_f=0; i_f < field_list_src.size(); i_f++) {

    size_t index_src = field_list_src[i_f];
    size_t index_dst = field_list_dst[i_f];

    int m3[3]={0},g3[3]={0},is3[3]={0},id3[3]={0},ns3[3]={0},nd3[3]={0};

    field_src.field_size (index_src,&m3[0],&m3[1],&m3[2]);
    field_src.ghost_depth(index_src,&g3[0],&g3[1],&g3[2]);
    const bool accumulate = accumulate_(index_src,index_dst);

    loop_limits (is3,ns3,m3,g3,op_load,accumulate);
#ifdef DEBUG_ACCUM
    if (accumulate)  {

    CkPrintf ("%d %d DEBUG_ACCUM face to face %d  %d %d %d  %d %d %d\n",
	      CkMyPe(),__LINE__,
	      accumulate?1:0,is3[0],is3[1],is3[2],ns3[0],ns3[1],ns3[2]);
    fflush(stdout);
    }
#endif    
    invert_face();
    loop_limits (id3,nd3,m3,g3,op_store,accumulate);
#ifdef DEBUG_ACCUM
    if (accumulate)  {
    CkPrintf ("%d %d DEBUG_ACCUM face to face %d  %d %d %d  %d %d %d\n",
	      CkMyPe(),__LINE__,
	      accumulate?1:0,id3[0],id3[1],id3[2],nd3[0],nd3[1],nd3[2]);
    fflush(stdout);
    }
#endif    
    invert_face();

    // Adjust loop limits if accumulating to include ghost zones
    // on neighbor axes

    precision_type precision = field_src.precision(index_src);
    
    char * values_src = field_src.values(index_src);
    char * values_dst = field_dst.values(index_dst);
    
    Simulation * simulation = proxy_simulation.ckLocalBranch();
    Problem * problem   = simulation->problem();

    if (refresh_type_ == refresh_fine) {

      // Prolong field

      bool need_padding = (g3[0]%2==1) || (g3[1]%2==1) || (g3[2]%2==1);

      ASSERT("FieldFace::face_to_face()",
	     "Odd ghost zones not implemented yet: prolong needs padding",
	     ! need_padding);

      Prolong * prolong = prolong_ ? prolong_ : problem->prolong();

      prolong->apply (precision, 
		      values_dst,m3,id3, nd3,
		      values_src,m3,is3, ns3);

    } else if (refresh_type_ == refresh_coarse) {

      // Restrict field

      Restrict * restrict = restrict_ ? restrict_ : problem->restrict();

      restrict->apply (precision, 
		       values_dst,m3,id3, nd3,
		       values_src,m3,is3, ns3);

    } else {

      // Copy faces to ghosts

      union { float * fs4; double * fs8; long double * fs16; };
      union { float * fd4; double * fd8;long double * fd16;  };
      fs4 = (float *) values_src;
      fd4 = (float *) values_dst;
      
      // Copy field to array
      
      if (precision == precision_single) {
	copy_ ( fd4, m3,nd3,id3,fs4, m3, ns3,is3,accumulate);
      } else if (precision == precision_double) {
	copy_ ( fd8, m3,nd3,id3,fs8, m3, ns3,is3,accumulate);
      } else if (precision == precision_quadruple) {
	copy_ ( fd16,m3,nd3,id3,fs16,m3,ns3,is3,accumulate);
      } else {
	ERROR("FieldFace::face_to_face()", "Unsupported precision");
      }
    }
  }
}

//----------------------------------------------------------------------

int FieldFace::num_bytes_array(Field field) throw()
{
  int array_size = 0;

  std::vector<int> field_list = field_list_src_(field);
  
  for (size_t i_f=0; i_f < field_list.size(); i_f++) {

    size_t index_field = field_list[i_f];

    precision_type precision = field.precision(index_field);
    int bytes_per_element = cello::sizeof_precision (precision);

    int nd3[3],ng3[3],im3[3],n3[3];

    field.field_size (index_field,&nd3[0],&nd3[1],&nd3[2]);
    field.ghost_depth(index_field,&ng3[0],&ng3[1],&ng3[2]);

    int index_src = field_list_src_(field)[i_f];
    int index_dst = field_list_dst_(field)[i_f];
    const bool accumulate = accumulate_(index_src,index_dst);
    int op_type = (refresh_type_ == refresh_fine) ? op_load : op_store;
    loop_limits (im3,n3,nd3,ng3,op_type,accumulate);
#ifdef DEBUG_ACCUM
    // if (accumulate)  {
    // CkPrintf ("%d %d DEBUG_ACCUM %d  %d %d %d  %d %d %d\n",
    // 	      CkMyPe(),__LINE__,
    // 	      accumulate?1:0,im3[0],im3[1],im3[2],n3[0],n3[1],n3[2]);
    // fflush(stdout);
    // }
#endif    

    array_size += n3[0]*n3[1]*n3[2]*bytes_per_element;

  }

  ASSERT("FieldFace::num_bytes_array()",
	 "array_size must be > 0, maybe field_list.size() is 0?",
	 array_size);

  return array_size;

}

//----------------------------------------------------------------------

int FieldFace::data_size () const
{
  int count = 0;

  count += 3*sizeof(int);  // face_[3]
  count += 3*sizeof(bool); // ghost_[3]
  count += 3*sizeof(int);  // child_[3];
  count += 1*sizeof(int);  // refresh_type_ (restrict,prolong,copy)
  count += refresh_->data_size(); // refresh_

  return count;

}

//----------------------------------------------------------------------

char * FieldFace::save_data (char * buffer) const
{
  char * p = buffer;
  int n;

  memcpy(p,face_, n=3*sizeof(int));  
  p+=n;

  memcpy(p,ghost_,n=3*sizeof(bool)); 
  p+=n;

  memcpy(p,child_,n=3*sizeof(int));  
  p+=n;

  memcpy(p,&refresh_type_,n=sizeof(int));  
  p+=n;

  p = refresh_->save_data(p);

  ASSERT2("FieldFace::save_data()",
	  "Buffer has size %d but expecting size %d",
	  (p-buffer),data_size(),
	  ((p-buffer) == data_size()));
  
  return p;
}

//----------------------------------------------------------------------

char * FieldFace::load_data (char * buffer)
{

  char * p = buffer;
  int n;

  memcpy(face_,p, n=3*sizeof(int));
  p+=n;

  memcpy(ghost_,p,n=3*sizeof(bool));
  p+=n;

  memcpy(child_,p,n=3*sizeof(int));
  p+=n;

  memcpy(&refresh_type_,p,n=sizeof(int));
  p+=n;

  Refresh * refresh = new Refresh;
  set_refresh(refresh,true);

  p = refresh_->load_data(p);

  ASSERT2("FieldFace::save_data()",
	  "Buffer has size %d but expecting size %d",
	  (p-buffer),data_size(),
	  ((p-buffer) == data_size()));

  return p;
}

//======================================================================

template<class T>
size_t FieldFace::load_
( T * array_face, const T * field_face, 
  int nd3[3], int n3[3],int im3[3], bool accumulate ) throw()
{
  // NOTE: don't check accumulate since loading array; accumulate
  // is handled in corresponding store_() at the receiving end
    // add values
#ifdef DEBUG_ACCUM_COUNT
  double sum1=0.0, sum2=0.0;
#endif    
  for (int iz=0; iz <n3[2]; iz++)  {
    int kz = iz+im3[2];
    for (int iy=0; iy < n3[1]; iy++) {
      int ky = iy+im3[1];
      for (int ix=0; ix < n3[0]; ix++) {
	int kx = ix+im3[0];
	int index_array = ix +   n3[0]*(iy +   n3[1] * iz);
	int index_field = kx + nd3[0]*(ky + nd3[1] * kz);
	array_face[index_array] = field_face[index_field];
#ifdef DEBUG_ACCUM_COUNT
	if (accumulate) {
	  sum1+=field_face[index_field];
	}
#endif    
      }
    }
  }
#ifdef DEBUG_ACCUM_COUNT
  if (accumulate) {
    CkPrintf ("DEBUG_ACCUM_COUNT %14.10f load\n",sum1);
  }
#endif    

  return (sizeof(T) * n3[0] * n3[1] * n3[2]);

}

//----------------------------------------------------------------------

template<class T> size_t FieldFace::store_
( T * ghost, const T * array,
  int nd3[3], int n3[3],int im3[3], bool accumulate) throw()
{
#define FORTRAN_STORE

#ifdef FORTRAN_STORE

  // This is to get around a bug on SDSC Comet where this function
  // crashes with -O3 (See Enzo-P / Cello bug report #90)
  // http://client64-249.sdsc.edu/cello-bug/show_bug.cgi?id=90

  union {
    float *       ghost_4;
    double *      ghost_8;
    long double * ghost_16;
  };
  union {
    float *       array_4;
    double *      array_8;
    long double * array_16;
  };

  ghost_4 = (float *) ghost;
  array_4 = (float *) array;
  
  int im = im3[0] + nd3[0]*(im3[1] + nd3[1]*im3[2]);

  int iaccumulate = accumulate ? 1 : 0;
  
  if (sizeof(T)==sizeof(float)) {
    FORTRAN_NAME(field_face_store_4)(ghost_4 + im,   array_4, nd3,n3,
				     &iaccumulate);
  } else if (sizeof(T)==sizeof(double)) {
    FORTRAN_NAME(field_face_store_8)(ghost_8 + im,   array_8, nd3,n3,
				     &iaccumulate);
  } else if (sizeof(T)==sizeof(long double)) {
    FORTRAN_NAME(field_face_store_16)(ghost_16 + im, array_16, nd3,n3,
				      &iaccumulate);
  } else {
    ERROR1 ("FieldFace::store_()",
	   "unknown float precision sizeof(T) = %d\n",sizeof(T));
  }

#else

  if (accumulate) {
    // add values
#ifdef DEBUG_ACCUM_COUNT
    double sum1=0.0, sum2=0.0;
#endif    
    for (int iz=0; iz <n3[2]; iz++)  {
      int kz = iz+im3[2];
      for (int iy=0; iy < n3[1]; iy++) {
	int ky = iy+im3[1];
	for (int ix=0; ix < n3[0]; ix++) {
	  int kx = ix+im3[0];
	  int index_array = ix +  n3[0]*(iy +  n3[1] * iz);
	  int index_field = kx + nd3[0]*(ky + nd3[1] * kz);
#ifdef DEBUG_ACCUM_COUNT
	  sum1+=array[index_array];
	  sum2+=ghost[index_field];
#endif    
#ifdef DEBUG_BYPASS_ACCUM
	  ghost[index_field] = array[index_array];
#else
	  ghost[index_field] += array[index_array];
#endif	  
	}
      }
    }
#ifdef DEBUG_ACCUM_COUNT
    CkPrintf ("DEBUG_ACCUM_COUNT %14.10f %14.10f store\n",sum1,sum2);
#endif    
  } else {
    // copy values
    for (int iz=0; iz <n3[2]; iz++)  {
      int kz = iz+im3[2];
      for (int iy=0; iy < n3[1]; iy++) {
	int ky = iy+im3[1];
	for (int ix=0; ix < n3[0]; ix++) {
	  int kx = ix+im3[0];
	  int index_array = ix +  n3[0]*(iy +  n3[1] * iz);
	  int index_field = kx + nd3[0]*(ky + nd3[1] * kz);
	  ghost[index_field] = array[index_array];
	}
      }
    }
  }
#endif

  return (sizeof(T) * n3[0] * n3[1] * n3[2]);

}

//----------------------------------------------------------------------

template<class T> void FieldFace::copy_
( T       * vd, int md3[3],int nd3[3],int id3[3],
  const T * vs, int ms3[3],int ns3[3],int is3[3],
  bool accumulate) throw()
{
  if (accumulate) {

#ifdef DEBUG_ACCUM_COUNT
    double sum1=0.0, sum2=0.0;
#endif    
    for (int iz=0; iz <ns3[2]; iz++)  {
      for (int iy=0; iy < ns3[1]; iy++) {
	for (int ix=0; ix < ns3[0]; ix++) {
	  int i_src = (ix+is3[0]) + ms3[0]*((iy+is3[1]) + ms3[1] * (iz+is3[2]));
	  int i_dst = (ix+id3[0]) + md3[0]*((iy+id3[1]) + md3[1] * (iz+id3[2]));
#ifdef DEBUG_ACCUM_COUNT
	  sum1+=vs[i_src];
	  sum2+=vd[i_dst];
#endif    
#ifdef DEBUG_BYPASS_ACCUM
	  vd[i_dst] = vs[i_src];
#else
	  vd[i_dst] += vs[i_src];
#endif	  
	}
      }
    }
#ifdef DEBUG_ACCUM_COUNT
    CkPrintf ("DEBUG_ACCUM_COUNT %14.10f %14.10f copy\n",sum1,sum2);
#endif    
  } else {
    for (int iz=0; iz <ns3[2]; iz++)  {
      for (int iy=0; iy < ns3[1]; iy++) {
	for (int ix=0; ix < ns3[0]; ix++) {
	  int i_src = (ix+is3[0]) + ms3[0]*((iy+is3[1]) + ms3[1] * (iz+is3[2]));
	  int i_dst = (ix+id3[0]) + md3[0]*((iy+id3[1]) + md3[1] * (iz+id3[2]));
	  vd[i_dst] = vs[i_src];
	}
      }
    }
  }
}

//----------------------------------------------------------------------

void FieldFace::loop_limits
( int im3[3],int n3[3], const int nd3[3], const int ng3[3], int op_type,
  bool accumulate)
{
  im3[0]=0;
  im3[1]=0;
  im3[2]=0;
  n3[0]=0;
  n3[1]=0;
  n3[2]=0;

  const bool lcopy = (refresh_type_ == refresh_same);

  for (int axis=0; axis<3; axis++) {

    if (lcopy) {
      if (face_[axis] == 0 && ! ghost_[axis]) {
	im3[axis] = ng3[axis];
	n3[axis]  = nd3[axis] - 2*ng3[axis];
      }
      if (face_[axis] == 0 && ghost_[axis]) {
	im3[axis] = 0;
	n3 [axis] = nd3[axis];
      }
      if (face_[axis] == -1 && op_type == op_load) {
	im3[axis] = ng3[axis];
	n3 [axis] = ng3[axis];
      }
      if (face_[axis] == -1 && op_type == op_store) {
	im3[axis] = 0;
	n3 [axis] = ng3[axis];
      }      
      if (face_[axis] == +1 && op_type == op_load) {
	im3[axis] = nd3[axis]-2*ng3[axis];
	n3 [axis] = ng3[axis];
      }
      if (face_[axis] == +1 && op_type == op_store) {
	im3[axis] = nd3[axis]-ng3[axis];
	n3 [axis] = ng3[axis];
      }
    }

    // adjust limits to include ghost zones for oblique edges/corners
    // at coarse-fine level interfaces
    
    const bool full_block = (face_[0] == 0 && face_[1] == 0 && face_[2] == 0);

    // child offset: 0 or n/2

    const int co = child_[axis]*(nd3[axis]-2*ng3[axis])/2;

    if (refresh_type_ == refresh_fine) {

      if (face_[axis] == 0 && ! ghost_[axis] && op_type == op_load) {
	im3[axis] = ng3[axis] + co;
	n3[axis] = (nd3[axis]-2*ng3[axis])/2;

	// Bug #70 fix: always include ghosts in finer block when
	// face_[axis] = 0 see notes 150811

	if (! full_block) {
	  if (child_[axis] == 1) {
	    im3[axis] -= ng3[axis]/2;
	  }
	  n3[axis] += ng3[axis]/2;
	}

      }
      if (face_[axis] == 0 && ghost_[axis] && op_type == op_load) {
	im3[axis] = ng3[axis]/2 + co;
	n3[axis] = nd3[axis]/2;
      }
      if (face_[axis] == 0 && ! ghost_[axis] && op_type == op_store) {
	im3[axis] = ng3[axis];
	n3[axis]  = nd3[axis]-2*ng3[axis];

	// Bug #70 fix: always include ghosts in finer block when
	// face_[axis] = 0 see notes 150811

	if (! full_block) {
	  if (child_[axis] == 1) {
	    im3[axis] -= ng3[axis];
	  }
	  n3[axis] += ng3[axis];
	}

      }	  
      if (face_[axis] == 0 && ghost_[axis] && op_type == op_store) {
	im3[axis] = 0;
	n3[axis]  = nd3[axis];
      }
      if (face_[axis] == -1 && op_type == op_load) {
	im3[axis] = ng3[axis];
	n3[axis]  = ng3[axis]/2;
      }
      if (face_[axis] == -1 && op_type == op_store) {
	im3[axis] = 0;
	n3[axis]  = ng3[axis];
      }
      if (face_[axis] == +1 && op_type == op_load) {
	im3[axis] = nd3[axis]-3*ng3[axis]/2;
	n3[axis]  = ng3[axis]/2;
      }
      if (face_[axis] == +1 && op_type == op_store) {
	im3[axis] = nd3[axis]-ng3[axis];
	n3[axis]  = ng3[axis];
      }
    }

    if (refresh_type_ == refresh_coarse) {

      if (face_[axis] == 0 && !ghost_[axis] && op_type == op_load) {
	im3[axis] = ng3[axis];
	n3[axis]  = nd3[axis]-2*ng3[axis];
      }
      if (face_[axis] == 0 && !ghost_[axis] && op_type == op_store) {
	im3[axis] = ng3[axis] + co;
	n3[axis] = (nd3[axis]-2*ng3[axis])/2;
      }
      if (face_[axis] == 0 && ghost_[axis] && op_type == op_load) {
	im3[axis] = 0;
	n3[axis]  = nd3[axis];
      }
      if (face_[axis] == 0 && ghost_[axis] && op_type == op_store) {
	im3[axis] = ng3[axis]/2 + co;
	n3[axis] = nd3[axis]/2;
      }
      if (face_[axis] == -1 && op_type == op_load) {
	im3[axis] = ng3[axis];
	n3[axis]  = 2*ng3[axis];
      }
      if (face_[axis] == -1 && op_type == op_store) {
	im3[axis] = 0;
	n3[axis]  = ng3[axis];
      }
      if (face_[axis] == +1 && op_type == op_load) {
	im3[axis] = nd3[axis]-3*ng3[axis];
	n3[axis]  = 2*ng3[axis];
      }
      if (face_[axis] == +1 && op_type == op_store) {
	im3[axis] = nd3[axis]-ng3[axis];
	n3[axis]  = ng3[axis];
      }
    }
  }
  n3[0] = std::max(n3[0],1);
  n3[1] = std::max(n3[1],1);
  n3[2] = std::max(n3[2],1);

  // adjust for accumulate

#ifndef DEBUG_BYPASS_EXTEND
  if (accumulate) {
    // WARNING: accumulate loop index update is not yet implemented
    // for refresh_coarse or refresh_fine
    if (refresh_type_ == refresh_same) {
      for (int axis=0; axis<3; axis++) {
	if (! ghost_[axis]) {
	  if ((op_type == op_load && face_[axis] == -1) ||
	      (op_type == op_store && face_[axis] == 1)) {
	    im3[axis] -= 1;
	  }
	  if (face_[axis] != 0) {
	    n3[axis] += 1;
	  }
	}
      }
    }
  }
#endif  

}

//----------------------------------------------------------------------

void FieldFace::print(const char * message)
{
  CkPrintf (" FieldFace %s %p\n",message,this);
  CkPrintf ("    face_  %d %d %d\n",face_[0],face_[1],face_[2]);
  CkPrintf ("    ghost_  %d %d %d\n",ghost_[0],ghost_[1],ghost_[2]);
  CkPrintf ("    child_  %d %d %d\n",child_[0],child_[1],child_[2]);
  CkPrintf ("    refresh_type_ %d\n",refresh_type_);
  if (refresh_) refresh_->print();
}

//----------------------------------------------------------------------

void FieldFace::set_field_list(std::vector<int> field_list)
{
  refresh_->set_field_list(field_list);
}

//======================================================================

std::vector<int> FieldFace::field_list_src_(Field field) const 
{
  std::vector<int> field_list = refresh_->field_list_src();
  if (refresh_->all_fields()) {
    for (int i=0; i<field.field_count(); i++) {
      field_list.push_back(i);
    }
  }
  return field_list;
}

std::vector<int> FieldFace::field_list_dst_(Field field) const 
{
  std::vector<int> field_list = refresh_->field_list_dst();
  if (refresh_->all_fields()) {
    for (int i=0; i<field.field_count(); i++) {
      field_list.push_back(i);
    }
  }
  return field_list;
}

bool FieldFace::accumulate_(int index_src, int index_dst) const
{
  return ((index_src != index_dst) && refresh_->accumulate());
}
