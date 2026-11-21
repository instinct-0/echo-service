set(COMPILE_DEFINITIONS "")
set(COMPILE_OPTIONS "")

add_link_options(-lc)
add_compile_definitions(-DDEBUG=1)
add_compile_options(-std=c89 -pedantic -g0 -O0 -fno-omit-frame-pointer -Wall -Wextra)

########################
# Test: test_server ####
set(test_index 1)
set(tests_name
    "ServerInitialization"
    "ServerEcho"
)

add_executable(test_server
    ${CMAKE_SOURCE_DIR}/src/server.c test_server.c)

foreach(test_name IN LISTS tests_name)
    add_test(NAME "${test_name}" COMMAND test_server ${test_index})
    math(EXPR test_index "${test_index} + 1")
endforeach()
