cmake_minimum_required(VERSION 3.20)
project(tbcalendarplugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

find_package(Qt6 REQUIRED COMPONENTS Core Sql Concurrent)
find_package(KF6 REQUIRED COMPONENTS CalendarCore Declarative)

add_library(tbevents STATIC tbevents.cpp)
target_link_libraries(tbevents PUBLIC Qt6::Core Qt6::Sql KF6::CalendarCore KF6::CalendarEvents)
set_target_properties(tbevents PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(thunderbirdevents MODULE plugin.cpp)
target_link_libraries(thunderbirdevents PRIVATE tbevents Qt6::Concurrent)
set_target_properties(thunderbirdevents PROPERTIES PREFIX "")

add_executable(tbdump tbdump.cpp)
target_link_libraries(tbdump PRIVATE tbevents)

install(TARGETS thunderbirdevents DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/qt6/plugins/plasmacalendarplugins)
