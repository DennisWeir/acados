#include "catch/include/catch.hpp"
#include "test/sim/pendulum/casadi/casadi_pendulum.h"
#include "acados/utils/types.h"
#include "acados/utils/timing.h"
#include "acados/utils/print.h"
#include "acados/sim/sim_erk_integrator.h"
#include "test/test_utils/eigen.h"

#include "test/sim/pendulum/pendulum_helper.cpp"

extern real_t COMPARISON_TOLERANCE;
real_t COMPARISON_TOLERANCE_FD = 1e-5;
real_t FD_EPS = 1e-8;

using Eigen::MatrixXd;
using Eigen::VectorXd;

TEST_CASE("ERK simulation with adjoint sensitivities", "[simulation]") {
    int_t NX = 4;
    int_t NU = 1;
    real_t T = 0.5;

    int_t nhess = (int_t)(NX+NU+1)*(real_t)(NX+NU)/2.0;

    sim_in  sim_in, sim_in2;
    sim_out sim_out, sim_out2;
    sim_info info, info2;

    sim_RK_opts rk_opts, rk_opts2;
    sim_erk_workspace erk_work, erk_work2;

    real_t adj[NX+NU];
    real_t seed[NX+NU];

    create_ERK_integrator(&sim_in, &sim_out, &info, &rk_opts, &erk_work, NX, NU, T, false);

    // adjoint seed:
    for (int_t i = 0; i < NX; i++) seed[i] = 1.0;
    for (int_t i = 0; i < NU; i++) seed[NX+i] = 0.0;
    for (int_t i = 0; i < NX+NU; i++) sim_in.S_adj[i] = seed[i];

    SECTION("Adjoint sensitivities") {
        for (int_t i = 0; i < NX; i++) sim_in.x[i] = 0.0;
        sim_in.u[0] = 0.1;

        sim_erk(&sim_in, &sim_out, 0, &erk_work);

        for (int_t i = 0; i < NX+NU; i++) adj[i] = 0.0;
        for (int_t j = 0; j < NX+NU; j++) {
            for (int_t i = 0; i < NX; i++) {
                adj[j] += seed[i]*sim_out.S_forw[j*NX+i];
            }
        }

        print_matrix_name((char*)"stdout", (char*)"adj", adj, 1, NX+NU);
        print_matrix_name((char*)"stdout", (char*)"adj_test", sim_out.S_adj, 1, NX+NU);

        VectorXd true_adj = Eigen::Map<VectorXd>(&adj[0], NX+NU);
        VectorXd test_adj = Eigen::Map<VectorXd>(&sim_out.S_adj[0], NX+NU);
        REQUIRE(test_adj.isApprox(true_adj, COMPARISON_TOLERANCE));
    }

    real_t hess_FD[(NX+NU)*(NX+NU)];
    real_t hess_test[(NX+NU)*(NX+NU)];
    real_t hess_err[(NX+NU)*(NX+NU)];

    create_ERK_integrator(&sim_in2, &sim_out2, &info2, &rk_opts2, &erk_work2, NX, NU, T, true);

    // adjoint seed:
    for (int_t i = 0; i < NX+NU; i++) sim_in2.S_adj[i] = seed[i];

    SECTION("Symmetric second order sensitivities") {
        for (int_t i = 0; i < NX; i++) sim_in2.x[i] = 0.0;
        sim_in2.u[0] = 0.1;

        sim_erk(&sim_in2, &sim_out2, 0, &erk_work2);

        // hessian test:
        int_t index = 0;
        for (int_t j = 0; j < NX+NU; j++) {
            for (int_t i = 0; i < j; i++) {
                hess_test[j*(NX+NU)+i] = hess_test[i*(NX+NU)+j];
            }
            for (int_t i = j; i < NX+NU; i++) {
                hess_test[j*(NX+NU)+i] = sim_out2.S_hess[index];
                index++;
            }
        }

        // hessian FD:
        for (int_t s = 0; s < NX+NU; s++) {
            for (int_t i = 0; i < NX; i++) sim_in.x[i] = 0.0;
            sim_in.u[0] = 0.1;
            if ( s < NX ) {
                sim_in.x[s] = sim_in.x[s]+FD_EPS;
            } else {
                sim_in.u[s-NX] = sim_in.u[s-NX]+FD_EPS;
            }

            sim_erk(&sim_in, &sim_out, 0, &erk_work);

            for (int_t i = 0; i < NX+NU; i++)
                hess_FD[s*(NX+NU)+i] = (sim_out.S_adj[i] - adj[i])/FD_EPS;
        }

        for (int_t i = 0; i < (NX+NU)*(NX+NU); i++) {
            hess_err[i] = hess_FD[i]-hess_test[i];
        }

        print_matrix_name((char*)"stdout", (char*)"hess_FD", hess_FD, NX+NU, NX+NU);
        print_matrix_name((char*)"stdout", (char*)"hess_test", hess_test, NX+NU, NX+NU);
        print_matrix_name((char*)"stdout", (char*)"hess_err", hess_err, NX+NU, NX+NU);

        MatrixXd FD_hess = Eigen::Map<MatrixXd>(&hess_FD[0], NX+NU, NX+NU);
        MatrixXd test_hess = Eigen::Map<MatrixXd>(&hess_test[0], NX+NU, NX+NU);
        REQUIRE(test_hess.isApprox(FD_hess, COMPARISON_TOLERANCE_FD));
    }
}
