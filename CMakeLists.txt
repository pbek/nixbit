cmake_minimum_required(VERSION 3.20)

project(
  nixbit
  VERSION 1.0.0
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Widgets)

find_package(KF6 REQUIRED COMPONENTS CoreAddons I18n)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBGIT2 REQUIRED libgit2)

# Create a Qt resource file for the QML
qt_add_resources(QML_RESOURCES src/qml.qrc)

qt_add_executable(
  nixbit
  src/main.cpp
  src/gitmanager.cpp
  src/gitmanager.h
  src/processmanager.cpp
  src/processmanager.h
  src/trayiconmanager.cpp
  src/trayiconmanager.h
  src/settingsmanager.cpp
  src/settingsmanager.h
  ${QML_RESOURCES})

target_link_libraries(
  nixbit
  PRIVATE Qt6::Core
          Qt6::Gui
          Qt6::Qml
          Qt6::Quick
          Qt6::Widgets
          KF6::CoreAddons
          KF6::I18n
          ${LIBGIT2_LIBRARIES})

target_include_directories(nixbit PRIVATE ${LIBGIT2_INCLUDE_DIRS})

install(TARGETS nixbit ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
