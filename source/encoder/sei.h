/*****************************************************************************
* Copyright (C) 2013-2020 MulticoreWare, Inc
*
* Authors: Steve Borho <steve@borho.org>
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

#ifndef X265_SEI_H
#define X265_SEI_H

#include "common.h"
#include "bitstream.h"
#include "slice.h"
#include "md5.h"

namespace X265_NS {
// private namespace

class SEI : public SyntaxElementWriter
{
public:
    virtual ~SEI() {}
protected:

};

/* Film grain characteristics */
class FilmGrainCharacteristics : public SEI
{
  public:

    struct CompModelIntensityValues
    {
        uint8_t intensityIntervalLowerBound;
        uint8_t intensityIntervalUpperBound;
        int*    compModelValue;
    };

    struct CompModel
    {
        bool    bPresentFlag;
        uint8_t numModelValues;
        uint8_t m_filmGrainNumIntensityIntervalMinus1;
        CompModelIntensityValues* intensityValues;
    };

    CompModel   m_compModel[MAX_NUM_COMPONENT];
    bool        m_filmGrainCharacteristicsPersistenceFlag;
    bool        m_filmGrainCharacteristicsCancelFlag;
    bool        m_separateColourDescriptionPresentFlag;
    bool        m_filmGrainFullRangeFlag;
    uint8_t     m_filmGrainModelId;
    uint8_t     m_blendingModeId;
    uint8_t     m_log2ScaleFactor;
    uint8_t     m_filmGrainBitDepthLumaMinus8;
    uint8_t     m_filmGrainBitDepthChromaMinus8;
    uint8_t     m_filmGrainColourPrimaries;
    uint8_t     m_filmGrainTransferCharacteristics;
    uint8_t     m_filmGrainMatrixCoeffs;

    void writeSEI(const SPS&)
    {
        WRITE_FLAG(m_filmGrainCharacteristicsCancelFlag, "film_grain_characteristics_cancel_flag");

        if (!m_filmGrainCharacteristicsCancelFlag)
        {
            WRITE_CODE(m_filmGrainModelId, 2, "film_grain_model_id");
            WRITE_FLAG(m_separateColourDescriptionPresentFlag, "separate_colour_description_present_flag");
            if (m_separateColourDescriptionPresentFlag)
            {
                WRITE_CODE(m_filmGrainBitDepthLumaMinus8, 3, "film_grain_bit_depth_luma_minus8");
                WRITE_CODE(m_filmGrainBitDepthChromaMinus8, 3, "film_grain_bit_depth_chroma_minus8");
                WRITE_FLAG(m_filmGrainFullRangeFlag, "film_grain_full_range_flag");
                WRITE_CODE(m_filmGrainColourPrimaries, X265_BYTE, "film_grain_colour_primaries");
                WRITE_CODE(m_filmGrainTransferCharacteristics, X265_BYTE, "film_grain_transfer_characteristics");
                WRITE_CODE(m_filmGrainMatrixCoeffs, X265_BYTE, "film_grain_matrix_coeffs");
            }
            WRITE_CODE(m_blendingModeId, 2, "blending_mode_id");
            WRITE_CODE(m_log2ScaleFactor, 4, "log2_scale_factor");
            for (uint8_t c = 0; c < 3; c++)
            {
                WRITE_FLAG(m_compModel[c].bPresentFlag && m_compModel[c].m_filmGrainNumIntensityIntervalMinus1 + 1 > 0 && m_compModel[c].numModelValues > 0, "comp_model_present_flag[c]");
            }
            for (uint8_t c = 0; c < 3; c++)
            {
                if (m_compModel[c].bPresentFlag && m_compModel[c].m_filmGrainNumIntensityIntervalMinus1 + 1 > 0 && m_compModel[c].numModelValues > 0)
                {
                    assert(m_compModel[c].m_filmGrainNumIntensityIntervalMinus1 + 1 <= 256);
                    assert(m_compModel[c].numModelValues <= X265_BYTE);
                    WRITE_CODE(m_compModel[c].m_filmGrainNumIntensityIntervalMinus1 , X265_BYTE, "num_intensity_intervals_minus1[c]");
                    WRITE_CODE(m_compModel[c].numModelValues - 1, 3, "num_model_values_minus1[c]");
                    for (uint8_t interval = 0; interval < m_compModel[c].m_filmGrainNumIntensityIntervalMinus1 + 1; interval++)
                    {
                        WRITE_CODE(m_compModel[c].intensityValues[interval].intensityIntervalLowerBound, X265_BYTE, "intensity_interval_lower_bound[c][i]");
                        WRITE_CODE(m_compModel[c].intensityValues[interval].intensityIntervalUpperBound, X265_BYTE, "intensity_interval_upper_bound[c][i]");
                        for (uint8_t j = 0; j < m_compModel[c].numModelValues; j++)
                        {
                            WRITE_SVLC(m_compModel[c].intensityValues[interval].compModelValue[j],"comp_model_value[c][i]");
                        }
                    }
                }
            }
            WRITE_FLAG(m_filmGrainCharacteristicsPersistenceFlag, "film_grain_characteristics_persistence_flag");
        }
        if (m_bitIf->getNumberOfWrittenBits() % X265_BYTE != 0)
        {
            WRITE_FLAG(1, "payload_bit_equal_to_one");
            while (m_bitIf->getNumberOfWrittenBits() % X265_BYTE != 0)
            {
                WRITE_FLAG(0, "payload_bit_equal_to_zero");
            }
        }
    }
};

}
#endif // ifndef X265_SEI_H
