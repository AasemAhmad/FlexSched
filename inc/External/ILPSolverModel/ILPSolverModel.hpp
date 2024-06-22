/***********************************************
 * Author: Libur Bukata
 * Modified by: Aasem Ahmad
 ***********************************************/

#pragma once

#include "Shared/Constants.hpp"
#include "SparseMatrix.hpp"
#include <string>
#include <vector>

enum class ObjectiveFunction
{
    MINIMIZE,
    MAXIMIZE
};

enum class DecisionVariableType
{
    FLT,
    BIN,
    INT
};

enum class Operator
{
    LESS_EQUAL,
    EQUAL,
    GREATER_EQUAL
};

struct DecisionVariable
{
    DecisionVariable(DecisionVariableType t = DecisionVariableType::FLT, double lb = F64_MIN, double ub = F64_MAX)
        : type(t), lower_bound(lb), upper_bound(ub)
    {}

    DecisionVariableType type;
    double lower_bound;
    double upper_bound;
};

struct ILPSolverModel
{
    explicit ILPSolverModel(ObjectiveFunction o = ObjectiveFunction::MINIMIZE) : obj(o) {}
    size_t get_nb_constraints() const { return matrix_A.get_nb_rows(); }
    size_t get_nb_variables() const { return vector_x.size(); }
    void check_ilp_formulation() const;
    void log_statistics() const;

    ObjectiveFunction obj;
    SparseMatrix<double> matrix_A;
    std::vector<double> vector_c;
    std::vector<const std::tuple<std::vector<double>, std::vector<double>> *> gurobiC;
    std::vector<double> vector_b;
    std::vector<Operator> vector_op;
    std::vector<DecisionVariable> vector_x;
    std::vector<std::string> varDesc;
    std::vector<std::string> conDesc;
};
