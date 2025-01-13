find_package(loguru REQUIRED)
include_directories(${LOGURU_INCLUDE_DIR})

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_subdirectory("Scheduler")
add_subdirectory("External")
add_subdirectory("WldMgr")
add_subdirectory("CmdMgr")
add_subdirectory("Shared")
add_subdirectory("ResWrt")
add_subdirectory("Policy")

file (GLOB SOURCES "*.cpp")
add_executable (${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE External)
target_link_libraries(${PROJECT_NAME} PRIVATE WorkloadManager)
target_link_libraries(${PROJECT_NAME} PRIVATE CommandManager)
target_link_libraries(${PROJECT_NAME} PRIVATE Scheduler)
target_link_libraries(${PROJECT_NAME} PRIVATE Policy)
target_link_libraries(${PROJECT_NAME} PRIVATE ResultWriter)
target_link_libraries(${PROJECT_NAME} PRIVATE Shared)
target_link_libraries(${PROJECT_NAME} PRIVATE 
                      ${LOGURU_LIBRARIES}
                      Threads::Threads)


