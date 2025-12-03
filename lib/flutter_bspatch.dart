/// Flutter bspatch 插件
///
/// 提供简洁的 bspatch 补丁应用功能
library flutter_bspatch;

import 'dart:async';
import 'dart:ffi';
import 'dart:io';
import 'dart:isolate';
import 'dart:typed_data';

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
/// // 方式1：文件路径
/// await BsPatch.apply(
///   oldFile: '/path/to/old.apk',
///   patchFile: '/path/to/update.patch',
///   newFile: '/path/to/new.apk',
/// );
///
/// // 方式2：字节数据
/// final newData = await BsPatch.applyBytes(
///   oldData: oldBytes,
///   patchData: patchBytes,
/// );
/// ```
class BsPatch {
  BsPatch._();

  /// 应用补丁（文件方式）
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

  /// 应用补丁（字节方式）
  ///
  /// [oldData] - 原始数据
  /// [patchData] - 补丁数据
  ///
  /// 返回新文件的字节数据
  /// 在后台 Isolate 中执行，不阻塞 UI
  static Future<Uint8List> applyBytes({
    required Uint8List oldData,
    required Uint8List patchData,
  }) async {
    return await Isolate.run(() {
      return _applyBytesSync(oldData, patchData);
    });
  }

  /// 获取补丁应用后的新文件大小
  ///
  /// [patchData] - 补丁数据
  ///
  /// 返回新文件大小，如果补丁无效则抛出异常
  static int getNewSize(Uint8List patchData) {
    final patchPtr = malloc<Uint8>(patchData.length);
    try {
      patchPtr.asTypedList(patchData.length).setAll(0, patchData);

      final result = _bindings.bspatch_new_size(patchPtr, patchData.length);
      if (result < 0) {
        throw BsPatchException(result.toInt(), _getErrorString(result.toInt()));
      }
      return result.toInt();
    } finally {
      malloc.free(patchPtr);
    }
  }

  // ========== 私有方法 ==========

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

  static Uint8List _applyBytesSync(Uint8List oldData, Uint8List patchData) {
    // 分配输入缓冲区
    final oldPtr = malloc<Uint8>(oldData.length);
    final patchPtr = malloc<Uint8>(patchData.length);

    oldPtr.asTypedList(oldData.length).setAll(0, oldData);
    patchPtr.asTypedList(patchData.length).setAll(0, patchData);

    try {
      // 获取新文件大小
      final newSize = _bindings.bspatch_new_size(patchPtr, patchData.length);
      if (newSize < 0) {
        throw BsPatchException(
          newSize.toInt(),
          _getErrorString(newSize.toInt()),
        );
      }

      // 分配输出缓冲区
      final newPtr = malloc<Uint8>(newSize.toInt());
      final newSizePtr = malloc<Int64>();
      newSizePtr.value = newSize;

      try {
        final result = _bindings.bspatch_apply_bytes(
          oldPtr,
          oldData.length,
          patchPtr,
          patchData.length,
          newPtr,
          newSizePtr,
        );

        if (result != 0) {
          throw BsPatchException(result, _getErrorString(result));
        }

        // 复制结果
        return Uint8List.fromList(newPtr.asTypedList(newSizePtr.value.toInt()));
      } finally {
        malloc.free(newPtr);
        malloc.free(newSizePtr);
      }
    } finally {
      malloc.free(oldPtr);
      malloc.free(patchPtr);
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
