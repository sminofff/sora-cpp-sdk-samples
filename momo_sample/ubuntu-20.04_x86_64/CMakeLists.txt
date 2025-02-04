cmake_minimum_required(VERSION 3.23)

# Only interpret if() arguments as variables or keywords when unquoted.
cmake_policy(SET CMP0054 NEW)
# MSVC runtime library flags are selected by an abstraction.
cmake_policy(SET CMP0091 NEW)

set(WEBRTC_INCLUDE_DIR "" CACHE PATH "WebRTC のインクルードディレクトリ")
set(WEBRTC_LIBRARY_DIR "" CACHE PATH "WebRTC のライブラリディレクトリ")
set(WEBRTC_LIBRARY_NAME "webrtc" CACHE STRING "WebRTC のライブラリ名")
set(BOOST_ROOT "" CACHE PATH "Boost のルートディレクトリ")
set(SORA_DIR "" CACHE PATH "Sora のルートディレクトリ")
set(SDL2_DIR "" CACHE PATH "SDL2 のルートディレクトリ")
set(CLI11_DIR "" CACHE PATH "CLI11 のルートディレクトリ")

project(sora-sdl-sample C CXX)

list(APPEND CMAKE_PREFIX_PATH ${SORA_DIR} ${SDL2_DIR})
list(APPEND CMAKE_MODULE_PATH ${SORA_DIR}/share/cmake)

set(Boost_USE_STATIC_LIBS ON)

find_package(Boost REQUIRED COMPONENTS json filesystem)
find_package(Lyra REQUIRED)
find_package(WebRTC REQUIRED)
find_package(Sora REQUIRED)
find_package(SDL2 REQUIRED)
find_package(Threads REQUIRED)
find_package(Libva REQUIRED)
find_package(Libdrm REQUIRED)

add_executable(momo_sample)
set_target_properties(momo_sample PROPERTIES CXX_STANDARD 17 C_STANDARD 17)
set_target_properties(momo_sample PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_sources(momo_sample PRIVATE ../src/momo_sample.cpp ../src/sdl_renderer.cpp)

target_compile_options(momo_sample
  PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:-nostdinc++>"
    "$<$<COMPILE_LANGUAGE:CXX>:-isystem${LIBCXX_INCLUDE_DIR}>"
)
target_include_directories(momo_sample PRIVATE ${CLI11_DIR}/include)
target_link_libraries(momo_sample PRIVATE Sora::sora SDL2::SDL2 SDL2::SDL2main)
target_compile_definitions(momo_sample PRIVATE CLI11_HAS_FILESYSTEM=0)

# Lyra ファイルのコピー
add_custom_command(
  TARGET momo_sample POST_BUILD
  COMMAND
    ${CMAKE_COMMAND} -E copy_directory
    ${LYRA_DIR}/share/model_coeffs/
    ${CMAKE_CURRENT_BINARY_DIR}/model_coeffs/
  DEPENDS
    ${LYRA_DIR}/share/model_coeffs/lyra_config.binarypb
    ${LYRA_DIR}/share/model_coeffs/lyragan.tflite
    ${LYRA_DIR}/share/model_coeffs/quantizer.tflite
    ${LYRA_DIR}/share/model_coeffs/soundstream_encoder.tflite
)
