add_subdirectory(CPSolver)

add_library(Algorithms INTERFACE)

include_directories(${CPLEX_INCLUDE_DIRS})
message("CPLEX_INCLUDE_DIRS .... ${CPLEX_INCLUDE_DIRS}")
target_link_libraries(Algorithms INTERFACE CPSolver)

