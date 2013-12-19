#ifdef HAVE_CONFIG_H
 #include "config.hpp"
#endif

#include <map>
#include <vector>

#include "base/global_variables.hpp"
#include "base/thread_macros.hpp"
#include "base/vectors.hpp"
#include "communicate/communicate.hpp"
#include "geometry/geometry_mix.hpp"
#include "linalgs/linalgs.hpp"
#include "new_types/new_types_definitions.hpp"
#include "new_types/su3.hpp"
#include "operations/shift.hpp"
#include "routines/ios.hpp"
#ifdef USE_THREADS
 #include "routines/thread.hpp"
#endif

#include "smearing/APE.hpp"
#include "smearing/HYP.hpp"

namespace nissa
{  
  void index_transp(int &irank_transp,int &iloc_transp,int iloc_lx,void *pars)
  {
    int ii=((int*)pars)[0],prp_vol=((int*)pars)[1],cmp_vol=((int*)pars)[2];
    int i=ii+1;
    printf("rank %d, prp_vol: %d, cmp_vol: %d\n",rank,prp_vol,cmp_vol);
    
    //directions perpendicular to 0 and i
    int j=perp2_dir[0][ii][0],k=perp2_dir[0][ii][1];

    //find dest in the global indexing
    int *g=glb_coord_of_loclx[iloc_lx];
    int glb_dest_site=g[i]+glb_size[i]*(g[0]+glb_size[0]*(g[k]+glb_size[k]*g[j]));
    irank_transp=glb_dest_site/prp_vol;
    iloc_transp=glb_dest_site-irank_transp*prp_vol;
    
    printf("rank %d/%d site %d/%d going to %d %d %d %d, [%d %d %d %d] rank %d site %d\n",
	   rank,nranks,iloc_lx,loc_vol,g[i],g[0],g[k],g[j],i,0,k,j,irank_transp,iloc_transp);
  }

  //compute all possible rectangular paths among a defined interval
  THREADABLE_FUNCTION_4ARG(measure_all_rectangular_paths_new, all_rect_meas_pars_t*,pars, quad_su3*,ori_conf, int,iconf, int,create_output_file)
  {
    GET_THREAD_ID();
    
    //running conf
    quad_su3 *sme_conf=nissa_malloc("sme_conf",loc_vol+bord_vol+edge_vol,quad_su3);
    
    //compute competing volume
    int prp_vol=glb_size[0]*glb_size[1]*((int)ceil((double)glb_size[1]*glb_size[1]/nranks));
    int min_vol=prp_vol*rank,max_vol=min_vol+prp_vol;
    if(max_vol>=glb_vol) max_vol=glb_vol;
    int cmp_vol=max_vol-min_vol;
    
    //define the three remapper
    vector_remap_t *remap[3];
    for(int ii=0;ii<3;ii++)
      {
	int pars[3]={ii,prp_vol,cmp_vol};
	remap[ii]=new vector_remap_t(loc_vol,index_transp,pars);
	if(remap[ii]->nel_in!=cmp_vol) crash("expected %d obtained %d",cmp_vol,remap[ii]->nel_in);
      }
    
    //transposed configurations
    //we need 3 copies, each holding 1 smeared temporal links and 3*nape_spat_levls spatial links per site
    int nape_spat_levls=pars->nape_spat_levls;
    int ntot_sme=1+3*nape_spat_levls;
    su3 *transp_conf=nissa_malloc("transp_confs",3*cmp_vol*ntot_sme,su3);
    //local conf holders pre-transposing
    su3 *pre_transp_conf_holder=nissa_malloc("pre_transp_conf_holder",loc_vol,su3);
    su3 *post_transp_conf_holder=nissa_malloc("post_transp_conf_holder",loc_vol,su3);

    //hyp or temporal APE smear the conf
    if(pars->use_hyp_or_ape_temp==0) hyp_smear_conf_dir(sme_conf,ori_conf,pars->hyp_temp_alpha0,pars->hyp_temp_alpha1,pars->hyp_temp_alpha2,0);
    else ape_temporal_smear_conf(sme_conf,ori_conf,pars->ape_temp_alpha,pars->nape_temp_iters);
    
    //store temporal links and send them
    NISSA_PARALLEL_LOOP(ivol,0,loc_vol)
      su3_copy(pre_transp_conf_holder[ivol],sme_conf[ivol][0]);
    THREAD_BARRIER();
    for(int ii=0;ii<3;ii++)
      {
	remap[ii]->remap(post_transp_conf_holder,pre_transp_conf_holder,sizeof(su3));
	NISSA_PARALLEL_LOOP(icmp,0,cmp_vol)
	  su3_copy(transp_conf[icmp+cmp_vol*(0+ntot_sme*ii)],post_transp_conf_holder[icmp]);
	THREAD_BARRIER();
      }
    
    //spatial APE smearing
    for(int iape=0;iape<nape_spat_levls;iape++)
      {
        ape_spatial_smear_conf(sme_conf,sme_conf,pars->ape_spat_alpha,
	       (iape==0)?pars->nape_spat_iters[0]:(pars->nape_spat_iters[iape]-pars->nape_spat_iters[iape-1]));
	
	//store spatial links and send them
	for(int ii=0;ii<3;ii++)
	  {
	    int i=ii+1;
	    NISSA_PARALLEL_LOOP(ivol,0,loc_vol)
	      su3_copy(pre_transp_conf_holder[ivol],sme_conf[ivol][i]);
	    THREAD_BARRIER();
	    for(int ii=0;ii<3;ii++)
	      {
		remap[ii]->remap(post_transp_conf_holder,pre_transp_conf_holder,sizeof(su3));
		NISSA_PARALLEL_LOOP(icmp,0,cmp_vol)
		  su3_copy(transp_conf[icmp+cmp_vol*((i+iape)+ntot_sme*ii)],post_transp_conf_holder[icmp]);
		THREAD_BARRIER();
	      }
	  }
      }

    //free smeared conf and pre-post buffers
    nissa_free(post_transp_conf_holder);
    nissa_free(pre_transp_conf_holder);
    nissa_free(sme_conf);
    
    for(int ii=0;ii<3;ii++) delete remap[ii];
    
    ////////////////////////////////////////////////////////////////////////////////////
    
    //open file
    FILE *fout=NULL;
    if(rank==0 && IS_MASTER_THREAD) fout=open_file(pars->path,create_output_file?"w":"a");
    
    if(rank==0 && IS_MASTER_THREAD) fclose(fout);
    
    nissa_free(transp_conf);
  }}

  //compute all possible rectangular paths among a defined interval
  THREADABLE_FUNCTION_4ARG(measure_all_rectangular_paths, all_rect_meas_pars_t*,pars, quad_su3*,ori_conf, int,iconf, int,create_output_file)
  {
    GET_THREAD_ID();
    
    FILE *fout=NULL;
    if(rank==0 && IS_MASTER_THREAD) fout=open_file(pars->path,create_output_file?"w":"a");
    
    //hypped and APE spatial smeared conf
    quad_su3 *sme_conf=nissa_malloc("sme_conf",loc_vol+bord_vol+edge_vol,quad_su3);
    
    //allocate time path, time-spatial paths, closing paths and point contribution
    su3 *T_path=nissa_malloc("T_path",loc_vol+bord_vol,su3);
    su3 *TS_path=nissa_malloc("TS_path",loc_vol+bord_vol,su3);
    su3 *closed_path=nissa_malloc("closed_path",loc_vol+bord_vol,su3);
    double *point_path=nissa_malloc("point_path",loc_vol,double);
    
    //hyp or temporal APE smear the conf
    if(pars->use_hyp_or_ape_temp==0) hyp_smear_conf_dir(sme_conf,ori_conf,pars->hyp_temp_alpha0,pars->hyp_temp_alpha1,pars->hyp_temp_alpha2,0);
    else ape_temporal_smear_conf(sme_conf,ori_conf,pars->ape_temp_alpha,pars->nape_temp_iters);
    
    //loop over APE smeared levels
    for(int iape=0;iape<pars->nape_spat_levls;iape++)
      {
	//APE smearing
	ape_spatial_smear_conf(sme_conf,sme_conf,pars->ape_spat_alpha,
		  (iape==0)?pars->nape_spat_iters[0]:(pars->nape_spat_iters[iape]-pars->nape_spat_iters[iape-1]));
	
	//reset the Tpath link product
	NISSA_PARALLEL_LOOP(ivol,0,loc_vol)
	  su3_put_to_id(T_path[ivol]);
	
	//move along T up to Tmax
	for(int t=1;t<=pars->Tmax;t++)
	  {
	    //take the product
	    NISSA_PARALLEL_LOOP(ivol,0,loc_vol)
	      safe_su3_prod_su3(T_path[ivol],T_path[ivol],sme_conf[ivol][0]);
	    set_borders_invalid(T_path);
	    
	    //push up the vector along T
	    su3_vec_single_shift(T_path,0,+1);
	    
	    //results to be printed, averaged along the three dirs
	    double paths[pars->Dmax+1][3];
	    for(int ii=0;ii<3;ii++) for(int d=0;d<=pars->Dmax;d++) paths[d][ii]=0;
	    
	    //if T_path is long enough we move along spatial dirs
	    if(t>=pars->Tmin)
	      for(int ii=0;ii<3;ii++)
		{
		  int i=ii+1;
		  
		  //copy T_path
		  NISSA_PARALLEL_LOOP(ivol,0,loc_vol)
		    su3_copy(TS_path[ivol],T_path[ivol]);
		  
		  //move along i up to Dmax
		  for(int d=1;d<=pars->Dmax;d++)
		    {
		      //take the product
		      NISSA_PARALLEL_LOOP(ivol,0,loc_vol)
			safe_su3_prod_su3(TS_path[ivol],TS_path[ivol],sme_conf[ivol][i]);
		      set_borders_invalid(TS_path);
		      
		      //push up the vector along i
		      su3_vec_single_shift(TS_path,i,+1);
		      
		      //if TS_path is long enough we close the path
		      if(d>=pars->Dmin)
			{
			  //copy TS_path
			  NISSA_PARALLEL_LOOP(ivol,0,loc_vol)
			    su3_copy(closed_path[ivol],TS_path[ivol]);
			  set_borders_invalid(closed_path);
			  
			  //move back along time
			  for(int tp=1;tp<=t;tp++)
			    {
			      //push dw the vector along 0
			      su3_vec_single_shift(closed_path,0,-1);
			      
			      //take the product with dag
			      NISSA_PARALLEL_LOOP(ivol,0,loc_vol)
				safe_su3_prod_su3_dag(closed_path[ivol],closed_path[ivol],sme_conf[ivol][0]);
			      set_borders_invalid(closed_path);
			    }
			  
			  //move back along space
			  for(int dp=1;dp<=d;dp++)
			    {
			      //push dw the vector along i
			      su3_vec_single_shift(closed_path,i,-1);
			      
			      //take the product with dag
			      NISSA_PARALLEL_LOOP(ivol,0,loc_vol)
				safe_su3_prod_su3_dag(closed_path[ivol],closed_path[ivol],sme_conf[ivol][i]);
			      set_borders_invalid(closed_path);
			    }
			  
			  //take the trace and store it in the point contribution
			  NISSA_PARALLEL_LOOP(ivol,0,loc_vol)
			    point_path[ivol]=
			    closed_path[ivol][0][0][RE]+closed_path[ivol][1][1][RE]+closed_path[ivol][2][2][RE];
			  
			  //reduce among all threads and ranks and summ it
			  double temp;
			  double_vector_glb_collapse(&temp,point_path,loc_vol);
			  paths[d][ii]+=temp;
			}
		    }
		}
	    
	    //print all the Dmax contributions, with ncol*nspat_dir*glb_vol normalization
	    if(t>=pars->Tmin && rank==0 && IS_MASTER_THREAD)
	      for(int d=pars->Dmin;d<=pars->Dmax;d++)
		{
		  fprintf(fout,"%d %d  %d %d",iconf,iape,t,d);
		  for(int ii=0;ii<3;ii++) fprintf(fout,"\t%16.16lg",paths[d][ii]/(3*glb_vol));
		  fprintf(fout,"\n");
		}
	    THREAD_BARRIER();
	  }
      }
    
    //close file
    if(rank==0 && IS_MASTER_THREAD) fclose(fout);
    
    //free stuff
    nissa_free(sme_conf);
    nissa_free(T_path);
    nissa_free(TS_path);
    nissa_free(closed_path);
    nissa_free(point_path);
  }}
  
  void measure_all_rectangular_paths(all_rect_meas_pars_t *pars,quad_su3 **conf_eo,int iconf,int create_output_file)
  {
    quad_su3 *conf_lx=nissa_malloc("conf_lx",loc_vol+bord_vol+edge_vol,quad_su3);
    paste_eo_parts_into_lx_conf(conf_lx,conf_eo);
    
    measure_all_rectangular_paths(pars,conf_lx,iconf,create_output_file);
    
    nissa_free(conf_lx);
  }
}
