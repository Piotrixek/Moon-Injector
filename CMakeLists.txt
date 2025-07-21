cmake_minimum_required(VERSION 3.15)
project(MoonInjector LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# --- Library Locations ---
set(IMGUI_DIR "C:/libs/imgui-docking" CACHE PATH "Path to ImGui source directory")
set(ULTRALIGHT_SDK_DIR "C:/libs/Ultralight" CACHE PATH "Path to Ultralight SDK")
set(TINYFILEDIALOGS_DIR "C:/libs/tinyfiledialogs" CACHE PATH "Path to tinyfiledialogs")

# --- Gather ALL Source Files ---
# 1. Your application's source files
file(GLOB APP_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
# 2. ImGui's source files
file(GLOB IMGUI_SOURCES
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_win32.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_dx11.cpp"
)
# 3. tinyfiledialogs source file
set(TINY_DIALOGS_SRC "${TINYFILEDIALOGS_DIR}/tinyfiledialogs.c")


# --- Create ONE Executable from ALL sources ---
add_executable(${PROJECT_NAME} WIN32
    ${APP_SOURCES}
    ${IMGUI_SOURCES}
    ${TINY_DIALOGS_SRC}
)

# --- Add ALL Include Directories to the Executable ---
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${ULTRALIGHT_SDK_DIR}/include
    ${TINYFILEDIALOGS_DIR}
)

# --- Link against required libraries ---
target_link_libraries(${PROJECT_NAME} PRIVATE
    d3d11
    d3dcompiler
    dxgi
    gdiplus
    ${ULTRALIGHT_SDK_DIR}/lib/Ultralight.lib
    ${ULTRALIGHT_SDK_DIR}/lib/UltralightCore.lib
    ${ULTRALIGHT_SDK_DIR}/lib/AppCore.lib
    ${ULTRALIGHT_SDK_DIR}/lib/WebCore.lib

)

# --- Compiler Definitions ---
target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX)

# --- Post-Build Step: Copy Ultralight Assets ---
# this makes sure all the ultralight magic (dlls, resources) is where ur exe can find it
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${ULTRALIGHT_SDK_DIR}/bin"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${ULTRALIGHT_SDK_DIR}/resources"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/resources"
    COMMENT "Copying Ultralight DLLs and resources"
)


# --- Windows Specific Settings ---
if(MSVC)
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "-mwindows")
endif()