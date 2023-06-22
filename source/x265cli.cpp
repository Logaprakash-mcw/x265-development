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
#if _MSC_VER
#pragma warning(disable: 4127) // conditional expression is constant, yes I know
#endif

#include "x265cli.h"

#define START_CODE 0x00000001
#define START_CODE_BYTES 4

#ifdef __cplusplus
namespace X265_NS {
#endif

    static void printVersion(const x265_api* api)
    {
        x265_log(X265_LOG_INFO, "HEVC encoder version %s\n", api->version_str);
        x265_log(X265_LOG_INFO, "build info %s\n", api->build_info_str);
    }

    static void showHelp(x265_param *param)
    {

#define OPT(value) (value ? "enabled" : "disabled")
#define H0 printf
#define H1 printf

        H0("\nSyntax: x265 [options] infile [-o] outfile\n");
        H0("    infile can be YUV or Y4M\n");
        H0("    outfile is raw HEVC bitstream\n");
        H0("\nExecutable Options:\n");
        H0("-h/--help                        Show this help text and exit\n");
        H0("   --fullhelp                    Show all options and exit\n");
        H0("-V/--version                     Show version info and exit\n");
        H0("\nOutput Options:\n");
        H0("-o/--output <filename>           Filtered output file name\n");
        H0("-D/--output-depth 8|10|12        Output bit depth (also internal bit depth). Default %d\n", param->internalBitDepth);
        H0("\nInput Options:\n");
        H0("   --input <filename>            Raw YUV or Y4M input file name. `-` for stdin\n");
        H1("   --y4m                         Force parsing of input stream as YUV4MPEG2 regardless of file extension\n");
        H0("   --fps <float|rational>        Source frame rate (float or num/denom), auto-detected if Y4M\n");
        H0("   --input-res WxH               Source picture size [w x h], auto-detected if Y4M\n");
        H1("   --input-depth <integer>       Bit-depth of input file. Default 8\n");
        H1("   --input-csp <string>          Chroma subsampling, auto-detected if Y4M\n");
        H1("                                 0 - i400 (4:0:0 monochrome)\n");
        H1("                                 1 - i420 (4:2:0 default)\n");
        H1("                                 2 - i422 (4:2:2)\n");
        H1("                                 3 - i444 (4:4:4)\n");
        H0("   --pools <integer,...>         Comma separated thread count per thread pool (pool per NUMA node)\n");
        H0("                                 '-' implies no threads on node, '+' implies one thread per core on node\n");
        H0("-F/--frame-threads <integer>     Number of concurrently encoded frames. 0: auto-determined by core count\n");
        H0("   --[no-]wpp                    Enable Wavefront Parallel Processing. Default %s\n", OPT(param->bEnableWavefront));
        H0("\nSEI Message Options\n");
        H0("   --film-grain <filename>           File containing Film Grain Characteristics to be written as a SEI Message\n");

#undef OPT
#undef H0
#undef H1

        exit(1);
    }

    void CLIOptions::destroy()
    {
        if (isAbrLadderConfig)
        {
            for (int idx = 1; idx < argCnt; idx++)
                free(argString[idx]);
            free(argString);
        }

        if (input)
            input->release();
        input = NULL;
        if (recon)
            recon->release();
        recon = NULL;
        if (qpfile)
            fclose(qpfile);
        qpfile = NULL;
        if (fgChar)
            fclose(fgChar);
        fgChar = NULL;
        if (zoneFile)
            fclose(zoneFile);
        zoneFile = NULL;
        if (dolbyVisionRpu)
            fclose(dolbyVisionRpu);
        dolbyVisionRpu = NULL;
        //if (output)
        //    output->release();
        //output = NULL;
    }

    void CLIOptions::printStatus(uint32_t frameNum)
    {
        char buf[200];
        int64_t time = x265_mdate();

        if (!bProgress || !frameNum || (prevUpdateTime && time - prevUpdateTime < UPDATE_INTERVAL))
            return;

        int64_t elapsed = time - startTime;
        double fps = elapsed > 0 ? frameNum * 1000000. / elapsed : 0;
        //float bitrate = 0.008f * totalbytes * (param->fpsNum / param->fpsDenom) / ((float)frameNum);
        if (framesToBeEncoded)
        {
            int eta = (int)(elapsed * (framesToBeEncoded - frameNum) / ((int64_t)frameNum * 1000000));
            sprintf(buf, "x265 [%.1f%%] %d/%d frames, %.2f fps, eta %d:%02d:%02d",
                100. * frameNum / param->totalFrames, frameNum, param->totalFrames, fps, 
                eta / 3600, (eta / 60) % 60, eta % 60);
        }
        else
            sprintf(buf, "x265 %d frames: %.2f fps", frameNum, fps);

        fprintf(stderr, "%s  \r", buf + 5);
        SetConsoleTitle(buf);
        fflush(stderr); // needed in windows
        prevUpdateTime = time;
    }

    void CLIOptions::writeFG(x265_FilmGrainCharacteristics* filmgrain)
    {
        /* Write to the model file */
        fwrite((char* )&filmgrain->m_filmGrainCharacteristicsCancelFlag, sizeof(bool), 1, fgChar);
        fwrite((char* )&filmgrain->m_filmGrainCharacteristicsPersistenceFlag, sizeof(bool), 1, fgChar);
        fwrite((char* )&filmgrain->m_filmGrainModelId, sizeof(unsigned char), 1, fgChar);
        fwrite((char* )&filmgrain->m_separateColourDescriptionPresentFlag, sizeof(bool), 1, fgChar); // Always set to 0
        fwrite((char* )&filmgrain->m_blendingModeId, sizeof(unsigned char), 1, fgChar);
        fwrite((char* )&filmgrain->m_log2ScaleFactor, sizeof(unsigned char), 1, fgChar);
        fwrite((char* )&filmgrain->m_compModel[0]->bPresentFlag, sizeof(bool), 1, fgChar);
        fwrite((char* )&filmgrain->m_compModel[1]->bPresentFlag, sizeof(bool), 1, fgChar);
        fwrite((char* )&filmgrain->m_compModel[2]->bPresentFlag, sizeof(bool), 1, fgChar);
        for (int i = 0; i < 3; i++)
        {
            if (filmgrain->m_compModel[i]->bPresentFlag)
            {
                fwrite((char* )&filmgrain->m_compModel[i]->m_filmGrainNumIntensityIntervalMinus1, sizeof(unsigned char), 1, fgChar);
                fwrite((char* )&filmgrain->m_compModel[i]->numModelValues, sizeof(unsigned char), 1, fgChar);
                for (int j = 0; j <= filmgrain->m_compModel[i]->m_filmGrainNumIntensityIntervalMinus1; j++)
                {
                    fwrite((char* )&filmgrain->m_compModel[i]->intensityValues[j].intensityIntervalLowerBound, sizeof(unsigned char), 1, fgChar);// min intensity
                    fwrite((char* )&filmgrain->m_compModel[i]->intensityValues[j].intensityIntervalUpperBound, sizeof(unsigned char), 1, fgChar);// max intensity
                    for (int k = 0; k < filmgrain->m_compModel[i]->numModelValues; k++)
                    {
                        fwrite((char* )&filmgrain->m_compModel[i]->intensityValues[j].compModelValue[k], sizeof(int), 1, fgChar);// compModelValue
                    }
                }
            }
        }
    }
    //bool CLIOptions::parseZoneParam(int argc, char **argv, x265_param* globalParam, int zonefileCount)
    //{
    //    bool bError = false;
    //    int bShowHelp = false;
    //    int outputBitDepth = 0;
    //    const char *profile = NULL;

    //    /* Presets are applied before all other options. */
    //    for (optind = 0;;)
    //    {
    //        int c = getopt_long(argc, argv, short_options, long_options, NULL);
    //        if (c == -1)
    //            break;
    //        else if (c == 'D')
    //            outputBitDepth = atoi(optarg);
    //        else if (c == 'P')
    //            profile = optarg;
    //        else if (c == '?')
    //            bShowHelp = true;
    //    }

    //    if (!outputBitDepth && profile)
    //    {
    //        /* try to derive the output bit depth from the requested profile */
    //        if (strstr(profile, "10"))
    //            outputBitDepth = 10;
    //        else if (strstr(profile, "12"))
    //            outputBitDepth = 12;
    //        else
    //            outputBitDepth = 8;
    //    }

    //    api = x265_api_get(outputBitDepth);
    //    if (!api)
    //    {
    //        x265_log(X265_LOG_WARNING, "falling back to default bit-depth\n");
    //        api = x265_api_get(0);
    //    }

    //    if (bShowHelp)
    //    {
    //        printVersion(api);
    //        showHelp(globalParam);
    //    }

    //    for (optind = 0;;)
    //    {
    //        int long_options_index = -1;
    //        int c = getopt_long(argc, argv, short_options, long_options, &long_options_index);
    //        if (c == -1)
    //            break;

    //        if (long_options_index < 0 && c > 0)
    //        {
    //            for (size_t i = 0; i < sizeof(long_options) / sizeof(long_options[0]); i++)
    //            {
    //                if (long_options[i].val == c)
    //                {
    //                    long_options_index = (int)i;
    //                    break;
    //                }
    //            }

    //            if (long_options_index < 0)
    //            {
    //                /* getopt_long might have already printed an error message */
    //                if (c != 63)
    //                    x265_log(X265_LOG_WARNING, "internal error: short option '%c' has no long option\n", c);
    //                return true;
    //            }
    //        }
    //        if (long_options_index < 0)
    //        {
    //            x265_log(X265_LOG_WARNING, "short option '%c' unrecognized\n", c);
    //            return true;
    //        }

    //        if (bError)
    //        {
    //            const char *name = long_options_index > 0 ? long_options[long_options_index].name : argv[optind - 2];
    //            x265_log(X265_LOG_ERROR, "invalid argument: %s = %s\n", name, optarg);
    //            return true;
    //        }
    //    }

    //    if (optind < argc)
    //    {
    //        x265_log(X265_LOG_WARNING, "extra unused command arguments given <%s>\n", argv[optind]);
    //        return true;
    //    }
    //    return false;
    //}

    bool CLIOptions::parse(int argc, char **argv)
    {
        bool bError = false;
        int bShowHelp = false;
        int inputBitDepth = 8;
        int outputBitDepth = X265_DEPTH;
        int reconFileBitDepth = 0;
        const char *inputfn = NULL;
        const char *reconfn = NULL;
        const char *outputfn = NULL;
        argCnt = argc;
        argString = argv;

        if (argc <= 1)
        {
            x265_log(X265_LOG_ERROR, "No input file. Run x265 --help for a list of options.\n");
            return true;
        }

        /* Presets are applied before all other options. */
        for (optind = 0;;)
        {
            int optionsIndex = -1;
            int c = getopt_long(argc, argv, short_options, long_options, &optionsIndex);
            if (c == -1)
                break;
            else if (c == 'D')
                outputBitDepth = atoi(optarg);
            else if (c == '?')
                bShowHelp = true;

        }

        api = x265_api_get(outputBitDepth);
        if (!api)
        {
            x265_log(X265_LOG_WARNING, "falling back to default bit-depth\n");
            api = x265_api_get(0);
        }

        param = api->param_alloc();
        if (!param)
        {
            x265_log(X265_LOG_ERROR, "param alloc failed\n");
            return true;
        }

        api->param_default(param);

        if (bShowHelp)
        {
            printVersion(api);
            showHelp(param);
        }

        for (optind = 0;;)
        {
            int long_options_index = -1;
            int c = getopt_long(argc, argv, short_options, long_options, &long_options_index);
            if (c == -1)
                break;

            switch (c)
            {
            case 'h':
                printVersion(api);
                showHelp(param);
                break;

            case 'V':
                printVersion(api);
                //x265_report_simd(param);
                exit(0);

            default:
                if (long_options_index < 0 && c > 0)
                {
                    for (size_t i = 0; i < sizeof(long_options) / sizeof(long_options[0]); i++)
                    {
                        if (long_options[i].val == c)
                        {
                            long_options_index = (int)i;
                            break;
                        }
                    }

                    if (long_options_index < 0)
                    {
                        /* getopt_long might have already printed an error message */
                        if (c != 63)
                            x265_log(X265_LOG_WARNING, "internal error: short option '%c' has no long option\n", c);
                        return true;
                    }
                }
                if (long_options_index < 0)
                {
                    x265_log(X265_LOG_WARNING, "short option '%c' unrecognized\n", c);
                    return true;
                }
#define OPT(longname) \
                                            else if (!strcmp(long_options[long_options_index].name, longname))
#define OPT2(name1, name2) \
                                            else if (!strcmp(long_options[long_options_index].name, name1) || \
             !strcmp(long_options[long_options_index].name, name2))

                if (0);
                OPT2("frame-skip", "seek") this->seek = (uint32_t)x265_atoi(optarg, bError);
                OPT("frames") this->framesToBeEncoded = (uint32_t)x265_atoi(optarg, bError);
                OPT("output") reconfn = optarg;
                OPT("input") inputfn = optarg;
                //OPT("recon") reconfn = optarg;
                OPT("input-depth") inputBitDepth = (uint32_t)x265_atoi(optarg, bError);
                OPT("dither") this->bDither = true;
                OPT("recon-depth") reconFileBitDepth = (uint32_t)x265_atoi(optarg, bError);
                OPT("y4m") this->bForceY4m = true;
                OPT("output-depth")   /* handled above */;
                OPT("recon-y4m-exec") reconPlayCmd = optarg;

                else
                    bError |= !!api->param_parse(param, long_options[long_options_index].name, optarg);
                if (bError)
                {
                    const char *name = long_options_index > 0 ? long_options[long_options_index].name : argv[optind - 2];
                    x265_log(X265_LOG_ERROR, "invalid argument: %s = %s\n", name, optarg);
                    return true;
                }
#undef OPT
            }
        }

        if (optind < argc && !inputfn)
            inputfn = argv[optind++];
        if (optind < argc && !outputfn)
            outputfn = argv[optind++];
        if (optind < argc)
        {
            x265_log(X265_LOG_WARNING, "extra unused command arguments given <%s>\n", argv[optind]);
            return true;
        }

        if (argc <= 1)
        {
            api->param_default(param);
            printVersion(api);
            showHelp(param);
        }

        if (!inputfn || !reconfn)
        {
            x265_log(X265_LOG_ERROR, "input or output file not specified, try --help for help\n");
            return true;
        }

        param->internalBitDepth = inputBitDepth;

        if (param->internalBitDepth != api->bit_depth)
        {
            x265_log(X265_LOG_ERROR, "Only bit depths of %d are supported in this build\n", api->bit_depth);
            return true;
        }
        InputFileInfo info;
        info.filename = inputfn;
        info.depth = inputBitDepth;
        info.csp = param->internalCsp;
        info.width = param->sourceWidth;
        info.height = param->sourceHeight;
        info.fpsNum = param->fpsNum;
        info.fpsDenom = param->fpsDenom;
        info.skipFrames = seek;
        info.frameCount = 0;
        //getParamAspectRatio(param, info.sarWidth, info.sarHeight);


        this->input = InputFile::open(info, this->bForceY4m);
        if (!this->input || this->input->isFail())
        {
            x265_log_file(X265_LOG_ERROR, "unable to open input file <%s>\n", inputfn);
            return true;
        }

        if (info.depth < 8 || info.depth > 16)
        {
            x265_log(X265_LOG_ERROR, "Input bit depth (%d) must be between 8 and 16\n", inputBitDepth);
            return true;
        }

        /* Unconditionally accept height/width/csp/bitDepth from file info */
        param->sourceWidth = info.width;
        param->sourceHeight = info.height;
        param->internalCsp = info.csp;
        param->sourceBitDepth = info.depth;

        /* Accept fps and sar from file info if not specified by user */
        if (param->fpsDenom == 0 || param->fpsNum == 0)
        {
            param->fpsDenom = info.fpsDenom;
            param->fpsNum = info.fpsNum;
        }
        if (this->framesToBeEncoded == 0 && info.frameCount > (int)seek)
            this->framesToBeEncoded = info.frameCount - seek;
        param->totalFrames = this->framesToBeEncoded;

        /* Force CFR until we have support for VFR */
        info.timebaseNum = param->fpsDenom;
        info.timebaseDenom = param->fpsNum;

        this->input->startReader();
        reconFileBitDepth = param->internalBitDepth;
        this->recon = ReconFile::open(reconfn, param->sourceWidth, param->sourceHeight, reconFileBitDepth,
                param->fpsNum, param->fpsDenom, param->internalCsp);
        if (this->recon->isFail())
        {
            x265_log_file(X265_LOG_ERROR, "failed to open output file <%s> for writing\n", reconfn);
            return true;
        }
        general_log_file(this->recon->getName(), X265_LOG_INFO, "output file: %s\n", reconfn);
        return false;
    }

#ifdef __cplusplus
}
#endif