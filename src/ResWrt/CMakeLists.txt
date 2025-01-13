file(GLOB SOURCES "*.cpp")
add_library(ResultWriter ${LIBRARY_LINKAGE} ${SOURCES})

target_include_directories(ResultWriter PRIVATE External)
target_link_libraries(ResultWriter OpenXLSX::OpenXLSX)