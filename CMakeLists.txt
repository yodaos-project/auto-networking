CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
project(flora-dispatcher)
set(VERSION 0.1)

option(BUILD_DEBUG "debug or release" OFF)
option(BUILD_TEST "build test program" OFF)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" )
  set(COMPILER_IS_CLANG true)
else()
	set(COMPILER_IS_CLANG false)
endif()
# CMake policies: enable MACOSX_RPATH by default
if(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW)
endif()

if (BUILD_DEBUG)
	set (common_cxxflags "-g -O0")
else()
	set (common_cxxflags -O3)
	set (common_ldflags -s)
endif()
if (NOT ${COMPILER_IS_CLANG})
	set (common_ldflags "${common_ldflags} -pthread")
endif()
set (CMAKE_CXX_STANDARD 11)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${common_cxxflags}")
set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${common_ldflags}")

include(${CUSTOM_CMAKE_MODULES}/common.mk)

git_commit_id(commit_id ${OVERRIDE_SOURCE_DIR})
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGIT_COMMIT_ID=${commit_id}")

findPackage(mutils REQUIRED
	HINTS ${mutilsPrefix}
	HEADERS rlog.h caps.h thr-pool.h
	INC_PATH_SUFFIX include/log include/caps include/misc
	SHARED_LIBS rlog caps
)

findPackage(flora REQUIRED
	HINTS ${floraPrefix}
	HEADERS flora-cli.h
	SHARED_LIBS flora-cli
)

add_library(an-svc SHARED src/an-svc.cpp include/an-svc.h)
target_include_directories(an-svc PRIVATE
  include
	${mutils_INCLUDE_DIRS}
)
target_link_libraries(an-svc
	${mutils_LIBRARIES}
)

add_library(an-cli SHARED src/an-cli.cpp include/an-cli.h)
target_include_directories(an-cli PRIVATE
  include
	${mutils_INCLUDE_DIRS}
  ${flora_INCLUDE_DIRS}
)
target_link_libraries(an-cli
	${mutils_LIBRARIES}
	${flora_LIBRARIES}
)

install(TARGETS an-svc an-cli
  LIBRARY DESTINATION lib
)
file(GLOB an_headers include/*.h)
install(FILES ${an_headers} DESTINATION include/auto-networking)

if (BUILD_TEST)
add_executable(an-cli-test test/an-cli-test.cpp)
target_include_directories(an-cli-test PRIVATE
  include
  ${flora_INCLUDE_DIRS}
)
target_link_libraries(an-cli-test
  an-cli
)
install(TARGETS an-cli-test
  RUNTIME DESTINATION bin
)
endif(BUILD_TEST)
