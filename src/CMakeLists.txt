# Main ESP-IDF component: every .cpp under src/, including subfolders.
FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.cpp)

idf_component_register(SRCS ${app_sources}
                       INCLUDE_DIRS ".")

# Stricter warnings — for our own code only
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra)
