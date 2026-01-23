cmake_minimum_required(VERSION 3.26)

project(argos_native_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------
# Configuración común
# -----------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(SentencePiece REQUIRED sentencepiece)

find_package(absl CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(ctranslate2 REQUIRED)
find_package(wxWidgets CONFIG REQUIRED)

link_directories(/home/reimen/vcpkg/installed/x64-linux/lib) # Quick fix for pkg-config libs
include_directories(${CMAKE_SOURCE_DIR}/src ${SentencePiece_INCLUDE_DIRS})

# -----------------------
# Función para crear build por plataforma
# -----------------------
function(configure_target PLATFORM TRIPLET)
    # Carpeta de build específica
    set(BUILD_DIR "${CMAKE_BINARY_DIR}/build_${PLATFORM}")
    file(MAKE_DIRECTORY ${BUILD_DIR})

    # Ejecuta configuración y compilación secuencial
    execute_process(
        COMMAND cmake
            -S ${CMAKE_SOURCE_DIR}
            -B ${BUILD_DIR}
            -DCMAKE_TOOLCHAIN_FILE=/home/reimen/vcpkg/scripts/buildsystems/vcpkg.cmake
            -DVCPKG_TARGET_TRIPLET=${TRIPLET}
            -DIS_CHILD_BUILD=ON
        RESULT_VARIABLE res_config
    )
    if(NOT res_config EQUAL 0)
        message(FATAL_ERROR "CMake configuration failed for ${PLATFORM}")
    endif()

    execute_process(
        COMMAND cmake --build ${BUILD_DIR} --config Release
        RESULT_VARIABLE res_build
    )
    if(NOT res_build EQUAL 0)
        message(FATAL_ERROR "Build failed for ${PLATFORM}")
    endif()
endfunction()

# -----------------------
# Definición del ejecutable (compartido)
# -----------------------
add_executable(Fast_translator
    src/main.cpp
    src/utils.cpp
    src/translation.cpp
    src/tokenizer_bpe.cpp
    src/language_graph.cpp
)

target_link_libraries(Fast_translator
    PRIVATE
    ${SentencePiece_LIBRARIES}
    ctranslate2
    protobuf::libprotobuf
)

# -----------------------
# Definición del gestor GUI (wxWidgets)
# -----------------------
add_executable(Fast_translator_manager
    src/gui_main.cpp
)
target_link_libraries(Fast_translator_manager
    PRIVATE
    wx::core
    wx::base
    wx::net
)

# -----------------------
# Builds secuenciales
# -----------------------
# Linux
# Linux
if(NOT IS_CHILD_BUILD)
    configure_target("linux" "x64-linux")
    
    # Windows cross-compile (MinGW)
    # configure_target("windows" "x64-windows") # Commented out to avoid failing if not setup
endif()
