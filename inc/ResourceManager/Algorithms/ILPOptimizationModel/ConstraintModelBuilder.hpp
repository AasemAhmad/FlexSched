#pragma once

#include "Algorithms/ILPOptimizationModel/VariableMappingBuilder.hpp"
#include "External/ILPSolverModel/ILPSolverModel.hpp"
#include "ProblemInstance/ProblemInstance.hpp"
#include <utility>
#include <vector>

class ConstraintModelBuilder
{
  public:
    explicit ConstraintModelBuilder(const ProblemInstance &problem_instance) : problem_instance(problem_instance) {}
    ConstraintModelBuilder(const ConstraintModelBuilder &) = delete;
    ConstraintModelBuilder &operator=(ConstraintModelBuilder &&) = delete;

    void reset();

    size_t move_constraints_to_model(ILPSolverModel &ilp_model);

    void add_job_processing_time_constraints(const TimeIndexedModelVariableMapping::map3to1 &x,
                                             const TimeIndexedModelVariableMapping::map1to1 &p);

    void add_job_start_time_constraints(const TimeIndexedModelVariableMapping::map1to1 &s,
                                        const TimeIndexedModelVariableMapping::map3to1 &x,
                                        const TimeIndexedModelVariableMapping::map1to1 &p,
                                        const TimeIndexedModelVariableMapping::map1to1 &cMax);

    void add_precedence_constraints(const TimeIndexedModelVariableMapping::map1to1 &s,
                                    const TimeIndexedModelVariableMapping::map1to1 &p);

    void add_renewable_resource_constraints(const TimeIndexedModelVariableMapping::map3to1 &x);

    void add_constraint(SparseMatrix<double>::Row &row, Operator op, const double &b, const std::string &conDesc);

  private:
    void add_resource_constraints_helper(const TimeIndexedModelVariableMapping::map3to1 &x, const JobConstPtr &job,
                                         SparseMatrix<double>::Row &row, size_t t, size_t k) const;
    size_t constraints_counter = 0;
    const ProblemInstance &problem_instance;
    SparseMatrix<double> constraint_matrix;
    std::vector<Operator> constraint_operator;
    std::vector<double> constraint_bound;
    std::vector<std::string> constraint_description;
};
