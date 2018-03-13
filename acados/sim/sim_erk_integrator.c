/*
 *    This file is part of acados.
 *
 *    acados is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Lesser General Public
 *    License as published by the Free Software Foundation; either
 *    version 3 of the License, or (at your option) any later version.
 *
 *    acados is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public
 *    License along with acados; if not, write to the Free Software Foundation,
 *    Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

// standard
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
// acados
#include "acados/utils/mem.h"
#include "acados/sim/sim_common.h"
#include "acados/sim/sim_erk_integrator.h"

//#include "acados/sim/sim_casadi_wrapper.h"



int sim_erk_model_calculate_size(void *config, sim_dims *dims)
{

	int size = 0;

	size += sizeof(erk_model);

	return size;

}



void *sim_erk_model_assign(void *config, sim_dims *dims, void *raw_memory)
{

	char *c_ptr = (char *) raw_memory;

	erk_model *model = (erk_model *) c_ptr;
	c_ptr += sizeof(erk_model);

	return model;

}



void sim_erk_model_set_forward_vde(sim_in *in, void *fun)
{
    erk_model *model = in->model;
    model->forw_vde_expl = (external_function_generic *) fun;
}



void sim_erk_model_set_adjoint_vde(sim_in *in, void *fun)
{
    erk_model *model = in->model;
    model->adj_vde_expl = (external_function_generic *) fun;
}



int sim_erk_opts_calculate_size(void *config_, sim_dims *dims)
{

	sim_solver_config *config = config_;

	int ns_max = NS_MAX;

    int size = sizeof(sim_rk_opts);

    size += ns_max * ns_max * sizeof(double);  // A_mat
    size += ns_max * sizeof(double);  // b_vec
    size += ns_max * sizeof(double);  // c_vec

    make_int_multiple_of(8, &size);
    size += 1 * 8;

    return size;
}



void *sim_erk_opts_assign(void *config_, sim_dims *dims, void *raw_memory)
{
	sim_solver_config *config = config_;

	int ns_max = NS_MAX;

    char *c_ptr = (char *) raw_memory;

    sim_rk_opts *opts = (sim_rk_opts *) c_ptr;
    c_ptr += sizeof(sim_rk_opts);

    align_char_to(8, &c_ptr);

    assign_double(ns_max*ns_max, &opts->A_mat, &c_ptr);
    assign_double(ns_max, &opts->b_vec, &c_ptr);
    assign_double(ns_max, &opts->c_vec, &c_ptr);

    assert((char*)raw_memory + sim_erk_opts_calculate_size(config, dims) >= c_ptr);

    opts->newton_iter = 0;
    opts->scheme = NULL;
    opts->jac_reuse = false;

    return (void *)opts;
}



void sim_erk_opts_initialize_default(void *config_, sim_dims *dims, void *opts_)
{
	sim_solver_config *config = config_;
    sim_rk_opts *opts = opts_;

	opts->ns = 4; // ERK 4
    int ns = opts->ns;

    assert(ns == 4 && "only number of stages = 4 implemented!");

    memcpy(opts->A_mat,((real_t[]){0, 0.5, 0, 0, 0, 0, 0.5, 0, 0, 0, 0, 1, 0, 0, 0, 0}),
        sizeof(*opts->A_mat) * (ns * ns));
    memcpy(opts->b_vec, ((real_t[]){1.0 / 6, 2.0 / 6, 2.0 / 6, 1.0 / 6}),
        sizeof(*opts->b_vec) * (ns));
    memcpy(opts->c_vec, ((real_t[]){0.0, 0.5, 0.5, 1.0}),
        sizeof(*opts->c_vec) * (ns));

    opts->num_steps = 2;
    opts->num_forw_sens = dims->nx + dims->nu;
    opts->sens_forw = true;
    opts->sens_adj = false;
    opts->sens_hess = false;
}



void sim_erk_opts_update_tableau(void *config_, sim_dims *dims, void *opts_)
{

	sim_solver_config *config = config_;
    sim_rk_opts *opts = opts_;

    int ns = opts->ns;

    assert(ns == 4 && "only number of stages = 4 implemented!");

    assert(ns <= NS_MAX && "ns > NS_MAX!");

    memcpy(opts->A_mat,((real_t[]){0, 0.5, 0, 0, 0, 0, 0.5, 0, 0, 0, 0, 1, 0, 0, 0, 0}),
        sizeof(*opts->A_mat) * (ns * ns));
    memcpy(opts->b_vec, ((real_t[]){1.0 / 6, 2.0 / 6, 2.0 / 6, 1.0 / 6}),
        sizeof(*opts->b_vec) * (ns));
    memcpy(opts->c_vec, ((real_t[]){0.0, 0.5, 0.5, 1.0}),
        sizeof(*opts->c_vec) * (ns));

	return;
}



int sim_erk_memory_calculate_size(void *config, sim_dims *dims, void *opts_)
{
    return 0;
}



void *sim_erk_memory_assign(void *config, sim_dims *dims, void *opts_, void *raw_memory)
{
    return NULL;
}



int sim_erk_workspace_calculate_size(void *config_, sim_dims *dims, void *opts_)
{
	sim_solver_config *config = config_;
	sim_rk_opts *opts = opts_;

    int ns = opts->ns;

    int nx = dims->nx;
    int nu = dims->nu;
    int nf = opts->num_forw_sens;

    int nX = nx*(1+nf); // (nx) for ODE and (nf*nx) for VDE
    int nhess = (nf + 1) * nf / 2;
    uint num_steps = opts->num_steps;  // number of steps

    int size = sizeof(sim_erk_workspace);

    size += (nX + nu) * sizeof(double); // rhs_forw_in

    if(opts->sens_adj){
        size += num_steps * ns * nX * sizeof(double); // K_traj
        size += (num_steps + 1) * nX *sizeof(double); // out_forw_traj
    }else{
        size += ns * nX * sizeof(double); // K_traj
        size += nX *sizeof(double); // out_forw_traj
    }

    if (opts->sens_hess && opts->sens_adj){
        size += (nx + nX + nu) * sizeof(double); //rhs_adj_in
        size += (nx + nu + nhess) * sizeof(double); //out_adj_tmp
        size += ns * (nx + nu + nhess) * sizeof(double); //adj_traj
    }else if (opts->sens_adj){
        size += (nx * 2 + nu) * sizeof(double); //rhs_adj_in
        size += (nx + nu)* sizeof(double); //out_adj_tmp
        size += ns * (nx + nu) * sizeof(double); //adj_traj
    }

    make_int_multiple_of(8, &size);
    size += 1 * 8;

    return size;
}



static void *sim_erk_cast_workspace(void *config_, sim_dims *dims, void *opts_, void *raw_memory)
{
	sim_solver_config *config = config_;
	sim_rk_opts *opts = opts_;

    int ns = opts->ns;

    int nx = dims->nx;
    int nu = dims->nu;
    int nf = opts->num_forw_sens;

    int nX = nx*(1+nf); // (nx) for ODE and (nf*nx) for VDE
    int nhess = (nf + 1) * nf / 2;
    int num_steps = opts->num_steps;  // number of steps

    char *c_ptr = (char *)raw_memory;

    sim_erk_workspace *workspace = (sim_erk_workspace *) c_ptr;
    c_ptr += sizeof(sim_erk_workspace);

    align_char_to(8, &c_ptr);

    assign_double(nX + nu, &workspace->rhs_forw_in, &c_ptr);

    if(opts->sens_adj)
    {
        assign_double(ns*num_steps*nX, &workspace->K_traj, &c_ptr);
        assign_double((num_steps + 1)*nX, &workspace->out_forw_traj, &c_ptr);
    } else
    {
        assign_double(ns*nX, &workspace->K_traj, &c_ptr);
        assign_double(nX, &workspace->out_forw_traj, &c_ptr);
    }

    if (opts->sens_hess && opts->sens_adj)
    {
        assign_double(nx+nX+nu, &workspace->rhs_adj_in, &c_ptr);
        assign_double(nx+nu+nhess, &workspace->out_adj_tmp, &c_ptr);
        assign_double(ns*(nx+nu+nhess), &workspace->adj_traj, &c_ptr);
    } else if (opts->sens_adj)
    {
        assign_double((nx*2+nu), &workspace->rhs_adj_in, &c_ptr);
        assign_double(nx+nu, &workspace->out_adj_tmp, &c_ptr);
        assign_double(ns*(nx+nu), &workspace->adj_traj, &c_ptr);
    }

    assert((char*)raw_memory + sim_erk_workspace_calculate_size(config, dims, opts_) >= c_ptr);

    return (void *)workspace;
}



int sim_erk(void *config_, sim_in *in, sim_out *out, void *opts_, void *mem_, void *work_)
{
	sim_solver_config *config = config_;
	sim_rk_opts *opts = opts_;

    int ns = opts->ns;

    sim_dims *dims = in->dims;
    sim_erk_workspace *workspace = (sim_erk_workspace *) sim_erk_cast_workspace(config, dims, opts, work_);

    int i, j, s, istep;
    double a = 0, b =0; // temp values of A_mat and b_vec
    int nx = dims->nx;
    int nu = dims->nu;

    int nf = opts->num_forw_sens;
    if (!opts->sens_forw)
        nf = 0;

    int nhess = (nf + 1) * nf / 2;
    int nX = nx * (1 + nf);

    double *x = in->x;
    double *u = in->u;
    double *S_forw_in = in->S_forw;
    int num_steps = opts->num_steps;
    double step = in->T/num_steps;

    double *S_adj_in = in->S_adj;

    double *A_mat = opts->A_mat;
    double *b_vec = opts->b_vec;
    //    double *c_vec = opts->c_vec;

    double *K_traj = workspace->K_traj;
    double *forw_traj = workspace->out_forw_traj;
    double *rhs_forw_in = workspace->rhs_forw_in;

    double *adj_tmp = workspace->out_adj_tmp;
    double *adj_traj = workspace->adj_traj;
    double *rhs_adj_in = workspace->rhs_adj_in;

    double *xn = out->xn;
    double *S_forw_out = out->S_forw;
    double *S_adj_out = out->S_adj;
    double *S_hess_out = out->S_hess;

	erk_model *model = in->model;

    acados_timer timer, timer_ad;
    double timing_ad = 0.0;

    acados_tic(&timer);

/************************************************
* forward sweep
************************************************/

	// initialize integrator variables
    for (i = 0; i < nx; i++)
        forw_traj[i] = x[i];  // x0
    if (opts->sens_forw)
	{
        for (i = 0; i < nx * nf; i++)
            forw_traj[nx + i] = S_forw_in[i];  // sensitivities
    }
    for (i = 0; i < nu; i++)
        rhs_forw_in[nX + i] = u[i]; // controls

    for (istep = 0; istep < num_steps; istep++)
	{
        if (opts->sens_adj)
		{
            K_traj = workspace->K_traj + istep * ns * nX;
            forw_traj = workspace->out_forw_traj + (istep + 1) * nX;
            for (i = 0; i < nX; i++)
                forw_traj[i] = forw_traj[i - nX];
        }

        for (s = 0; s < ns; s++)
		{
            for (i = 0; i < nX; i++)
                rhs_forw_in[i] = forw_traj[i];
            for (j = 0; j < s; j++)
			{
                a = A_mat[j * ns + s];
                if (a!=0){
                    a *= step;
                    for (i = 0; i < nX; i++)
                        rhs_forw_in[i] += a * K_traj[j * nX + i];
                }
            }

            acados_tic(&timer_ad);
            model->forw_vde_expl->evaluate(model->forw_vde_expl, rhs_forw_in, K_traj+s*nX);  // forward VDE evaluation
            timing_ad += acados_toc(&timer_ad);
        }
        for (s = 0; s < ns; s++)
		{
            b = step * b_vec[s];
            for (i = 0; i < nX; i++)
                forw_traj[i] += b * K_traj[s * nX + i];  // ERK step
        }
    }

	// store trajectory
    for (i = 0; i < nx; i++)
        xn[i] = forw_traj[i];
	// store forward sensitivities
    if (opts->sens_forw)
	{
        for (i = 0; i < nx * nf; i++)
            S_forw_out[i] = forw_traj[nx + i];
    }

/************************************************
* adjoint sweep
************************************************/

    if (opts->sens_adj)
	{

		// initialize integrator variables
        for (i = 0; i < nx; i++)
            adj_tmp[i] = S_adj_in[i];
        for (i = 0; i < nu; i++)
            adj_tmp[nx+i] = 0.0;

        int nForw = nx;
        int nAdj = nx + nu;
        if (opts->sens_hess)
		{
            nForw = nX;
            nAdj = nx + nu + nhess;
            for (i = 0; i < nhess; i++)
                adj_tmp[nx + nu + i] = 0.0;
        }

//        printf("\nnFOrw=%d nAdj=%d\n", nForw, nAdj);

        for (i = 0; i < nu; i++)
            rhs_adj_in[nForw + nx + i] = u[i];

        for (istep = num_steps - 1; istep > -1; istep--)
		{
            K_traj = workspace->K_traj + istep * ns * nX;
            forw_traj = workspace->out_forw_traj + (istep+1) * nX;
            for (s = ns - 1; s > -1; s--) {
                // forward variables:
                for (i = 0; i < nForw; i++)
                    rhs_adj_in[i] = forw_traj[i]; // extract x trajectory
                for (j = 0; j < s; j++) {
                    a = A_mat[j * ns + s];
                    if (a!=0){
                        a*=step;
                        for (i = 0; i < nForw; i++)
                            rhs_adj_in[i] += a *K_traj[j * nX + i];
                    } // plus k traj
                }
                // adjoint variables:
                b = step * b_vec[s];
                for (i = 0; i < nx; i++)
                    rhs_adj_in[nForw + i] = b * adj_tmp[i];
                for (j = s + 1; j < ns; j++)
				{
                    a = A_mat[s * ns + j];
                    if (a!=0)
					{
                        a *= step;
                        for (i = 0; i < nx; i++)
                            rhs_adj_in[nForw + i] += a * adj_traj[j * nAdj + i];
                    }
                }
                acados_tic(&timer_ad);
                if (opts->sens_hess)
				{
                    model->hess_ode_expl->evaluate(model->hess_ode_expl, rhs_adj_in, adj_traj+s*nAdj);
                }
				else
				{
                    model->adj_vde_expl->evaluate(model->adj_vde_expl, rhs_adj_in, adj_traj+s*nAdj); // adjoint VDE evaluation
                }
                timing_ad += acados_toc(&timer_ad);

                // printf("\nadj_traj:\n");
                // for (int ii=0;ii<ns*nAdj;ii++)
                //     printf("%3.1f ", adj_traj[ii]);
            }
            for (s = 0; s < ns; s++)
                for (i = 0; i < nAdj; i++)
                    adj_tmp[i] += adj_traj[s * nAdj + i];  // ERK step
        }

		// store adjoint sensitivities
        for (i = 0; i < nx + nu; i++)
            S_adj_out[i] = adj_tmp[i];
		// store hessian
        if (opts->sens_hess)
		{
            for (i = 0; i < nhess; i++)
                S_hess_out[i] = adj_tmp[nx + nu + i];
        }
    }

	// store timings
    out->info->CPUtime = acados_toc(&timer);
    out->info->LAtime = 0.0;
    out->info->ADtime = timing_ad;

	// return
    return 0;  // success

}



void sim_erk_config_initialize_default(void *config_)
{

	sim_solver_config *config = config_;

	config->evaluate = &sim_erk;
	config->opts_calculate_size = &sim_erk_opts_calculate_size;
	config->opts_assign = &sim_erk_opts_assign;
	config->opts_initialize_default = &sim_erk_opts_initialize_default;
	config->opts_update_tableau = &sim_erk_opts_update_tableau;
	config->memory_calculate_size = &sim_erk_memory_calculate_size;
	config->memory_assign = &sim_erk_memory_assign;
	config->workspace_calculate_size = &sim_erk_workspace_calculate_size;
	config->model_calculate_size = &sim_erk_model_calculate_size;
	config->model_assign = &sim_erk_model_assign;
    config->model_set_forward_vde = &sim_erk_model_set_forward_vde;
    config->model_set_adjoint_vde = &sim_erk_model_set_adjoint_vde;
	config->config_initialize_default = &sim_erk_config_initialize_default;

	return;

}
