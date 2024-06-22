find_package(loguru REQUIRED)
include_directories(${LOGURU_INCLUDE_DIR})

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_subdirectory("ResourceManager")
add_subdirectory("External")
add_subdirectory("WorkloadManager")
add_subdirectory("Solution")
add_subdirectory("ResultWriter")

file (GLOB SOURCES "*.cpp")
add_executable (${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE External)
target_link_libraries(${PROJECT_NAME} PRIVATE WorkloadManager)
target_link_libraries(${PROJECT_NAME} PRIVATE ResourceManager)
target_link_libraries(${PROJECT_NAME} PRIVATE ResultWriter)
target_link_libraries(${PROJECT_NAME} PRIVATE Solution)
target_link_libraries(${PROJECT_NAME} PRIVATE Shared)
target_link_libraries(${PROJECT_NAME} PRIVATE 
                      ${LOGURU_LIBRARIES}
                      Threads::Threads)


