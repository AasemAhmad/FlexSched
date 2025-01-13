add_subdirectory(CPSolver)
add_subdirectory(Interface)

file(GLOB SOURCES "*.cpp")
add_library(CPP ${LIBRARY_LINKAGE} ${SOURCES})

include_directories(${CPLEX_INCLUDE_DIRS})
message("CPLEX_INCLUDE_DIRS .... ${CPLEX_INCLUDE_DIRS}")

target_link_libraries(CPP INTERFACE CPSolver)
target_link_libraries(CPP INTERFACE Interface)