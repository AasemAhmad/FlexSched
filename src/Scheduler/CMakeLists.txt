add_subdirectory(Algo)
add_subdirectory(Solution)

file (GLOB SOURCES "*.cpp")
add_library(Scheduler ${LIBRARY_LINKAGE} ${SOURCES})

include_directories(${CPLEX_CP_INCLUDE_DIRS})

target_link_libraries(Scheduler INTERFACE Algorithms)
target_link_libraries(Scheduler PUBLIC CPSolver)
target_link_libraries(Scheduler PRIVATE External)
target_link_libraries(Scheduler PRIVATE Solution)
target_link_libraries(Scheduler PRIVATE Shared)
