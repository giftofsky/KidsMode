cmake_minimum_required(VERSION 3.21)
project(KidsMode VERSION 0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Network Widgets Test)
qt_standard_project_setup()

add_library(KidsModeCore STATIC
    src/sessiontimer.cpp
    src/sessiontimer.h
    src/policy.cpp
    src/policy.h
    src/state.cpp
    src/state.h
    src/pin.cpp
    src/pin.h
)
target_include_directories(KidsModeCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(KidsModeCore PUBLIC Qt6::Core Qt6::Network)

qt_add_executable(KidsMode WIN32
    resources/app.rc
    src/main.cpp
    src/appicon.cpp
    src/appicon.h
    src/sessioncontroller.cpp
    src/sessioncontroller.h
    src/launcherwindow.cpp
    src/launcherwindow.h
    src/hudwindow.cpp
    src/hudwindow.h
    src/settingsdialog.cpp
    src/settingsdialog.h
    src/logging.cpp
    src/logging.h
)
target_link_libraries(KidsMode PRIVATE KidsModeCore Qt6::Widgets shell32 comctl32)

enable_testing()

foreach(test_name tst_sessiontimer tst_policy tst_state tst_pin)
    qt_add_executable(${test_name} tests/${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE KidsModeCore Qt6::Test)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
