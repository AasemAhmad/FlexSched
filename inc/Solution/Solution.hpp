#pragma once

#include "Settings.hpp"
#include "WorkloadManager/WorkloadManager.hpp"
#include <map>
#include <string>

enum class MODEL_STATUS
{
    MODEL_SOL_OPTIMAL,
    MODEL_SOL_FEASIBLE,
    MODEL_SOL_INFEASIBLE,
    MODEL_SOL_UNBOUNDED,
    MODEL_SOL_BOUNDED,
    MODEL_SOL_INFEASIBLE_OR_UNBOUNDED,
    MODEL_SOL_UNKNOWN,
    MODEL_SOL_ERROR
};

enum class SolutionState
{
    OPTIMAL,
    FEASIBLE,
    INFEASIBLE,
    UNBOUNDED,
    INFEASIBLE_OR_UNBOUNDED,
    BOUNDED,
    ERROR,
    UNKNOWN
};

SolutionState convert(const MODEL_STATUS &model_status);

std::string convert_solution_state_to_string(const SolutionState &solution_state);

struct SolutionInfo
{
    size_t start_time;
    size_t duration;
    size_t mode_id;
    size_t nb_compute_res_units;
    size_t nb_io_res_units;
    std::string get_job_allocation_as_string() const;
};

struct Solution
{
    SolutionState solution_state = SolutionState::UNKNOWN;
    double gap = 0;
    double objective_bound = -1;
    size_t makespan = 0;
    double runtime = 0.0;
    double mem_usage = 0.0;
    std::map<std::string, SolutionInfo> solution_info;
    std::string get_solution_as_string() const;
};