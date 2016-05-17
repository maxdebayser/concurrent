include_directories(${CONCURRENT_SOURCE_DIR}/src/concurrent)
include_directories(${CONCURRENT_SOURCE_DIR}/src/concurrent/disruptor)

add_subdirectory(concurrent)
add_subdirectory(unit_test)
