/*****************************************************************************
 * Copyright (C) 2013-2020 MulticoreWare, Inc
 *
 * Authors: Steve Borho <steve@borho.org>
 *          Min Chen <chenm003@163.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *
 * This program is also available under a commercial proprietary license.
 * For more information, contact us at license @ x265.com.
 *****************************************************************************/

#ifndef X265CLI_H
#define X265CLI_H 1

#include "common.h"
#include "param.h"
#include "input/input.h"
#include "output/output.h"
#include "output/reconplay.h"

#include <getopt.h>

#ifdef _WIN32
#include <windows.h>
#define SetThreadExecutionState(es)
#else
#define GetConsoleTitle(t, n)
#define SetConsoleTitle(t)
#define SetThreadExecutionState(es)
#endif

#ifdef __cplusplus
namespace X265_NS {
#endif

static const char short_options[] = "o:D:P:p:f:F:r:I:i:b:s:t:q:m:hwV?";
static const struct option long_options[] =
{
    { "help",                 no_argument, NULL, 'h' },
    { "fullhelp",             no_argument, NULL, 0 },
    { "version",              no_argument, NULL, 'V' },
    { "asm",            required_argument, NULL, 0 },
    { "no-asm",               no_argument, NULL, 0 },
    { "pools",          required_argument, NULL, 0 },
    { "numa-pools",     required_argument, NULL, 0 },
    { "frame-threads",  required_argument, NULL, 'F' },
    { "y4m",                  no_argument, NULL, 0 },
    { "output",         required_argument, NULL, 'o' },
    { "output-depth",   required_argument, NULL, 'D' },
    { "input",          required_argument, NULL, 0 },
    { "input-depth",    required_argument, NULL, 0 },
    { "input-res",      required_argument, NULL, 0 },
    { "input-csp",      required_argument, NULL, 0 },
    { "fps",            required_argument, NULL, 0 },
    { "frame-skip",     required_argument, NULL, 0 },
    { "frames",         required_argument, NULL, 'f' },
    { "recon",          required_argument, NULL, 'r' },
    { "recon-depth",    required_argument, NULL, 0 },
    { "no-wpp",               no_argument, NULL, 0 },
    { "wpp",                  no_argument, NULL, 0 },
    { "qp",             required_argument, NULL, 'q' },
    { "recon-y4m-exec", required_argument, NULL, 0 },
    { "mcstf",                 no_argument, NULL, 0 },
    { "film-grain", required_argument, NULL, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 }
};

    struct CLIOptions
    {
        InputFile* input;
        ReconFile* recon;
        //OutputFile* output;
        FILE*       qpfile;
        FILE*       zoneFile;
        FILE*    dolbyVisionRpu;    /* File containing Dolby Vision BL RPU metadata */
        FILE*    scenecutAwareQpConfig; /* File containing scenecut aware frame quantization related CLI options */
        const char* reconPlayCmd;
        const x265_api* api;
        x265_param* param;
        bool bProgress;
        bool bForceY4m;
        bool bDither;
        uint32_t seek;              // number of frames to skip from the beginning
        uint32_t framesToBeEncoded; // number of frames to encode
        uint64_t totalbytes;
        int64_t startTime;
        int64_t prevUpdateTime;

        int argCnt;
        char** argString;

        /* ABR ladder settings */
        bool isAbrLadderConfig;
        bool enableScaler;
        char*    encName;
        char*    reuseName;
        uint32_t encId;
        int      refId;
        uint32_t loadLevel;
        uint32_t saveLevel;
        uint32_t numRefs;

        /* in microseconds */
        static const int UPDATE_INTERVAL = 250000;
        CLIOptions()
        {
            input = NULL;
            recon = NULL;
            //output = NULL;
            qpfile = NULL;
            zoneFile = NULL;
            dolbyVisionRpu = NULL;
            scenecutAwareQpConfig = NULL;
            reconPlayCmd = NULL;
            api = NULL;
            param = NULL;
            framesToBeEncoded = seek = 0;
            totalbytes = 0;
            bProgress = true;
            bForceY4m = false;
            startTime = x265_mdate();
            prevUpdateTime = 0;
            bDither = false;
            isAbrLadderConfig = false;
            enableScaler = false;
            encName = NULL;
            reuseName = NULL;
            encId = 0;
            refId = -1;
            loadLevel = 0;
            saveLevel = 0;
            numRefs = 0;
            argCnt = 0;
        }

        void destroy();
        void printStatus(uint32_t frameNum);
        bool parse(int argc, char **argv);
        //bool parseZoneParam(int argc, char **argv, x265_param* globalParam, int zonefileCount);
    };
#ifdef __cplusplus
}
#endif

#endif
