/***********************************************
 * Author: Libur Bukata
 * Modified by: Aasem Ahmad
 ***********************************************/

#pragma once

enum class MODEL_ILP_STATUS
{
    ILP_OPTIMAL,
    ILP_FEASIBLE,
    ILP_INFEASIBLE,
    ILP_UNBOUNDED,
    ILP_UNKNOWN
};