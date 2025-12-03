import 'dart:ffi';
import 'dart:io';
import 'dart:isolate';

import 'package:ffi/ffi.dart';

import 'flutter_bspatch_bindings_generated.dart';

/// bspatch 异常
class BsPatchException implements Exception {
  final int code;
  final String message;

  BsPatchException(this.code, this.message);

  @override
  String toString() => 'BsPatchException($code): $message';
}

/// bspatch 核心类
///
/// 使用示例：
/// ```dart
/// await BsPatch.apply(
///   oldFile: '/path/to/old.apk',
///   patchFile: '/path/to/update.patch',
///   newFile: '/path/to/new.apk',
/// );
/// ```
class BsPatch {
  BsPatch._();

  /// 应用补丁
  ///
  /// [oldFile] - 原始文件路径
  /// [patchFile] - 补丁文件路径
  /// [newFile] - 输出文件路径
  ///
  /// 在后台 Isolate 中执行，不阻塞 UI
  static Future<void> apply({
    required String oldFile,
    required String patchFile,
    required String newFile,
  }) async {
    final result = await Isolate.run(() {
      return _applySync(oldFile, patchFile, newFile);
    });

    if (result != 0) {
      throw BsPatchException(result, _getErrorString(result));
    }
  }

  static int _applySync(String oldFile, String patchFile, String newFile) {
    final oldFilePtr = oldFile.toNativeUtf8();
    final patchFilePtr = patchFile.toNativeUtf8();
    final newFilePtr = newFile.toNativeUtf8();

    try {
      return _bindings.bspatch_apply(
        oldFilePtr.cast(),
        patchFilePtr.cast(),
        newFilePtr.cast(),
      );
    } finally {
      malloc.free(oldFilePtr);
      malloc.free(patchFilePtr);
      malloc.free(newFilePtr);
    }
  }

  static String _getErrorString(int code) {
    final ptr = _bindings.bspatch_error_string(code);
    return ptr.cast<Utf8>().toDartString();
  }
}

// ========== FFI 绑定 ==========

const String _libName = 'flutter_bspatch';

final DynamicLibrary _dylib = () {
  if (Platform.isMacOS || Platform.isIOS) {
    return DynamicLibrary.open('$_libName.framework/$_libName');
  }
  if (Platform.isAndroid || Platform.isLinux) {
    return DynamicLibrary.open('lib$_libName.so');
  }
  if (Platform.isWindows) {
    return DynamicLibrary.open('$_libName.dll');
  }
  throw UnsupportedError('Unknown platform: ${Platform.operatingSystem}');
}();

final FlutterBspatchBindings _bindings = FlutterBspatchBindings(_dylib);
