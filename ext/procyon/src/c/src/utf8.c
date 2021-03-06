// Copyright 2017 The Procyon Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "utf8.h"

#include <string.h>

#define UFFFD_REPLACEMENT_CHARACTER 0xfffd;

static bool is_ascii(uint8_t byte) { return byte < 0200; }
static bool is_continuation(uint8_t byte) { return (0200 <= byte) && (byte < 0300); }
static bool is_header(uint8_t byte) { return (0302 <= byte) && (byte < 0365); }

static size_t continuation_count_for_header(uint8_t byte, uint32_t* rune) {
    if (byte >= 0360) {
        *rune = byte & 0007;
        return 3;
    } else if (byte >= 0340) {
        *rune = byte & 0017;
        return 2;
    } else {
        *rune = byte & 0037;
        return 1;
    }
}

int32_t pn_rune(const char* data, size_t size, size_t index) {
    data += index;
    size -= index;

    if (is_ascii(data[0])) {
        return data[0];
    } else if (!is_header(data[0])) {
        return UFFFD_REPLACEMENT_CHARACTER;
    }
    uint32_t rune;
    size_t   continuation_count = continuation_count_for_header(data[0], &rune);
    if (continuation_count > size) {
        return UFFFD_REPLACEMENT_CHARACTER;
    }
    while (continuation_count-- > 0) {
        if (!is_continuation(*++data)) {
            return UFFFD_REPLACEMENT_CHARACTER;
        }
        rune = (rune << 6) | (*data & 0077);
    }
    if (((0xd800 <= rune) && (rune < 0xe000)) || (0x110000 < rune)) {
        return UFFFD_REPLACEMENT_CHARACTER;
    }
    return rune;
}

size_t pn_rune_next(const char* data, size_t size, size_t index) {
    if (is_header(data[index])) {
        uint8_t h = data[index];
        if (h < 0340) {  // 2 bytes: U+80 ... U+7FF
            if ((index + 1) >= size) {
                return index + 1;  // invalid: unexpected end of string
            }
            uint8_t c[] = {data[index + 1]};
            if (!is_continuation(c[0])) {
                return index + 1;
            }
            return index + 2;
        } else if (h < 0360) {  // 3 bytes: U+800 ... U+D7FF, U+E000 ... U+FFFF
            if ((index + 2) >= size) {
                return index + 1;
            }
            uint8_t c[] = {data[index + 1], data[index + 2]};
            if (((h == 0340) && (c[0] < 0240)) ||   // U+800 ... U+FFF (no overlong encoding)
                ((h == 0355) && (0240 <= c[0])) ||  // U+D000 ... U+D7FF (no surrogate)
                !(is_continuation(c[0]) && is_continuation(c[1]))) {
                return index + 1;
            }
            return index + 3;
        } else {  // 4 bytes: U+40000 ... U+10FFFF
            if ((index + 3) >= size) {
                return index + 1;
            }
            uint8_t c[] = {data[index + 1], data[index + 2], data[index + 3]};
            if (((h == 0360) && (c[0] < 0220)) ||   // U+10000 ... U+3FFFF (no overlong encoding)
                ((h == 0364) && (0220 <= c[0])) ||  // U+100000 ... U+10FFFF (end of code space)
                !(is_continuation(c[0]) && is_continuation(c[1]) && is_continuation(c[2]))) {
                return index + 1;
            }
            return index + 4;
        }
    } else {
        return index + 1;  // default: ASCII or invalid header, so one byte at a time.
    }
}

size_t pn_rune_prev(const char* data, size_t size, size_t index) {
    if ((index >= 4) && (pn_rune_next(data, size, index - 4) == index)) {
        return index - 4;
    } else if ((index >= 3) && (pn_rune_next(data, size, index - 3) == index)) {
        return index - 3;
    } else if ((index >= 2) && (pn_rune_next(data, size, index - 2) == index)) {
        return index - 2;
    } else {
        return index - 1;
    }
}

void pn_chr(uint8_t rune, char* data, size_t* size) {
    if (rune < 0x80) {
        *size = 1;
        *data = rune;
    } else {
        *size = 3;
        memcpy(data, "\357\277\275", 3);
    }
}

void pn_unichr(uint32_t rune, char* data, size_t* size) {
    if (((0xd800 <= rune) && (rune < 0xe000)) || (0x110000 <= rune)) {
        *size = 3;
        memcpy(data, "\357\277\275", 3);
    } else if (rune < 0x80) {
        *size = 1;
        *data = rune;
    } else if (rune < 0x800) {
        *size   = 2;
        data[0] = 0xc0 | (rune >> 6);
        data[1] = 0x80 | ((rune >> 0) & 0x3f);
    } else if (rune < 0x10000) {
        *size   = 3;
        data[0] = 0xe0 | (rune >> 12);
        data[1] = 0x80 | ((rune >> 6) & 0x3f);
        data[2] = 0x80 | ((rune >> 0) & 0x3f);
    } else {
        *size   = 4;
        data[0] = 0xf0 | (rune >> 18);
        data[1] = 0x80 | ((rune >> 12) & 0x3f);
        data[2] = 0x80 | ((rune >> 6) & 0x3f);
        data[3] = 0x80 | ((rune >> 0) & 0x3f);
    }
}

static const int8_t widths_0[][8] = {
        [0] = {-1, -1, -1, -1, -1, -1, -1, -1},  [1] = {-1, -1, -1, -1, -1, -1, -1, 0},
        [2] = {-1, -1, -1, -1, -1, -1, -1, 1},   [3] = {-1, -1, -1, -1, -1, -1, 0, 0},
        [4] = {-1, -1, -1, -1, -1, -1, 1, -1},   [5] = {-1, -1, -1, -1, -1, -1, 1, 1},
        [6] = {-1, -1, -1, -1, -1, 0, 0, -1},    [7] = {-1, -1, -1, -1, -1, 1, -1, -1},
        [8] = {-1, -1, -1, -1, -1, 1, 0, 1},     [9] = {-1, -1, -1, -1, -1, 1, 1, 1},
        [10] = {-1, -1, -1, -1, -1, 2, 2, 2},    [11] = {-1, -1, -1, -1, 0, 0, 0, 0},
        [12] = {-1, -1, -1, -1, 1, 1, -1, -1},   [13] = {-1, -1, -1, -1, 1, 1, -1, 1},
        [14] = {-1, -1, -1, -1, 1, 1, 1, 0},     [15] = {-1, -1, -1, -1, 1, 1, 1, 1},
        [16] = {-1, -1, -1, 0, 0, 0, 0, 0},      [17] = {-1, -1, -1, 1, 1, -1, -1, -1},
        [18] = {-1, -1, -1, 1, 1, 1, 1, 1},      [19] = {-1, -1, 0, -1, -1, -1, -1, 0},
        [20] = {-1, -1, 0, -1, 0, 0, -1, 0},     [21] = {-1, -1, 0, 0, -1, 1, 1, 1},
        [22] = {-1, -1, 0, 0, 0, 0, 0, 0},       [23] = {-1, -1, 0, 0, 1, -1, -1, -1},
        [24] = {-1, -1, 0, 1, -1, 1, 1, 1},      [25] = {-1, -1, 1, -1, -1, -1, -1, 1},
        [26] = {-1, -1, 1, -1, -1, 1, 1, -1},    [27] = {-1, -1, 1, 1, -1, 1, 1, 1},
        [28] = {-1, -1, 1, 1, 1, -1, -1, -1},    [29] = {-1, -1, 1, 1, 1, -1, 1, 1},
        [30] = {-1, -1, 1, 1, 1, 1, 1, 1},       [31] = {-1, 0, -1, -1, -1, -1, -1, -1},
        [32] = {-1, 0, 0, 0, -1, 1, 1, 1},       [33] = {-1, 0, 0, 0, 0, 0, 0, 0},
        [34] = {-1, 0, 0, 2, 2, 2, 2, 2},        [35] = {-1, 1, -1, 1, -1, -1, -1, -1},
        [36] = {-1, 1, -1, 1, -1, 1, -1, 1},     [37] = {-1, 1, -1, 1, -1, 1, 1, 1},
        [38] = {-1, 1, 0, 0, 0, 0, 0, 0},        [39] = {-1, 1, 1, -1, -1, -1, -1, -1},
        [40] = {-1, 1, 1, -1, -1, 1, 1, 1},      [41] = {-1, 1, 1, -1, 1, -1, -1, 1},
        [42] = {-1, 1, 1, -1, 1, -1, 1, 1},      [43] = {-1, 1, 1, 1, -1, 1, -1, 1},
        [44] = {-1, 1, 1, 1, -1, 1, 1, 1},       [45] = {-1, 1, 1, 1, 1, -1, -1, -1},
        [46] = {-1, 1, 1, 1, 1, -1, 1, -1},      [47] = {-1, 1, 1, 1, 1, -1, 1, 1},
        [48] = {-1, 1, 1, 1, 1, 1, 1, -1},       [49] = {-1, 1, 1, 1, 1, 1, 1, 1},
        [50] = {-1, 2, 2, 2, 2, 2, 2, 2},        [51] = {0, -1, -1, -1, -1, -1, -1, -1},
        [52] = {0, -1, -1, 0, 0, 0, -1, -1},     [53] = {0, -1, -1, 0, 0, 0, 0, 0},
        [54] = {0, -1, -1, 0, 0, 0, 1, -1},      [55] = {0, -1, 0, 0, 0, 0, -1, -1},
        [56] = {0, -1, 0, 0, 0, 0, 1, 1},        [57] = {0, 0, -1, -1, -1, -1, -1, -1},
        [58] = {0, 0, -1, 0, 0, -1, 0, 0},       [59] = {0, 0, -1, 0, 0, 0, -1, -1},
        [60] = {0, 0, -1, 0, 0, 0, 0, 0},        [61] = {0, 0, -1, 0, 0, 1, -1, -1},
        [62] = {0, 0, 0, -1, -1, -1, -1, -1},    [63] = {0, 0, 0, -1, -1, -1, -1, 0},
        [64] = {0, 0, 0, -1, -1, -1, -1, 1},     [65] = {0, 0, 0, -1, -1, -1, 0, 0},
        [66] = {0, 0, 0, -1, -1, 1, 1, 1},       [67] = {0, 0, 0, 0, -1, -1, -1, -1},
        [68] = {0, 0, 0, 0, -1, -1, 1, 1},       [69] = {0, 0, 0, 0, -1, 1, 1, 1},
        [70] = {0, 0, 0, 0, 0, -1, -1, -1},      [71] = {0, 0, 0, 0, 0, -1, -1, 0},
        [72] = {0, 0, 0, 0, 0, -1, 0, -1},       [73] = {0, 0, 0, 0, 0, -1, 0, 0},
        [74] = {0, 0, 0, 0, 0, -1, 1, 0},        [75] = {0, 0, 0, 0, 0, -1, 1, 1},
        [76] = {0, 0, 0, 0, 0, 0, -1, -1},       [77] = {0, 0, 0, 0, 0, 0, -1, 0},
        [78] = {0, 0, 0, 0, 0, 0, 0, -1},        [79] = {0, 0, 0, 0, 0, 0, 0, 0},
        [80] = {0, 0, 0, 0, 0, 0, 0, 1},         [81] = {0, 0, 0, 0, 0, 0, 1, 0},
        [82] = {0, 0, 0, 0, 0, 0, 1, 1},         [83] = {0, 0, 0, 0, 0, 1, -1, -1},
        [84] = {0, 0, 0, 0, 0, 1, 0, 0},         [85] = {0, 0, 0, 0, 0, 1, 1, 0},
        [86] = {0, 0, 0, 0, 0, 1, 1, 1},         [87] = {0, 0, 0, 0, 1, 0, 0, 0},
        [88] = {0, 0, 0, 0, 1, 1, 1, 1},         [89] = {0, 0, 0, 1, -1, -1, 1, 1},
        [90] = {0, 0, 0, 1, 0, 0, -1, -1},       [91] = {0, 0, 0, 1, 0, 0, 0, 0},
        [92] = {0, 0, 0, 1, 1, -1, 1, 1},        [93] = {0, 0, 0, 1, 1, 0, 0, 0},
        [94] = {0, 0, 0, 1, 1, 1, 1, 1},         [95] = {0, 0, 1, 0, 0, 0, 0, 0},
        [96] = {0, 0, 1, 0, 0, 0, 0, 1},         [97] = {0, 0, 1, 1, -1, -1, -1, -1},
        [98] = {0, 0, 1, 1, 1, -1, 1, 1},        [99] = {0, 0, 1, 1, 1, 0, -1, -1},
        [100] = {0, 0, 1, 1, 1, 0, 0, 0},        [101] = {0, 0, 1, 1, 1, 1, 0, 0},
        [102] = {0, 0, 1, 1, 1, 1, 1, 1},        [103] = {0, 1, -1, -1, -1, -1, -1, -1},
        [104] = {0, 1, 0, 0, 0, 0, 1, 1},        [105] = {0, 1, 0, 0, 0, 1, 1, 0},
        [106] = {0, 1, 1, 1, 1, -1, -1, -1},     [107] = {0, 1, 1, 1, 1, 0, 1, 1},
        [108] = {0, 1, 1, 1, 1, 1, 0, 0},        [109] = {0, 1, 1, 1, 1, 1, 1, 1},
        [110] = {1, -1, -1, -1, -1, -1, -1, -1}, [111] = {1, -1, -1, -1, -1, -1, -1, 0},
        [112] = {1, -1, -1, -1, 1, -1, -1, 1},   [113] = {1, -1, -1, -1, 1, 1, 1, 1},
        [114] = {1, -1, -1, 1, 1, 1, 1, 1},      [115] = {1, -1, 0, 0, -1, -1, -1, -1},
        [116] = {1, -1, 1, -1, -1, -1, 1, 1},    [117] = {1, -1, 1, -1, -1, 1, -1, -1},
        [118] = {1, -1, 1, 1, -1, 1, 1, -1},     [119] = {1, -1, 1, 1, -1, 1, 1, 1},
        [120] = {1, -1, 1, 1, 1, 1, -1, -1},     [121] = {1, -1, 1, 1, 1, 1, -1, 1},
        [122] = {1, -1, 1, 1, 1, 1, 1, 1},       [123] = {1, 0, 0, 0, -1, -1, 1, -1},
        [124] = {1, 0, 0, 0, -1, 0, 0, -1},      [125] = {1, 0, 0, 0, -1, 1, 1, 1},
        [126] = {1, 0, 0, 0, 0, 0, -1, -1},      [127] = {1, 0, 0, 0, 0, 0, 0, -1},
        [128] = {1, 0, 0, 0, 0, 0, 0, 0},        [129] = {1, 0, 0, 0, 0, 1, 1, 1},
        [130] = {1, 0, 0, 1, 0, 0, 1, 0},        [131] = {1, 0, 1, -1, -1, -1, -1, -1},
        [132] = {1, 0, 1, 1, 0, 0, 0, 0},        [133] = {1, 0, 1, 1, 1, 1, 0, 0},
        [134] = {1, 0, 1, 1, 1, 1, 1, 1},        [135] = {1, 1, -1, -1, -1, -1, -1, -1},
        [136] = {1, 1, -1, -1, -1, -1, -1, 1},   [137] = {1, 1, -1, -1, -1, -1, 0, 0},
        [138] = {1, 1, -1, -1, -1, -1, 1, 1},    [139] = {1, 1, -1, -1, -1, 0, 0, 0},
        [140] = {1, 1, -1, -1, -1, 1, 0, 0},     [141] = {1, 1, -1, -1, -1, 1, 1, 1},
        [142] = {1, 1, -1, -1, 0, -1, 0, 0},     [143] = {1, 1, -1, -1, 0, 1, 0, 0},
        [144] = {1, 1, -1, -1, 1, 0, 0, 1},      [145] = {1, 1, -1, -1, 1, 1, 1, 1},
        [146] = {1, 1, -1, 1, -1, 1, -1, -1},    [147] = {1, 1, -1, 1, -1, 1, 1, 1},
        [148] = {1, 1, -1, 1, 1, -1, 1, 1},      [149] = {1, 1, -1, 1, 1, 1, 1, -1},
        [150] = {1, 1, -1, 1, 1, 1, 1, 1},       [151] = {1, 1, 0, 0, -1, -1, -1, -1},
        [152] = {1, 1, 0, 0, -1, -1, 0, 0},      [153] = {1, 1, 0, 0, -1, -1, 1, 1},
        [154] = {1, 1, 0, 0, 0, -1, -1, -1},     [155] = {1, 1, 0, 0, 0, 0, 0, 0},
        [156] = {1, 1, 0, 0, 0, 0, 1, 1},        [157] = {1, 1, 0, 0, 0, 1, -1, -1},
        [158] = {1, 1, 0, 0, 0, 1, 0, 0},        [159] = {1, 1, 0, 0, 0, 1, 1, -1},
        [160] = {1, 1, 0, 0, 0, 1, 1, 0},        [161] = {1, 1, 0, 0, 1, 1, 1, 1},
        [162] = {1, 1, 0, 1, 1, 1, 0, 1},        [163] = {1, 1, 1, -1, -1, -1, -1, -1},
        [164] = {1, 1, 1, -1, -1, -1, -1, 1},    [165] = {1, 1, 1, -1, -1, -1, 1, 1},
        [166] = {1, 1, 1, -1, -1, 0, 0, 0},      [167] = {1, 1, 1, -1, -1, 1, 1, 1},
        [168] = {1, 1, 1, -1, 1, -1, 1, 1},      [169] = {1, 1, 1, -1, 1, 1, -1, -1},
        [170] = {1, 1, 1, -1, 1, 1, -1, 1},      [171] = {1, 1, 1, -1, 1, 1, 1, 1},
        [172] = {1, 1, 1, 0, 0, 0, -1, -1},      [173] = {1, 1, 1, 0, 0, 0, 0, 0},
        [174] = {1, 1, 1, 0, 0, 0, 1, 1},        [175] = {1, 1, 1, 0, 0, 1, 0, 0},
        [176] = {1, 1, 1, 0, 1, 1, 1, -1},       [177] = {1, 1, 1, 0, 1, 1, 1, 1},
        [178] = {1, 1, 1, 1, -1, -1, -1, -1},    [179] = {1, 1, 1, 1, -1, -1, -1, 1},
        [180] = {1, 1, 1, 1, -1, -1, 1, 1},      [181] = {1, 1, 1, 1, -1, 1, -1, -1},
        [182] = {1, 1, 1, 1, -1, 1, 1, 1},       [183] = {1, 1, 1, 1, 0, 0, -1, -1},
        [184] = {1, 1, 1, 1, 0, 0, 0, 0},        [185] = {1, 1, 1, 1, 0, 1, 1, 1},
        [186] = {1, 1, 1, 1, 1, -1, -1, -1},     [187] = {1, 1, 1, 1, 1, -1, -1, 1},
        [188] = {1, 1, 1, 1, 1, -1, 1, -1},      [189] = {1, 1, 1, 1, 1, -1, 1, 1},
        [190] = {1, 1, 1, 1, 1, 0, -1, -1},      [191] = {1, 1, 1, 1, 1, 0, 0, -1},
        [192] = {1, 1, 1, 1, 1, 0, 0, 0},        [193] = {1, 1, 1, 1, 1, 0, 0, 1},
        [194] = {1, 1, 1, 1, 1, 0, 1, 0},        [195] = {1, 1, 1, 1, 1, 0, 1, 1},
        [196] = {1, 1, 1, 1, 1, 1, -1, -1},      [197] = {1, 1, 1, 1, 1, 1, -1, 1},
        [198] = {1, 1, 1, 1, 1, 1, 0, -1},       [199] = {1, 1, 1, 1, 1, 1, 0, 0},
        [200] = {1, 1, 1, 1, 1, 1, 0, 1},        [201] = {1, 1, 1, 1, 1, 1, 1, -1},
        [202] = {1, 1, 1, 1, 1, 1, 1, 0},        [203] = {1, 1, 1, 1, 1, 1, 1, 1},
        [204] = {1, 1, 1, 1, 1, 1, 1, 2},        [205] = {1, 1, 1, 1, 1, 1, 2, 1},
        [206] = {1, 1, 1, 1, 1, 2, 1, 1},        [207] = {1, 1, 1, 1, 1, 2, 2, 1},
        [208] = {1, 1, 1, 1, 1, 2, 2, 2},        [209] = {1, 1, 1, 1, 2, 1, 1, 1},
        [210] = {1, 1, 1, 1, 2, 1, 2, 1},        [211] = {1, 1, 1, 1, 2, 2, 1, 1},
        [212] = {1, 1, 1, 1, 2, 2, 2, 2},        [213] = {1, 1, 1, 2, 1, 1, 1, 1},
        [214] = {1, 1, 1, 2, 2, -1, -1, -1},     [215] = {1, 1, 1, 2, 2, 1, 1, 1},
        [216] = {1, 1, 1, 2, 2, 2, 1, 2},        [217] = {1, 1, 1, 2, 2, 2, 2, 1},
        [218] = {1, 1, 1, 2, 2, 2, 2, 2},        [219] = {1, 1, 2, 1, 1, 1, 1, 1},
        [220] = {1, 1, 2, 1, 1, 2, 1, 1},        [221] = {1, 1, 2, 2, 1, 1, 1, 1},
        [222] = {1, 1, 2, 2, 1, 2, 1, 1},        [223] = {1, 2, 1, 1, 1, 1, 1, 1},
        [224] = {1, 2, 2, 1, 1, 1, 1, 1},        [225] = {1, 2, 2, 2, 2, 1, 1, 1},
        [226] = {1, 2, 2, 2, 2, 2, 2, 2},        [227] = {2, -1, -1, -1, -1, -1, -1, -1},
        [228] = {2, 1, 1, 1, 1, 1, 1, 1},        [229] = {2, 1, 1, 1, 1, 2, 1, 1},
        [230] = {2, 1, 1, 1, 2, 1, 1, 1},        [231] = {2, 1, 1, 2, 1, 1, 1, 1},
        [232] = {2, 1, 2, 2, 2, 2, 2, 2},        [233] = {2, 2, -1, -1, -1, -1, -1, -1},
        [234] = {2, 2, -1, 2, 2, 2, 2, 2},       [235] = {2, 2, 0, 0, 0, 0, 0, 0},
        [236] = {2, 2, 2, -1, -1, -1, -1, -1},   [237] = {2, 2, 2, -1, 2, 2, 2, 2},
        [238] = {2, 2, 2, 1, 1, -1, -1, -1},     [239] = {2, 2, 2, 1, 1, 1, 1, 1},
        [240] = {2, 2, 2, 1, 1, 1, 1, 2},        [241] = {2, 2, 2, 2, -1, -1, -1, -1},
        [242] = {2, 2, 2, 2, 1, 1, 1, 1},        [243] = {2, 2, 2, 2, 2, -1, -1, -1},
        [244] = {2, 2, 2, 2, 2, 1, 1, 2},        [245] = {2, 2, 2, 2, 2, 1, 2, 2},
        [246] = {2, 2, 2, 2, 2, 2, -1, -1},      [247] = {2, 2, 2, 2, 2, 2, 1, 1},
        [248] = {2, 2, 2, 2, 2, 2, 1, 2},        [249] = {2, 2, 2, 2, 2, 2, 2, -1},
        [250] = {2, 2, 2, 2, 2, 2, 2, 1},        [251] = {2, 2, 2, 2, 2, 2, 2, 2},
};
static const uint8_t widths_1[][8] = {
        [0]   = {0, 0, 0, 0, 0, 0, 0, 0},
        [1]   = {0, 0, 0, 0, 0, 0, 2, 203},
        [2]   = {0, 0, 0, 0, 0, 0, 135, 0},
        [3]   = {0, 0, 0, 0, 0, 0, 251, 251},
        [4]   = {0, 0, 0, 0, 5, 203, 203, 203},
        [5]   = {0, 0, 0, 0, 49, 189, 203, 203},
        [6]   = {0, 0, 0, 0, 203, 203, 169, 18},
        [7]   = {0, 0, 0, 0, 203, 203, 189, 196},
        [8]   = {0, 0, 0, 0, 203, 203, 203, 201},
        [9]   = {0, 0, 0, 0, 203, 203, 203, 203},
        [10]  = {0, 0, 0, 0, 233, 0, 0, 0},
        [11]  = {0, 0, 11, 79, 60, 79, 79, 79},
        [12]  = {0, 0, 79, 79, 79, 79, 51, 0},
        [13]  = {0, 0, 203, 203, 203, 0, 203, 203},
        [14]  = {0, 0, 203, 203, 203, 196, 83, 0},
        [15]  = {0, 0, 203, 203, 203, 203, 203, 190},
        [16]  = {0, 0, 203, 203, 203, 203, 203, 203},
        [17]  = {0, 1, 94, 203, 0, 0, 0, 0},
        [18]  = {5, 203, 79, 89, 203, 203, 203, 203},
        [19]  = {10, 251, 251, 251, 251, 249, 50, 251},
        [20]  = {15, 168, 203, 203, 150, 203, 203, 203},
        [21]  = {21, 203, 201, 30, 203, 203, 150, 181},
        [22]  = {24, 165, 120, 42, 17, 165, 203, 137},
        [23]  = {25, 37, 41, 36, 41, 171, 171, 46},
        [24]  = {30, 30, 30, 28, 249, 201, 0, 12},
        [25]  = {32, 164, 114, 203, 203, 122, 118, 142},
        [26]  = {32, 187, 114, 203, 203, 122, 119, 143},
        [27]  = {32, 197, 150, 203, 203, 122, 119, 143},
        [28]  = {41, 117, 15, 49, 43, 27, 132, 61},
        [29]  = {48, 48, 48, 0, 201, 201, 203, 203},
        [30]  = {49, 203, 203, 186, 203, 203, 203, 203},
        [31]  = {49, 203, 203, 203, 203, 203, 132, 64},
        [32]  = {49, 204, 49, 203, 203, 203, 196, 0},
        [33]  = {50, 251, 251, 251, 251, 251, 251, 251},
        [34]  = {63, 52, 31, 46, 5, 203, 99, 0},
        [35]  = {65, 55, 111, 0, 5, 203, 203, 163},
        [36]  = {69, 187, 114, 203, 203, 122, 119, 143},
        [37]  = {69, 189, 122, 203, 203, 122, 203, 140},
        [38]  = {69, 189, 122, 203, 203, 203, 203, 175},
        [39]  = {71, 52, 3, 13, 153, 203, 203, 0},
        [40]  = {71, 52, 111, 9, 152, 70, 70, 0},
        [41]  = {71, 54, 1, 13, 153, 203, 203, 196},
        [42]  = {73, 55, 6, 4, 153, 203, 39, 0},
        [43]  = {73, 55, 6, 163, 153, 203, 0, 203},
        [44]  = {73, 56, 14, 203, 153, 203, 203, 203},
        [45]  = {76, 5, 203, 135, 79, 79, 102, 196},
        [46]  = {77, 59, 110, 0, 153, 203, 135, 38},
        [47]  = {78, 79, 79, 53, 58, 62, 0, 0},
        [48]  = {79, 66, 203, 203, 203, 203, 203, 203},
        [49]  = {79, 79, 79, 79, 79, 79, 0, 0},
        [50]  = {79, 79, 79, 79, 79, 79, 79, 60},
        [51]  = {79, 79, 79, 79, 79, 79, 79, 79},
        [52]  = {79, 79, 79, 79, 79, 79, 80, 173},
        [53]  = {79, 79, 79, 79, 79, 79, 203, 30},
        [54]  = {79, 79, 79, 79, 79, 86, 195, 203},
        [55]  = {79, 79, 88, 190, 203, 135, 203, 135},
        [56]  = {79, 79, 128, 203, 161, 203, 203, 203},
        [57]  = {79, 79, 251, 233, 79, 79, 251, 251},
        [58]  = {80, 196, 30, 203, 203, 203, 0, 1},
        [59]  = {80, 203, 203, 146, 0, 0, 0, 0},
        [60]  = {81, 0, 203, 135, 0, 0, 0, 0},
        [61]  = {84, 192, 79, 33, 79, 79, 79, 75},
        [62]  = {86, 178, 203, 203, 203, 173, 88, 186},
        [63]  = {86, 203, 203, 203, 203, 203, 184, 79},
        [64]  = {88, 0, 203, 135, 0, 0, 0, 0},
        [65]  = {88, 203, 203, 203, 203, 203, 173, 79},
        [66]  = {88, 203, 203, 203, 203, 203, 203, 158},
        [67]  = {93, 88, 203, 203, 203, 156, 203, 203},
        [68]  = {94, 203, 203, 203, 128, 82, 203, 203},
        [69]  = {94, 203, 203, 203, 202, 79, 75, 203},
        [70]  = {94, 203, 203, 203, 203, 203, 79, 92},
        [71]  = {94, 203, 203, 203, 203, 203, 173, 79},
        [72]  = {94, 203, 203, 203, 203, 203, 203, 79},
        [73]  = {102, 203, 203, 203, 203, 203, 184, 79},
        [74]  = {106, 0, 203, 135, 203, 186, 0, 0},
        [75]  = {109, 157, 203, 203, 49, 203, 186, 0},
        [76]  = {109, 197, 203, 138, 195, 203, 203, 201},
        [77]  = {109, 203, 203, 183, 0, 0, 0, 0},
        [78]  = {113, 203, 203, 203, 203, 196, 186, 0},
        [79]  = {120, 203, 201, 203, 203, 203, 203, 203},
        [80]  = {124, 11, 182, 49, 203, 203, 178, 63},
        [81]  = {125, 187, 114, 203, 203, 122, 116, 143},
        [82]  = {125, 189, 122, 203, 203, 122, 182, 143},
        [83]  = {128, 94, 203, 203, 203, 203, 173, 96},
        [84]  = {130, 0, 203, 203, 203, 163, 186, 0},
        [85]  = {131, 0, 0, 18, 203, 173, 191, 0},
        [86]  = {135, 0, 18, 203, 203, 203, 203, 203},
        [87]  = {135, 0, 203, 203, 203, 110, 203, 135},
        [88]  = {148, 203, 203, 203, 203, 203, 203, 203},
        [89]  = {155, 81, 203, 156, 203, 203, 203, 203},
        [90]  = {157, 0, 0, 0, 0, 0, 0, 0},
        [91]  = {162, 177, 203, 203, 173, 178, 203, 135},
        [92]  = {164, 203, 203, 203, 203, 203, 179, 203},
        [93]  = {173, 102, 203, 203, 203, 203, 203, 203},
        [94]  = {177, 183, 203, 145, 203, 203, 203, 174},
        [95]  = {178, 0, 0, 0, 0, 0, 0, 0},
        [96]  = {178, 0, 203, 203, 203, 203, 176, 0},
        [97]  = {178, 203, 196, 0, 0, 0, 0, 0},
        [98]  = {180, 155, 79, 98, 163, 0, 0, 0},
        [99]  = {182, 203, 203, 203, 41, 49, 171, 35},
        [100] = {182, 203, 203, 203, 203, 203, 203, 203},
        [101] = {184, 62, 203, 138, 0, 0, 0, 0},
        [102] = {185, 178, 0, 16, 33, 79, 0, 0},
        [103] = {186, 0, 128, 79, 79, 79, 79, 78},
        [104] = {187, 203, 78, 0, 0, 0, 0, 0},
        [105] = {188, 30, 122, 203, 203, 203, 203, 203},
        [106] = {188, 76, 203, 145, 0, 0, 0, 0},
        [107] = {189, 203, 180, 182, 203, 203, 29, 201},
        [108] = {193, 203, 203, 203, 203, 131, 203, 203},
        [109] = {196, 0, 203, 150, 150, 203, 203, 9},
        [110] = {196, 0, 203, 203, 203, 186, 203, 203},
        [111] = {196, 122, 203, 203, 203, 203, 197, 112},
        [112] = {196, 196, 203, 36, 203, 203, 203, 196},
        [113] = {197, 7, 203, 203, 203, 203, 203, 203},
        [114] = {197, 167, 189, 189, 203, 203, 203, 149},
        [115] = {200, 189, 203, 163, 0, 0, 0, 0},
        [116] = {201, 0, 0, 0, 0, 0, 0, 0},
        [117] = {201, 0, 18, 8, 203, 203, 201, 188},
        [118] = {201, 18, 203, 203, 203, 203, 203, 178},
        [119] = {201, 19, 72, 79, 5, 203, 23, 0},
        [120] = {201, 121, 203, 197, 203, 135, 203, 203},
        [121] = {201, 150, 203, 203, 203, 203, 127, 20},
        [122] = {201, 201, 201, 201, 79, 79, 79, 79},
        [123] = {202, 0, 128, 88, 203, 203, 203, 203},
        [124] = {202, 79, 67, 2, 251, 251, 251, 243},
        [125] = {202, 80, 203, 178, 0, 0, 0, 0},
        [126] = {203, 0, 0, 0, 0, 0, 203, 196},
        [127] = {203, 0, 91, 79, 79, 107, 160, 57},
        [128] = {203, 0, 203, 110, 203, 203, 203, 203},
        [129] = {203, 0, 203, 135, 203, 203, 203, 203},
        [130] = {203, 0, 203, 203, 203, 196, 0, 0},
        [131] = {203, 40, 33, 79, 79, 79, 79, 81},
        [132] = {203, 49, 203, 203, 203, 186, 33, 79},
        [133] = {203, 110, 0, 0, 0, 0, 0, 0},
        [134] = {203, 110, 203, 144, 0, 0, 0, 0},
        [135] = {203, 120, 201, 120, 203, 203, 203, 203},
        [136] = {203, 120, 203, 203, 203, 203, 120, 201},
        [137] = {203, 122, 163, 0, 0, 15, 0, 0},
        [138] = {203, 122, 203, 203, 203, 202, 78, 79},
        [139] = {203, 135, 0, 0, 0, 0, 0, 0},
        [140] = {203, 135, 203, 135, 203, 196, 79, 78},
        [141] = {203, 135, 203, 165, 203, 203, 203, 203},
        [142] = {203, 141, 203, 203, 203, 203, 203, 203},
        [143] = {203, 150, 203, 178, 44, 150, 203, 178},
        [144] = {203, 163, 0, 0, 203, 203, 203, 203},
        [145] = {203, 163, 203, 203, 203, 203, 199, 62},
        [146] = {203, 172, 203, 135, 203, 203, 203, 203},
        [147] = {203, 173, 79, 79, 203, 203, 109, 203},
        [148] = {203, 178, 203, 203, 203, 203, 203, 203},
        [149] = {203, 178, 251, 251, 251, 251, 251, 249},
        [150] = {203, 180, 203, 203, 203, 203, 203, 203},
        [151] = {203, 182, 203, 203, 201, 203, 203, 170},
        [152] = {203, 186, 203, 203, 203, 201, 203, 203},
        [153] = {203, 189, 154, 0, 203, 203, 159, 0},
        [154] = {203, 196, 134, 203, 203, 203, 79, 79},
        [155] = {203, 196, 203, 196, 0, 0, 0, 0},
        [156] = {203, 201, 203, 178, 110, 0, 0, 0},
        [157] = {203, 201, 203, 186, 203, 203, 203, 203},
        [158] = {203, 203, 0, 0, 0, 0, 0, 0},
        [159] = {203, 203, 22, 79, 79, 33, 78, 0},
        [160] = {203, 203, 30, 203, 203, 203, 203, 203},
        [161] = {203, 203, 110, 0, 109, 203, 203, 178},
        [162] = {203, 203, 120, 203, 203, 203, 203, 203},
        [163] = {203, 203, 135, 45, 0, 49, 0, 0},
        [164] = {203, 203, 150, 203, 203, 184, 79, 198},
        [165] = {203, 203, 151, 0, 203, 189, 115, 0},
        [166] = {203, 203, 178, 0, 203, 201, 49, 203},
        [167] = {203, 203, 178, 203, 203, 203, 203, 178},
        [168] = {203, 203, 186, 0, 0, 0, 0, 0},
        [169] = {203, 203, 189, 203, 203, 203, 203, 203},
        [170] = {203, 203, 192, 78, 79, 79, 79, 71},
        [171] = {203, 203, 196, 0, 203, 0, 0, 0},
        [172] = {203, 203, 196, 196, 203, 203, 203, 203},
        [173] = {203, 203, 196, 203, 203, 203, 163, 203},
        [174] = {203, 203, 196, 203, 203, 203, 203, 141},
        [175] = {203, 203, 197, 203, 203, 203, 203, 203},
        [176] = {203, 203, 199, 74, 85, 104, 203, 203},
        [177] = {203, 203, 199, 95, 87, 126, 203, 201},
        [178] = {203, 203, 199, 101, 105, 82, 129, 203},
        [179] = {203, 203, 201, 0, 201, 201, 201, 201},
        [180] = {203, 203, 201, 0, 203, 203, 135, 0},
        [181] = {203, 203, 201, 49, 49, 203, 203, 203},
        [182] = {203, 203, 202, 68, 203, 203, 203, 203},
        [183] = {203, 203, 203, 102, 203, 203, 194, 133},
        [184] = {203, 203, 203, 123, 203, 163, 0, 0},
        [185] = {203, 203, 203, 135, 0, 0, 0, 0},
        [186] = {203, 203, 203, 135, 203, 203, 203, 203},
        [187] = {203, 203, 203, 139, 79, 67, 203, 203},
        [188] = {203, 203, 203, 166, 203, 203, 203, 186},
        [189] = {203, 203, 203, 179, 203, 203, 203, 136},
        [190] = {203, 203, 203, 186, 203, 203, 203, 203},
        [191] = {203, 203, 203, 189, 26, 47, 203, 147},
        [192] = {203, 203, 203, 196, 203, 135, 203, 203},
        [193] = {203, 203, 203, 197, 203, 203, 203, 203},
        [194] = {203, 203, 203, 199, 203, 203, 203, 203},
        [195] = {203, 203, 203, 201, 0, 0, 145, 203},
        [196] = {203, 203, 203, 201, 2, 203, 0, 0},
        [197] = {203, 203, 203, 201, 79, 67, 79, 67},
        [198] = {203, 203, 203, 201, 203, 138, 0, 0},
        [199] = {203, 203, 203, 201, 203, 203, 203, 203},
        [200] = {203, 203, 203, 202, 79, 62, 203, 135},
        [201] = {203, 203, 203, 203, 0, 0, 0, 0},
        [202] = {203, 203, 203, 203, 173, 90, 203, 135},
        [203] = {203, 203, 203, 203, 178, 2, 0, 0},
        [204] = {203, 203, 203, 203, 178, 9, 203, 203},
        [205] = {203, 203, 203, 203, 184, 79, 79, 18},
        [206] = {203, 203, 203, 203, 191, 18, 201, 0},
        [207] = {203, 203, 203, 203, 192, 100, 62, 16},
        [208] = {203, 203, 203, 203, 196, 0, 203, 203},
        [209] = {203, 203, 203, 203, 196, 203, 203, 203},
        [210] = {203, 203, 203, 203, 197, 7, 203, 203},
        [211] = {203, 203, 203, 203, 199, 79, 67, 15},
        [212] = {203, 203, 203, 203, 199, 79, 103, 0},
        [213] = {203, 203, 203, 203, 199, 82, 203, 203},
        [214] = {203, 203, 203, 203, 201, 0, 0, 0},
        [215] = {203, 203, 203, 203, 201, 49, 203, 203},
        [216] = {203, 203, 203, 203, 203, 0, 203, 203},
        [217] = {203, 203, 203, 203, 203, 2, 110, 1},
        [218] = {203, 203, 203, 203, 203, 110, 0, 0},
        [219] = {203, 203, 203, 203, 203, 128, 78, 0},
        [220] = {203, 203, 203, 203, 203, 163, 203, 186},
        [221] = {203, 203, 203, 203, 203, 173, 79, 0},
        [222] = {203, 203, 203, 203, 203, 173, 79, 80},
        [223] = {203, 203, 203, 203, 203, 173, 88, 163},
        [224] = {203, 203, 203, 203, 203, 178, 0, 0},
        [225] = {203, 203, 203, 203, 203, 178, 203, 203},
        [226] = {203, 203, 203, 203, 203, 201, 0, 0},
        [227] = {203, 203, 203, 203, 203, 201, 186, 0},
        [228] = {203, 203, 203, 203, 203, 201, 203, 0},
        [229] = {203, 203, 203, 203, 203, 201, 203, 203},
        [230] = {203, 203, 203, 203, 203, 202, 76, 79},
        [231] = {203, 203, 203, 203, 203, 202, 91, 82},
        [232] = {203, 203, 203, 203, 203, 202, 97, 49},
        [233] = {203, 203, 203, 203, 203, 203, 49, 203},
        [234] = {203, 203, 203, 203, 203, 203, 79, 79},
        [235] = {203, 203, 203, 203, 203, 203, 80, 203},
        [236] = {203, 203, 203, 203, 203, 203, 102, 0},
        [237] = {203, 203, 203, 203, 203, 203, 105, 108},
        [238] = {203, 203, 203, 203, 203, 203, 163, 0},
        [239] = {203, 203, 203, 203, 203, 203, 163, 2},
        [240] = {203, 203, 203, 203, 203, 203, 163, 30},
        [241] = {203, 203, 203, 203, 203, 203, 178, 0},
        [242] = {203, 203, 203, 203, 203, 203, 184, 79},
        [243] = {203, 203, 203, 203, 203, 203, 189, 203},
        [244] = {203, 203, 203, 203, 203, 203, 192, 79},
        [245] = {203, 203, 203, 203, 203, 203, 196, 0},
        [246] = {203, 203, 203, 203, 203, 203, 196, 49},
        [247] = {203, 203, 203, 203, 203, 203, 196, 196},
        [248] = {203, 203, 203, 203, 203, 203, 201, 0},
        [249] = {203, 203, 203, 203, 203, 203, 203, 0},
        [250] = {203, 203, 203, 203, 203, 203, 203, 15},
        [251] = {203, 203, 203, 203, 203, 203, 203, 110},
        [252] = {203, 203, 203, 203, 203, 203, 203, 163},
        [253] = {203, 203, 203, 203, 203, 203, 203, 186},
        [254] = {203, 203, 203, 203, 203, 203, 203, 201},
        [255] = {203, 203, 203, 203, 203, 203, 203, 203},
        [256] = {203, 203, 203, 203, 203, 203, 203, 207},
        [257] = {203, 203, 203, 203, 203, 203, 203, 218},
        [258] = {203, 203, 203, 203, 203, 225, 231, 203},
        [259] = {203, 203, 203, 215, 203, 203, 203, 203},
        [260] = {203, 203, 203, 221, 203, 224, 203, 203},
        [261] = {203, 203, 207, 203, 209, 203, 203, 203},
        [262] = {203, 203, 208, 203, 203, 203, 228, 204},
        [263] = {203, 203, 211, 203, 203, 203, 203, 203},
        [264] = {203, 203, 213, 203, 223, 221, 203, 207},
        [265] = {203, 203, 229, 203, 203, 203, 180, 203},
        [266] = {203, 205, 226, 239, 203, 186, 0, 0},
        [267] = {203, 210, 216, 203, 203, 203, 203, 203},
        [268] = {203, 217, 251, 251, 251, 203, 203, 219},
        [269] = {203, 251, 242, 203, 203, 203, 203, 204},
        [270] = {206, 221, 203, 203, 203, 228, 203, 203},
        [271] = {209, 203, 203, 203, 203, 178, 203, 203},
        [272] = {211, 205, 209, 203, 203, 219, 222, 220},
        [273] = {227, 0, 251, 251, 249, 0, 0, 0},
        [274] = {232, 251, 251, 251, 251, 251, 251, 251},
        [275] = {236, 0, 251, 251, 251, 251, 251, 241},
        [276] = {247, 209, 238, 0, 203, 214, 212, 227},
        [277] = {249, 0, 203, 203, 203, 203, 203, 203},
        [278] = {251, 203, 251, 251, 251, 251, 251, 251},
        [279] = {251, 227, 233, 0, 246, 0, 0, 0},
        [280] = {251, 240, 242, 203, 251, 251, 230, 251},
        [281] = {251, 243, 251, 251, 251, 241, 0, 0},
        [282] = {251, 243, 251, 251, 251, 251, 251, 251},
        [283] = {251, 249, 251, 251, 251, 251, 251, 236},
        [284] = {251, 251, 203, 203, 203, 203, 203, 203},
        [285] = {251, 251, 237, 251, 249, 241, 189, 203},
        [286] = {251, 251, 242, 203, 251, 251, 251, 251},
        [287] = {251, 251, 246, 0, 0, 0, 251, 241},
        [288] = {251, 251, 249, 34, 251, 251, 251, 251},
        [289] = {251, 251, 251, 0, 0, 0, 0, 0},
        [290] = {251, 251, 251, 234, 251, 251, 251, 251},
        [291] = {251, 251, 251, 249, 0, 0, 0, 0},
        [292] = {251, 251, 251, 249, 251, 251, 251, 251},
        [293] = {251, 251, 251, 251, 203, 203, 203, 203},
        [294] = {251, 251, 251, 251, 228, 203, 203, 203},
        [295] = {251, 251, 251, 251, 228, 208, 248, 251},
        [296] = {251, 251, 251, 251, 241, 0, 203, 203},
        [297] = {251, 251, 251, 251, 241, 0, 251, 251},
        [298] = {251, 251, 251, 251, 251, 235, 251, 250},
        [299] = {251, 251, 251, 251, 251, 243, 0, 0},
        [300] = {251, 251, 251, 251, 251, 251, 236, 0},
        [301] = {251, 251, 251, 251, 251, 251, 241, 0},
        [302] = {251, 251, 251, 251, 251, 251, 251, 241},
        [303] = {251, 251, 251, 251, 251, 251, 251, 244},
        [304] = {251, 251, 251, 251, 251, 251, 251, 245},
        [305] = {251, 251, 251, 251, 251, 251, 251, 246},
        [306] = {251, 251, 251, 251, 251, 251, 251, 247},
        [307] = {251, 251, 251, 251, 251, 251, 251, 249},
        [308] = {251, 251, 251, 251, 251, 251, 251, 250},
        [309] = {251, 251, 251, 251, 251, 251, 251, 251},
};
static const uint16_t widths_2[][8] = {
        [0]  = {0, 0, 0, 0, 0, 0, 0, 0},
        [1]  = {0, 0, 0, 0, 51, 51, 51, 49},
        [2]  = {0, 0, 0, 0, 255, 103, 17, 10},
        [3]  = {0, 0, 0, 0, 309, 309, 309, 309},
        [4]  = {0, 0, 9, 239, 0, 0, 0, 0},
        [5]  = {0, 0, 190, 161, 204, 145, 193, 97},
        [6]  = {0, 8, 0, 0, 0, 0, 0, 0},
        [7]  = {9, 254, 5, 255, 255, 255, 255, 255},
        [8]  = {13, 195, 157, 12, 255, 255, 148, 255},
        [9]  = {18, 147, 255, 176, 154, 48, 212, 223},
        [10] = {25, 34, 27, 46, 26, 39, 22, 35},
        [11] = {31, 125, 28, 106, 183, 132, 61, 115},
        [12] = {37, 43, 82, 42, 38, 44, 21, 119},
        [13] = {47, 0, 0, 0, 0, 0, 0, 0},
        [14] = {52, 54, 102, 0, 0, 0, 0, 0},
        [15] = {57, 285, 255, 253, 33, 294, 254, 24},
        [16] = {72, 58, 70, 87, 69, 96, 71, 75},
        [17] = {80, 128, 201, 206, 246, 173, 163, 0},
        [18] = {83, 123, 98, 251, 0, 0, 0, 0},
        [19] = {91, 249, 73, 45, 213, 124, 65, 76},
        [20] = {99, 23, 143, 2, 0, 0, 0, 0},
        [21] = {111, 175, 196, 6, 189, 0, 250, 160},
        [22] = {138, 110, 159, 0, 121, 60, 0, 0},
        [23] = {146, 249, 108, 245, 197, 78, 225, 141},
        [24] = {148, 129, 130, 0, 149, 281, 289, 273},
        [25] = {151, 155, 255, 252, 92, 255, 156, 15},
        [26] = {164, 0, 120, 200, 36, 40, 0, 0},
        [27] = {177, 184, 7, 11, 66, 56, 81, 41},
        [28] = {182, 170, 140, 0, 63, 62, 68, 211},
        [29] = {205, 142, 133, 127, 255, 255, 255, 50},
        [30] = {214, 144, 255, 255, 255, 255, 255, 256},
        [31] = {219, 94, 237, 85, 29, 208, 255, 202},
        [32] = {222, 178, 89, 113, 309, 293, 255, 255},
        [33] = {224, 231, 194, 236, 255, 255, 228, 1},
        [34] = {226, 0, 0, 0, 0, 0, 0, 0},
        [35] = {229, 199, 255, 232, 210, 217, 179, 122},
        [36] = {234, 74, 221, 139, 187, 0, 0, 0},
        [37] = {244, 59, 234, 64, 0, 0, 230, 77},
        [38] = {251, 198, 0, 14, 235, 109, 158, 0},
        [39] = {255, 90, 0, 0, 255, 180, 0, 0},
        [40] = {255, 116, 0, 0, 0, 0, 0, 0},
        [41] = {255, 133, 238, 240, 0, 0, 0, 0},
        [42] = {255, 135, 136, 79, 162, 188, 186, 247},
        [43] = {255, 139, 290, 301, 309, 309, 309, 287},
        [44] = {255, 169, 191, 100, 114, 105, 255, 255},
        [45] = {255, 220, 134, 0, 0, 0, 0, 0},
        [46] = {255, 227, 255, 255, 255, 95, 0, 0},
        [47] = {255, 255, 30, 251, 153, 165, 242, 55},
        [48] = {255, 255, 93, 255, 233, 181, 131, 84},
        [49] = {255, 255, 192, 167, 216, 203, 0, 0},
        [50] = {255, 255, 209, 255, 255, 255, 255, 150},
        [51] = {255, 255, 255, 104, 255, 101, 0, 0},
        [52] = {255, 255, 255, 245, 215, 207, 67, 218},
        [53] = {255, 255, 255, 255, 51, 53, 20, 255},
        [54] = {255, 255, 255, 255, 172, 112, 243, 107},
        [55] = {255, 255, 255, 255, 248, 171, 0, 0},
        [56] = {255, 255, 255, 255, 255, 16, 160, 126},
        [57] = {255, 255, 255, 255, 255, 255, 185, 0},
        [58] = {255, 255, 255, 255, 255, 255, 255, 255},
        [59] = {255, 255, 255, 255, 259, 265, 174, 137},
        [60] = {255, 255, 255, 255, 260, 255, 255, 258},
        [61] = {263, 269, 264, 272, 270, 267, 262, 255},
        [62] = {271, 255, 166, 32, 152, 225, 266, 4},
        [63] = {275, 279, 0, 0, 295, 304, 286, 280},
        [64] = {292, 278, 309, 307, 309, 309, 309, 309},
        [65] = {298, 33, 288, 309, 19, 309, 283, 297},
        [66] = {308, 274, 309, 303, 306, 268, 261, 257},
        [67] = {309, 284, 309, 276, 255, 241, 255, 168},
        [68] = {309, 309, 282, 277, 255, 255, 255, 255},
        [69] = {309, 309, 309, 300, 0, 0, 0, 0},
        [70] = {309, 309, 309, 302, 0, 0, 0, 0},
        [71] = {309, 309, 309, 309, 117, 88, 255, 86},
        [72] = {309, 309, 309, 309, 291, 3, 309, 309},
        [73] = {309, 309, 309, 309, 309, 309, 296, 118},
        [74] = {309, 309, 309, 309, 309, 309, 309, 255},
        [75] = {309, 309, 309, 309, 309, 309, 309, 299},
        [76] = {309, 309, 309, 309, 309, 309, 309, 305},
        [77] = {309, 309, 309, 309, 309, 309, 309, 309},
};
static const uint8_t widths_3[][8] = {
        [0] = {0, 0, 0, 0, 0, 0, 0, 0},          [1] = {0, 0, 0, 0, 3, 71, 56, 15},
        [2] = {0, 0, 0, 0, 58, 38, 0, 2},        [3] = {0, 0, 58, 40, 0, 0, 0, 0},
        [4] = {1, 0, 0, 0, 0, 0, 0, 0},          [5] = {7, 53, 48, 9, 27, 10, 12, 11},
        [6] = {8, 60, 30, 61, 58, 59, 35, 43},   [7] = {13, 0, 0, 0, 51, 0, 0, 20},
        [8] = {16, 26, 37, 36, 4, 18, 22, 0},    [9] = {25, 5, 49, 55, 21, 17, 41, 6},
        [10] = {32, 42, 58, 47, 23, 28, 29, 54}, [11] = {52, 39, 44, 50, 58, 14, 0, 0},
        [12] = {58, 57, 46, 0, 0, 0, 0, 0},      [13] = {58, 58, 34, 0, 0, 0, 0, 0},
        [14] = {62, 63, 66, 67, 24, 0, 0, 0},    [15] = {65, 64, 77, 77, 77, 77, 77, 77},
        [16] = {72, 70, 0, 0, 0, 0, 45, 0},      [17] = {77, 77, 68, 33, 19, 31, 77, 77},
        [18] = {77, 77, 77, 73, 0, 0, 0, 0},     [19] = {77, 77, 77, 75, 77, 69, 0, 0},
        [20] = {77, 77, 77, 77, 77, 77, 74, 77}, [21] = {77, 77, 77, 77, 77, 77, 77, 76},
        [22] = {77, 77, 77, 77, 77, 77, 77, 77},
};
static const uint8_t widths_4[][8] = {
        [0] = {0, 0, 0, 0, 0, 0, 0, 0},         [1] = {4, 0, 0, 0, 0, 0, 0, 0},
        [2] = {5, 10, 6, 15, 20, 22, 22, 22},   [3] = {9, 8, 12, 13, 3, 0, 2, 22},
        [4] = {19, 0, 0, 16, 0, 11, 7, 14},     [5] = {22, 22, 17, 22, 22, 18, 0, 1},
        [6] = {22, 22, 22, 22, 22, 22, 22, 21}, [7] = {22, 22, 22, 22, 22, 22, 22, 22},
};
static const uint8_t widths_5[][8] = {
        [0] = {0, 0, 0, 0, 0, 0, 0, 0},
        [1] = {0, 0, 0, 0, 1, 0, 0, 0},
        [2] = {2, 5, 3, 4, 7, 6, 7, 6},
};
static const int8_t widths_6[] = {2, 0, 0, 1, 0};

static int8_t get_width(uint32_t rune) {
    uint8_t b6 = (rune & 07000000) >> 18;
    uint8_t b5 = (rune & 00700000) >> 15;
    uint8_t b4 = (rune & 00070000) >> 12;
    uint8_t b3 = (rune & 00007000) >> 9;
    uint8_t b2 = (rune & 00000700) >> 6;
    uint8_t b1 = (rune & 00000070) >> 3;
    uint8_t b0 = (rune & 00000007) >> 0;
    int     x  = widths_6[b6];
    x          = widths_5[x][b5];
    x          = widths_4[x][b4];
    x          = widths_3[x][b3];
    x          = widths_2[x][b2];
    x          = widths_1[x][b1];
    x          = widths_0[x][b0];
    return x;
}

size_t pn_rune_width(uint32_t rune) {
    if (rune >= 0x110000) {
        return 1;
    }
    int8_t w = get_width(rune);
    if (w == -1) {
        return 1;
    }
    return w;
}

size_t pn_str_width(const char* data, size_t size) {
    size_t total = 0;
    for (size_t i = 0; i < size; i = pn_rune_next(data, size, i)) {
        total += pn_rune_width(pn_rune(data, size, i));
    }
    return total;
}

bool pn_isprint(uint32_t rune) {
    if (rune >= 0x110000) {
        return false;
    }
    return get_width(rune) != -1;
}
