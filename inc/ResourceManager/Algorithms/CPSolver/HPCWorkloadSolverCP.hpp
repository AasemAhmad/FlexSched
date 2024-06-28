#pragma once

//FIXME
#include "ProblemInstance/ProblemInstance.hpp"
#include "Solution/Solution.hpp"
#include <ilcp/cp.h>

class CPSolver
{
  public:
    explicit CPSolver(const ProblemInstance &);
    CPSolver(const CPSolver &) = delete;
    CPSolver &operator=(const CPSolver &) = delete;

    Solution solve();

  private:
    void init_resource_arrays(const IloModel &model);
    void add_job_start_time_constraints(const IloModel &model);
    void add_job_modes_constraints(const IloModel &model);
    void add_precedence_constraints(const IloModel &model) const;
    void add_capacity_constraints(const IloModel &model) const;
    void solve_cp_model(IloCP &cp, const IloModel &model) const;
    void set_solution_status(const IloCP &cp, Solution &solution) const;
    void set_solution(const IloCP &cp, Solution &solution) const;
    void add_objective(const IloModel &) const;

    const ProblemInstance &problem_instance;
    IloCumulFunctionExprArray processes;
    IloIntArray capacities;
    IloIntervalVarArray tasks;
    IloIntervalVarArray2 modes;
    IloIntExprArray starts;
    IloIntExprArray ends;
};
