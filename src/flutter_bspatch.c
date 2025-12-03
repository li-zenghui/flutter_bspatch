/*
 * Flutter bspatch FFI wrapper
 * 封装 bspatch 功能供 Flutter 调用
 * 支持 BSDIFF40 和 ENDSLEY/BSDIFF43 两种格式
 */

#include "flutter_bspatch.h"
#include "bspatch.h"
#include "bzlib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// bzip2 需要的错误处理函数（当 BZ_NO_STDIO 定义时）
void bz_internal_error(int errcode) {
    (void)errcode;
    // 静默处理错误，实际错误会通过返回值传递
}

// 补丁文件魔数
static const char BSPATCH_MAGIC_40[] = "BSDIFF40";      // 原始格式
static const char BSPATCH_MAGIC_43[] = "ENDSLEY/BSDIFF43"; // mendsley 格式

#define HEADER_SIZE_40 32  // BSDIFF40: 8 + 8 + 8 + 8
#define HEADER_SIZE_43 24  // BSDIFF43: 16 + 8

// 从字节数组读取 int64_t（小端序，带符号位）
static int64_t read_offset(const uint8_t *buf) {
    int64_t y;
    y = buf[7] & 0x7F;
    y = y * 256; y += buf[6];
    y = y * 256; y += buf[5];
    y = y * 256; y += buf[4];
    y = y * 256; y += buf[3];
    y = y * 256; y += buf[2];
    y = y * 256; y += buf[1];
    y = y * 256; y += buf[0];
    if (buf[7] & 0x80) y = -y;
    return y;
}

// 补丁格式类型
typedef enum {
    PATCH_FORMAT_UNKNOWN = 0,
    PATCH_FORMAT_40,      // BSDIFF40
    PATCH_FORMAT_43       // ENDSLEY/BSDIFF43
} patch_format_t;

// 检测补丁格式
static patch_format_t detect_format(const uint8_t *patch_data, int64_t patch_size) {
    if (patch_size >= HEADER_SIZE_40 && memcmp(patch_data, BSPATCH_MAGIC_40, 8) == 0) {
        return PATCH_FORMAT_40;
    }
    if (patch_size >= HEADER_SIZE_43 && memcmp(patch_data, BSPATCH_MAGIC_43, 16) == 0) {
        return PATCH_FORMAT_43;
    }
    return PATCH_FORMAT_UNKNOWN;
}

// bzip2 内存读取上下文
typedef struct {
    bz_stream strm;
    const uint8_t *data;
    int64_t size;
    int64_t pos;
    int initialized;
} bz2_mem_context;

// bzip2 内存流读取回调
static int bz2_mem_read(const struct bspatch_stream *stream, void *buffer, int length) {
    bz2_mem_context *ctx = (bz2_mem_context *)stream->opaque;
    
    ctx->strm.next_out = (char *)buffer;
    ctx->strm.avail_out = (unsigned int)length;
    
    while (ctx->strm.avail_out > 0) {
        if (ctx->strm.avail_in == 0 && ctx->pos < ctx->size) {
            int64_t remaining = ctx->size - ctx->pos;
            unsigned int chunk = remaining > 4096 ? 4096 : (unsigned int)remaining;
            ctx->strm.next_in = (char *)(ctx->data + ctx->pos);
            ctx->strm.avail_in = chunk;
            ctx->pos += chunk;
        }
        
        int ret = BZ2_bzDecompress(&ctx->strm);
        if (ret != BZ_OK && ret != BZ_STREAM_END) {
            return -1;
        }
        if (ret == BZ_STREAM_END && ctx->strm.avail_out > 0) {
            return -1; // 数据不足
        }
    }
    
    return 0;
}

// BSDIFF40 格式的三流读取上下文
typedef struct {
    bz_stream ctrl_strm;
    bz_stream diff_strm;
    bz_stream extra_strm;
    const uint8_t *ctrl_data;
    const uint8_t *diff_data;
    const uint8_t *extra_data;
    int64_t ctrl_size;
    int64_t diff_size;
    int64_t extra_size;
    int64_t ctrl_pos;
    int64_t diff_pos;
    int64_t extra_pos;
    int phase; // 0=ctrl, 1=diff, 2=extra
    int ctrl_count; // 0, 1, 2 for the three control values
} bz2_40_context;

// BSDIFF40 格式的流读取回调
static int bz2_40_read(const struct bspatch_stream *stream, void *buffer, int length) {
    bz2_40_context *ctx = (bz2_40_context *)stream->opaque;
    bz_stream *strm;
    const uint8_t *data;
    int64_t *pos;
    int64_t size;
    
    // 根据当前阶段选择正确的流
    switch (ctx->phase) {
        case 0: // control
            strm = &ctx->ctrl_strm;
            data = ctx->ctrl_data;
            pos = &ctx->ctrl_pos;
            size = ctx->ctrl_size;
            break;
        case 1: // diff
            strm = &ctx->diff_strm;
            data = ctx->diff_data;
            pos = &ctx->diff_pos;
            size = ctx->diff_size;
            break;
        case 2: // extra
            strm = &ctx->extra_strm;
            data = ctx->extra_data;
            pos = &ctx->extra_pos;
            size = ctx->extra_size;
            break;
        default:
            return -1;
    }
    
    strm->next_out = (char *)buffer;
    strm->avail_out = (unsigned int)length;
    
    while (strm->avail_out > 0) {
        if (strm->avail_in == 0 && *pos < size) {
            int64_t remaining = size - *pos;
            unsigned int chunk = remaining > 4096 ? 4096 : (unsigned int)remaining;
            strm->next_in = (char *)(data + *pos);
            strm->avail_in = chunk;
            *pos += chunk;
        }
        
        int ret = BZ2_bzDecompress(strm);
        if (ret != BZ_OK && ret != BZ_STREAM_END) {
            return -1;
        }
        if (ret == BZ_STREAM_END && strm->avail_out > 0) {
            return -1;
        }
    }
    
    // 更新阶段（用于 bspatch 的读取模式）
    if (ctx->phase == 0) {
        ctx->ctrl_count++;
        if (ctx->ctrl_count >= 3) {
            ctx->ctrl_count = 0;
        }
    }
    
    return 0;
}

// 从文件读取全部内容
static uint8_t *read_file_contents(const char *path, int64_t *size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    uint8_t *data = (uint8_t *)malloc((size_t)*size);
    if (!data) {
        fclose(f);
        return NULL;
    }
    
    if (fread(data, 1, (size_t)*size, f) != (size_t)*size) {
        free(data);
        fclose(f);
        return NULL;
    }
    
    fclose(f);
    return data;
}

// 写入文件
static int write_file_contents(const char *path, const uint8_t *data, int64_t size) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    
    if (fwrite(data, 1, (size_t)size, f) != (size_t)size) {
        fclose(f);
        return -1;
    }
    
    fclose(f);
    return 0;
}

FFI_PLUGIN_EXPORT int64_t bspatch_new_size(
    const uint8_t *patch_data,
    int64_t patch_size
) {
    if (!patch_data) {
        return BSPATCH_ERROR_INVALID_PATCH;
    }
    
    patch_format_t format = detect_format(patch_data, patch_size);
    int64_t newsize;
    
    switch (format) {
        case PATCH_FORMAT_40:
            // BSDIFF40: newsize 在偏移 24
            newsize = read_offset(patch_data + 24);
            break;
        case PATCH_FORMAT_43:
            // BSDIFF43: newsize 在偏移 16
            newsize = read_offset(patch_data + 16);
            break;
        default:
            return BSPATCH_ERROR_INVALID_PATCH;
    }
    
    if (newsize < 0) {
        return BSPATCH_ERROR_CORRUPT_PATCH;
    }
    
    return newsize;
}

// 应用 BSDIFF40 格式补丁
static int apply_bsdiff40(
    const uint8_t *old_data,
    int64_t old_size,
    const uint8_t *patch_data,
    int64_t patch_size,
    uint8_t *new_data,
    int64_t new_size
) {
    // 读取头部
    int64_t ctrl_len = read_offset(patch_data + 8);
    int64_t diff_len = read_offset(patch_data + 16);
    
    if (ctrl_len < 0 || diff_len < 0) {
        return BSPATCH_ERROR_CORRUPT_PATCH;
    }
    
    // 计算各块的位置
    const uint8_t *ctrl_block = patch_data + 32;
    const uint8_t *diff_block = ctrl_block + ctrl_len;
    const uint8_t *extra_block = diff_block + diff_len;
    int64_t extra_len = patch_size - 32 - ctrl_len - diff_len;
    
    if (extra_len < 0) {
        return BSPATCH_ERROR_CORRUPT_PATCH;
    }
    
    // 初始化三个 bzip2 流
    bz2_40_context ctx;
    memset(&ctx, 0, sizeof(ctx));
    
    ctx.ctrl_data = ctrl_block;
    ctx.ctrl_size = ctrl_len;
    ctx.diff_data = diff_block;
    ctx.diff_size = diff_len;
    ctx.extra_data = extra_block;
    ctx.extra_size = extra_len;
    
    if (BZ2_bzDecompressInit(&ctx.ctrl_strm, 0, 0) != BZ_OK ||
        BZ2_bzDecompressInit(&ctx.diff_strm, 0, 0) != BZ_OK ||
        BZ2_bzDecompressInit(&ctx.extra_strm, 0, 0) != BZ_OK) {
        BZ2_bzDecompressEnd(&ctx.ctrl_strm);
        BZ2_bzDecompressEnd(&ctx.diff_strm);
        BZ2_bzDecompressEnd(&ctx.extra_strm);
        return BSPATCH_ERROR_MEMORY;
    }
    
    // 手动实现 bspatch 算法（针对 BSDIFF40 的三流格式）
    int64_t oldpos = 0, newpos = 0;
    uint8_t buf[8];
    int result = BSPATCH_SUCCESS;
    
    while (newpos < new_size) {
        int64_t ctrl[3];
        
        // 读取控制数据
        ctx.phase = 0;
        for (int i = 0; i < 3; i++) {
            ctx.ctrl_strm.next_out = (char *)buf;
            ctx.ctrl_strm.avail_out = 8;
            
            while (ctx.ctrl_strm.avail_out > 0) {
                if (ctx.ctrl_strm.avail_in == 0 && ctx.ctrl_pos < ctx.ctrl_size) {
                    int64_t remaining = ctx.ctrl_size - ctx.ctrl_pos;
                    unsigned int chunk = remaining > 4096 ? 4096 : (unsigned int)remaining;
                    ctx.ctrl_strm.next_in = (char *)(ctx.ctrl_data + ctx.ctrl_pos);
                    ctx.ctrl_strm.avail_in = chunk;
                    ctx.ctrl_pos += chunk;
                }
                
                int ret = BZ2_bzDecompress(&ctx.ctrl_strm);
                if (ret != BZ_OK && ret != BZ_STREAM_END) {
                    result = BSPATCH_ERROR_CORRUPT_PATCH;
                    goto cleanup;
                }
            }
            ctrl[i] = read_offset(buf);
        }
        
        // 检查边界
        if (ctrl[0] < 0 || ctrl[1] < 0 || newpos + ctrl[0] > new_size) {
            result = BSPATCH_ERROR_CORRUPT_PATCH;
            goto cleanup;
        }
        
        // 读取 diff 数据
        ctx.phase = 1;
        ctx.diff_strm.next_out = (char *)(new_data + newpos);
        ctx.diff_strm.avail_out = (unsigned int)ctrl[0];
        
        while (ctx.diff_strm.avail_out > 0) {
            if (ctx.diff_strm.avail_in == 0 && ctx.diff_pos < ctx.diff_size) {
                int64_t remaining = ctx.diff_size - ctx.diff_pos;
                unsigned int chunk = remaining > 4096 ? 4096 : (unsigned int)remaining;
                ctx.diff_strm.next_in = (char *)(ctx.diff_data + ctx.diff_pos);
                ctx.diff_strm.avail_in = chunk;
                ctx.diff_pos += chunk;
            }
            
            int ret = BZ2_bzDecompress(&ctx.diff_strm);
            if (ret != BZ_OK && ret != BZ_STREAM_END) {
                result = BSPATCH_ERROR_CORRUPT_PATCH;
                goto cleanup;
            }
        }
        
        // 应用 diff（与旧数据相加）
        for (int64_t i = 0; i < ctrl[0]; i++) {
            if (oldpos + i >= 0 && oldpos + i < old_size) {
                new_data[newpos + i] += old_data[oldpos + i];
            }
        }
        
        newpos += ctrl[0];
        oldpos += ctrl[0];
        
        // 检查边界
        if (newpos + ctrl[1] > new_size) {
            result = BSPATCH_ERROR_CORRUPT_PATCH;
            goto cleanup;
        }
        
        // 读取 extra 数据
        ctx.phase = 2;
        ctx.extra_strm.next_out = (char *)(new_data + newpos);
        ctx.extra_strm.avail_out = (unsigned int)ctrl[1];
        
        while (ctx.extra_strm.avail_out > 0) {
            if (ctx.extra_strm.avail_in == 0 && ctx.extra_pos < ctx.extra_size) {
                int64_t remaining = ctx.extra_size - ctx.extra_pos;
                unsigned int chunk = remaining > 4096 ? 4096 : (unsigned int)remaining;
                ctx.extra_strm.next_in = (char *)(ctx.extra_data + ctx.extra_pos);
                ctx.extra_strm.avail_in = chunk;
                ctx.extra_pos += chunk;
            }
            
            int ret = BZ2_bzDecompress(&ctx.extra_strm);
            if (ret != BZ_OK && ret != BZ_STREAM_END) {
                result = BSPATCH_ERROR_CORRUPT_PATCH;
                goto cleanup;
            }
        }
        
        newpos += ctrl[1];
        oldpos += ctrl[2];
    }
    
cleanup:
    BZ2_bzDecompressEnd(&ctx.ctrl_strm);
    BZ2_bzDecompressEnd(&ctx.diff_strm);
    BZ2_bzDecompressEnd(&ctx.extra_strm);
    
    return result;
}

// 应用 ENDSLEY/BSDIFF43 格式补丁
static int apply_bsdiff43(
    const uint8_t *old_data,
    int64_t old_size,
    const uint8_t *patch_data,
    int64_t patch_size,
    uint8_t *new_data,
    int64_t new_size
) {
    // 初始化 bzip2 解压
    bz2_mem_context ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.data = patch_data + HEADER_SIZE_43;
    ctx.size = patch_size - HEADER_SIZE_43;
    ctx.pos = 0;
    
    if (BZ2_bzDecompressInit(&ctx.strm, 0, 0) != BZ_OK) {
        return BSPATCH_ERROR_MEMORY;
    }
    
    // 设置 bspatch 流
    struct bspatch_stream stream;
    stream.opaque = &ctx;
    stream.read = bz2_mem_read;
    
    // 执行补丁
    int result = bspatch(old_data, old_size, new_data, new_size, &stream);
    
    // 清理
    BZ2_bzDecompressEnd(&ctx.strm);
    
    return result != 0 ? BSPATCH_ERROR_CORRUPT_PATCH : BSPATCH_SUCCESS;
}

FFI_PLUGIN_EXPORT int bspatch_apply_bytes(
    const uint8_t *old_data,
    int64_t old_size,
    const uint8_t *patch_data,
    int64_t patch_size,
    uint8_t *new_data,
    int64_t *new_size
) {
    if (!old_data || !patch_data || !new_data || !new_size) {
        return BSPATCH_ERROR_INVALID_PATCH;
    }
    
    // 获取并验证新文件大小
    int64_t expected_size = bspatch_new_size(patch_data, patch_size);
    if (expected_size < 0) {
        return (int)expected_size;
    }
    
    if (*new_size < expected_size) {
        *new_size = expected_size;
        return BSPATCH_ERROR_SIZE_MISMATCH;
    }
    
    // 根据格式选择处理方法
    patch_format_t format = detect_format(patch_data, patch_size);
    int result;
    
    switch (format) {
        case PATCH_FORMAT_40:
            result = apply_bsdiff40(old_data, old_size, patch_data, patch_size, new_data, expected_size);
            break;
        case PATCH_FORMAT_43:
            result = apply_bsdiff43(old_data, old_size, patch_data, patch_size, new_data, expected_size);
            break;
        default:
            return BSPATCH_ERROR_INVALID_PATCH;
    }
    
    if (result == BSPATCH_SUCCESS) {
        *new_size = expected_size;
    }
    
    return result;
}

FFI_PLUGIN_EXPORT int bspatch_apply(
    const char *old_file,
    const char *patch_file,
    const char *new_file
) {
    int result = BSPATCH_SUCCESS;
    uint8_t *old_data = NULL;
    uint8_t *patch_data = NULL;
    uint8_t *new_data = NULL;
    int64_t old_size, patch_size, new_size;
    
    // 读取原始文件
    old_data = read_file_contents(old_file, &old_size);
    if (!old_data) {
        result = BSPATCH_ERROR_IO;
        goto cleanup;
    }
    
    // 读取补丁文件
    patch_data = read_file_contents(patch_file, &patch_size);
    if (!patch_data) {
        result = BSPATCH_ERROR_IO;
        goto cleanup;
    }
    
    // 获取新文件大小
    new_size = bspatch_new_size(patch_data, patch_size);
    if (new_size < 0) {
        result = (int)new_size;
        goto cleanup;
    }
    
    // 分配新文件缓冲区
    new_data = (uint8_t *)malloc((size_t)new_size);
    if (!new_data) {
        result = BSPATCH_ERROR_MEMORY;
        goto cleanup;
    }
    
    // 应用补丁
    int64_t actual_size = new_size;
    result = bspatch_apply_bytes(old_data, old_size, patch_data, patch_size, new_data, &actual_size);
    if (result != BSPATCH_SUCCESS) {
        goto cleanup;
    }
    
    // 写入新文件
    if (write_file_contents(new_file, new_data, new_size) != 0) {
        result = BSPATCH_ERROR_IO;
        goto cleanup;
    }
    
cleanup:
    if (old_data) free(old_data);
    if (patch_data) free(patch_data);
    if (new_data) free(new_data);
    
    return result;
}

FFI_PLUGIN_EXPORT const char *bspatch_error_string(int error_code) {
    switch (error_code) {
        case BSPATCH_SUCCESS:
            return "Success";
        case BSPATCH_ERROR_INVALID_PATCH:
            return "Invalid patch file";
        case BSPATCH_ERROR_CORRUPT_PATCH:
            return "Corrupt patch file";
        case BSPATCH_ERROR_MEMORY:
            return "Memory allocation failed";
        case BSPATCH_ERROR_IO:
            return "I/O error";
        case BSPATCH_ERROR_SIZE_MISMATCH:
            return "Buffer size mismatch";
        default:
            return "Unknown error";
    }
}
