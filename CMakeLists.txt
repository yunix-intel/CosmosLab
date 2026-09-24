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

# 发布形态: 无控制台窗口 (崩溃排查用 SS_DEBUG=1 运行时加日志文件, 见 main.cpp)。
# 开发期如需看日志, 临时改回 FALSE 即可。
set_target_properties(solar_system PROPERTIES WIN32_EXECUTABLE TRUE)
# ★ 产品名: 宇宙实验室 CosmosLab。target 名保持 solar_system 不动
#   (QML 模块 URI 联动风险), 只改输出文件名 -> cosmoslab.exe。
set_target_properties(solar_system PROPERTIES OUTPUT_NAME cosmoslab)

qt_add_qml_module(solar_system
    URI SolarSystem
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/EvoOverlay.qml
        qml/StellarPanel.qml
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
        src/stellardata.h      src/stellardata.cpp
        src/agndata.h          src/agndata.cpp
        src/ismdata.h          src/ismdata.cpp
        src/postfx.h           src/postfx.cpp
        src/scenerenderer.h    src/scenerenderer.cpp
        src/sceneitem.h        src/sceneitem.cpp
)

target_include_directories(solar_system PRIVATE src)

target_link_libraries(solar_system PRIVATE
    Qt6::Core Qt6::Gui
    Qt6::Qml Qt6::Quick Qt6::QuickControls2
    Qt6::OpenGL)

# 配音播放走 WinMM mciSendString (系统库, 无新增依赖)
if(WIN32)
    target_link_libraries(solar_system PRIVATE winmm)
endif()

# 静态链接 GCC 运行库。
# 注意: 这**不能**免除 libstdc++-6.dll 依赖 —— MSYS2 的 Qt DLL 自身会拉进来,
# 部署时仍须把 ucrt64/bin 下的那一批 DLL 一并拷走 (见 skill 文档)。
if(MINGW)
    target_link_options(solar_system PRIVATE
        -static-libgcc -static-libstdc++ -static)
endif()
