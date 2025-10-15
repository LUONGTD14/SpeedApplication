cmake_minimum_required(VERSION 3.22.1)
project(video_speed_editor)

add_library(video_speed_editor SHARED
        video_editor.cpp
        sonic/sonic.c
)

include_directories(sonic)

find_library(log-lib log)
find_library(android-lib android)
find_library(mediandk-lib mediandk)
find_library(OpenSLES-lib OpenSLES)

target_link_libraries(video_speed_editor
        ${log-lib}
        ${android-lib}
        ${mediandk-lib}
        ${OpenSLES-lib}
)
