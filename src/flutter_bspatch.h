#ifndef FLUTTER_BSPATCH_H
#define FLUTTER_BSPATCH_H

#include <stdint.h>
#include <stddef.h>

#if _WIN32
#define FFI_PLUGIN_EXPORT __declspec(dllexport)
#else
#define FFI_PLUGIN_EXPORT
#endif

// 错误码定义
#define BSPATCH_SUCCESS          0
#define BSPATCH_ERROR_INVALID_PATCH    -1
#define BSPATCH_ERROR_CORRUPT_PATCH    -2
#define BSPATCH_ERROR_MEMORY           -3
#define BSPATCH_ERROR_IO               -4
#define BSPATCH_ERROR_SIZE_MISMATCH    -5

/**
 * 应用 bspatch 补丁（文件路径方式）
 * 
 * @param old_file    原始文件路径
 * @param patch_file  补丁文件路径  
 * @param new_file    输出文件路径
 * @return            0 表示成功，负数表示错误码
 */
FFI_PLUGIN_EXPORT int bspatch_apply(
    const char* old_file,
    const char* patch_file,
    const char* new_file
);

/**
 * 应用 bspatch 补丁（内存方式）
 * 
 * @param old_data       原始数据指针
 * @param old_size       原始数据大小
 * @param patch_data     补丁数据指针
 * @param patch_size     补丁数据大小
 * @param new_data       输出数据指针（由调用者分配）
 * @param new_size       输出数据大小（输入：缓冲区大小，输出：实际大小）
 * @return               0 表示成功，负数表示错误码
 */
FFI_PLUGIN_EXPORT int bspatch_apply_bytes(
    const uint8_t* old_data,
    int64_t old_size,
    const uint8_t* patch_data,
    int64_t patch_size,
    uint8_t* new_data,
    int64_t* new_size
);

/**
 * 从补丁文件获取新文件大小
 * 
 * @param patch_data  补丁数据指针
 * @param patch_size  补丁数据大小
 * @return            新文件大小，负数表示错误
 */
FFI_PLUGIN_EXPORT int64_t bspatch_new_size(
    const uint8_t* patch_data,
    int64_t patch_size
);

/**
 * 获取错误描述
 * 
 * @param error_code  错误码
 * @return            错误描述字符串
 */
FFI_PLUGIN_EXPORT const char* bspatch_error_string(int error_code);

#endif // FLUTTER_BSPATCH_H
