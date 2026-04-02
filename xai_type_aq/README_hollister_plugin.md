# 阿丘算法库交付说明

本包用于霍利斯特侧运行时加载阿丘算法库。

霍利斯特侧不需要在编译期链接阿丘业务 `lib`，也不需要单独配置 `OpenCV`、`VisionFlow` 的链接项。  
霍利斯特侧只需要在运行时加载 `xai_type_aq.dll`，并调用导出接口完成实例创建、推理和释放。

当前版本不再打包 VisionFlow 运行时。  
阿丘算法库会在运行时自动搜索宿主环境中的 VisionFlow，并在 `start()` 时完成加载。

## 1. 交付内容

- `bin/`
  - `xai_type_aq.dll`
  - `opencv_world470.dll`
  - MSVC 运行时 DLL
- `include/`
  - `xai_type_base.h`
  - `xai_inst_base.h`
  - `xai_def.h`
  - `mmessage.h`
- `model/`
  - 示例模型 `try.vfmodel`
- `tools/`
  - 本地自测程序 `xai_type_aq_verify_plugin.exe`

## 2. 接入方式

霍利斯特侧按动态库方式接入：

1. 运行时加载 `bin/xai_type_aq.dll`
2. 通过 `GetProcAddress` 获取导出函数
3. 调用 `createInstance()` 创建 `XAITypeBase*`
4. 调用 `start()` 完成模型和运行时初始化
5. 调用 `do_process_roi()` 完成单 ROI 推理
6. 调用 `stop()` 释放资源
7. 调用 `destroyInstance()` 销毁实例

## 3. 导出接口

- `createInstance() -> XAITypeBase*`
- `destroyInstance(XAITypeBase*)`

## 4. VisionFlow 搜索规则

阿丘算法库会按以下顺序查找 VisionFlow：

1. 已经被宿主进程加载的 `visionflow.dll`
2. 环境变量 `AQ_VISIONFLOW_DLL_DIR`
3. 环境变量 `AQ_VISIONFLOW_ROOT`
4. 环境变量 `VISIONFLOW_DLL_DIR`
5. 环境变量 `VISIONFLOW_ROOT`
6. 宿主程序目录及其上一级目录下的 `VisionFlow*/bin`
7. 阿丘算法库所在目录及其上一级目录下的 `VisionFlow*/bin`

如果宿主环境已经把 VisionFlow 放进 `PATH`，或者宿主本身已经加载过 VisionFlow，则不需要额外处理。

## 5. 部署要求

- `bin/` 下的 DLL 需要位于宿主程序可搜索路径中
- 宿主环境需要提供完整的 VisionFlow 运行时
- 当前版本按 OpenCV 4.7.0 构建，运行时依赖 `opencv_world470.dll`
- 如果自动搜索不到 VisionFlow，建议显式设置：
  - `AQ_VISIONFLOW_ROOT=<VisionFlow根目录>`
  - 或 `AQ_VISIONFLOW_DLL_DIR=<VisionFlow bin目录>`
- 模型路径建议使用英文路径。当前 VisionFlow 在中文绝对路径下稳定性较差

## 6. 日志说明

- 阿丘算法库默认会在 `xai_type_aq.dll` 同目录生成 `aq_plugin.log`
- 如需指定日志文件位置，可设置环境变量：
  - `AQ_LOG_FILE=<完整日志文件路径>`
- 日志内容包含：
  - `createInstance/destroyInstance`
  - `start/stop`
  - VisionFlow 搜索与加载过程
  - 模型路径解析与模型加载
  - 推理输入预处理
  - 运行时执行、结果读取、去重与最终返回
  - 每个错误返回点的原因
- VisionFlow 自身日志仍会输出到 `visionflow.log`

## 7. 头文件用途

- `xai_type_base.h`
  - 算法基类接口
- `xai_inst_base.h`
  - 基础实例接口定义
- `xai_def.h`
  - 输入、输出、配置结构体定义
- `mmessage.h`
  - 日志接口声明

## 8. 本地验证结果

已按动态加载方式完成本地验证：

- `xai_type_aq.dll` 不再在加载阶段直接依赖 `visionflow.dll`
- 运行时可通过自动搜索加载 VisionFlow
- `LoadLibrary(xai_type_aq.dll)` 成功
- `GetProcAddress(createInstance/destroyInstance)` 成功
- `start_rc=0`
- `process_rc=1`
- `stop_rc=0`
