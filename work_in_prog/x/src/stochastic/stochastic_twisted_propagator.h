#ifndef _stochastic_twisted_propagator_H
#define _stochastic_twisted_propagator_H

#include "../../../../src/new_types/new_types_definitions.h"

#include "../routines/fourier.h"

void generate_stochastic_A_twisted_propagator(spin1prop *q_A,spin1prop *q_prop,quark_info qu,spin1field *g_A,gluon_info gl);
void generate_stochastic_A_dag_twisted_propagator(spin1prop *q_A,spin1prop *q_prop,quark_info qu,spin1field *g_A,gluon_info gl);
void generate_stochastic_A_B_dag_twisted_propagator(spin1prop *q_AB,spin1prop *q_prop,quark_info qu,spin1field *g_A,spin1field *g_B,gluon_info gl);
void generate_stochastic_A_B_dag_twisted_propagator_source(spin1prop *q_AB,spin1prop *q_prop,quark_info qu,spin1field *g_A,spin1field *g_B,gluon_info gl);

#endif
