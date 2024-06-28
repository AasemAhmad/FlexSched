// FIXME

#include "Algorithms/ILPOptimizationModel/VariableMappingBuilder.hpp"
#include "External/pempek_assert.hpp"
#include "ProblemInstance/ProblemInstance.hpp"
#include "Shared/Queue.hpp"
#include "Shared/Utils.hpp"
#include <cmath>
#include <format>

size_t calculate_processing_time_upper_bound(const JobConstPtr &job)
{
    auto upper_bound_iterator = std::ranges::max_element(
        job->modes, [](const Mode &a, const Mode &b) { return a.processing_time < b.processing_time; });

    PPK_ASSERT_ERROR(upper_bound_iterator != job->modes.end(), "Error while calculating the upper bound");

    return upper_bound_iterator->processing_time;
}

TimeIndexedModelVariableMapping::TimeIndexedModelVariableMapping(const ProblemInstance &problem_instance)
    : problem_instance(problem_instance)
{
    PPK_ASSERT_ERROR(problem_instance.makespan_upper_bound > 0, "Makespan upper must be strictly positive");
    this->add_objective_function_variables();
    this->add_jobs_start_time_variables();
    this->add_jobs_processing_time_variables();
    this->add_jobs_start_time_binary_variables();
    this->add_jobs_resources_allocation_variables();
}

size_t TimeIndexedModelVariableMapping::get_nb_variables() const
{
    return (c_max.size() + x.size() + p.size() + s.size() + y.size());
}

void TimeIndexedModelVariableMapping::add_objective_function_variables()
{
    const std::source_location loc = std::source_location::current();

    size_t idx = get_nb_variables();
    variables.emplace_back(DecisionVariableType::INT, 0, problem_instance.makespan_upper_bound);
    var_desc.emplace_back(std::format("c_max_{}", idx));
    set_value(c_max, std::to_string(1), idx, loc);
}

void TimeIndexedModelVariableMapping::add_jobs_processing_time_variables()
{
    const std::source_location loc = std::source_location::current();
    size_t idx = get_nb_variables();

    for (const JobConstPtr &job : this->problem_instance.job_queue)
    {
        variables.emplace_back(DecisionVariableType::INT, 0,
                               static_cast<double>(calculate_processing_time_upper_bound(job)));
        var_desc.emplace_back(std::format("p_{}", job->id));
        set_value(p, job->id, idx, loc);
        ++idx;
    }
}

void TimeIndexedModelVariableMapping::add_jobs_start_time_variables()
{
    const std::source_location loc = std::source_location::current();

    size_t idx = get_nb_variables();
    for (const JobConstPtr &job : this->problem_instance.job_queue)
    {
        variables.emplace_back(DecisionVariableType::INT, 0,
                               static_cast<double>(problem_instance.makespan_upper_bound));
        var_desc.emplace_back(std::format("s_{}", job->id));
        set_value(s, job->id, idx, loc);
        ++idx;
    }
}

void TimeIndexedModelVariableMapping::add_jobs_start_time_binary_variables()
{
    const std::source_location loc = std::source_location::current();

    size_t idx = get_nb_variables();

    for (const JobConstPtr &job : this->problem_instance.job_queue)
    {
        for (size_t t = 0; t < problem_instance.makespan_upper_bound; ++t)
        {
            size_t mode_id = 1;
            for ([[maybe_unused]] const auto &_ : job->modes)
            {
                PPK_ASSERT_ERROR(mode_id <= job->modes.size(), "Invalid value %ld", mode_id);

                variables.emplace_back(DecisionVariableType::BIN, 0.0, 1.0);
                var_desc.emplace_back(std::format("x_{{{}#{}#{}}}", job->id, mode_id, t));
                set_value(x, {job->id, std::to_string(mode_id), std::to_string(t)}, idx, loc);
                ++idx;
                ++mode_id;
            }
        }
    }
}

void TimeIndexedModelVariableMapping::add_jobs_resources_allocation_variables()
{
    // TODO if necessary
}

TimeIndexedModelVariableMapping::map1to1 lookup(const std::vector<double> &solution,
                                                const TimeIndexedModelVariableMapping::map1to1 &mapping)
{
    const std::source_location loc = std::source_location::current();

    TimeIndexedModelVariableMapping::map1to1 my_map;
    for (const auto &[key, value] : mapping)
    {
        PPK_ASSERT_ERROR(value < solution.size(), "Invalid index");
        set_value(my_map, key, solution[value], loc);
    }
    return my_map;
}

TimeIndexedModelVariableMapping::map2to1 lookup(const std::vector<double> &solution,
                                                const TimeIndexedModelVariableMapping::map2to1 &mapping)
{
    const std::source_location loc = std::source_location::current();

    TimeIndexedModelVariableMapping::map2to1 my_map;
    for (const auto &[key, value] : mapping)
    {
        PPK_ASSERT_ERROR(value < solution.size(), "Invalid index");
        set_value(my_map, {std::get<0>(key), std::get<1>(key)}, solution[value], loc);
    }
    return my_map;
}

TimeIndexedModelVariableMapping::map1to1 lookup(const std::vector<double> &solution,
                                                const TimeIndexedModelVariableMapping::map3to1 &mapping)
{
    const std::source_location loc = std::source_location::current();

    TimeIndexedModelVariableMapping::map1to1 my_map;
    for (const auto &[key, value] : mapping)
    {
        PPK_ASSERT_ERROR(value < solution.size(), "Invalid index");

        if (solution[value] > 0)
        {
            set_value(my_map, std::get<0>(key), std::stoi(std::get<1>(key)), loc);
        }
    }
    return my_map;
}
