cmake_minimum_required(VERSION 3.20)

project(
  nixbit
  VERSION 0.5.0
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Configure version header
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in"
               "${CMAKE_CURRENT_BINARY_DIR}/version.h" @ONLY)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(
  Qt6 REQUIRED
  COMPONENTS Core
             Gui
             Qml
             Quick
             Widgets
             DBus
             Network)

find_package(KF6 REQUIRED COMPONENTS CoreAddons I18n)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBGIT2 REQUIRED libgit2)

qt_add_executable(
  nixbit
  src/main.cpp
  src/gitmanager.cpp
  src/gitmanager.h
  src/processmanager.cpp
  src/processmanager.h
  src/outputhighlighter.cpp
  src/outputhighlighter.h
  src/trayiconmanager.cpp
  src/trayiconmanager.h
  src/settingsmanager.cpp
  src/settingsmanager.h
  src/systemmonitor.cpp
  src/systemmonitor.h
  src/systemresumedetector.cpp
  src/systemresumedetector.h
  src/generationmanager.cpp
  src/generationmanager.h
  src/utils/cli.cpp
  src/utils/cli.h)

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

qt_add_qml_module(
  nixbit
  URI
  nixbit
  VERSION
  1.0
  OUTPUT_DIRECTORY
  ${CMAKE_CURRENT_BINARY_DIR}/qml/nixbit
  NO_LINT
  NO_PLUGIN
  QML_FILES
  src/main.qml
  src/SettingsDialog.qml
  src/GenerationsDialog.qml
  src/CommitsDialog.qml)

target_include_directories(nixbit PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
                                          ${LIBGIT2_INCLUDE_DIRS})

target_link_libraries(
  nixbit
  PRIVATE Qt6::Core
          Qt6::Gui
          Qt6::Qml
          Qt6::Quick
          Qt6::Widgets
          Qt6::DBus
          Qt6::Network
          KF6::CoreAddons
          KF6::I18n
          ${LIBGIT2_LIBRARIES})

install(TARGETS nixbit ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

# Install desktop file
install(FILES nixbit.desktop DESTINATION ${KDE_INSTALL_APPDIR})

# Install application icon
install(FILES src/nixbit.svg
        DESTINATION ${KDE_INSTALL_FULL_ICONDIR}/hicolor/scalable/apps)
