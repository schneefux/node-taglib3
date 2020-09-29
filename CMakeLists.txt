cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project (taglib3)

#
# a relatively stable snapshot of taglib2 branch for our purposes.
#
set(REVISION "65a6a4e225fff1288148de92721418e1c634713b")

#
# import ExternalProject module
#
include(ExternalProject)

#
# compiler options
#
set(CMAKE_VERBOSE_MAKEFILE OFF)
set(SUPPORT_LARGE_FILES ON)
if(UNIX AND NOT APPLE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -std=c++11")
elseif(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W0 /D_WIN32_WINNT=0x0601")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -stdlib=libc++ -mmacosx-version-min=10.10")
endif()

#
# debug info
#
MESSAGE(STATUS "runtime:" ${NODE_RUNTIME})
MESSAGE(STATUS "runtime version:" ${NODE_RUNTIMEVERSION})
MESSAGE(STATUS "architecture:" ${NODE_ARCH})

#
# download and make TagLib
#
ExternalProject_Add(
  taglib
  PREFIX "${CMAKE_SOURCE_DIR}/taglib"
  GIT_REPOSITORY https://github.com/taglib/taglib
  GIT_TAG ${REVISION}
  INSTALL_DIR "${CMAKE_SOURCE_DIR}/taglib"
  CMAKE_ARGS "-DCMAKE_INSTALL_PREFIX=${CMAKE_SOURCE_DIR}/taglib"
  CMAKE_CACHE_ARGS "-DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=true"
)

#
# force rebuild of TagLib source (useful when switching between node/electron)
#
ExternalProject_Add_Step(
  taglib
  forcebuild
  COMMAND ${CMAKE_COMMAND} -E echo_append ""
  COMMENT "Forcing build step for taglib"
  DEPENDEES configure
  DEPENDERS build
  ALWAYS 1
)

#
# include TagLib
#
link_directories(${CMAKE_SOURCE_DIR}/taglib/lib)
include_directories(BEFORE ${CMAKE_JS_INC} ${CMAKE_SOURCE_DIR}/taglib/include)

#
# node-taglib3 specifics
#

file(GLOB SOURCE_FILES "src/*.cc" "src/*.h")
add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES} ${CMAKE_JS_SRC})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")

#
# add TagLib as a dependency (after project has been defined)
#
add_dependencies(${PROJECT_NAME} taglib)

#
# link TagLib and cmakejs
#
target_link_libraries(${PROJECT_NAME} ${CMAKE_JS_LIB} tag)

#
# define NAPI_VERSION
#
add_definitions(-DNAPI_VERSION=3)
