
cmake_minimum_required(VERSION 2.8.8)
project("string_hash")


# Options. Turn on with 'cmake -Dmyvarname=ON'.
option(test "Build all tests." ON) # Makes boolean 'test' available.

include_directories(
   "${PROJECT_SOURCE_DIR}"
   "${PROJECT_BINARY_DIR}"
)

# hash
add_library(hash lookup8.c)

################################
# Testing
################################
if (test)
  # This adds another subdirectory, which has 'project(gtest)'.
  add_subdirectory(gtest)

  enable_testing()

  # Include the gtest library. gtest_SOURCE_DIR is available due to
  # 'project(gtest)' above.
  #include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})
  include_directories(${GTEST_INCLUDE_DIRS})

  ##############
  # Unit Tests
  ##############
  add_executable(string_hash_test string_hash_test.cc)

  # Standard linking to gtest stuff.
  #target_link_libraries(string_hash_test gtest gtest_main)
  target_link_libraries(string_hash_test ${GTEST_LIBS_DIR}/libgtest.a ${GTEST_LIBS_DIR}/libgtest_main.a)

  # Extra linking for the project.
  target_link_libraries(string_hash_test hash glog)

  add_dependencies(string_hash_test googletest)

  # This is so you can do 'make test' to see all your tests run, instead of
  # manually running the executable runUnitTests to see those specific tests.
  add_test(NAME string_hash_test COMMAND string_hash_test)

  # You can also omit NAME and COMMAND. The second argument could be some other
  # test executable.
  #add_test(that-other-test-I-made runUnitTests)
endif()
