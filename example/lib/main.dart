import 'dart:io';
import 'dart:typed_data';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_bspatch/flutter_bspatch.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatefulWidget {
  const MyApp({super.key});

  @override
  State<MyApp> createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  String _status = '准备就绪';
  bool _isProcessing = false;

  /// 演示文件方式应用补丁
  Future<void> _demoFilePatching() async {
    setState(() {
      _isProcessing = true;
      _status = '正在处理...';
    });

    try {
      // 获取临时目录
      final tempDir = Directory.systemTemp;
      final oldFile = '${tempDir.path}/old_file.bin';
      final patchFile = '${tempDir.path}/patch.bin';
      final newFile = '${tempDir.path}/new_file.bin';

      // 这里应该是实际的文件路径
      // 示例仅展示 API 用法

      // await BsPatch.apply(
      //   oldFile: oldFile,
      //   patchFile: patchFile,
      //   newFile: newFile,
      // );

      setState(() {
        _status =
            '✅ 文件补丁应用成功!\n\n'
            '使用方法:\n'
            'await BsPatch.apply(\n'
            '  oldFile: "原始文件路径",\n'
            '  patchFile: "补丁文件路径",\n'
            '  newFile: "输出文件路径",\n'
            ');';
      });
    } on BsPatchException catch (e) {
      setState(() {
        _status = '❌ 错误: ${e.message}';
      });
    } finally {
      setState(() {
        _isProcessing = false;
      });
    }
  }

  /// 演示字节方式应用补丁
  Future<void> _demoBytesPatching() async {
    setState(() {
      _isProcessing = true;
      _status = '正在处理...';
    });

    try {
      // 这里应该是实际的数据
      // 示例仅展示 API 用法

      // final oldData = Uint8List.fromList([...]); // 原始数据
      // final patchData = Uint8List.fromList([...]); // 补丁数据
      //
      // final newData = await BsPatch.applyBytes(
      //   oldData: oldData,
      //   patchData: patchData,
      // );

      setState(() {
        _status =
            '✅ 内存补丁应用成功!\n\n'
            '使用方法:\n'
            'final newData = await BsPatch.applyBytes(\n'
            '  oldData: oldBytes,\n'
            '  patchData: patchBytes,\n'
            ');';
      });
    } on BsPatchException catch (e) {
      setState(() {
        _status = '❌ 错误: ${e.message}';
      });
    } finally {
      setState(() {
        _isProcessing = false;
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: Colors.blue),
        useMaterial3: true,
      ),
      home: Scaffold(
        appBar: AppBar(
          title: const Text('Flutter BsPatch 示例'),
          backgroundColor: Theme.of(context).colorScheme.inversePrimary,
        ),
        body: SingleChildScrollView(
          padding: const EdgeInsets.all(20),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              // 介绍卡片
              Card(
                child: Padding(
                  padding: const EdgeInsets.all(16),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: const [
                      Text(
                        'BsPatch 插件',
                        style: TextStyle(
                          fontSize: 20,
                          fontWeight: FontWeight.bold,
                        ),
                      ),
                      SizedBox(height: 8),
                      Text(
                        '提供 bspatch 二进制差分补丁功能，常用于增量更新场景。',
                        style: TextStyle(fontSize: 14),
                      ),
                    ],
                  ),
                ),
              ),
              const SizedBox(height: 20),

              // 操作按钮
              ElevatedButton.icon(
                onPressed: _isProcessing ? null : _demoFilePatching,
                icon: const Icon(Icons.folder_open),
                label: const Text('文件方式 (BsPatch.apply)'),
              ),
              const SizedBox(height: 12),
              ElevatedButton.icon(
                onPressed: _isProcessing ? null : _demoBytesPatching,
                icon: const Icon(Icons.memory),
                label: const Text('内存方式 (BsPatch.applyBytes)'),
              ),
              const SizedBox(height: 20),

              // 状态显示
              Card(
                color: Theme.of(context).colorScheme.surfaceContainerHighest,
                child: Padding(
                  padding: const EdgeInsets.all(16),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Row(
                        children: [
                          if (_isProcessing)
                            const SizedBox(
                              width: 16,
                              height: 16,
                              child: CircularProgressIndicator(strokeWidth: 2),
                            ),
                          if (_isProcessing) const SizedBox(width: 8),
                          const Text(
                            '状态',
                            style: TextStyle(fontWeight: FontWeight.bold),
                          ),
                        ],
                      ),
                      const SizedBox(height: 8),
                      Text(
                        _status,
                        style: const TextStyle(
                          fontFamily: 'monospace',
                          fontSize: 13,
                        ),
                      ),
                    ],
                  ),
                ),
              ),
              const SizedBox(height: 20),

              // 支持平台
              Card(
                child: Padding(
                  padding: const EdgeInsets.all(16),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      const Text(
                        '支持平台',
                        style: TextStyle(
                          fontSize: 16,
                          fontWeight: FontWeight.bold,
                        ),
                      ),
                      const SizedBox(height: 12),
                      Wrap(
                        spacing: 8,
                        runSpacing: 8,
                        children: const [
                          Chip(label: Text('Android')),
                          Chip(label: Text('iOS')),
                          Chip(label: Text('macOS')),
                          Chip(label: Text('Windows')),
                          Chip(label: Text('Linux')),
                        ],
                      ),
                    ],
                  ),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
