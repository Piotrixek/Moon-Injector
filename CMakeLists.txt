cmake_minimum_required(VERSION 3.15)
project(MoonInjector LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# --- Library Locations ---
set(IMGUI_DIR "C:/libs/imgui-docking")
set(ULTRALIGHT_SDK_DIR "C:/libs/Ultralight")
set(TINYFILEDIALOGS_DIR "C:/libs/tinyfiledialogs")
set(BLACKBONE_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/BlackBone")
set(SQLITECPP_DIR "C:/libs/SQLiteCpp-3.3.3")

# --- Add SQLiteCpp as a sub-project ---
# this will build sqlite3 and sqlitecpp for us
add_subdirectory(${SQLITECPP_DIR} extern/SQLiteCpp)

# --- Gather ALL Source Files ---
# 1. your application's source files
file(GLOB APP_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
# 2. imgui's source files
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

# --- Add BlackBone as a sub-project to build from source ---
add_subdirectory(${BLACKBONE_ROOT_DIR}/src/BlackBone)


# --- Add ALL Include Directories to the Executable ---
target_include_directories(${PROJECT_NAME} PRIVATE
    # your project's own headers
    ${CMAKE_CURRENT_SOURCE_DIR}

    # external libs
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${ULTRALIGHT_SDK_DIR}/include
    ${TINYFILEDIALOGS_DIR}
    ${SQLITECPP_DIR}/include

    # The main BlackBone include path.
    "${BLACKBONE_ROOT_DIR}/src"
    
    # Include path for Microsoft DIA SDK headers
    "${BLACKBONE_ROOT_DIR}/src/3rd_party/DIA"
)


# --- Link against required libraries ---
target_link_libraries(${PROJECT_NAME} PRIVATE
    d3d11
    d3dcompiler
    dxgi
    gdiplus
    ntdll # blackbone dependency

    # Link against the Ultralight libraries
    ${ULTRALIGHT_SDK_DIR}/lib/Ultralight.lib
    ${ULTRALIGHT_SDK_DIR}/lib/UltralightCore.lib
    ${ULTRALIGHT_SDK_DIR}/lib/AppCore.lib
    ${ULTRALIGHT_SDK_DIR}/lib/WebCore.lib

    # Link against the BlackBone target built from source
    BlackBone

    # Link against the DIA SDK library required by BlackBone's PDB parser
    "${BLACKBONE_ROOT_DIR}/src/3rd_party/DIA/lib/amd64/diaguids.lib"

    # Link against SQLiteCpp and its dependency
    SQLiteCpp
    sqlite3
)

# --- Compiler Definitions ---
target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX)

# --- Post-Build Step: Copy Ultralight Assets ---
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
