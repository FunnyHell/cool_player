cmake_minimum_required(VERSION 3.16)
project(cool_player LANGUAGES CXX)

set(CMAKE_CXX_STANDART 17)

# ---------- найти FFmpeg и PortAudio через pkg-config ----------
find_package(PkgConfig REQUIRED)                               # :contentReference[oaicite:0]{index=0}
pkg_check_modules(AVFORMAT REQUIRED IMPORTED_TARGET libavformat) # ffmpeg cpp пример :contentReference[oaicite:1]{index=1}
pkg_check_modules(AVCODEC  REQUIRED IMPORTED_TARGET libavcodec)
pkg_check_modules(AVUTIL   REQUIRED IMPORTED_TARGET libavutil)
pkg_check_modules(SWRESAMPLE REQUIRED IMPORTED_TARGET libswresample)

pkg_check_modules(PORTAUDIO REQUIRED IMPORTED_TARGET portaudio-2.0) # PortAudio doc :contentReference[oaicite:2]{index=2}

# ---------- источник и заголовки ----------
add_executable(cool_player
    main.cpp
    src/AudioPlayer.cpp
)
target_include_directories(cool_player PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ---------- линкируем импортированные targets ----------
target_link_libraries(cool_player PRIVATE
    PkgConfig::AVFORMAT
    PkgConfig::AVCODEC
    PkgConfig::AVUTIL
    PkgConfig::SWRESAMPLE
    PkgConfig::PORTAUDIO
)
