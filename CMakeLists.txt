cmake_minimum_required(VERSION 2.8.4)
project(TinyCThread)
enable_testing()

find_package(Threads REQUIRED)

add_library(tinycthread STATIC source/tinycthread.c)
target_link_libraries(tinycthread ${CMAKE_THREAD_LIBS_INIT})
set_property(TARGET tinycthread APPEND PROPERTY INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/source")
set_property(TARGET tinycthread APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/source")
set_property(TARGET tinycthread PROPERTY POSITION_INDEPENDENT_CODE True)

option(TINYCTHREAD_DISABLE_TESTS "Disable TinyCThread unit tests")
option(TINYCTHREAD_INSTALL "Install a static library for TinyCThread")

if(NOT TINYCTHREAD_DISABLE_TESTS)
  add_executable(test-tinycthread "${CMAKE_CURRENT_SOURCE_DIR}/test/test.c")
  target_link_libraries(test-tinycthread tinycthread)

  add_test(NAME tinycthread
    COMMAND $<TARGET_FILE:test-tinycthread>)
endif(NOT TINYCTHREAD_DISABLE_TESTS)

if(TINYCTHREAD_INSTALL)
  if(CMAKE_INSTALL_LIBDIR)
    install(TARGETS tinycthread ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
  else()
    install(TARGETS tinycthread ARCHIVE DESTINATION lib)
  endif()

  if(CMAKE_INSTALL_INCLUDEDIR)
    install(FILES source/tinycthread.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
  else()
    install(FILES source/tinycthread.h DESTINATION include)
  endif()
endif(TINYCTHREAD_INSTALL)
