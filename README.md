# flutter_bspatch

Flutter 插件，提供 bspatch 二进制差分补丁功能。基于 [bsdiff](https://github.com/mendsley/bsdiff) 实现。

## ✨ 特性

- 🚀 **简洁 API** - 一个方法搞定一切
- 📱 **全平台支持** - Android、iOS、macOS、Windows、Linux
- ⚡ **高性能** - 基于原生 C 代码，通过 FFI 直接调用
- 🔒 **异步执行** - 在后台 Isolate 中运行，不阻塞 UI
- 📦 **兼容性好** - 同时支持 BSDIFF40 和 BSDIFF43 格式

## 📦 安装

```yaml
dependencies:
  flutter_bspatch: ^0.0.1
```

## 🔧 使用方法

```dart
import 'package:flutter_bspatch/flutter_bspatch.dart';

await BsPatch.patch(
  oldFile: '/path/to/old_version.apk',
  patchFile: '/path/to/update.patch',
  newFile: '/path/to/new_version.apk',
);
```

## ⚠️ 错误处理

```dart
try {
  await BsPatch.patch(
    oldFile: oldPath,
    patchFile: patchPath,
    newFile: newPath,
  );
} on BsPatchException catch (e) {
  print('错误码: ${e.code}');
  print('错误信息: ${e.message}');
}
```

### 错误码

| 错误码 | 说明 |
|--------|------|
| -1 | 无效的补丁文件 |
| -2 | 补丁文件损坏 |
| -3 | 内存分配失败 |
| -4 | I/O 错误 |
| -5 | 缓冲区大小不匹配 |

## 🛠 生成补丁

使用 bsdiff 工具生成补丁文件：

```bash
# 安装 bsdiff (macOS)
brew install bsdiff

# 生成补丁
bsdiff old_file new_file patch_file
```

## 📋 许可证

- 本插件：MIT License
- bspatch：BSD License (Colin Percival)
- bzip2：BSD License
