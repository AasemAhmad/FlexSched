
#include "ResourceManager/Algorithms/CPSolver/HPCWorkloadSolverCP.hpp"
#include "Settings.hpp"
#include "Shared/Utils.hpp"
#include "Solution/Solution.hpp"
#include <intervalset.hpp>
#include <loguru.hpp>
#include <math.h>
#include <memory>
#include <tuple>
#include <vector>

HPCWorkloadSolverCP::HPCWorkloadSolverCP(const std::shared_ptr<WorkloadManager> &w,
                                         const std::vector<std::shared_ptr<Resource>> &r)
    : _workload_manager(w), _resources(r)
{}

Solution HPCWorkloadSolverCP::solve()
{
    LOG_F(INFO, "start creating the CP model at date %g", _workload_manager->_date);
    LOG_F(INFO, "nb_jobs = %ld", _workload_manager->get_total_nb_jobs());

    Solution solution;
    IloEnv env;
    try
    {
        _model = IloModel(env);
        _tasks = IloIntervalVarArray(env, _workload_manager->get_total_nb_jobs());
        _modes = IloIntervalVarArray2(env, _workload_manager->get_total_nb_jobs());
        _nb_compute_res_units = IloIntExprArray(env, _workload_manager->get_total_nb_jobs());
        _nb_io_res_units = IloIntExprArray(env, _workload_manager->get_total_nb_jobs());
        _ends = IloIntExprArray(env);
        _starts = IloIntExprArray(env);

        size_t nb_resources = _resources.size();
        LOG_F(INFO, "nb_resources = %ld", nb_resources);

        _processes = IloCumulFunctionExprArray(env, nb_resources);
        _capacities = IloIntArray(env, nb_resources);

        for (size_t j = 0; j < this->_workload_manager->get_total_nb_jobs(); ++j)
        {
            _nb_compute_res_units[j] = IloIntExpr(env);
            _nb_io_res_units[j] = IloIntExpr(env);
        }

        _initialize_resource_arrays();

        _init_task_and_mode_arrays();

        _add_capacity_constraints();
        _add_objective_and_solve(solution);

    } catch (IloException &ex)
    {
        LOG_F(ERROR, "Error: ");
    } catch (...)
    {
        LOG_F(ERROR, "Error: ");
    }
    env.end();
    return solution;
}

void HPCWorkloadSolverCP::_initialize_resource_arrays()
{
    IloEnv env = _model.getEnv();
    size_t resource_index = 0;
    for (const auto &resource : _resources)
    {
        auto resource_type = static_cast<int>(resource->type);
        _processes[resource_type] = IloCumulFunctionExpr(env);
        _capacities[resource_type] = resource->resource_units.size();

        for (size_t i = 0; i < resource->resource_units.size(); ++i)
        {
            size_t begin = resource->available_at[i];
            if (begin > 0)
            {
                _processes[resource_index] += IloPulse(env, 0, begin, 1);
            }
        }
        ++resource_index;
    }
}

void HPCWorkloadSolverCP::_init_task_and_mode_arrays()
{
    IloEnv env = _model.getEnv();
    PPK_ASSERT_ERROR(_workload_manager->waiting_queue, "unassigned pointer, nullptr");

    size_t job_index = 0;
    for (const auto &job : *_workload_manager->waiting_queue)
    {
        LOG_F(INFO, "job modes size = %ld", job->modes.size());

        IloIntervalVar task = _add_job_start_time_constraints(job);

        _add_job_modes_constraints(job, job_index);

        _tasks[job_index] = task;
        _model.add(IloAlternative(env, task, _modes[job_index]));
        _ends.add(IloEndOf(task));
        _starts.add(IloStartOf(task));

        ++job_index;
    }
}

IloIntervalVar HPCWorkloadSolverCP::_add_job_start_time_constraints(const JobPtr &job) const
{
    IloEnv env = _model.getEnv();

    IloIntervalVar task(env);
    task.setStartMin(std::floor(_workload_manager->_date));

    if (job->info.job_type == JobType::CONFIGURABLE && job->info.current_activity_index > 0)
    {
        task.setStartMax(std::floor(_workload_manager->_date));
    }

    if (job->info.job_type == JobType::FIXED)
    {
        if (job->info.current_activity_index > 0 && !Settings::PREEMPTION_ENABLED)
        {
            task.setStartMax(std::floor(_workload_manager->_date));
        }
    }
    return task;
}

void HPCWorkloadSolverCP::_add_job_modes_constraints(const JobPtr &job, size_t job_index)
{
    IloEnv env = _model.getEnv();
    _modes[job_index] = IloIntervalVarArray(env);

    for (const auto &mode : job->modes)
    {
        IloInt compute_res_units = mode.nb_requested_compute_resource_units;
        IloInt io_res_units = mode.nb_requested_io_resource_units;
        IloInt proc_time = mode.estimated_processing_time;
        IloIntervalVar alt(env, proc_time);
        alt.setOptional();
        _modes[job_index].add(alt);

        _processes[static_cast<int>(ResourceType::COMPUTE)] += IloPulse(alt, compute_res_units);
        _processes[static_cast<int>(ResourceType::IO)] += IloPulse(alt, io_res_units);

        _nb_compute_res_units[job_index] += compute_res_units * IloPresenceOf(env, alt);
        _nb_io_res_units[job_index] += io_res_units * IloPresenceOf(env, alt);
    }
}

void HPCWorkloadSolverCP::_add_capacity_constraints()
{
    for (size_t i = 0; i < _processes.getSize(); ++i)
    {
        _model.add(_processes[i] <= _capacities[i]);
    }
}

void HPCWorkloadSolverCP::_add_objective_and_solve(Solution &solution)
{
    IloEnv env = _model.getEnv();
    IloObjective objective = IloMinimize(env, IloMax(_ends));
    _model.add(objective);

    IloCP cp(_model);
    cp.setParameter(IloCP::TimeLimit, Settings::SolverSettings::MAX_RUNTIME);
    cp.setParameter(IloCP::Workers, 1);

    if (cp.solve())
    {
        LOG_F(INFO, "model is solved");
        _set_solution(cp, solution);
    } else
    {
        LOG_F(INFO, "CP is not solvable");
    }
    cp.end();
}

void HPCWorkloadSolverCP::_set_solution(IloCP &cp, Solution &solution)
{
    switch (cp.getStatus())
    {
        using enum MODEL_STATUS;
    case IloAlgorithm::Optimal:
        solution.solution_state = convert(MODEL_SOL_OPTIMAL);
        break;
    case IloAlgorithm::Feasible:
        solution.solution_state = convert(MODEL_SOL_FEASIBLE);
        break;
    case IloAlgorithm::Infeasible:
        solution.solution_state = convert(MODEL_SOL_INFEASIBLE);
        break;
    case IloAlgorithm::Unknown:
        solution.solution_state = convert(MODEL_SOL_UNKNOWN);
        break;
    case IloAlgorithm::Unbounded:
        solution.solution_state = convert(MODEL_SOL_UNBOUNDED);
        break;
    }

    if (cp.getStatus() == IloAlgorithm::Optimal || cp.getStatus() == IloAlgorithm::Feasible)
    {
        _set_solution_helper(cp, solution);
    }
}

void HPCWorkloadSolverCP::_set_solution_helper(IloCP &cp, Solution &solution)
{
    solution.makespan = cp.getObjValue();
    solution.gap = cp.getObjGap();
    solution.objective_bound = cp.getObjBound();
    solution.runtime = cp.getInfo(IloCP::TotalTime);

    size_t job_index = 0;
    size_t task_id = 0;

    for (const auto &job : *_workload_manager->waiting_queue)
    {
        std::string id = job->id;
        SolutionInfo solution_info;
        solution_info.start_time = cp.getStartMax(_tasks[task_id]);
        solution_info.duration = cp.getValue(_ends[task_id]) - cp.getStartMax(_tasks[task_id]);
        solution_info.mode_id;
        solution_info.nb_compute_res_units = cp.getValue(_nb_compute_res_units[job_index]);
        solution_info.nb_io_res_units = cp.getValue(_nb_io_res_units[job_index]);

        ++task_id;
        ++job_index;

        const auto &[iterator, emplaced] = solution.solution_info.try_emplace(id, std::move(solution_info));
        PPK_ASSERT_ERROR(emplaced, "solution info could not be inserted");
    }
    LOG_F(INFO, "Solution details updated successfully");
}
