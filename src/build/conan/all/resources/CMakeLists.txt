cmake_minimum_required(VERSION 3.19)

set( BLPWTK2_VERSION "${BLPWTK2_CR_VERSION}_${BLPWTK2_BB_VERSION}" )

project(blpwtk2-devkit VERSION "${BLPWTK2_CR_VERSION}" LANGUAGES CXX)

if (WIN32)
    set(CONAN_BLD_INFO ${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    if (EXISTS ${CONAN_BLD_INFO})
        message(STATUS "Found generated conanbuildinfo.cmake, calling conan_basic_setup")
        include(${CONAN_BLD_INFO})
        conan_basic_setup(TARGETS)
    endif()
endif()


set( import_target "blpwtk2.${BLPWTK2_CR_VERSION}_${BLPWTK2_BB_VERSION}.dll" )
add_library( ${import_target} STATIC IMPORTED )
set_property( TARGET ${import_target}
              PROPERTY IMPORTED_LOCATION $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/${BLPWTK2_VERSION}/lib${BITNESS_PATH_SUFFIX}/blpwtk2.${BLPWTK2_VERSION}.dll.lib>
                                         $<INSTALL_INTERFACE:lib/blpwtk2.${BLPWTK2_VERSION}.dll.lib> )
target_include_directories( ${import_target} INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/${BLPWTK2_VERSION}/include/blpwtk2>;$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/${BLPWTK2_VERSION}/include/v8>
                                                       $<INSTALL_INTERFACE:include/blpwtk2>;$<INSTALL_INTERFACE:include/v8> )

install( DIRECTORY ${BLPWTK2_VERSION}/include/ DESTINATION include FILES_MATCHING PATTERN "*.h" )
install( DIRECTORY ${BLPWTK2_VERSION}/lib/release${BITNESS_PATH_SUFFIX}/ DESTINATION lib FILES_MATCHING PATTERN "*.lib" )
install( DIRECTORY ${BLPWTK2_VERSION}/bin/release${BITNESS_PATH_SUFFIX}/ DESTINATION bin )

set( target blpwtk2 )
add_library( ${target} INTERFACE )
target_link_libraries( ${target} INTERFACE "blpwtk2.${BLPWTK2_CR_VERSION}_${BLPWTK2_BB_VERSION}.dll.lib" )
target_include_directories( ${target} INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/${BLPWTK2_VERSION}/include/blpwtk2>;$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/${BLPWTK2_VERSION}/include/v8>
                                            $<INSTALL_INTERFACE:include/blpwtk2>;$<INSTALL_INTERFACE:include/v8> )

install( TARGETS ${target} EXPORT "${target}Config" ARCHIVE DESTINATION "lib/" COMPONENT ${target} )
install( EXPORT "${target}Config" DESTINATION "lib/cmake/" COMPONENT ${target} )