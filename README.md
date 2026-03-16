# OSG 高斯点云渲染插件

基于 OpenSceneGraph 与 OpenGL 的高斯点云渲染插件 DLL，可在任意 OSG 应用中加载使用。

## 功能

- **高斯点云加载与渲染**：从 PLY（3DGS 格式）加载并渲染
- **视锥体裁剪**：基于八叉树的 CPU 侧裁剪（可选 GPU Compute Shader 扩展）
- **深度排序**：接口预留，可扩展 GPU 排序
- **八叉树空间组织**：加速裁剪与 LOD
- **LOD**：按屏幕尺寸过滤点
- **头部优先加载**：异步加载时优先加载文件前部
- **静态节点**：支持 `setStatic(true)` 以优化不变场景
- **性能统计**：`GaussianNode::getPerfStats()` 返回可见数量等

## 构建

### 依赖

- CMake ≥ 3.12
- OpenSceneGraph（含 osg、osgDB、osgUtil、osgViewer、osgGA、OpenThreads）

### 配置与编译

```bash
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=/path/to/OSG
cmake --build . --config Release
```

安装后可将 `osgdb_gaussian.dll`（或 `osgdb_gaussian.so`）拷贝到 OSG 的插件目录，或设置 `OSG_LIBRARY_PATH` 指向该目录。

## 使用方式

### 1. 作为 OSG 插件（推荐）

将 `osgdb_gaussian.dll` 放入 OSG 能搜索到的插件路径，然后：

```cpp
#include <osgDB/ReadFile>

// 扩展名 .gaussian 会自动加载本插件
osg::ref_ptr<osg::Node> node = osgDB::readNodeFile("your_point_cloud.gaussian");
```

若希望用本插件加载 `.gply` 文件，可在读取前添加扩展别名：

```cpp
#include <osgDB/Registry>
#include <osgDB/ReadFile>

osgDB::Registry::instance()->addFileExtensionAlias("gply", "gaussian");
osg::ref_ptr<osg::Node> node = osgDB::readNodeFile("your_point_cloud.gply");
```

### 2. 在代码中直接使用节点与读取器

```cpp
#include "GaussianNode.h"
#include "GaussianPLYReader.h"

// 同步加载
osgGaussian::GaussianPLYReader reader;
osgGaussian::LoadOptions opt;
opt.buildOctree = true;
opt.octreeMaxDepth = 8;
std::vector<osgGaussian::GaussianPoint> points = reader.load("data.ply", opt);

osg::ref_ptr<osgGaussian::GaussianNode> node = new osgGaussian::GaussianNode();
node->addGaussiansWithOctree(points, 8, 1024);
node->setStatic(true);
node->setLODParameters(2.0f, 256.0f, 4);
node->setPointSizeScale(1.5f);

// 加入场景
root->addChild(node.get());

// 性能统计
const auto& stats = node->getPerfStats();
// stats.visibleCount, stats.totalCount
```

### 3. 异步加载

```cpp
osgGaussian::GaussianPLYReader reader;
osgGaussian::LoadOptions opt;
opt.asyncLoad = true;
opt.headPriority = true;

reader.loadAsync("large.ply", opt,
    [node](const std::vector<osgGaussian::GaussianPoint>& chunk) {
        node->addGaussians(chunk);
    },
    []() {
        node->buildOctree();  // 在加载完成回调中构建八叉树
        node->uploadGPUData();
    });
```

## PLY 格式

支持 3D Gaussian Splatting 常用属性（与顺序无关，按名称匹配）：

- 位置：`x`, `y`, `z`
- 颜色：`red`/`green`/`blue`（0–255）或 `f_dc_0`/`f_dc_1`/`f_dc_2`
- 不透明度：`opacity`
- 尺度：`scale_0`, `scale_1`, `scale_2`
- 旋转：`rot_0`, `rot_1`, `rot_2`, `rot_3`

支持 ASCII 与 binary little/big endian PLY。

## 性能测试

在应用层可基于 `GaussianNode::getPerfStats()` 做简单性能监控：

```cpp
const auto& s = gaussianNode->getPerfStats();
printf("Visible: %u / %u\n", s.visibleCount, s.totalCount);
```

可将上述调用放在每帧或按需采样，用于统计可见点数和总点数。

## 文件说明

| 路径 | 说明 |
|------|------|
| `include/GaussianNode.h` | 渲染节点：八叉树、LOD、绘制 |
| `include/GaussianPLYReader.h` | PLY 加载：同步/异步、头部优先 |
| `include/GaussianPoint.h` | 点数据结构 |
| `include/Octree.h` | 八叉树与可见性收集 |
| `include/ReaderWriterGaussian.h` | OSG 插件 ReaderWriter |
| `src/Shaders.inl` | 预留 Compute/渲染着色器（可扩展 SSBO 路径） |

## 许可证

与 OpenSceneGraph 兼容的 LGPL 或按项目需求自行选择。
