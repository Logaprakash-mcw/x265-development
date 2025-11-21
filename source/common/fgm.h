/*****************************************************************************
 * Copyright (C) 2013-2020 MulticoreWare, Inc
 *
 * Authors: Keshav E <keshav@multicorewareinc.com>
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

#ifndef __SEIFILMGRAINANALYZER__
#define __SEIFILMGRAINANALYZER__

#include "x265.h"
#include "common.h"
#include "picyuv.h"
#include "mv.h"
#include "piclist.h"
#include "yuv.h"
#include "motion.h"
#include "temporalfilter.h"

#include "frame.h"
#include "sei.h"

#pragma once

#include <numeric>
#include <cmath>
#include <algorithm>
#include <vector>

#define PI 3.14159265358979323846;
#define LDBL_MAX          1.7976931348623158e+308
#define DBL_EPSILON       2.2204460492503131e-016
// POLYFIT
const int      MAXPAIRS                                  = 256;
const int      MAXORDER                                  = 8;     // maximum order of polinomial fitting
const int      MAX_REAL_SCALE                            = 16;
const int      ORDER                                     = 4;     // order of polinomial function
const int      QUANT_LEVELS                              = 4;     // number of quantization levels in lloyd max quantization
const int      INTERVAL_SIZE                             = 16;
const int      MIN_ELEMENT_NUMBER_PER_INTENSITY_INTERVAL = 8;
const int      MIN_POINTS_FOR_INTENSITY_ESTIMATION       = 40;    // 5*8 = 40; 5 intervals with at least 8 points
const int      MIN_BLOCKS_FOR_CUTOFF_ESTIMATION          = 2;     // 2 blocks of 64 x 64 size
const int      POINT_STEP                                = 16;    // step size in point extension
const int      MAX_NUM_POINT_TO_EXTEND                   = 4;     // max point in extension
const double   POINT_SCALE                               = 1.25;  // scaling in point extension
const double   VAR_SCALE_DOWN                            = 1.2;   // filter out large points
const double   VAR_SCALE_UP                              = 0.6;   // filter out large points
const int      NUM_PASSES                                = 2;     // number of passes when fitting the function
const int      NBRS                                      = 1;     // minimum number of surrounding points in order to keep it for further analysis (within the widnow range)
const int      WINDOW                                    = 1;     // window to check surrounding points
const int      MIN_INTENSITY                             = 40;
const int      MAX_INTENSITY                             = 950;

const int MAX_NUM_INTENSITIES =                          256;
const int MAX_NUM_MODEL_VALUES =                           6;  ///<Maximum number of model values supported in FGC SEI
const int MAX_ALLOWED_MODEL_VALUES =                       3;
const int MAX_ALLOWED_COMP_MODEL_PAIRS =                  10;
const int MAX_STANDARD_DEVIATION =                       255;  // for 8-bit format; for higher bit depths, internal scaling is performed
const int DATA_BASE_SIZE =                                64;
const int BLK_8 =                                          8;
const int BLK_16 =                                        16;
const int BLK_32 =                                        32;
const int BIT_DEPTH_8 =                                    8;
//! \ingroup SEIFilmGrainAnalyzer
//! \{

// ====================================================================================================================
// Class definition
// ====================================================================================================================

struct Picture;

typedef std::vector<std::vector<int> > PelMatrix;
typedef std::vector<std::vector<double> >           PelMatrixDouble;

typedef std::vector<std::vector<long double> >      PelMatrixLongDouble;
typedef std::vector<long double>                   PelVectorLongDouble;
typedef int16_t                                    TMatrixCoeff;      ///< transform matrix coefficient

namespace X265_NS {

class Canny
{
public:
    Canny();
    ~Canny();

    unsigned int      m_convWidthG, m_convHeightG;  // Pixel's row and col positions for Gauss filtering

    void detect_edges(PicYuv* orig, PicYuv* dest, unsigned int uiBitDepth, uint8_t compID);

private:
    static const int  m_gx[3][3];                               // Sobel kernel x
    static const int  m_gy[3][3];                               // Sobel kernel y
    static const int  m_gauss5x5[5][5];                         // Gauss 5x5 kernel, integer approximation
    x265_param*       m_param;

    unsigned int      m_convWidthS, m_convHeightS; // Pixel's row and col positions for Sobel filtering

    double            m_lowThresholdRatio; // low threshold rato
    int               m_highThresholdRatio; // high threshold rato

    void gradient   ( PicYuv* buff1, PicYuv* buff2,
                    unsigned int width, unsigned int height,
                    unsigned int convWidthS, unsigned int convHeightS, unsigned int bitDepth, uint8_t compID );
    void suppressNonMax ( PicYuv* buff1, PicYuv* buff2, unsigned int width, unsigned int height, uint8_t compID );
    void doubleThreshold( PicYuv *buff, unsigned int width, unsigned int height, /*unsigned int windowSizeRatio,*/
                        unsigned int bitDepth, uint8_t compID);
    void edgeTracking   ( PicYuv* buff1, unsigned int width, unsigned int height,
                        unsigned int windowWidth, unsigned int windowHeight, unsigned int bitDepth, uint8_t compID );
};


class Morph
{
public:
    Morph();
    ~Morph();

    int dilation  (PicYuv* buff, unsigned int bitDepth, uint8_t compID, int numIter, int iter = 0);
    int erosion   (PicYuv* buff, unsigned int bitDepth, uint8_t compID, int numIter, int iter = 0);

private:
    unsigned int m_kernelSize; // Dilation and erosion kernel size
};


class FGAnalyser
{
public:
    FGAnalyser()
    {
        m_sourcePadding[0] = m_sourcePadding[1] = 0;
        m_bitDepthsIn = 8;
        m_bitDepths = 8;
        inputChroma = X265_CSP_I420;
        param = NULL;
        //fout = NULL;
        m_normTap = 4.0;
        m_tapFilter[0] = 1;
        m_tapFilter[1] = 2;
        m_tapFilter[2] = 1;
        m_lowIntensityRatio = 0.1;
        m_numComponents = 1;
        m_log2ScaleFactor = 2;
    };
    ~FGAnalyser(){};

    //FILE*       fout;
    x265_param* param;
    void init(x265_param* m_param);
    void destroy();
    void initBufs(PicYuv* original, PicYuv* pic);
    void estimate_grain_parameters();
    void set_film_grain_parameters();
    x265_FilmGrainCharacteristics* get_film_grain_parameters();
    x265_FilmGrainCharacteristics filmgrain;

    int                                     getLog2scaleFactor()  { return m_log2ScaleFactor; };
  //FilmGrainCharacteristics::CompModel  getCompModel(int idx) { return m_compModel[idx];  };

private:

    int         m_sourcePadding[2];
    int         m_bitDepthsIn;
  //int                              m_frameSkip;
    int         inputChroma;
    uint8_t     m_bitDepths;
    uint8_t     m_numComponents;
    
  //bool          m_doAnalysis[MAX_NUM_COMPONENT] = { false, false, false };

    Canny    m_edgeDetector;
    Morph    m_morphOperation;
    double   m_lowIntensityRatio;                    // supress everything below 0.1*maxIntensityOffset

    double m_tapFilter[3];
    double m_normTap;

  //// fg model parameters
    int                                    m_log2ScaleFactor;
    x265_FilmGrainCharacteristics::CompModel m_compModel[MAX_NUM_COMPONENT];

    PicYuv *m_originalBuf = NULL;
    PicYuv *m_workingBuf  = NULL;
    PicYuv *m_maskBuf     = NULL;
    std::vector<int> stored_vec_mean_intensity[3];
    std::vector<int> stored_vec_variance_intensity[3];
    std::vector<int> stored_element_number_per_interval[3];

    void findMask                     ();

    //void estimate_grain_parameters    ();
    void block_transform(int16_t *buff, std::vector<PelMatrix> &squared_dct_grain_block_list, int offsetX, int offsetY, unsigned int bitDepth, int stride, int windowSize);
    //void block_transform              (const PicYuv& buff1, std::vector<PelMatrix>& squared_dct_grain_block_list, int offsetX, int offsetY, unsigned int bitDepth, uint8_t compID);
    void estimate_cutoff_freq         (const std::vector<PelMatrix>& blocks, std::vector<int> vec_mean, int bitDepth, int compID, int windowSize);
    int  cutoff_frequency             (std::vector<double>& mean, int windowSize);
    void estimate_scaling_factors     (std::vector<int>& data_x, std::vector<int>& data_y, unsigned int bitDepth, uint8_t compID);
    bool fit_function                 (std::vector<int>& data_x, std::vector<int>& data_y, std::vector<double>& coeffs, std::vector<double>& scalingVec,
                                         int order, int bitDepth, bool second_pass, int compID);
    void avg_scaling_vec              (std::vector<double> &scalingVec, uint8_t compID, int bitDepth);
    bool lloyd_max                    (std::vector<double>& scalingVec, std::vector<int>& quantizedVec, double& distortion, int numQuantizedLevels, int bitDepth);
    void quantize                     (std::vector<double>& scalingVec, std::vector<double>& quantizedVec, double& distortion, std::vector<double> partition, std::vector<double> codebook);
    void extend_points                (std::vector<int>& data_x, std::vector<int>& data_y, int bitDepth);

    void setEstimatedParameters       (std::vector<int>& quantizedVec, unsigned int bitDepth, uint8_t compID);
    void define_intervals_and_scalings(std::vector<std::vector<int> >& parameters, std::vector<int>& quantizedVec, int bitDepth);
    void scale_down                   (std::vector<std::vector<int> >& parameters, int bitDepth);
    void confirm_intervals            (std::vector<std::vector<int> >& parameters);

    long double ldpow                 (long double n, unsigned p);
    template <typename T>  int meanVar(T *buffer, int windowSize, int stride, int offsetX, int offsetY, bool getVar);
    int         count_edges           (PicYuv* buffer, int windowSize, uint8_t compID, int offsetX, int offsetY);

  //void subsample                    (const PicYuv& input, PicYuv& output, uint8_t compID, const int factor = 2, const int padding = 0) const;
    void upsample                     (const PicYuv& input, PicYuv& output, uint8_t compID, const int factor = 2, const int padding = 0) const;
    void combineMasks                 (PicYuv& buff, PicYuv& buff2, uint8_t compID);
    void suppressLowIntensity         (const PicYuv* buff1, PicYuv* buff2, unsigned int bitDepth, uint8_t compID);

}; // END CLASS DEFINITION

}

#endif