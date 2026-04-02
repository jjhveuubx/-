# XAITypeAQ Delivery Notes

当前工程支持两种交付形态：

- `DLL 插件模式`
  - 适用于霍利斯特这类 `LoadLibrary/GetProcAddress` 动态加载框架
  - 交付物核心是 `xai_type_aq.dll`
  - 导出函数：
    - `createInstance()`
    - `destroyInstance(XAITypeBase*)`
- `静态库模式`
  - 适用于宿主直接链接并实例化 `XAITypeAQ`
  - 交付物核心是 `xai_type_aq.lib`

## 当前建议

如果霍利斯特框架是动态库加载，优先使用 `DLL 插件模式`。

霍利斯特侧只需要：

- 拿到 `xai_type_aq.dll`
- 通过 `GetProcAddress` 获取 `createInstance/destroyInstance`
- 以 `XAITypeBase*` 方式调用 `start/stop/do_process_roi`

不需要直接链接 `XAITypeAQ` 类本身。

## 运行时依赖

当前版本内部使用：

- `xai_api`
- `VisionFlow`
- `OpenCV`

因此即使霍利斯特是动态加载阿丘的 `dll`，运行时仍然需要把配套依赖 DLL 放在可搜索路径中。  
如果后续要做到“最终只有一个 DLL 文件”，需要额外拿到可静态链接的 `VisionFlow/OpenCV` 版本。

## 参数约定

当前对 `aq_param.prep_param` 的映射如下：

- `prepare_char_1`
  - VisionFlow 输入节点名，默认 `输入`
- `prepare_char_2`
  - 指定只解析某个输出工具名；为空时遍历全部可解析输出
- `prepare_int_1`
  - 备用窗宽下限
- `prepare_int_2`
  - 备用窗宽上限
- `prepare_int_3`
  - 最大 GPU 数；GPU runtime 创建失败时回退 CPU
- `prepare_int_4`
  - 单次 `do_process_roi()` 内部重复推理次数，默认 `1`

## 输入图像

当前优先使用最新版 `XAI_src_t`：

- 主通道：`xhl_l`
- 第二通道：`log_l`，为空时回退到 `xhl_h`
- 第三通道：`diff`，为空时回退到 `xhl_h`

如果正式模型要求调整通道组合，修改 `BuildInferImage()` 即可。
