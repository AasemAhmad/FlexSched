find_package(CPLEX REQUIRED)
include_directories(${CPLEX_INCLUDE_DIRS} ${CPLEX_ILOCPLEX_INCLUDE_DIRS} ${CPLEX_CONCERT_INCLUDE_DIRS})
set (CPLEX_LIBS ${CPLEX_LIBRARIES} ${CPLEX_ILOCPLEX_LIBRARIES} ${CPLEX_CONCERT_LIBRARIES})
add_definitions(${CPLEX_ILOCPLEX_DEFINITIONS} ${CPLEX_CONCERT_DEFINITIONS} -Wno-unused-private-field)

file(GLOB SOURCES "*.cpp")
add_library(CplexSolver ${LIBRARY_LINKAGE} ${SOURCES})
target_link_libraries(CplexSolver PRIVATE ${CPLEX_LIBS})
target_link_libraries(CplexSolver PRIVATE dl)