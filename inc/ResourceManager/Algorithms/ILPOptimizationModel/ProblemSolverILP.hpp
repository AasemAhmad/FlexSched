#pragma once

#include "Algorithms/ILPOptimizationModel/ConstraintModelBuilder.hpp"
#include "Algorithms/ILPOptimizationModel/VariableMappingBuilder.hpp"
#include "External/ILPSolverModel/ILPSolverModel.hpp"
#include "Settings.hpp"
#include "Solution/Solution.hpp"

class ProblemSolverILP
{
  public:
    explicit ProblemSolverILP(const ProblemInstance &problem_instance);
    ProblemSolverILP(const ProblemSolverILP &) = delete;
    ProblemSolverILP &operator=(const ProblemSolverILP &) = delete;

    Solution solve(Solution &init_solution, double rel_gap = Settings::Solver::ILP_RELATIVE_GAP,
                   double time_limit = Settings::Solver::MAX_RUNTIME) const;

  private:
    void construct(ConstraintModelBuilder &generator);
    TimeIndexedModelVariableMapping variable_mapping_ilp;
    ILPSolverModel ilp_model;
};
