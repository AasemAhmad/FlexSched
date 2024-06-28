//FIXME

#include "Algorithms/ILPOptimizationModel/ConstraintModelBuilder.hpp"
#include "Shared/Utils.hpp"
#include <loguru.hpp>

void ConstraintModelBuilder::reset()
{
    constraint_matrix.clear();
    constraint_operator.clear();
    constraint_bound.clear();
    constraint_description.clear();
}

size_t ConstraintModelBuilder::move_constraints_to_model(ILPSolverModel &ilp_model)
{
    size_t nb_moved_constraints = constraint_matrix.get_nb_rows();

    for (auto &row : constraint_matrix)
    {
        ilp_model.matrix_A.add_row(std::move(row));
    }

    std::ranges::move(constraint_operator, std::back_inserter(ilp_model.vector_op));
    std::ranges::move(constraint_bound, std::back_inserter(ilp_model.vector_b));
    std::ranges::move(this->constraint_description, std::back_inserter(ilp_model.conDesc));

    reset();

    return nb_moved_constraints;
}

void ConstraintModelBuilder::add_job_processing_time_constraints(const TimeIndexedModelVariableMapping::map3to1 &x,
                                                                 const TimeIndexedModelVariableMapping::map1to1 &p)
{
    const std::source_location loc = std::source_location::current();

    LOG_F(INFO, "%s started", source_location_to_string(loc).c_str());

    for (const JobConstPtr &job : this->problem_instance.job_queue)
    {
        double b = 0.0;
        Operator op = Operator::EQUAL;
        SparseMatrix<double>::Row row;
        size_t mode_id = 1;
        for (const auto &mode : job->modes)
        {
            for (size_t t = 0; t < problem_instance.makespan_upper_bound; ++t)
            {
                PPK_ASSERT_ERROR(mode_id <= job->modes.size(), "Invalid value %ld", mode_id);
                row.emplace_back(get_value(x, {job->id, std::to_string(mode_id), std::to_string(t)}, loc),
                                 mode.processing_time);
            }
            ++mode_id;
        }

        row.emplace_back(get_value(p, job->id, loc), -1.0);
        add_constraint(row, op, b, "processing_time_constraint");
    }

    for (const auto &job : this->problem_instance.job_queue)
    {
        double b = 1.0;
        Operator op = Operator::EQUAL;
        SparseMatrix<double>::Row row;
        size_t mode_id = 1;
        for ([[maybe_unused]] const auto &_ : job->modes)
        {
            for (size_t t = 0; t < problem_instance.makespan_upper_bound; ++t)
            {
                PPK_ASSERT_ERROR(mode_id <= job->modes.size(), "Invalid value %ld", mode_id);
                row.emplace_back(get_value(x, {job->id, std::to_string(mode_id), std::to_string(t)}, loc), 1);
            }
            ++mode_id;
        }
        add_constraint(row, op, b, "start_time_for_selected_mode_constraint");
    }
}

void ConstraintModelBuilder::add_job_start_time_constraints(const TimeIndexedModelVariableMapping::map1to1 &s,
                                                            const TimeIndexedModelVariableMapping::map3to1 &x,
                                                            const TimeIndexedModelVariableMapping::map1to1 &p,
                                                            const TimeIndexedModelVariableMapping::map1to1 &cMax)
{
    const std::source_location loc = std::source_location::current();

    LOG_F(INFO, "%s started", source_location_to_string(loc).c_str());

    for (const auto &job : this->problem_instance.job_queue)
    {
        double b = 0.0;
        Operator op = Operator::LESS_EQUAL;
        SparseMatrix<double>::Row row = {{get_value(s, job->id, loc), 1.0},
                                         {get_value(p, job->id, loc), 1.0},
                                         {get_value(cMax, std::to_string(1), loc), -1.0}};
        add_constraint(row, op, b, "start_time_constraint");
    }

    for (const JobConstPtr &job : this->problem_instance.job_queue)
    {
        double b = 0.0;
        Operator op = Operator::EQUAL;
        SparseMatrix<double>::Row row;
        for (size_t t = 0; t < problem_instance.makespan_upper_bound; ++t)
        {
            size_t mode_id = 1;
            for ([[maybe_unused]] const auto &_ : job->modes)
            {
                PPK_ASSERT_ERROR(mode_id <= job->modes.size(), "Invalid value %ld", mode_id);
                row.emplace_back(get_value(x, {job->id, std::to_string(mode_id), std::to_string(t)}, loc), t);
                ++mode_id;
            }
        }
        row.emplace_back(get_value(s, job->id, loc), -1.0);
        add_constraint(row, op, b, "start_time_constraint");
    }

    LOG_F(INFO, "%s finished successfully", source_location_to_string(loc).c_str());
}

void ConstraintModelBuilder::add_precedence_constraints(const TimeIndexedModelVariableMapping::map1to1 &s,
                                                        const TimeIndexedModelVariableMapping::map1to1 &p)
{
    const std::source_location loc = std::source_location::current();
    LOG_F(INFO, "%s started", source_location_to_string(loc).c_str());

    for (const JobConstPtr &job : this->problem_instance.job_queue)
    {
        for (const auto &succ : job->successors)
        {
            double b = 0.0;
            Operator op = Operator::LESS_EQUAL;
            SparseMatrix<double>::Row row = {{get_value(s, job->id, std::source_location::current()), 1.0},
                                             {get_value(s, succ, loc), -1.0},
                                             {get_value(p, job->id, loc), 1.0}};
            add_constraint(row, op, b, "precedence_constraint");
        }
    }

    LOG_F(INFO, "%s finished successfully", source_location_to_string(loc).c_str());
}

void ConstraintModelBuilder::add_renewable_resource_constraints(const TimeIndexedModelVariableMapping::map3to1 &x)
{

    const std::source_location loc = std::source_location::current();

    LOG_F(INFO, "%s started", source_location_to_string(loc).c_str());

    size_t nb_resources = problem_instance.resources.size();

    for (size_t t = 0; t < problem_instance.makespan_upper_bound; ++t)
    {
        for (size_t k = 0; k < nb_resources; ++k)
        {
            auto b = static_cast<double>(this->problem_instance.resources.at(k).units);
            Operator op = Operator::LESS_EQUAL;
            SparseMatrix<double>::Row row;

            for (const JobConstPtr &job : problem_instance.job_queue)
            {
                add_resource_constraints_helper(x, job, row, t, k);
            }

            add_constraint(row, op, b, "renewable_resource_constraint");
        }
    }

    LOG_F(INFO, "%s finished successfully", source_location_to_string(loc).c_str());
}

void ConstraintModelBuilder::add_resource_constraints_helper(const TimeIndexedModelVariableMapping::map3to1 &x,
                                                             const JobConstPtr &job, SparseMatrix<double>::Row &row,
                                                             size_t t, size_t k) const
{
    const std::source_location loc = std::source_location::current();

    size_t mode_id = 1;
    for (const auto &mode : job->modes)
    {
        PPK_ASSERT_ERROR(mode_id <= job->modes.size(), "Invalid Value %ld", mode_id);
        int s_start = std::max(static_cast<int>(t) - static_cast<int>(mode.processing_time) + 1, 0);
        PPK_ASSERT_ERROR(s_start >= 0, "invalid value %d", s_start);
        for (size_t s = s_start; s <= t; ++s)
        {
            row.emplace_back(get_value(x, {job->id, std::to_string(mode_id), std::to_string(s)}, loc),
                             mode.requested_resources.at(k).units);
        }
        ++mode_id;
    }
}

void ConstraintModelBuilder::add_constraint(SparseMatrix<double>::Row &row, Operator op, const double &b,
                                            const std::string &con_desc)
{
    this->constraint_matrix.add_row(std::move(row));
    this->constraint_operator.emplace_back(op);
    this->constraint_bound.emplace_back(b);
    this->constraint_description.emplace_back(con_desc);
}