cmake_minimum_required(VERSION 3.16)

project(nixbit VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(QT_MIN_VERSION "6.5.0")
set(KF_MIN_VERSION "6.0.0")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Core
    Quick
    Widgets
)

find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    CoreAddons
    I18n
)

add_executable(nixbit
    src/main.cpp
)

target_link_libraries(nixbit
    Qt6::Core
    Qt6::Quick
    Qt6::Widgets
    KF6::CoreAddons
    KF6::I18n
)

install(TARGETS nixbit ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES src/main.qml DESTINATION ${KDE_INSTALL_DATADIR}/nixbit)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
