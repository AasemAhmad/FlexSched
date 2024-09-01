#pragma once
#include "ResourceManager/ResourceManager.hpp"
#include "Solution/Solution.hpp"
#include "WorkloadManager/WorkloadManager.hpp"
#include <ilcp/cp.h>

class HPCWorkloadSolverCP
{
  public:
    explicit HPCWorkloadSolverCP(const std::shared_ptr<WorkloadManager> &w,
                                 const std::vector<std::shared_ptr<Resource>> &r);
    HPCWorkloadSolverCP(const HPCWorkloadSolverCP &) = delete;
    HPCWorkloadSolverCP &operator=(const HPCWorkloadSolverCP &) = delete;

    Solution solve();

  private:
    void _initialize_resource_arrays();
    void _init_task_and_mode_arrays();
    IloIntervalVar _add_job_start_time_constraints(const JobPtr &job) const;
    void _add_job_modes_constraints(const JobPtr &job, size_t job_index);
    void _add_capacity_constraints();
    void _add_objective_and_solve(Solution &solution);
    void _set_solution(IloCP &cp, Solution &solution);
    void _set_solution_helper(IloCP &cp, Solution &solution);

    std::shared_ptr<WorkloadManager> _workload_manager;
    std::vector<std::shared_ptr<Resource>> _resources;

    IloModel _model;
    IloCumulFunctionExprArray _processes;
    IloIntArray _capacities;
    IloIntervalVarArray _tasks;
    IloIntervalVarArray2 _modes;
    IloIntExprArray _nb_compute_res_units;
    IloIntExprArray _nb_io_res_units;
    IloIntExprArray _ends;
    IloIntExprArray _starts;
};
