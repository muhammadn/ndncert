# cmake version to be used
cmake_minimum_required(VERSION 3.5)

find_library(dledger REQUIRED)

add_executable(ndncert-ca-server ndncert-ca-server.cpp ndncert-dledger-util.cpp)
target_link_libraries(ndncert-ca-server PUBLIC ndn-cert)
target_link_libraries(ndncert-ca-server PUBLIC dledger)

add_executable(ndncert-ca-status ndncert-ca-status.cpp)
target_link_libraries(ndncert-ca-status PUBLIC ndn-cert)

add_executable(ndncert-client ndncert-client.cpp)
target_link_libraries(ndncert-client PUBLIC ndn-cert)