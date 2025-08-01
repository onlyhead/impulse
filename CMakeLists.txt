cmake_minimum_required(VERSION 3.15)

set(project_name impulse)
project(${project_name} VERSION 0.0.1 LANGUAGES CXX)

add_compile_options(-Wall -Wextra -Wpedantic)
cmake_policy(SET CMP0074 NEW)
cmake_policy(SET CMP0135 NEW)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED On)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(params
    -Wno-reorder
    -Wno-unused-result
    -Wno-unused-variable
    -Wno-unused-parameter
)

string(TOUPPER ${project_name} project_name_upper)
option(${project_name_upper}_BUILD_EXAMPLES "Build examples" OFF)
option(${project_name_upper}_ENABLE_TESTS "Enable tests" OFF)
include(FetchContent)

# --------------------------------------------------------------------------------------------------
set(ext_deps)

FetchContent_Declare(concord GIT_REPOSITORY https://github.com/onlyhead/concord.git GIT_TAG 2.0.2)
FetchContent_MakeAvailable(concord)
list(APPEND ext_deps concord::concord)

# FetchContent_Declare(capnproto GIT_REPOSITORY https://github.com/capnproto/capnproto.git GIT_TAG v1.2.0)
# FetchContent_MakeAvailable(capnproto)
# list(APPEND ext_deps CapnProto::capnp)

# --------------------------------------------------------------------------------------------------
add_library(${project_name} INTERFACE)
# Allow users to link via `${project_name}::${project_name}`
add_library(${project_name}::${project_name} ALIAS ${project_name})
target_include_directories(${project_name} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

install(
  DIRECTORY include/
  DESTINATION include
)
include_directories(include)

# --------------------------------------------------------------------------------------------------
include(GNUInstallDirs)

#Install headers
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

#Install and export the INTERFACE target
install(TARGETS ${project_name} EXPORT ${project_name}Targets)

#Install the export file so that `find_package(${project_name})` works
install(EXPORT ${project_name}Targets
  FILE ${project_name}Targets.cmake
  NAMESPACE ${project_name}::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${project_name}
)

#Also export it for in‐source FetchContent usage
export(EXPORT ${project_name}Targets
  FILE "${CMAKE_CURRENT_BINARY_DIR}/${project_name}Targets.cmake"
  NAMESPACE ${project_name}::
)


# --------------------------------------------------------------------------------------------------
if(${project_name_upper}_BUILD_EXAMPLES)

  file(GLOB exe CONFIGURE_DEPENDS examples/*.cpp)
  # ----------------------------------------------
  set(exec_names)
  foreach(src_file IN LISTS exe)
    get_filename_component(exec_name "${src_file}" NAME_WE)
    add_executable(${exec_name} "${src_file}")
      foreach(lib_file IN LISTS internal_deps)
        target_compile_options(${exec_name} PRIVATE ${params})
        target_sources(${exec_name} PRIVATE "${lib_file}")
      endforeach()
    target_link_libraries(${exec_name} ${ext_deps})
    install(TARGETS ${exec_name} DESTINATION bin)
    list(APPEND exec_names ${exec_name})
  endforeach()
  # ----------------------------------------------
  foreach(exec IN LISTS exec_names)
    file(REMOVE "${CMAKE_CURRENT_LIST_DIR}/.execs")
    file(WRITE "${CMAKE_CURRENT_LIST_DIR}/.execs")
    get_filename_component(exec_name "${exec}" NAME)
    file(APPEND "${CMAKE_CURRENT_LIST_DIR}/.execs" "${CMAKE_CURRENT_LIST_DIR}/build/${exec_name}\n")
  endforeach()
  # ----------------------------------------------
  install(TARGETS
    ${exec_names}
    DESTINATION lib/${PROJECT_NAME}
  )
endif()


# --------------------------------------------------------------------------------------------------
if(${project_name_upper}_ENABLE_TESTS)
  enable_testing()
  add_definitions(-DENABLE_DOCTEST_IN_LIBRARY)
  include(FetchContent)
  FetchContent_Declare(DocTest GIT_REPOSITORY "https://github.com/onqtam/doctest")
  FetchContent_MakeAvailable(DocTest)

  file(GLOB test_src CONFIGURE_DEPENDS test/*.cpp)

  foreach(src_file IN LISTS test_src)
    get_filename_component(test_name "${src_file}" NAME_WE)
    add_executable(${test_name} "${src_file}")
      foreach(lib_file IN LISTS internal_deps)
        target_compile_options(${test_name} PRIVATE ${params})
        target_sources(${test_name} PRIVATE "${lib_file}")
      endforeach()
    target_link_libraries(${test_name} ${ext_deps} doctest_with_main)
    add_test(NAME ${test_name} COMMAND ${test_name})
  endforeach()
endif()
