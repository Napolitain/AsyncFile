if (NOT CPM_HAS_GTEST)
    CPMAddPackage(
            NAME googletest
            GITHUB_REPOSITORY google/googletest
            GIT_TAG v1.17.0
            VERSION 1.17.0a
            OPTIONS "INSTALL_GTEST OFF" "gtest_force_shared_crt"
    )
    include(googletest)
    enable_testing()
    # set cmake var to indicate gtest dep is available
    set(CPM_HAS_GTEST TRUE)
endif ()

add_executable(AsyncFileTests test_async_file_writer.cpp)
target_link_libraries(AsyncFileTests PRIVATE gtest gtest_main libAsyncFile)
gtest_discover_tests(AsyncFileTests)
