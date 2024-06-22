add_subdirectory(Cplex)
add_subdirectory(Gurobi)
add_subdirectory(ILPSolverModel)
add_subdirectory(OpenXLSX)

file(GLOB SOURCES "*.cpp")
add_library(External ${LIBRARY_LINKAGE} ${SOURCES})
target_link_libraries(External OpenXLSX::OpenXLSX)