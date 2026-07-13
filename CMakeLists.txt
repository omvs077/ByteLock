cmake_minimum_required(VERSION 3.21)
project(ByteLock VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent Network)
find_package(OpenSSL REQUIRED)
find_package(ZXing CONFIG REQUIRED)

set(SOURCES
    src/main.cpp
    src/ui/MainWindow.cpp
    src/MasterConfig.cpp
    src/SetupDialog.cpp
    src/WelcomeGuideDialog.cpp
    src/SettingsDialog.cpp
    src/MobilePairing.cpp
    src/PairingDialog.cpp
    src/LocalPairingServer.cpp
    src/RecoveryDialog.cpp
    src/pairing.qrc
    resources/app.rc
    resources/icons.qrc
    src/engine/CryptoEngine.cpp
    src/engine/FolderPacker.cpp
)

set(HEADERS
    src/ui/MainWindow.h
    src/engine/Result.h
    src/engine/SecureBytes.h
    src/engine/CryptoEngine.h
    src/engine/FolderPacker.h
    src/LocalPairingServer.h
    src/RecoveryDialog.h
)

add_executable(ByteLock WIN32 ${SOURCES} ${HEADERS})

target_link_libraries(ByteLock PRIVATE
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Network
    ZXing::Core
    ZXing::ZXing
    OpenSSL::SSL
    OpenSSL::Crypto
    Crypt32
)

target_include_directories(ByteLock PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

if(WIN32)
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

