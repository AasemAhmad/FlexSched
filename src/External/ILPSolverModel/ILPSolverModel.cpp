/***********************************************
 * Author: Libur Bukata
 * Modified by: Aasem Ahmad
 ***********************************************/

#include "External/ILPSolverModel/ILPSolverModel.hpp"
#include "External/pempek_assert.hpp"
#include "loguru.hpp"

void ILPSolverModel::check_ilp_formulation() const
{
    const size_t nb_variables = matrix_A.get_nb_columns();
    const size_t nb_constraints = matrix_A.get_nb_rows();
    const size_t c_size = std::max(vector_c.size(), gurobiC.size());
    PPK_ASSERT_ERROR((nb_variables != 0 && nb_constraints != 0), "Empty matrix 'A' of the ILP formulation!");
    PPK_ASSERT_ERROR((nb_variables == vector_x.size() && nb_variables == c_size),
                     "Invalid horizontal size of 'A', 'c', 'x'!");
    PPK_ASSERT_ERROR((nb_constraints == vector_op.size() && nb_constraints == vector_b.size()),
                     "Invalid vertical size of 'A', 'ops', 'b'!");
    PPK_ASSERT_ERROR((varDesc.empty() || (varDesc.size() == nb_variables)), "Incomplete description of variables!");
    PPK_ASSERT_ERROR((conDesc.empty() || (conDesc.size() == nb_constraints)), "Incomplete description of constraints!");
}

void ILPSolverModel::log_statistics() const
{

    check_ilp_formulation();
    double matrix_density = matrix_A.get_matrix_density();
    size_t nb_binary_variables = 0;
    size_t nb_float_variables = 0;
    size_t nb_integer_variables = 0;

    for (const auto &var : vector_x)
    {
        PPK_ASSERT_ERROR(var.type == DecisionVariableType::FLT || var.type == DecisionVariableType::BIN ||
                             var.type == DecisionVariableType::INT,
                         "Unknow Decsion Variable Type");

        switch (var.type)
        {
            using enum DecisionVariableType;
        case FLT:
            ++nb_float_variables;
            break;
        case BIN:
            ++nb_binary_variables;
            break;
        case INT:
            ++nb_integer_variables;
            break;
        }
    }

    LOG_F(INFO, "Number of variables: %ld", get_nb_variables());
    LOG_F(INFO, "Number of float variables: %ld", nb_float_variables);
    LOG_F(INFO, "Number of binary variables: %ld", nb_binary_variables);
    LOG_F(INFO, "Number of integer variables: %ld", nb_integer_variables);
    LOG_F(INFO, "Number of constraints: %ld ", get_nb_constraints());
    LOG_F(INFO, "Density of matrix A: %f", matrix_density * 100.0);
}