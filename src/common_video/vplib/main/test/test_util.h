/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef COMMON_VIDEO_VPLIB_TEST_UTIL_H
#define COMMON_VIDEO_VPLIB_TEST_UTIL_H

#include <string.h>
#include <fstream>
#include <cstdlib>


class CmdArgs
{
public:
    CmdArgs() : width(-1), height(-1), dstWidth(-1), dstHeight(-1),
                intMethod(-1), inputFile(""), outputFile(""), testNum(-1)
    {}
    int width;
    int height;
    int dstWidth;
    int dstHeight;
    int intMethod;
    std::string inputFile;
    std::string outputFile;
    int testNum;
};

int interpolation_test(CmdArgs& args);
int convert_test(CmdArgs& args);
int scale_test();

#define PRINT_ERR_MSG(msg)                              \
    do {                                                \
        fprintf(stderr, "Error at line %i of %s\n%s",   \
            __LINE__, __FILE__, msg);                   \
    } while(0)

#define TEST(expr)                                              \
    do {                                                        \
        if (!(expr)) {                                          \
            PRINT_ERR_MSG("Assertion failed: " #expr "\n\n");   \
        }                                                       \
    } while(0)

#endif  // COMMON_VIDEO_VPLIB_TEST_UTIL_H
