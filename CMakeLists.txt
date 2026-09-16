cmake_minimum_required(VERSION 3.16)
project(solar_system VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core Gui Qml Quick QuickControls2)

qt_standard_project_setup()

# ---------------------------------------------------------------------------
#  目标声明 —— 不含源文件。
#
#  ★ 源文件只能出现一次: 若同时写进 qt_add_executable 与下面的模块 SOURCES,
#    会被编译两次, 链接期报 "multiple definition of main"。
# ---------------------------------------------------------------------------
qt_add_executable(solar_system)

# 保留控制台窗口, 便于观察自检与调试日志。
# (注意: 这不是 qt_add_executable 的关键字 —— 写成关键字会被当作源文件路径。)
set_target_properties(solar_system PROPERTIES WIN32_EXECUTABLE FALSE)

qt_add_qml_module(solar_system
    URI SolarSystem
    VERSION 1.0
    QML_FILES
        qml/Main.qml
    SOURCES
        src/main.cpp
        src/celestialdata.h    src/celestialdata.cpp
        src/ephemeris.h        src/ephemeris.cpp
        src/camera.h           src/camera.cpp
        src/mesh.h             src/mesh.cpp
        src/shaders.h
        src/textures.h         src/textures.cpp
        src/scene.h            src/scene.cpp
        src/belts.h              src/belts.cpp
        src/comet.h              src/comet.cpp
        src/cosmos.h             src/cosmos.cpp
        src/cosmosdata.h         src/cosmosdata.cpp
        src/bodyregistry.h       src/bodyregistry.cpp
        src/orbtest.cpp
        src/smalldata.h          src/smalldata.cpp
        src/galaxyarms.h        src/galaxyarms.cpp
        src/galaxydata.h
        src/galaxy.h           src/galaxy.cpp
        src/postfx.h           src/postfx.cpp
        src/scenerenderer.h    src/scenerenderer.cpp
        src/sceneitem.h        src/sceneitem.cpp
)

target_include_directories(solar_system PRIVATE src)

target_link_libraries(solar_system PRIVATE
    Qt6::Core Qt6::Gui
    Qt6::Qml Qt6::Quick Qt6::QuickControls2
    Qt6::OpenGL)

# 静态链接 GCC 运行库。
# 注意: 这**不能**免除 libstdc++-6.dll 依赖 —— MSYS2 的 Qt DLL 自身会拉进来,
# 部署时仍须把 ucrt64/bin 下的那一批 DLL 一并拷走 (见 skill 文档)。
if(MINGW)
    target_link_options(solar_system PRIVATE
        -static-libgcc -static-libstdc++ -static)
endif()
