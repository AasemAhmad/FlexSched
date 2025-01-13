add_subdirectory(CPP)

add_library(Algorithms INTERFACE)
target_link_libraries(Algorithms INTERFACE CPP)

