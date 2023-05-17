cmake_minimum_required(VERSION 3.0.0)
project(upromise VERSION 0.1.0)

include(GNUInstallDirs)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
message("BUILD TYPE: " ${CMAKE_BUILD_TYPE})

include(CTest)
enable_testing()

option(WITH_TEST "build with test cases" OFF)
set(WITH_TEST ON)

add_library(upromise src/upromise.c src/coroutine.c)
target_include_directories(upromise
    PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

if(WITH_TEST)
    find_package(Catch2 2 REQUIRED)
    find_package(Threads REQUIRED)

    add_executable(upromise-test test/test.cpp)
    target_include_directories(upromise-test PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(upromise-test upromise Catch2::Catch2WithMain Threads::Threads)
endif()

install(TARGETS upromise
        EXPORT upromiseTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

file(GLOB headers include/upromise/*.h)
install(FILES ${headers} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/upormise)

install(EXPORT upromiseTargets
        FILE upromiseTargets.cmake
        NAMESPACE upromise::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/upromise
)

include(CMakePackageConfigHelpers)

set_property(TARGET upromise PROPERTY VERSION ${PROJECT_VERSION})
set_property(TARGET upromise PROPERTY SOVERSION ${PROJECT_VERSION_MAJOR})
set_property(TARGET upromise PROPERTY INTERFACE_upromise_MAJOR_VERSION ${PROJECT_VERSION_MAJOR})
set_property(TARGET upromise APPEND PROPERTY COMPATIBLE_INTERFACE_STRING upromise_MAJOR_VERSION)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/upromiseConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/upromiseConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/upromise
)

install(FILES
          "${CMAKE_CURRENT_BINARY_DIR}/upromiseConfig.cmake"
          "${CMAKE_CURRENT_BINARY_DIR}/upromiseConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/upromise
)
export(EXPORT upromiseTargets
       FILE "${CMAKE_CURRENT_BINARY_DIR}/cmake/upromiseTargets.cmake"
       NAMESPACE upromise::
)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
