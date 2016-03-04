#ifdef HAVE_CONFIG_H
 #include "config.hpp"
#endif

#include "base/thread_macros.hpp"
#include "hmc/backfield.hpp"
#include "hmc/gauge/gluonic_action.hpp"
#include "hmc/gauge/topological_action.hpp"
#include "hmc/momenta/momenta_action.hpp"
#include "hmc/multipseudo/multipseudo_rhmc_step.hpp"
#include "geometry/geometry_eo.hpp"
#include "inverters/staggered/cgm_invert_stD2ee_m2.hpp"
#include "linalgs/linalgs.hpp"
#ifdef USE_THREADS
 #include "routines/thread.hpp"
#endif

#include <algorithm>

namespace nissa
{
  //compute quark action for a set of quark
  THREADABLE_FUNCTION_8ARG(compute_quark_action, double*,glb_action, quad_su3**,eo_conf, int,nfl, std::vector<quad_u1**>,u1b, std::vector<std::vector<pseudofermion_t> >*,pf, std::vector<quark_content_t>,quark_content, hmc_evol_pars_t*,simul_pars, std::vector<rat_approx_t>*,rat_appr)
  {
    //allocate chi
    color *chi_e=nissa_malloc("chi_e",loc_volh,color);
    
    //quark action
    (*glb_action)=0;
    for(int ifl=0;ifl<nfl;ifl++)
      {
	//add background field
	add_backfield_to_conf(eo_conf,u1b[ifl]);
	
	for(int ipf=0;ipf<simul_pars->npseudo_fs[ifl];ipf++)
	  {
	    verbosity_lv1_master_printf("Computing action for flavour %d/%d, pseudofermion %d/%d\n",ifl+1,nfl,ipf+1,simul_pars->npseudo_fs[ifl]);
	    
	    //compute chi with background field
	    double flav_action;
	    switch(quark_content[ifl].discretiz)
	      {
	      case ferm_discretiz::ROOT_STAG:
		summ_src_and_all_inv_stD2ee_m2_cgm(chi_e,eo_conf,&(*rat_appr)[3*ifl+1],1000000,simul_pars->pf_action_residue,(*pf)[ifl][ipf].stag);
		double_vector_glb_scalar_prod(&flav_action,(double*)chi_e,(double*)((*pf)[ifl][ipf].stag),loc_volh*2*NCOL);
		break;
		default: crash("still not implemented");
	      }
	    
	    //compute scalar product
	    (*glb_action)+=flav_action;
	  }
	
	//remove background field
	rem_backfield_from_conf(eo_conf,u1b[ifl]);
      }
    
    //free
    nissa_free(chi_e);
  }
  THREADABLE_FUNCTION_END
  
  //Compute the total action of the rooted staggered e/o improved theory
  THREADABLE_FUNCTION_9ARG(full_theory_action, double*,tot_action, quad_su3**,eo_conf, quad_su3**,sme_conf, quad_su3**,H, std::vector<std::vector<pseudofermion_t> > *,pf, theory_pars_t*,theory_pars, hmc_evol_pars_t*,simul_pars, std::vector<rat_approx_t>*,rat_appr, double,external_quark_action)
  {
    verbosity_lv1_master_printf("Computing action\n");
    
    //compute the three parts of the action
    double quark_action;
    if(external_quark_action>=0)
      {
	verbosity_lv1_master_printf("No need to compute pseudofermion action\n");
	quark_action=external_quark_action;
      }
    else compute_quark_action(&quark_action,sme_conf,theory_pars->nflavs(),theory_pars->backfield,pf,theory_pars->quarks,simul_pars,rat_appr);
    verbosity_lv1_master_printf("Quark_action: %16.16lg\n",quark_action);
    
    //gauge action
    double gluon_action;
    gluonic_action(&gluon_action,eo_conf,theory_pars->gauge_action_name,theory_pars->beta);
    verbosity_lv1_master_printf("Gluon_action: %16.16lg\n",gluon_action);
    
    //momenta action
    double mom_action=momenta_action(H);
    verbosity_lv1_master_printf("Mom_action: %16.16lg\n",mom_action);
    
    double topo_action=(theory_pars->topotential_pars.flag?topotential_action(eo_conf,theory_pars->topotential_pars):0);
    if(theory_pars->topotential_pars.flag) verbosity_lv1_master_printf("Topological_action: %16.16lg\n",topo_action);
    
    //total action
    (*tot_action)=quark_action+gluon_action+mom_action+topo_action;
  }
  THREADABLE_FUNCTION_END
}