cmake_minimum_required(VERSION 3.12)
project(osgGaussianPlugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)

# 查找 OpenSceneGraph：优先 find_package，失败则用 CMAKE_PREFIX_PATH 手动指定
set(OSG_ROOT "" CACHE PATH "OpenSceneGraph install root (e.g. E:/osg/OpenSceneGraph-3.6.5-VC2022-64-2025-04)")
if(CMAKE_PREFIX_PATH)
    list(GET CMAKE_PREFIX_PATH 0 _first)
    if(EXISTS "${_first}/include/osg/Version")
        set(OSG_ROOT "${_first}")
    endif()
endif()
if(NOT OSG_ROOT AND DEFINED ENV{OSG_ROOT})
    set(OSG_ROOT "$ENV{OSG_ROOT}")
endif()

find_package(OpenSceneGraph QUIET COMPONENTS osg osgDB osgUtil osgViewer osgGA OpenThreads)
set(HAVE_OSGTEXT FALSE)
if(OPENSCENEGRAPH_FOUND)
    find_package(OpenSceneGraph QUIET COMPONENTS osgText)
    if(OPENSCENEGRAPH_FOUND)
        set(HAVE_OSGTEXT TRUE)
    endif()
endif()
if(NOT OPENSCENEGRAPH_FOUND AND OSG_ROOT)
    message(STATUS "Using OSG_ROOT: ${OSG_ROOT}")
    set(OPENSCENEGRAPH_INCLUDE_DIRS "${OSG_ROOT}/include")
    set(OPENSCENEGRAPH_LIBRARIES
        optimized "${OSG_ROOT}/lib/osg.lib"
        optimized "${OSG_ROOT}/lib/osgDB.lib"
        optimized "${OSG_ROOT}/lib/osgUtil.lib"
        optimized "${OSG_ROOT}/lib/osgViewer.lib"
        optimized "${OSG_ROOT}/lib/osgGA.lib"
        optimized "${OSG_ROOT}/lib/OpenThreads.lib"
        debug "${OSG_ROOT}/lib/osgd.lib"
        debug "${OSG_ROOT}/lib/osgDBd.lib"
        debug "${OSG_ROOT}/lib/osgUtild.lib"
        debug "${OSG_ROOT}/lib/osgViewerd.lib"
        debug "${OSG_ROOT}/lib/osgGAd.lib"
        debug "${OSG_ROOT}/lib/OpenThreadsd.lib"
    )
    if(EXISTS "${OSG_ROOT}/lib/osgText.lib")
        list(APPEND OPENSCENEGRAPH_LIBRARIES optimized "${OSG_ROOT}/lib/osgText.lib" debug "${OSG_ROOT}/lib/osgTextd.lib")
        set(HAVE_OSGTEXT TRUE)
    endif()
    set(OPENSCENEGRAPH_FOUND TRUE)
endif()
if(NOT OPENSCENEGRAPH_FOUND)
    message(FATAL_ERROR "OpenSceneGraph not found. Set -DCMAKE_PREFIX_PATH=E:/osg/OpenSceneGraph-3.6.5-VC2022-64-2025-04 (no space after -D)")
endif()

include_directories(
    ${OPENSCENEGRAPH_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set(SHARED_SOURCES
    src/GaussianNode.cpp
    src/GaussianPLYReader.cpp
    src/Octree.cpp
    src/ReaderWriterGaussian.cpp
)

# OSG 插件 DLL 命名: osgdb_<format>.dll 以便 OSG 按扩展名自动加载
# 这里命名为 osgdb_gply.dll，仅接管 .gply / .gaussian / .ply（通过 ReaderWriter 逻辑区分）
add_library(osgdb_gply SHARED ${SHARED_SOURCES})

target_link_libraries(osgdb_gply
    ${OPENSCENEGRAPH_LIBRARIES}
)

# Windows: OSG 内部会调用 OpenGL API，需要链接系统 OpenGL 库
if (WIN32)
    target_link_libraries(osgdb_gply opengl32)
endif()

target_include_directories(osgdb_gply PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# MSVC: 以 UTF-8 解析源码，避免 C4819（中文注释/字符串导致的代码页警告）
if (MSVC)
    target_compile_options(osgdb_gply PRIVATE /utf-8)
endif()

# Windows: 导出 DLL 符号（如需要）
if(WIN32)
    target_compile_definitions(osgdb_gply PRIVATE
        OSG_LIBRARY
        _USE_MATH_DEFINES
    )
endif()
if(HAVE_OSGTEXT)
    target_compile_definitions(osgdb_gply PRIVATE HAVE_OSGTEXT=1)
endif()

# 安装到 OSG 插件目录或指定目录
install(TARGETS osgdb_gply
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# 可选：性能测试/查看器示例（需 OSG）
option(BUILD_GAUSSIAN_VIEWER "Build example gaussian viewer" OFF)
if(BUILD_GAUSSIAN_VIEWER)
    add_executable(gaussian_viewer examples/gaussian_viewer.cpp)
    target_link_libraries(gaussian_viewer ${OPENSCENEGRAPH_LIBRARIES})
    target_include_directories(gaussian_viewer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${OPENSCENEGRAPH_INCLUDE_DIRS})
endif()

# 小工具：打印当前 OpenGL / GLSL 版本（用于调试环境）
add_executable(glinfo src/glinfo.cpp)
target_link_libraries(glinfo ${OPENSCENEGRAPH_LIBRARIES})
target_include_directories(glinfo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${OPENSCENEGRAPH_INCLUDE_DIRS})
if (WIN32)
    target_link_libraries(glinfo opengl32)
endif()
