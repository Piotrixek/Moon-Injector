cmake_minimum_required(VERSION 3.15)
project(MoonInjector LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)


set(IMGUI_DIR "C:/libs/imgui-docking")
set(ULTRALIGHT_SDK_DIR "C:/libs/Ultralight")
set(TINYFILEDIALOGS_DIR "C:/libs/tinyfiledialogs")
set(BLACKBONE_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/BlackBone")
set(SQLITECPP_DIR "C:/libs/SQLiteCpp-3.3.3")



add_subdirectory(${SQLITECPP_DIR} extern/SQLiteCpp)



file(GLOB APP_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

file(GLOB IMGUI_SOURCES
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_win32.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_dx11.cpp"
)

set(TINY_DIALOGS_SRC "${TINYFILEDIALOGS_DIR}/tinyfiledialogs.c")



add_executable(${PROJECT_NAME} WIN32
    ${APP_SOURCES}
    ${IMGUI_SOURCES}
    ${TINY_DIALOGS_SRC}
)


add_subdirectory(${BLACKBONE_ROOT_DIR}/src/BlackBone)



target_include_directories(${PROJECT_NAME} PRIVATE

    ${CMAKE_CURRENT_SOURCE_DIR}


    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${ULTRALIGHT_SDK_DIR}/include
    ${TINYFILEDIALOGS_DIR}
    ${SQLITECPP_DIR}/include


    "${BLACKBONE_ROOT_DIR}/src"


    "${BLACKBONE_ROOT_DIR}/src/3rd_party/DIA"
)



target_link_libraries(${PROJECT_NAME} PRIVATE
    d3d11
    d3dcompiler
    dxgi
    gdiplus
    ntdll


    ${ULTRALIGHT_SDK_DIR}/lib/Ultralight.lib
    ${ULTRALIGHT_SDK_DIR}/lib/UltralightCore.lib
    ${ULTRALIGHT_SDK_DIR}/lib/AppCore.lib
    ${ULTRALIGHT_SDK_DIR}/lib/WebCore.lib


    BlackBone


    "${BLACKBONE_ROOT_DIR}/src/3rd_party/DIA/lib/amd64/diaguids.lib"


    SQLiteCpp
    sqlite3
)


target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX)


add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${ULTRALIGHT_SDK_DIR}/bin"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${ULTRALIGHT_SDK_DIR}/resources"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/resources"
    COMMENT "Copying Ultralight DLLs and resources"
)



if(MSVC)
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "-mwindows")
endif()
