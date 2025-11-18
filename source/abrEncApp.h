/*****************************************************************************
* Copyright (C) 2013-2020 MulticoreWare, Inc
*
* Authors: Pooja Venkatesan <pooja@multicorewareinc.com>
*          Aruna Matheswaran <aruna@multicorewareinc.com>
*           
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

#ifndef ABR_ENCODE_H
#define ABR_ENCODE_H

#include "x265.h"
#include "threading.h"
#include "x265cli.h"

namespace X265_NS {
    // private namespace

    class PassEncoder;
    class Scaler;
    class Reader;

    class AbrEncoder
    {
    public:
        uint8_t           m_numEncodes;
        PassEncoder        **m_passEnc;
        uint32_t           m_queueSize;
        ThreadSafeInteger  m_numActiveEncodes;

        x265_picture       ***m_inputPicBuffer; //[numEncodes][queueSize]
        x265_picture       ***m_denoisedInputPicBuffer; //[numEncodes][queueSize]
        int                **m_readFlag;

        ThreadSafeInteger  *m_picWriteCnt;
        ThreadSafeInteger  *m_picReadCnt;
        ThreadSafeInteger  **m_picIdxReadCnt;

        AbrEncoder(CLIOptions cliopt[], uint8_t numEncodes, int& ret);
        bool allocBuffers();
        void destroy();

    };

    class PassEncoder : public Thread
    {
    public:

        uint32_t m_id;
        x265_param *m_param;
        AbrEncoder *m_parent;
        x265_encoder *m_encoder;
        Reader *m_reader;
        Scaler *m_scaler;
        bool m_inputOver;

        int m_threadActive;
        int m_lastIdx;
        uint32_t m_outputNalsCount;

        x265_picture **m_inputPicBuffer;
        x265_picture **m_denoisedInputPicBuffer;
        x265_picture **m_outputRecon;

        CLIOptions m_cliopt;
        InputFile* m_input;
        InputFile* m_denoisedInput;
        const char* m_reconPlayCmd;

        int m_ret;

        PassEncoder(uint32_t id, CLIOptions cliopt, AbrEncoder *parent);
        int init();

        void startThreads();

        bool readPicture(x265_picture*, int denoised);
        void destroy();

    private:
        void threadMain();
    };


    class Reader : public Thread
    {
    public:
        PassEncoder *m_parentEnc;
        int m_id;
        InputFile* m_input;
        InputFile* m_denoisedInput;
        int m_threadActive;

        Reader(int id, PassEncoder *parentEnc);
        void threadMain();
    };
}

#endif // ifndef ABR_ENCODE_H
#pragma once
