# The Flutter tooling requires that developers have CMake 3.10 or later
# installed. You should not increase this version, as doing so will cause
# the plugin to fail to compile for some customers of the plugin.
cmake_minimum_required(VERSION 3.10)

project(flutter_bspatch_library VERSION 0.0.1 LANGUAGES C)

# bzip2 源文件
set(BZIP2_SOURCES
  third_party/bzip2/blocksort.c
  third_party/bzip2/huffman.c
  third_party/bzip2/crctable.c
  third_party/bzip2/randtable.c
  third_party/bzip2/compress.c
  third_party/bzip2/decompress.c
  third_party/bzip2/bzlib.c
)

# bspatch 源文件
set(BSPATCH_SOURCES
  third_party/bsdiff/bspatch.c
)

# 主插件库
add_library(flutter_bspatch SHARED
  flutter_bspatch.c
  ${BZIP2_SOURCES}
  ${BSPATCH_SOURCES}
)

# 头文件搜索路径
target_include_directories(flutter_bspatch PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/bzip2
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/bsdiff
)

set_target_properties(flutter_bspatch PROPERTIES
  PUBLIC_HEADER flutter_bspatch.h
  OUTPUT_NAME "flutter_bspatch"
)

target_compile_definitions(flutter_bspatch PUBLIC DART_SHARED_LIB)

# bzip2 特定的编译定义
target_compile_definitions(flutter_bspatch PRIVATE BZ_NO_STDIO)

# 禁用一些警告（bzip2 和 bspatch 的旧代码可能有警告）
if(MSVC)
  target_compile_options(flutter_bspatch PRIVATE /W3 /wd4996)
else()
  target_compile_options(flutter_bspatch PRIVATE -w)
endif()

if(ANDROID)
  # Support Android 15 16k page size
  target_link_options(flutter_bspatch PRIVATE "-Wl,-z,max-page-size=16384")
endif()
