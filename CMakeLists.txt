# cmake version to be used
cmake_minimum_required(VERSION 3.5)

# project name
project(ndncert)

# flags
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(NDN_CXX REQUIRED libndn-cxx)
find_package(sqlite3 REQUIRED)
find_package(openssl REQUIRED)

# files
set(NDNCERT_LIB_SOURCE_FILES
        ./src/ca-module.cpp
        ./src/ca-state.cpp
        ./src/configuration.cpp
        ./src/requester.cpp
        ./src/requester-state.cpp
        ./src/ca-storage/ca-storage.cpp
        ./src/ca-storage/ca-memory.cpp
        ./src/ca-storage/ca-sqlite.cpp
        ./src/identity-challenge/challenge-module.cpp
        ./src/identity-challenge/challenge-credential.cpp
        ./src/identity-challenge/challenge-email.cpp
        ./src/identity-challenge/challenge-pin.cpp
        ./src/crypto-support/crypto-helper.cpp
        ./src/crypto-support/enc-tlv.cpp
        ./src/protocol-detail/info.cpp
        ./src/protocol-detail/probe.cpp
        ./src/protocol-detail/new-renew-revoke.cpp
        ./src/protocol-detail/challenge.cpp
        ./src/protocol-detail/error.cpp
        ./src/ndncert-common.cpp
        src/name-assignments/assignment-funcs.cpp
        src/name-assignments/assignment-param.cpp
        src/name-assignments/assignment-hash.cpp
        src/name-assignments/assignment-random.cpp
        )

# include
include_directories(${NDN_CXX_INCLUDE_DIRS})
include_directories(src)
include_directories(build/src)

# link
link_directories(${NDN_CXX_LIBRARY_DIRS})

# target

add_library(ndn-cert SHARED ${NDNCERT_LIB_SOURCE_FILES})
target_compile_options(ndn-cert PUBLIC ${NDN_CXX_CFLAGS})
target_link_libraries(ndn-cert PUBLIC ${NDN_CXX_LIBRARIES})

add_subdirectory(tools)
add_subdirectory(tests)
