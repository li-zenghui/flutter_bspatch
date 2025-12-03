// Relative import to be able to reuse the C sources.
// See the comment in ../flutter_bspatch.podspec for more information.

// bzip2 源文件
#include "../../src/third_party/bzip2/blocksort.c"
#include "../../src/third_party/bzip2/huffman.c"
#include "../../src/third_party/bzip2/crctable.c"
#include "../../src/third_party/bzip2/randtable.c"
#include "../../src/third_party/bzip2/compress.c"
#include "../../src/third_party/bzip2/decompress.c"
#include "../../src/third_party/bzip2/bzlib.c"

// bspatch 源文件
#include "../../src/third_party/bsdiff/bspatch.c"

// 主插件封装
#include "../../src/flutter_bspatch.c"
