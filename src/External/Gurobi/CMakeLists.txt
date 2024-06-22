find_package(GUROBI REQUIRED)

add_compile_options(${GUROBI_COMPILER_FLAGS})

file(GLOB SOURCES "*.cpp")
add_library(GurobiSolver ${LIBRARY_LINKAGE} ${SOURCES})

target_include_directories(GurobiSolver PRIVATE ${GUROBI_INCLUDE_DIRS})
target_link_libraries(GurobiSolver PRIVATE ${GUROBI_LIBRARIES})