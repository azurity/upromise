cmake_minimum_required(VERSION 3.0.0)
project(upromise VERSION 0.1.0)

include(GNUInstallDirs)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message("Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
    "MinSizeRel" "RelWithDebInfo")
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

include(Catch)
catch_discover_tests(upromise-test)

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
# set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Promises/A+ library for C/C++"
    CACHE STRING "Package description for the package metadata"
)

set(CPACK_VERBATIM_VARIABLES YES)

set(CPACK_PACKAGE_INSTALL_DIRECTORY ${CPACK_PACKAGE_NAME})
SET(CPACK_OUTPUT_FILE_PREFIX "packages")

set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

set(CPACK_PACKAGE_CONTACT "784227594@qq.com")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "azurity <784227594@qq.com>")

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

set(CPACK_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
set(CPACK_DEB_COMPONENT_INSTALL YES)

set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "see so https://github.com/azurity/upromise")

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/changelog.gz"
    COMMAND gzip -cnk9 "${CMAKE_CURRENT_SOURCE_DIR}/changelog" > "${CMAKE_CURRENT_BINARY_DIR}/changelog.gz"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/changelog"
    COMMENT "Compressing changelog"
)
add_custom_target(changelog ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/changelog.gz")
configure_file("${CPACK_RESOURCE_FILE_LICENSE}" "${CMAKE_CURRENT_BINARY_DIR}/copyright" COPYONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/changelog.gz" "${CMAKE_CURRENT_BINARY_DIR}/copyright"
    DESTINATION "${CMAKE_INSTALL_DOCDIR}"
)

include(CPack)
