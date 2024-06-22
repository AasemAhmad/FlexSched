/***********************************************
 * Author: Libur Bukata
 * Modified by: Aasem Ahmad
 ***********************************************/

#include "External/ILPSolverModel/ILPSolverInterface.hpp"
#include "External/pempek_assert.hpp"
#include "Settings.hpp"
#include "gurobi_c++.h"
#include "loguru.hpp"
#include <format>
#include <tuple>

void init_variables(const Solution &init_solution, std::vector<GRBVar> &vars);

std::vector<GRBVar> generate_problem_gurobi(const Solution &init_solution, const ILPSolverModel &ilp_model,
                                            GRBModel &grb_model)
{
    std::vector<GRBVar> vars;
    grb_model.set(GRB_IntAttr_ModelSense, (ilp_model.obj == ObjectiveFunction::MINIMIZE ? GRB_MINIMIZE : GRB_MAXIMIZE));

    size_t var_index = 0;
    for (const auto &var_x : ilp_model.vector_x)
    {
        GRBVar grb_var;
        double c_value = ilp_model.vector_c[var_index];
        std::string desc = ilp_model.varDesc[var_index];

        switch (var_x.type)
        {

        case DecisionVariableType::FLT:
            grb_var = grb_model.addVar(var_x.lower_bound, var_x.upper_bound, c_value, GRB_CONTINUOUS, desc);
            break;
        case DecisionVariableType::BIN:
            grb_var = grb_model.addVar(var_x.lower_bound, var_x.upper_bound, c_value, GRB_BINARY, desc);
            break;
        default:
            grb_var = grb_model.addVar(var_x.lower_bound, var_x.upper_bound, c_value, GRB_INTEGER, desc);
        }
        vars.push_back(grb_var);
        ++var_index;
    }

    grb_model.update();

    if (!ilp_model.gurobiC.empty())
    {

        PPK_ASSERT_ERROR(ilp_model.get_nb_variables() == ilp_model.gurobiC.size(),
                         "Invalid initialization of gurobi criterion functions!");

        for (size_t v = 0; v < ilp_model.get_nb_variables(); ++v)
        {
            const std::tuple<std::vector<double>, std::vector<double>> *cf = ilp_model.gurobiC[v];
            if (cf == nullptr)
            {
                continue;
            }

            const std::vector<double> &x = get<0>(*cf);
            const std::vector<double> &y = get<1>(*cf);

            PPK_ASSERT_ERROR(x.size() == y.size(), "Invalid initialization of Gurobi functions!");

            if (!x.empty() && !y.empty())
            {
                grb_model.setPWLObj(vars[v], x.size(), (double *)x.data(), (double *)y.data());
            }
        }
    }

    grb_model.update();

    var_index = 0;
    for (const auto &var_op : ilp_model.vector_op)
    {
        GRBLinExpr expr;

        for (const auto &[index, value] : ilp_model.matrix_A[var_index])
        {
            expr += value * vars[index];
        }

        double b_value = ilp_model.vector_b[var_index];
        std::string desc = ilp_model.conDesc[var_index];

        switch (var_op)
        {
            using enum Operator;
        case LESS_EQUAL:
            grb_model.addConstr(expr <= b_value, desc);
            break;
        case EQUAL:
            grb_model.addConstr(expr == b_value, desc);
            break;
        case GREATER_EQUAL:
            grb_model.addConstr(expr >= b_value, desc);
        }
        ++var_index;
    }

    // TODO enable initilization of solution in case
    if (Settings::Solver::INIT_ILP_SOLUTION)
    {
        init_variables(init_solution, vars);
    }

    return vars;
}

std::vector<GRBEnv> threadEnv;

std::string GurobiSolver::get_solver_identification() const
{
    std::string identification = "Gurobi";
    int major = -1;
    int minor = -1;
    int release = -1;
    GRBversion(&major, &minor, &release);
    if (major != -1 && minor != -1 && release != -1)
    {
        identification += std::format(" {}.{}.{}", major, minor, release);
    }
    return identification;
}

void GurobiSolver::initialize_local_environments(size_t nb_of_threads) const
{
    try
    {
        threadEnv.resize(nb_of_threads);
    } catch (GRBException &e)
    {
        // FIXME
    }
}

void init_variables(const Solution &init_solution, std::vector<GRBVar> &vars)
{
    // TODO
}

SolutionILP GurobiSolver::solve_ilp(Solution &init_solution, const ILPSolverModel &ilp_model, bool verbose, double gap,
                                    double time_limit, size_t nb_of_threads, size_t thread_id) const
{

    SolutionILP solution_ilp;

    try
    {
        PPK_ASSERT_ERROR(thread_id >= 0, "thread_id has negative value");

        PPK_ASSERT_ERROR((threadEnv.size() > static_cast<size_t>(thread_id)),
                         "Either the Gurobi environments were not initialized or the number of concurrent threads "
                         "specified incorrectly in the initialize_local_environments method!");

        threadEnv[thread_id].set(GRB_IntParam_Threads, static_cast<int>(nb_of_threads));

        if (time_limit != 0.0)
        {
            threadEnv[thread_id].set(GRB_DoubleParam_TimeLimit, time_limit);
        }
        if (gap != 0.0)
        {
            threadEnv[thread_id].set(GRB_DoubleParam_MIPGap, gap);
        }

        if (verbose)
        {
            threadEnv[thread_id].set(GRB_IntParam_OutputFlag, 1);
            threadEnv[thread_id].set(GRB_IntParam_LogToConsole, 1);
        } else
        {
            threadEnv[thread_id].set(GRB_IntParam_OutputFlag, 0);
            threadEnv[thread_id].set(GRB_IntParam_LogToConsole, 0);
        }

        GRBModel grb_model(threadEnv[thread_id]);

        const std::vector<GRBVar> &vars = generate_problem_gurobi(init_solution, ilp_model, grb_model);

        grb_model.optimize();

        switch (grb_model.get(GRB_IntAttr_Status))
        {
        case GRB_OPTIMAL:
            if (gap == 0.0)
            {
                solution_ilp.status = MODEL_STATUS::MODEL_SOL_OPTIMAL;
            } else
            {
                solution_ilp.status = MODEL_STATUS::MODEL_SOL_FEASIBLE;
            }
            break;
        case GRB_TIME_LIMIT:
            if (grb_model.get(GRB_IntAttr_SolCount) > 0)
            {
                solution_ilp.status = MODEL_STATUS::MODEL_SOL_FEASIBLE;
            } else
            {
                solution_ilp.status = MODEL_STATUS::MODEL_SOL_UNKNOWN;
            }
            break;
        case GRB_INFEASIBLE: {
            solution_ilp.status = MODEL_STATUS::MODEL_SOL_INFEASIBLE;
            break;
        }
        case GRB_UNBOUNDED: {
            solution_ilp.status = MODEL_STATUS::MODEL_SOL_UNBOUNDED;
            break;
        }
        default:
            solution_ilp.status = MODEL_STATUS::MODEL_SOL_UNKNOWN;
        }

        if (solution_ilp.status == MODEL_STATUS::MODEL_SOL_OPTIMAL ||
            solution_ilp.status == MODEL_STATUS::MODEL_SOL_FEASIBLE)
        {
            solution_ilp.criterion = grb_model.get(GRB_DoubleAttr_ObjVal);
            for (long v = 0; v < grb_model.get(GRB_IntAttr_NumVars); ++v)
            {
                solution_ilp.solution.push_back(vars[v].get(GRB_DoubleAttr_X));
            }
        }

        if (grb_model.get(GRB_IntAttr_IsMIP))
        {
            solution_ilp.bound = grb_model.get(GRB_DoubleAttr_ObjBound);
        } else if (solution_ilp.status == MODEL_STATUS::MODEL_SOL_OPTIMAL)
        {
            solution_ilp.bound = solution_ilp.criterion;
        }

    } catch (GRBException &e)
    {
        // FIXME
    } catch (...)
    {

        // FIXME
    }

    return solution_ilp;
}
