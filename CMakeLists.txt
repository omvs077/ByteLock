cmake_minimum_required(VERSION 3.21)
project(ByteLock VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent Svg)
find_package(OpenSSL REQUIRED)

set(SOURCES
    src/main.cpp
    resources/resources.qrc
    src/ui/MainWindow.cpp
    src/ui/TitleBar.cpp
    src/ui/IconUtils.cpp
    src/ui/Sidebar.cpp
    src/ui/DashboardPage.cpp
    src/ui/SettingsPage.cpp
    src/ui/AboutPage.cpp
    src/ui/PasswordLineEdit.cpp
    src/ui/AuthGateDialog.cpp
    src/viewmodel/MainViewModel.cpp
    src/engine/CryptoEngine.cpp
    src/engine/FolderPacker.cpp
    src/engine/DefaultPasswordStore.cpp
    src/persistence/FolderRegistry.cpp
)

set(HEADERS
    src/ui/MainWindow.h
    src/ui/TitleBar.h
    src/ui/IconUtils.h
    src/ui/Sidebar.h
    src/ui/DashboardPage.h
    src/ui/SettingsPage.h
    src/ui/AboutPage.h
    src/ui/PasswordLineEdit.h
    src/ui/AuthGateDialog.h
    src/viewmodel/MainViewModel.h
    src/engine/Result.h
    src/engine/SecureBytes.h
    src/engine/CryptoEngine.h
    src/engine/FolderPacker.h
    src/engine/DefaultPasswordStore.h
    src/persistence/FolderRegistry.h
)

add_executable(ByteLock WIN32 ${SOURCES} ${HEADERS})

target_link_libraries(ByteLock PRIVATE
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Svg
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_include_directories(ByteLock PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

if(WIN32)
    target_link_libraries(ByteLock PRIVATE user32)

    get_target_property(_qt6_core_location Qt6::Core IMPORTED_LOCATION_RELEASE)
    if(NOT _qt6_core_location)
        get_target_property(_qt6_core_location Qt6::Core IMPORTED_LOCATION_DEBUG)
    endif()
    get_filename_component(_qt6_bin_dir "${_qt6_core_location}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt6_bin_dir}")

    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ByteLock POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" "$<TARGET_FILE:ByteLock>"
            COMMENT "Running windeployqt to bundle Qt DLLs"
        )
    endif()
endif()
