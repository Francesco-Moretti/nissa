#ifdef HAVE_CONFIG_H
 #include "config.hpp"
#endif

#include "base/global_variables.hpp"
#include "base/thread_macros.hpp"
#include "base/vectors.hpp"
#include "communicate/communicate.hpp"
#include "new_types/dirac.hpp"
#include "new_types/su3.hpp"
#ifdef USE_THREADS
 #include "routines/thread.hpp"
#endif

namespace nissa
{
#define APPLY_NABLA_I(TYPE)                                             \
  THREADABLE_FUNCTION_4ARG(apply_nabla_i, TYPE*,out, TYPE*,in, quad_su3*,conf, int,mu) \
  {                                                                     \
    GET_THREAD_ID();                                                    \
                                                                        \
    NAME3(communicate_lx,TYPE,borders)(in);                             \
    communicate_lx_quad_su3_borders(conf);                              \
                                                                        \
    TYPE *temp=nissa_malloc("temp",loc_vol+bord_vol,TYPE);              \
    vector_reset(temp);                                                 \
                                                                        \
    NISSA_PARALLEL_LOOP(ix,0,loc_vol)                                   \
      {                                                                 \
        int Xup,Xdw;                                                    \
        Xup=loclx_neighup[ix][mu];					\
        Xdw=loclx_neighdw[ix][mu];					\
									\
        NAME2(unsafe_su3_prod,TYPE)(      temp[ix],conf[ix][mu] ,in[Xup]); \
        NAME2(su3_dag_subt_the_prod,TYPE)(temp[ix],conf[Xdw][mu],in[Xdw]); \
      }                                                                 \
    									\
    vector_copy(out,temp);                                              \
    nissa_free(temp);                                                   \
                                                                        \
    set_borders_invalid(out);                                           \
  }									\
  THREADABLE_FUNCTION_END
  
  //instantiate the application function
  APPLY_NABLA_I(spincolor)
  APPLY_NABLA_I(colorspinspin)
  APPLY_NABLA_I(su3spinspin)
  
  void insert_external_source_handle(complex out,spin1field *aux,int ivol,int mu,void *pars){complex_copy(out,aux[ivol][mu]);}
  void insert_tadpole_handle(complex out,spin1field *aux,int ivol,int mu,void *pars){out[RE]=((double*)pars)[mu];out[IM]=0;}

  /*
    insert the operator:  \sum_mu  [
    f_fw * ( - i t3 g5 - gmu) A_{x,mu} U_{x,mu} \delta{x',x+mu} + f_bw * ( - i t3 g5 + gmu) A_{x-mu,mu} U^+_{x-mu,mu} \delta{x',x-mu}]
  */
#define INSERT_OPERATORS(TYPE)						\
  void insert_operator(TYPE *out,quad_su3 *conf,spin1field *curr,TYPE *in,complex fact_fw,complex fact_bw,int r,void(*get_curr)(complex,spin1field*,int,int,void*),void *pars=NULL) \
  {									\
  GET_THREAD_ID();							\
									\
  /*reset the output and communicate borders*/				\
  vector_reset(out);							\
  NAME3(communicate_lx,TYPE,borders)(in);				\
  communicate_lx_quad_su3_borders(conf);				\
  if(curr) communicate_lx_spin1field_borders(curr);			\
									\
  NISSA_PARALLEL_LOOP(ivol,0,loc_vol)					\
    for(int mu=0;mu<4;mu++)						\
      {									\
	/*find neighbors*/						\
	int ifw=loclx_neighup[ivol][mu];				\
	int ibw=loclx_neighdw[ivol][mu];				\
									\
	/*transport down and up*/					\
	TYPE fw,bw;							\
	NAME2(unsafe_su3_prod,TYPE)(fw,conf[ivol][mu],in[ifw]);		\
	NAME2(unsafe_su3_dag_prod,TYPE)(bw,conf[ibw][mu],in[ibw]);	\
									\
	/*weight the two pieces*/					\
	NAME2(TYPE,prodassign_complex)(fw,fact_fw);			\
	NAME2(TYPE,prodassign_complex)(bw,fact_bw);			\
									\
	/*insert the current*/						\
	complex fw_curr,bw_curr;					\
	get_curr(fw_curr,curr,ivol,mu,pars);				\
	get_curr(bw_curr,curr,ibw,mu,pars);				\
	NAME2(TYPE,prodassign_complex)(fw,fw_curr);			\
	NAME2(TYPE,prodassign_complex)(bw,bw_curr);			\
									\
	/*summ and subtract the two*/					\
	TYPE bw_M_fw,bw_P_fw;						\
	NAME2(TYPE,subt)(bw_M_fw,bw,fw);				\
	NAME2(TYPE,summ)(bw_P_fw,bw,fw);				\
									\
	/*put -i g5 t3 on the summ*/					\
	TYPE g5_bw_P_fw;						\
	NAME2(unsafe_dirac_prod,TYPE)(g5_bw_P_fw,base_gamma+5,bw_P_fw); \
	NAME2(TYPE,summ_the_prod_idouble)(out[ivol],g5_bw_P_fw,-tau3[r]); \
									\
	/*put gmu on the difference*/					\
	TYPE gmu_bw_M_fw;						\
	NAME2(unsafe_dirac_prod,TYPE)(gmu_bw_M_fw,base_gamma+map_mu[mu],bw_M_fw); \
	NAME2(TYPE,summassign)(out[ivol],gmu_bw_M_fw);			\
      }									\
  									\
  set_borders_invalid(out);						\
  }									\
  									\
  /*insert the tadpole*/						\
  THREADABLE_FUNCTION_5ARG(insert_tadpole, TYPE*,out, quad_su3*,conf, TYPE*,in, int,r, double*,tad) \
  {									\
    /*call with no source insertion, plus between fw and bw, and a global -0.25*/ \
    complex fw_factor={-0.25,0},bw_factor={-0.25,0};			\
    insert_operator(out,conf,NULL,in,fw_factor,bw_factor,r,insert_tadpole_handle,tad); \
  }									\
  THREADABLE_FUNCTION_END						\
									\
  /*insert the external source, that is one of the two extrema of the stoch prop*/ \
  THREADABLE_FUNCTION_5ARG(insert_external_source, TYPE*,out, quad_su3*,conf, spin1field*,curr, TYPE*,in, int,r) \
  {									\
    /*call with no source insertion, minus between fw and bw, and a global -i*0.5*/ \
    complex fw_factor={0,-0.5},bw_factor={0,+0.5};			\
    insert_operator(out,conf,curr,in,fw_factor,bw_factor,r,insert_external_source_handle); \
  }									\
  THREADABLE_FUNCTION_END						\
									\
  /*multiply with gamma*/						\
  THREADABLE_FUNCTION_4ARG(prop_multiply_with_gamma, TYPE*,out, int,ig, TYPE*,in, int,twall) \
  {									\
    GET_THREAD_ID();							\
    NISSA_PARALLEL_LOOP(ivol,0,loc_vol)					\
      {									\
	NAME2(safe_dirac_prod,TYPE)(out[ivol],base_gamma+ig,in[ivol]); \
	NAME2(TYPE,prodassign_double)(out[ivol],(twall==-1||glb_coord_of_loclx[ivol][0]==twall)); \
      }									\
    set_borders_invalid(out);						\
  }									\
  THREADABLE_FUNCTION_END						\
  									\
  /*multiply with an imaginary factor*/					\
  THREADABLE_FUNCTION_2ARG(prop_multiply_with_idouble, TYPE*,out, double,f) \
  {									\
    GET_THREAD_ID();							\
    NISSA_PARALLEL_LOOP(ivol,0,loc_vol)					\
      NAME2(TYPE,prodassign_idouble)(out[ivol],f);			\
    set_borders_invalid(out);						\
  }									\
  THREADABLE_FUNCTION_END
  
  INSERT_OPERATORS(colorspinspin);
  INSERT_OPERATORS(su3spinspin);
}
