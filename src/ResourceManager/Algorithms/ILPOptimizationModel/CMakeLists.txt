file(GLOB SOURCES "*.cpp")
add_library(ILPOptimizationModel ${LIBRARY_LINKAGE} ${SOURCES})

target_link_libraries(ILPOptimizationModel PRIVATE ILPSolverModel)
target_link_libraries(ILPOptimizationModel PRIVATE Solution)
target_link_libraries(ILPOptimizationModel PRIVATE GurobiSolver)