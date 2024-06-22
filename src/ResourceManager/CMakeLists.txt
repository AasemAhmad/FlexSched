add_subdirectory(Algorithms)
add_subdirectory(Comm)

file (GLOB SOURCES "*.cpp")
add_library(ResourceManager ${LIBRARY_LINKAGE} ${SOURCES})

include_directories(${CPLEX_CP_INCLUDE_DIRS})

target_link_libraries(ResourceManager INTERFACE Algorithms)
target_link_libraries(ResourceManager PUBLIC CPSolver)
target_link_libraries(ResourceManager PRIVATE Comm)
target_link_libraries(ResourceManager PRIVATE External)
target_link_libraries(ResourceManager PRIVATE Solution)
target_link_libraries(ResourceManager PRIVATE Shared)