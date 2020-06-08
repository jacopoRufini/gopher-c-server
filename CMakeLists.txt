cmake_minimum_required(VERSION 3.14)
project(ps_server C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")

if (UNIX)
    add_executable(ps_server main.c shared/shared.h shared/shared.c unix/unix.h unix/unix.c core/core.h core/constants.h)
    add_executable(ps_test unix/test.c)
endif (UNIX)

if (WIN32)
    link_libraries(ws2_32 wsock32)
    add_executable(ps_server main.c shared/shared.h shared/shared.c win/win.h win/win.c core/core.h core/constants.h)
    add_executable(listener win/listener.c shared/shared.h shared/shared.c win/win.h win/win.c core/core.h core/constants.h)
    add_executable(receiver win/receiver.c shared/shared.h shared/shared.c win/win.h win/win.c core/core.h core/constants.h)
    add_executable(logger win/logger.c core/constants.h)
endif (WIN32)
