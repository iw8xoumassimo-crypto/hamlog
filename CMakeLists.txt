cmake_minimum_required(VERSION 3.21)
project(HamLog VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core Widgets Sql Network PrintSupport)

if(WIN32)
    find_package(Qt6 QUIET COMPONENTS AxContainer)
endif()

qt_standard_project_setup()

set(SOURCES
    src/main.cpp
    src/types.cpp
    src/database.cpp
    src/dxcc.cpp
    src/adif.cpp
    src/cabrillo.cpp
    src/locator.cpp
    src/services.cpp
    src/cat_service.cpp
    src/udp_listener.cpp
    src/theme_manager.cpp
    src/ui/main_window.cpp
    src/ui/qso_entry.cpp
    src/ui/log_view.cpp
    src/ui/cluster_view.cpp
    src/ui/settings_dialog.cpp
    src/ui/stats_dialog.cpp
    src/ui/awards_dialog.cpp
    src/ui/online_dialog.cpp
    src/ui/dashboard_dialog.cpp
    src/ui/integrity_dialog.cpp
    src/ui/callbook_panel.cpp
    src/ui/contest_panel.cpp
    src/ui/world_map_widget.cpp
    src/ui/log_manager_dialog.cpp
    src/ui/dx_hunter_dialog.cpp
    src/ui/qsl_dialog.cpp
    src/ui/export_adif_dialog.cpp
    src/ui/qsl_label_dialog.cpp
    src/ui/contest_dialog.cpp
    src/ui/duplicates_dialog.cpp
)

qt_add_executable(HamLog WIN32 ${SOURCES})

target_include_directories(HamLog PRIVATE src)

target_link_libraries(HamLog PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Sql
    Qt6::Network
    Qt6::PrintSupport
)

if(WIN32 AND TARGET Qt6::AxContainer)
    target_link_libraries(HamLog PRIVATE Qt6::AxContainer)
    target_compile_definitions(HamLog PRIVATE HAVE_AXCONTAINER)
endif()

if(WIN32)
    target_compile_definitions(HamLog PRIVATE UNICODE _UNICODE)
endif()

install(TARGETS HamLog
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
