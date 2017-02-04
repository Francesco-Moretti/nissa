#ifndef _ROOTST_EOIMPR_EIGENVALUES_HPP
#define _ROOTST_EOIMPR_EIGENVALUES_HPP

#include "new_types/new_types_definitions.hpp"

namespace nissa
{
  double eo_color_norm2(color *v);
  double eo_color_normalize(color *out,color *in,double norm);
  double max_eigenval(quark_content_t &quark_content,quad_su3 **eo_conf,int niters);
  void rootst_eoimpr_set_expansions(hmc_evol_pars_t *evol_pars,quad_su3 **eo_conf,theory_pars_t *theory_pars);
}

#endif