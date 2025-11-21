#include "fgm.h"

using namespace X265_NS;

const int Canny::m_gx[3][3] = { { -1, 0, 1 }, { -2, 0, 2 }, { -1, 0, 1 } };
const int Canny::m_gy[3][3] = { { -1, -2, -1 }, { 0, 0, 0 }, { 1, 2, 1 } };

const int Canny::m_gauss5x5[5][5] = { { 2, 4, 5, 4, 2 },
                                    { 4, 9, 12, 9, 4 },
                                    { 5, 12, 15, 12, 5 },
                                    { 4, 9, 12, 9, 4 },
                                    { 2, 4, 5, 4, 2 } };

Canny::Canny()
{
    // init();
    m_param = NULL;
    m_convWidthG = 5;
    m_convHeightG = 5;
    m_convWidthS = 3;
    m_convHeightS = 3;
    m_lowThresholdRatio = 0.1;
    m_highThresholdRatio = 3;

}

Canny::~Canny()
{
  // uninit();
}

void Canny::gradient(PicYuv *buff1, PicYuv *buff2, unsigned int width, unsigned int height, unsigned int convWidthS, unsigned int convHeightS, unsigned int bitDepth, uint8_t compID)
{
  /*
  buff1 - magnitude; buff2 - orientation (Only luma in buff2)
  */

    // 360 degrees are split into the 8 equal parts; edge direction is quantized
    const double edge_threshold_22_5  = 22.5;
    const double edge_threshold_67_5  = 67.5;
    const double edge_threshold_112_5 = 112.5;
    const double edge_threshold_157_5 = 157.5;

    const int maxClpRange = (1 << bitDepth) - 1;
    const int padding     = convWidthS / 2;

    pixel *src          = buff1->m_picOrg[compID];
    pixel *orientation  = buff2->m_picOrg[compID];
    intptr_t stride = !compID ? buff1->m_stride : buff1->m_strideC;
    //uint32_t width = buff1->m_picWidth, height = buff1->m_picHeight;

    // tmp buffs
    PicYuv tmpBuf1, tmpBuf2;
    tmpBuf1.create(m_param);
    tmpBuf2.create(m_param);

    int16_t *tmp1Y = tmpBuf1.m_picDif[compID];
    int16_t *tmp2Y = tmpBuf2.m_picDif[compID];


    extendPicBorder(src, stride, width, height, padding, padding);
    //buff1->get(compID).extendBorderPel(padding, padding);

  // Gx
    for (int64_t i = 0; i < width; i++)
    {
        for (int64_t j = 0; j < height; j++)
        {
            int16_t acc = 0;
            for (int64_t x = 0; x < convWidthS; x++)
            {
                for (int64_t y = 0; y < convHeightS; y++)
                {
                    int64_t id = (y - convHeightS / 2 + j) * stride + (x - convWidthS / 2 + i);
                    acc += (src[id] * m_gx[x][y]);
                }
            }
            tmp1Y[j * stride + i] = acc;
        }
    }

    // Gy
    for (int64_t i = 0; i < width; i++)
    {
        for (int64_t j = 0; j < height; j++)
        {
            int16_t acc = 0;
            for (int64_t x = 0; x < convWidthS; x++)
            {
                for (int64_t y = 0; y < convHeightS; y++)
                {
                    int64_t id = (y - convHeightS / 2 + j) * stride + (x - convWidthS / 2 + i);
                    acc += (src[id] * m_gy[x][y]);
                }
            }
            tmp2Y[j * stride + i] = acc;
        }
    }

    // magnitude
    for (int64_t i = 0; i < width; i++)
    {
        for (int64_t j = 0; j < height; j++)
        {
            int id      = j * stride + i;
            pixel tmp   = (pixel)((abs(tmp1Y[id]) + abs(tmp2Y[id])) / 2);
            src[id]     = (pixel) x265_clip3((pixel) 0, (pixel) maxClpRange, tmp);
        }
    }

    // edge direction - quantized edge directions
    for (int64_t i = 0; i < width; i++)
    {
        for (int64_t j = 0; j < height; j++)
        {
            int id       = j * stride + i;
            double theta = (atan2(tmp1Y[id], tmp2Y[id]) * 180) / PI;

            /* Convert actual edge direction to approximate value - quantize directions */
            if (((-edge_threshold_22_5 < theta) && (theta <= edge_threshold_22_5)) ||
                ((edge_threshold_157_5 < theta) ||
                (theta <= -edge_threshold_157_5)))
            {
                orientation[id] = 0;
            }
            if (((-edge_threshold_157_5 < theta) && (theta <= -edge_threshold_112_5)) ||
                ((edge_threshold_22_5 < theta) && (theta <= edge_threshold_67_5)))
            {
                orientation[id] = 45;
            }
            if (((-edge_threshold_112_5 < theta) && (theta <= -edge_threshold_67_5)) || 
                ((edge_threshold_67_5 < theta) && (theta <= edge_threshold_112_5)))
            {
                orientation[id] = 90;
            }
            if (((-edge_threshold_67_5 < theta) && (theta <= -edge_threshold_22_5)) ||
                ((edge_threshold_112_5 < theta) && (theta <= edge_threshold_157_5)))
            {
                orientation[id] = 135;
            }
        }
    }

    extendPicBorder(src, stride, width, height, padding, padding);
    tmpBuf1.destroy();
    tmpBuf2.destroy();
}

void Canny::suppressNonMax(PicYuv *buff1, PicYuv *buff2, unsigned int width, unsigned int height, uint8_t compID)
{
    pixel *src          = buff1->m_picOrg[compID];
    pixel *orientation  = buff2->m_picOrg[compID];
    int stride = !compID ? buff1->m_stride : buff1->m_strideC;
    //int width = buff1->m_picWidth, height = buff1->m_picHeight;
    for (int64_t i = 0; i < width; i++)
    {
        for (int64_t j = 0; j < height; j++)
        {
            int rowShift = 0, colShift = 0;
            int id       = j * stride + i;
            switch (orientation[id])
            {
                case 0:
                    rowShift = 1;
                    colShift = 0;
                    break;
                case 45:
                    rowShift = 1;
                    colShift = 1;
                    break;
                case 90:
                    rowShift = 0;
                    colShift = 1;
                    break;
                case 135:
                    rowShift = -1;
                    colShift = 1;
                    break;
                default: x265_log(X265_LOG_ERROR,"Unsupported gradient direction."); break;
            }

            pixel pelCurrent             = src[id];
            pixel pelEdgeDirectionTop    = src[(j + rowShift) * stride + (i + colShift)];
            pixel pelEdgeDirectionBottom = src[(j - rowShift) * stride + (i - colShift)];
            if ((pelCurrent < pelEdgeDirectionTop) || (pelCurrent < pelEdgeDirectionBottom))
            {
                orientation[id] = 0;   // supress
            }
            else
            {
                orientation[id] = src[id];   // keep
            }
        }
    }
    memcpy(src, orientation, stride * height * sizeof( pixel ) );
}

void Canny::doubleThreshold(PicYuv *buff, unsigned int width, unsigned int height, unsigned int bitDepth, uint8_t compID)
{
    pixel strongPel = ((pixel) 1 << bitDepth) - 1;
    pixel weekPel   = ((pixel) 1 << (bitDepth - 1)) - 1;

    pixel highThreshold = 0;
    pixel lowThreshold  = strongPel;

    pixel *src          = buff->m_picOrg[compID];
    int stride          = !compID ? buff->m_stride : buff->m_strideC;
    for (int64_t i = 0; i < width; i++)
    {
        for (int64_t j = 0; j < height; j++)
        {
            highThreshold = std::max<pixel>(highThreshold, src[j * stride + i]);
        }
    }

    // global low and high threshold
    lowThreshold = (pixel)(m_lowThresholdRatio * highThreshold);
    highThreshold = x265_clip3(0, (1 << bitDepth) - 1, m_highThresholdRatio * lowThreshold);   // Canny recommended a upper:lower
                                                                                               // ratio between 2:1 and 3:1.

    // strong, week, supressed
    for (int64_t i = 0; i < width; i++)
    {
        for (int64_t j = 0; j < height; j++)
        {
            int id = j * stride + i;
            if (src[id] > highThreshold)
            {
                src[id] = strongPel;
            }
            else if (src[id] <= highThreshold && src[id] > lowThreshold)
            {
                src[id] = weekPel;
            }
            else
            {
                src[id] = 0;
            }
        }
    }
    extendPicBorder(src, stride, width, height, 1, 1);
}

void Canny::edgeTracking(PicYuv *buff, unsigned int width, unsigned int height, unsigned int windowWidth, unsigned int windowHeight, unsigned int bitDepth, uint8_t compID)
{
    pixel strongPel = ((pixel) 1 << bitDepth) - 1;
    pixel weekPel   = ((pixel) 1 << (bitDepth - 1)) - 1;

    pixel *src          = buff->m_picOrg[compID];
    int stride          = !compID ? buff->m_stride : buff->m_strideC;

    for (int64_t i = 0; i < width; i++)
    {
        for (int64_t j = 0; j < height; j++)
        {
            int id = j * stride + i;
            if (src[id] == weekPel)
            {
                bool strong = false;

                for (int64_t x = 0; x < windowWidth; x++)
                {
                    for (int64_t y = 0; y < windowHeight; y++)
                    {
                        int64_t idW = (y - windowHeight / 2 + j) * stride + (x - windowWidth / 2 + i);
                        if (src[idW] == strongPel)
                        {
                            strong = true;
                            break;
                        }
                    }
                }

                if (strong)
                {
                    src[id] = strongPel;
                }
                else
                {
                    src[id] = 0;   // supress
                }
            }
        }
    }
}

void Canny::detect_edges(PicYuv *orig, PicYuv *dest, unsigned int uiBitDepth, uint8_t compID)
{
  /* No noise reduction - Gaussian blur is skipped;
   Gradient calculation;
   Non-maximum suppression;
   Double threshold;
   Edge Tracking by Hysteresis.*/

  unsigned int width      = (!compID ? orig->m_picWidth : orig->m_picWidthC),
               height     = (!compID ? orig->m_picHeight : orig->m_picHeightC);   // Width and Height of current frame
  unsigned int convWidthS  = m_convWidthS,
               convHeightS = m_convHeightS;   // Pixel's row and col positions for Sobel filtering
  unsigned int bitDepth    = uiBitDepth;
  m_param = orig->m_param;

  // tmp buff
  PicYuv orientationBuf;
  orientationBuf.create(m_param);

  //dest->copyFromFrame(orig);   // we skip blur in canny detector to catch as much as possible edges and textures

  /* Gradient calculation */

  gradient(dest, &orientationBuf, width, height, convWidthS, convHeightS, bitDepth, compID);
  /* Non - maximum suppression */
  suppressNonMax(dest, &orientationBuf, width, height, compID);
  /* Double threshold */
  doubleThreshold(dest, width, height, /*4,*/ bitDepth, compID);
  /* Edge Tracking by Hysteresis */
  edgeTracking(dest, width, height, convWidthS, convHeightS, bitDepth, compID);

  orientationBuf.destroy();
}

// ====================================================================================================================
// Morphologigal operations - Dilation and Erosion
// ====================================================================================================================
Morph::Morph()
{
    m_kernelSize = 3;
  // init();
}

Morph::~Morph()
{
  // uninit();
}



int Morph::dilation(PicYuv *buff, unsigned int bitDepth, uint8_t compID, int numIter, int iter)
{
    if (iter == numIter)
    {
        return iter;
    }

    unsigned int width      = (!compID ? buff->m_picWidth : buff->m_picWidthC),
                 height     = (!compID ? buff->m_picHeight : buff->m_picHeightC);   // Width and Height of current frame
    unsigned int windowSize = m_kernelSize;
    unsigned int padding    = windowSize / 2;

    pixel strongPel = ((pixel) 1 << bitDepth) - 1;
    int stride = !compID ? buff->m_stride : buff->m_strideC;

    PicYuv tmpBuf;
    tmpBuf.create(buff->m_param);
    tmpBuf.copyFromFrame(buff);
    //memcpy(tmpBuf.m_picOrg[compID], buff->m_picOrg[compID], height * stride * sizeof(pixel));

    pixel* src = buff->m_picOrg[compID];
    extendPicBorder(src, stride, width, height, padding, padding);

    for (int64_t i = 0; i < width; i++)
    {
        for (int64_t j = 0; j < height; j++)
        {
            bool strong = false;
            for (int64_t x = 0; x < windowSize; x++)
            {
                for (int64_t y = 0; y < windowSize; y++)
                {
                    int xx = x - windowSize / 2 + i;
                    int yy = y - windowSize / 2 + j;
                    if (src[xx + yy * stride] == strongPel)
                    {
                        strong = true;
                        break;
                    }
                }
            }
            if (strong)
            {
                tmpBuf.m_picOrg[compID][j * stride + i] = strongPel;
            }
        }
    }
    buff->copyFromFrame(&tmpBuf);
    tmpBuf.destroy();

    iter++;

    iter = dilation(buff, bitDepth, compID, numIter, iter);

    return iter;
}

int Morph::erosion(PicYuv *buff, unsigned int bitDepth, uint8_t compID, int numIter, int iter)
{
    if (iter == numIter)
    {
        return iter;
    }
    unsigned int width      = (!compID ? buff->m_picWidth : buff->m_picWidthC),
                 height     = (!compID ? buff->m_picHeight : buff->m_picHeightC);   // Width and Height of current frame
    int windowSize = m_kernelSize;
    int padding    = windowSize / 2;

    pixel* src = buff->m_picOrg[compID];
    int stride = !compID ? buff->m_stride : buff->m_strideC;

    PicYuv tmpBuf;
    tmpBuf.create(buff->m_param);
    tmpBuf.copyFromFrame(buff);
    //memcpy(tmpBuf.m_picOrg[compID], buff->m_picOrg[compID], height * stride * sizeof(pixel));

    extendPicBorder(src, stride, width, height, padding, padding);

    for (int64_t i = 0; i < width; i++)
    {
        for (int64_t j = 0; j < height; j++)
        {
            bool week = false;
            for (int64_t x = 0; x < windowSize; x++)
            {
                for (int64_t y = 0; y < windowSize; y++)
                {
                    if (src[(x - windowSize / 2 + i) + (y - windowSize / 2 + j) * stride] == 0)
                    {
                        week = true;
                        break;
                    }
                }
            }
            if (week)
            {
                tmpBuf.m_picOrg[compID][i + j * stride] = 0;
            }
        }
    }

    buff->copyFromFrame(&tmpBuf);
    tmpBuf.destroy();

    iter++;

    iter = erosion(buff, bitDepth, compID, numIter, iter);

    return iter;
}

void FGAnalyser::init(x265_param* m_param)
{
    param = m_param;
    m_numComponents = 1; //HM Decoder supports only Luma
    m_log2ScaleFactor = 2;
    // HM allows only Luma
    for (int i = 0; i < MAX_NUM_COMPONENT; i++)
    {
        m_compModel[i].bPresentFlag           = true;
        m_compModel[i].numModelValues        = 1;
        m_compModel[i].m_filmGrainNumIntensityIntervalMinus1 = 0;
        m_compModel[i].intensityValues = (x265_FilmGrainCharacteristics::CompModelIntensityValues *)malloc(sizeof(x265_FilmGrainCharacteristics::CompModelIntensityValues) * MAX_NUM_INTENSITIES);
        for (int j = 0; j < MAX_NUM_INTENSITIES; j++)
        {
            m_compModel[i].intensityValues[j].intensityIntervalLowerBound = 10;
            m_compModel[i].intensityValues[j].intensityIntervalUpperBound = 250;
            m_compModel[i].intensityValues[j].compModelValue = (int *)malloc(sizeof(int) * MAX_NUM_INTENSITIES);
            for (int k = 0; k < m_compModel[i].numModelValues; k++)
            {
                // half intensity for chroma. Provided value is default value, manually tuned.
                m_compModel[i].intensityValues[j].compModelValue[k] = (i == 0 ? 26 : 13);
            }
        }
    }

    // initialize picture parameters and create buffers
    inputChroma     = m_param->internalCsp;
    m_bitDepthsIn   = m_param->internalBitDepth;

    if (!m_originalBuf)
    {
        m_originalBuf = new PicYuv;
        m_originalBuf->create(m_param);
    }
    if (!m_workingBuf)
    {
        m_workingBuf = new PicYuv;
        m_workingBuf->create(m_param);
    }
    if (!m_maskBuf)
    {
        m_maskBuf = new PicYuv;
        m_maskBuf->create(m_param);
    }
}

void FGAnalyser::initBufs(PicYuv* original, PicYuv *pic)
{
    m_originalBuf->copyFromFrame(original);
    m_workingBuf->copyFromFrame(pic);
    //memcpy(m_workingBuf->m_picBuf[0], m_workingBuf->m_picFilBuf[0], (m_workingBuf->m_picHeight + (2 * m_workingBuf->m_lumaMarginY)) * m_workingBuf->m_stride * sizeof(pixel));
    //m_workingBuf->m_picOrg[0] = m_workingBuf->m_picBuf[0] + m_workingBuf->m_lumaMarginY * m_workingBuf->m_stride + m_workingBuf->m_lumaMarginX;
    //extendPicBorder(m_workingBuf->m_picOrg[0], m_workingBuf->m_stride, m_workingBuf->m_picWidth, m_workingBuf->m_picHeight, m_workingBuf->m_lumaMarginX, m_workingBuf->m_lumaMarginY);
    //if( m_workingBuf->m_picCsp != X265_CSP_I400)
    //{
    //    memcpy(m_workingBuf->m_picBuf[1], m_workingBuf->m_picFilBuf[1], (m_workingBuf->m_picHeightC + (2 * m_workingBuf->m_chromaMarginY)) * m_workingBuf->m_strideC * sizeof(pixel));
    //    m_workingBuf->m_picOrg[1] = m_workingBuf->m_picBuf[1] + m_workingBuf->m_chromaMarginY * m_workingBuf->m_strideC + m_workingBuf->m_chromaMarginX;
    //    extendPicBorder(m_workingBuf->m_picOrg[1], m_workingBuf->m_strideC, m_workingBuf->m_picWidthC, m_workingBuf->m_picHeightC, m_workingBuf->m_chromaMarginX, m_workingBuf->m_chromaMarginY);
    //    memcpy(m_workingBuf->m_picBuf[2], m_workingBuf->m_picFilBuf[2], (m_workingBuf->m_picHeightC + (2 * m_workingBuf->m_chromaMarginY)) * m_workingBuf->m_strideC * sizeof(pixel));
    //    m_workingBuf->m_picOrg[2] = m_workingBuf->m_picBuf[2] + m_workingBuf->m_chromaMarginY * m_workingBuf->m_strideC + m_workingBuf->m_chromaMarginX;
    //    extendPicBorder(m_workingBuf->m_picOrg[2], m_workingBuf->m_strideC, m_workingBuf->m_picWidthC, m_workingBuf->m_picHeightC, m_workingBuf->m_chromaMarginX, m_workingBuf->m_chromaMarginY);
    //}
    findMask();
}

void FGAnalyser::suppressLowIntensity(const PicYuv *buff1, PicYuv *buff2, unsigned int bitDepth, uint8_t compID)
{
    // buff1 - intensity values ( luma or chroma samples); buff2 - mask

    int width                 = (!compID) ? buff2->m_picWidth : buff2->m_picWidthC;
    int height                = (!compID) ? buff2->m_picHeight : buff2->m_picHeightC;
    int stride                = (!compID ? buff2->m_stride : buff2->m_strideC);
    pixel maxIntensity          = ((pixel) 1 << bitDepth) - 1;
    pixel lowIntensityThreshold = (pixel)(m_lowIntensityRatio * maxIntensity);

    // strong, week, supressed
    pixel* src= buff1->m_picOrg[compID];
    pixel* dst= buff2->m_picOrg[compID];
    for (int i = 0; i < width; i++)
    {
        for (int j = 0; j < height; j++)
        {
            if (src[j * stride + i] < lowIntensityThreshold)
            {
                dst[j * stride + i] = maxIntensity;
            }
        }
    }
}

void FGAnalyser::upsample(const PicYuv &input, PicYuv &output, uint8_t compID, const int factor, const int padding) const
{
  // binary mask upsampling
  // use simple replication of pixels

  int inputWidth                 = (!compID) ? input.m_picWidth : input.m_picWidthC;
  int inputHeight                = (!compID) ? input.m_picHeight : input.m_picHeightC;
  int outputWidth                 = (!compID) ? output.m_picWidth : output.m_picWidthC;
  int outputHeight                = (!compID) ? output.m_picHeight : output.m_picHeightC;
  int inputStride       = (!compID ? input.m_stride : input.m_strideC);
  int outputStride       = (!compID ? output.m_stride : output.m_strideC);

  pixel *dst = output.m_picOrg[compID];
  for (int64_t i = 0; i < inputWidth; i++)
  {
    for (int64_t j = 0; j < inputHeight; j++)
    {
      pixel currentPel = input.m_picOrg[compID][j * inputStride + i];

      for (int64_t x = 0; x < factor; x++)
      {
        for (int64_t y = 0; y < factor; y++)
        {
            int id = (j * factor + y) * outputStride + (i * factor + x);
            dst[id] = currentPel;
        }
      }
    }
  }

  if (padding)
  {
      extendPicBorder(dst, outputStride, outputWidth, outputHeight, padding, padding);
  }
}

void FGAnalyser::combineMasks(PicYuv &buff1, PicYuv &buff2, uint8_t compID)
{
    int width   = (!compID) ? buff1.m_picWidth : buff1.m_picWidthC;
    int height  = (!compID) ? buff1.m_picHeight : buff1.m_picHeightC;
    int64_t stride  = (!compID ? buff1.m_stride : buff1.m_strideC);


    pixel *dst = buff1.m_picOrg[compID];
    pixel *src = buff2.m_picOrg[compID];

    for (int64_t i = 0; i < width; i++)
    {
        for (int64_t j = 0; j < height; j++)
        {
            int64_t id = j * stride + i;
            dst[id] = (dst[id] | src[id]);
        }
    }
}

void FGAnalyser::findMask()
{

    int bitDepth = m_workingBuf->m_param->internalBitDepth;

    // create tmp buffs
    PicYuv *workingBufSubsampled2 = new PicYuv;
    PicYuv *maskSubsampled2       = new PicYuv;
    PicYuv *workingBufSubsampled4 = new PicYuv;
    PicYuv *maskSubsampled4       = new PicYuv;
    PicYuv *maskUpsampled         = new PicYuv;

    x265_param param2, param4;

    memcpy(&param2, param, sizeof(x265_param));
    param2.sourceWidth = m_workingBuf->m_param->sourceWidth / 2;
    param2.sourceHeight = m_workingBuf->m_param->sourceHeight / 2;
    memcpy(&param4, param, sizeof(x265_param));
    param4.sourceWidth = m_workingBuf->m_param->sourceWidth / 4;
    param4.sourceHeight = m_workingBuf->m_param->sourceHeight / 4;

    workingBufSubsampled2->create(&param2);
    maskSubsampled2->create(&param2);
    workingBufSubsampled4->create(&param4);
    maskSubsampled4->create(&param4);
    maskUpsampled->create(m_workingBuf->m_param);
    for (uint8_t compID = 0; compID < m_numComponents; compID++)
    {
        int width =  (!compID ? m_workingBuf->m_picWidth : m_workingBuf->m_picWidthC);
        int height     = (!compID ? m_workingBuf->m_picHeight : m_workingBuf->m_picHeightC);
        int newWidth2  = width / 2;
        int newHeight2 = height / 2;
        int newWidth4  = width / 4;
        int newHeight4 = height / 4;
        int srcStride = (!compID ? m_workingBuf->m_stride : m_workingBuf->m_strideC);
        int dest1Stride = (!compID ? workingBufSubsampled2->m_stride : workingBufSubsampled2->m_strideC);
        int dest2Stride = (!compID ? workingBufSubsampled4->m_stride : workingBufSubsampled4->m_strideC);
        int marginX2 = (!compID ? workingBufSubsampled2->m_lumaMarginX : workingBufSubsampled2->m_chromaMarginX);
        int marginY2 = (!compID ? workingBufSubsampled2->m_lumaMarginY : workingBufSubsampled2->m_chromaMarginY);
        int marginX4 = (!compID ? workingBufSubsampled4->m_lumaMarginX : workingBufSubsampled4->m_chromaMarginX);
        int marginY4 = (!compID ? workingBufSubsampled4->m_lumaMarginY : workingBufSubsampled4->m_chromaMarginY);

        primitives.frameSubSampleLuma(m_workingBuf->m_picOrg[compID], workingBufSubsampled2->m_picOrg[compID], srcStride, dest1Stride, newWidth2, newHeight2);
        extendPicBorder(workingBufSubsampled2->m_picOrg[compID], dest1Stride, newWidth2, newHeight2, marginX2, marginY2);

        primitives.frameSubSampleLuma(workingBufSubsampled2->m_picOrg[compID], workingBufSubsampled4->m_picOrg[compID], dest1Stride, dest2Stride, newWidth4, newHeight4);
        extendPicBorder(workingBufSubsampled4->m_picOrg[compID], dest2Stride, newWidth4, newHeight4, marginX4, marginY4);

        m_maskBuf->copyFromFrame(m_workingBuf); // initialize mask buffer
        maskSubsampled2->copyFromFrame(workingBufSubsampled2); // initialize mask buffer
        maskSubsampled4->copyFromFrame(workingBufSubsampled4); // initialize mask buffer
    }
    for (uint8_t compID = 0; compID < m_numComponents; compID++)
    {
        // full resolution
        m_edgeDetector.detect_edges(m_workingBuf, m_maskBuf, bitDepth, compID);
        suppressLowIntensity(m_workingBuf, m_maskBuf, bitDepth, compID);
        m_morphOperation.dilation(m_maskBuf, bitDepth, compID, 4);

        // subsampled 2
        m_edgeDetector.detect_edges(workingBufSubsampled2, maskSubsampled2, bitDepth, compID);
        suppressLowIntensity(workingBufSubsampled2, maskSubsampled2, bitDepth, compID);
        m_morphOperation.dilation(maskSubsampled2, bitDepth, compID, 3);

        // upsample, combine maskBuf and maskUpsampled
        upsample(*maskSubsampled2, *maskUpsampled, compID, 2);
        combineMasks(*m_maskBuf, *maskUpsampled, compID);

        // subsampled 4
        m_edgeDetector.detect_edges(workingBufSubsampled4, maskSubsampled4, bitDepth, compID);
        suppressLowIntensity(workingBufSubsampled4, maskSubsampled4, bitDepth, compID);
        m_morphOperation.dilation(maskSubsampled4, bitDepth, compID, 2);

        // upsample, combine maskBuf and maskUpsampled
        upsample(*maskSubsampled4, *maskUpsampled, compID, 4);
        combineMasks(*m_maskBuf, *maskUpsampled, compID);

        // final dilation to fill the holes + erosion
        // m_morphOperation.erosion  (maskBuf, bitDepth, compID, 1);
        m_morphOperation.dilation(m_maskBuf, bitDepth, compID, 2);
        m_morphOperation.erosion(m_maskBuf, bitDepth, compID, 1);
    }
    workingBufSubsampled2->destroy();
    maskSubsampled2->destroy();
    workingBufSubsampled4->destroy();
    maskSubsampled4->destroy();
    maskUpsampled->destroy();

    delete workingBufSubsampled2;
    workingBufSubsampled2 = NULL;
    delete maskSubsampled2;
    maskSubsampled2 = NULL;
    delete workingBufSubsampled4;
    workingBufSubsampled4 = NULL;
    delete maskSubsampled4;
    maskSubsampled4 = NULL;
    delete maskUpsampled;
    maskUpsampled = NULL;
}

inline void subtract(int16_t* dest, pixel* src1, pixel* src2, int height, int width, int stride)
{
    for(int64_t i = 0; i< height; i++)
        for(int64_t j = 0; j < width; j++)
           dest[i * stride + j] = src1[i * stride + j] - src2[i * stride + j];
}

void FGAnalyser::estimate_grain_parameters()
{
    PicYuv *tmpBuff = new PicYuv;   // tmpBuff is diference between original and filtered => film grain estimate
    tmpBuff->create(param);
    tmpBuff->copyFromFrame(m_workingBuf);   // workingBuf is filtered image
    tmpBuff->subtract(m_originalBuf);   // find difference between original and filtered/reconstructed frame => film grain estimate

    int blockSize = BLK_8;
    // uint32_t picSizeInLumaSamples = m_workingBuf->m_picWidth * m_workingBuf->m_picHeight;
    // if (picSizeInLumaSamples <= (1920 * 1080))
    // {
    //     blockSize = BLK_8;
    // }
    // else if (picSizeInLumaSamples <= (3840 * 2160))
    // {
    //     blockSize = BLK_16;
    // }
    // else
    // {
    //     blockSize = BLK_32;
    // }

    //HM decoder supports only addition of Film Grains in the Luma
    for (int compID = 0; compID < m_numComponents; compID++)
    {   // loop over components
        int width        = (!compID ? m_workingBuf->m_picWidth : m_workingBuf->m_picWidthC);    // Width of current frame
        int height       = (!compID ? m_workingBuf->m_picHeight : m_workingBuf->m_picHeightC);  // Height of current frame
        int stride       = (!compID ? m_workingBuf->m_stride : m_workingBuf->m_strideC);        // Height of current frame
        int windowSize   = 16;                                                      // Size for Film Grain block
        int bitDepth     = m_workingBuf->m_param->internalBitDepth;
        int detect_edges = 0;
        int mean         = 0;
        int var          = 0;

        std::vector<int>       vec_mean;
        std::vector<int>       vec_var;
        std::vector<PelMatrix> squared_dct_grain_block_list;
        std::vector<int>       vec_mean_cutoff;

        for (int i = 0; i <= width - windowSize; i += windowSize)
        {
            // loop over windowSize x windowSize blocks
            for (int j = 0; j <= height - windowSize; j += windowSize)
            {
                detect_edges = count_edges(m_maskBuf, windowSize, compID, i, j);   // for flat region without edges

                if (detect_edges)   // selection of uniform, flat and low-complexity area; extend to other features, e.g., variance.
                {
                    mean = meanVar(m_workingBuf->m_picOrg[compID], windowSize, stride, i, j, false);
                    var  = meanVar(tmpBuff->m_picDif[compID], windowSize, stride, i, j, true);
                    double tmp = 3.0 * pow((double)(var), .5) + .5;
                    var = (int)tmp;
                    // find transformed blocks; cut-off frequency estimation is done on 64 x 64 blocks as low-pass filtering on 
                    // synthesis side is done on 64 x 64 blocks.
                    if (var < (MAX_REAL_SCALE << (bitDepth - BIT_DEPTH_8))>>1) // limit data points to meaningful values. 
                                                                            // higher variance can be result of not perfect mask
                                                                            // estimation (non-flat regions fall in estimation process)
                    {
                        block_transform(tmpBuff->m_picDif[compID], squared_dct_grain_block_list, i, j, bitDepth, stride, windowSize);
                        vec_mean_cutoff.push_back(mean);
                    }
                }
                int step = windowSize / blockSize;
                for (int k = 0; k < step; k++)
                {
                    for (int m = 0; m < step; m++)
                    {
                        detect_edges = count_edges(m_maskBuf, blockSize, compID, i + k * blockSize, j + m * blockSize);   // for flat region
                                                                                                                          // without edges

                        if (detect_edges)   // selection of uniform, flat and low-complexity area; extend to other features, e.g., variance.
                        {
                            // collect all data for parameter estimation; mean and variance are caluclated on blockSize x blockSize blocks
                            mean = meanVar(m_workingBuf->m_picOrg[compID], blockSize, stride, i + k * blockSize, j + m * blockSize, false);
                            var  = meanVar(tmpBuff->m_picDif[compID], blockSize, stride, i + k * blockSize, j + m * blockSize, true);
                            // regularize high variations; controls excessively fluctuating points
                            double tmp = 3.0 * pow((double)(var), .5) + .5;
                            var = (int)tmp;
                            if (var < (MAX_REAL_SCALE << (bitDepth - BIT_DEPTH_8))) // limit data points to meaningful values. 
                                                                                    // higher variance can be result of not perfect mask
                                                                                    // estimation (non-flat regions fall in estimation process)
                            {
                                vec_mean.push_back(mean);   // mean of the filtered frame
                                vec_var.push_back(var);     // variance of the film grain estimate
                            }
                        }
                    }
                }
            }
        }
        // calculate film grain parameters
        estimate_scaling_factors(vec_mean, vec_var, bitDepth, compID);
        estimate_cutoff_freq(squared_dct_grain_block_list, vec_mean_cutoff, bitDepth, compID, windowSize);
    }

    tmpBuff->destroy();
    delete tmpBuff;
    tmpBuff = NULL;
}

// find compModelValue[0] - different scaling based on the pixel intensity
void FGAnalyser::estimate_scaling_factors(std::vector<int> &data_x, std::vector<int> &data_y, unsigned int bitDepth, uint8_t compID)
{
    if (!m_compModel[compID].bPresentFlag || data_x.size() < MIN_POINTS_FOR_INTENSITY_ESTIMATION)   // if cutoff frequencies are not 
                                                                                                    // estimated previously, do not 
                                                                                                    // proceed since presentFlag is set 
                                                                                                    // to false in a previous step
    {
        return;     // also if there is no enough points to estimate film grain intensities, default or previously estimated
                    // parameters are used
    }

    // estimate intensity regions
    std::vector<double> coeffs;
    std::vector<double> scalingVec;
    std::vector<int>    quantVec;
    double              distortion = 0.0;

    // Fit the points with the curve. Quantization of the curve using Lloyd Max quantization.
    bool valid;
    for (int i = 0; i < NUM_PASSES; i++)   // if num_passes = 2, filtering of the dataset points is performed
    {
        valid = fit_function(data_x, data_y, coeffs, scalingVec, ORDER, bitDepth, i, compID);   // n-th order polynomial regression for 
                                                                                        // scaling function estimation
        if (!valid)
        {
            break;
        }
    }
    if (valid)
    {
        //avg_scaling_vec(scalingVec, compID, bitDepth);   // scale with previously fitted function to smooth the intensity
        valid = lloyd_max(scalingVec, quantVec, distortion, QUANT_LEVELS, bitDepth);   // train quantizer and quantize curve using Lloyd Max
    }

    // Based on quantized intervals, set intensity region and scaling parameter
    if (valid)   // if not valid, reuse previous parameters (for example, if var is all zero)
    {
        setEstimatedParameters(quantVec, bitDepth, compID);
    }
}

// Horizontal and Vertical cutoff frequencies estimation. Assumption is that for complete
// sequence there is only one set of the cut-off frequencies (implementation decision)
void FGAnalyser::estimate_cutoff_freq(const std::vector<PelMatrix> &blocks, std::vector<int> vec_mean, int bitDepth, int compID, int windowSize)
{
    int num_blocks = (int) blocks.size();
    if (num_blocks < MIN_BLOCKS_FOR_CUTOFF_ESTIMATION || m_compModel[compID].bPresentFlag == false)
    {
        return;
    }

    int intervals = m_compModel[compID].m_filmGrainNumIntensityIntervalMinus1 + 1;
    std::vector<PelMatrixDouble> mean_squared_dct_grain(intervals, PelMatrixDouble(windowSize, std::vector<double>(windowSize, 0.0)));
    std::vector<std::vector<double>> vec_mean_dct_grain_row(intervals, std::vector<double>(windowSize, 0.0));
    std::vector<std::vector<double>> vec_mean_dct_grain_col(intervals, std::vector<double>(windowSize, 0.0));

    static bool     isFirstCutoffEst[MAX_NUM_COMPONENT] = {true, true, true };
    static int      compModelValue_1 = 0;
    static int      compModelValue_2 = 0;

    int16_t intensityInterval[MAX_NUM_INTENSITIES];
    memset(intensityInterval, -1, sizeof(intensityInterval));
    for (int intensityCtr = 0; intensityCtr < intervals; intensityCtr++)
    {
        for (int multiGrainCtr = m_compModel[compID].intensityValues[intensityCtr].intensityIntervalLowerBound;
            multiGrainCtr <= m_compModel[compID].intensityValues[intensityCtr].intensityIntervalUpperBound; multiGrainCtr++)
        {
            intensityInterval[multiGrainCtr] = intensityCtr;
        }
    }

    int intervalIdx;
    std::vector<int> num_blocks_per_interval(intervals, 0);
    for (int i = 0; i < blocks.size(); i++)
    {
        intervalIdx = intensityInterval[vec_mean[i] >> (bitDepth - BIT_DEPTH_8)];
        if (intervalIdx != -1)
        {
            num_blocks_per_interval[intervalIdx]++;
        }
    }
    // iterate over the block and find avarage block
    for (int x = 0; x < windowSize; x++)
    {
        for (int y = 0; y < windowSize; y++)
        {
            for (int i = 0; i < blocks.size(); i++)
            {
                intervalIdx = intensityInterval[vec_mean[i] >> (bitDepth - BIT_DEPTH_8)];
                if (intervalIdx != -1)
                {
                    mean_squared_dct_grain[intervalIdx][y][x] += blocks[i][y][x];
                }
            }
            for (int i = 0; i<intervals; i++)
            {
                if (num_blocks_per_interval[i] != 0)
                    mean_squared_dct_grain[i][x][y] /= num_blocks_per_interval[i];
                vec_mean_dct_grain_row[i][x] += ((x != 0) || (y != 0)) ? mean_squared_dct_grain[i][x][y] : 0.0;
                vec_mean_dct_grain_col[i][y] += ((x != 0) || (y != 0)) ? mean_squared_dct_grain[i][x][y] : 0.0;
            }
        }
    }

    for (int i = 0; i < intervals; i++)
    {
        for (int x = 0; x < windowSize; x++)
        {
            vec_mean_dct_grain_row[i][x] /= (x == 0) ? windowSize - 1 : windowSize;
            vec_mean_dct_grain_col[i][x] /= (x == 0) ? windowSize - 1 : windowSize;
        }

        int cutoff_vertical   = cutoff_frequency(vec_mean_dct_grain_row[i], windowSize);
        int cutoff_horizontal = cutoff_frequency(vec_mean_dct_grain_col[i], windowSize);

        m_compModel[compID].numModelValues = 3; // we always write all 3 parameters. overhead is anyway small
        if (cutoff_horizontal != -1)
            m_compModel[compID].intensityValues[i].compModelValue[1] = cutoff_horizontal;
        if (cutoff_vertical != -1)
            m_compModel[compID].intensityValues[i].compModelValue[2] = cutoff_vertical;

    }
}

int FGAnalyser::cutoff_frequency(std::vector<double> &mean, int windowSize)
{
    std::vector<double> sum(windowSize, 0.0);

    // Regularize the curve to suppress peaks
    mean.push_back(mean.back());
    mean.insert(mean.begin(), mean.front());
    for (int j = 1; j < windowSize + 1; j++)
    {
        sum[j - 1] = (m_tapFilter[0] * mean[j - 1] + m_tapFilter[1] * mean[j] + m_tapFilter[2] * mean[j + 1]) / m_normTap;
    }

    double target = 0;
    for (int j = 0; j < windowSize; j++)
    {
        target += sum[j];
    }
    target /= windowSize;

    // find final cut-off frequency
    std::vector<int> intersectionPointList;

    for (int x = 0; x < windowSize - 1; x++)
    {
        if ((target < sum[x] && target >= sum[x + 1]) || (target > sum[x] && target <= sum[x + 1]))
        {   // there is intersection
            double first_point = fabs(target - sum[x]);
            double second_point = fabs(target - sum[x + 1]);
        if (first_point < second_point)
        {
            intersectionPointList.push_back(x);
        }
        else
        {
            intersectionPointList.push_back(x + 1);
        }
        }
    }

    int size = (int)intersectionPointList.size();

    if (size > 0)
    {
        return x265_clip3(2, 14, (intersectionPointList[size - 1] - 1));   // clip to RDD5 range
    }
    else
    {
        return -1;
    }
}

// DCT-2 64x64 as defined in VVC
void FGAnalyser::block_transform(int16_t *buff, std::vector<PelMatrix> &squared_dct_grain_block_list, int offsetX, int offsetY, unsigned int bitDepth, int stride, int windowSize)
{
    int log2WindowSize    = 4;               // Size for Film Grain block
    int max_dynamic_range = (1 << (bitDepth + log2WindowSize)) - 1;   // Dynamic range after DCT transform for 64x64 block
    int min_dynamic_range = -((1 << (bitDepth + log2WindowSize)) - 1);
    int sum = 0;

    const TMatrixCoeff *tmp            = g_trCoreDCT2P64[TRANSFORM_FORWARD][0];
    const int           transform_scale_1st = 8;   // upscaling of original transform as specified in VVC (for 64x64 block)
    const int add_1st = 1 << (transform_scale_1st - 1);
    const int           transform_scale_2nd = 8; // upscaling of original transform as specified in VVC (for windowSize x windowSize block)
    const int           add_2nd = 1 << (transform_scale_2nd - 1);

    std::vector<std::vector<TMatrixCoeff>> tr(windowSize, std::vector<TMatrixCoeff>(windowSize));  // Original
    std::vector<std::vector<TMatrixCoeff>> trt(windowSize, std::vector<TMatrixCoeff>(windowSize)); // Transpose
    for (int x = 0; x < windowSize; x++)
    {
        for (int y = 0; y < windowSize; y++)
        {
            tr[x][y]  = tmp[x * windowSize + y]; /* Matrix Original */
            trt[y][x] = tmp[x * windowSize + y]; /* Matrix Transpose */
        }
    }

    // DCT transform
    PelMatrix blockDCT(windowSize, std::vector<int>(windowSize));
    PelMatrix blockTmp(windowSize, std::vector<int>(windowSize));

    for (int x = 0; x < windowSize; x++)
    {
        for (int y = 0; y < windowSize; y++)
        {
            sum = 0;
            for (int k = 0; k < windowSize; k++)
            {
                int idx = (offsetY + y) * stride + (offsetX + k);
                sum += tr[x][k] * buff[idx];
            }
            blockTmp[x][y] = (sum + add_1st) >> transform_scale_1st;
        }
    }

    for (int x = 0; x < windowSize; x++)
    {
        for (int y = 0; y < windowSize; y++)
        {
            sum = 0;
            for (int k = 0; k < windowSize; k++)
            {
                sum += blockTmp[x][k] * trt[k][y];
            }
            blockDCT[x][y] = x265_clip3(min_dynamic_range, max_dynamic_range, (sum + add_1st) >> transform_scale_2nd);
        }
    }

    for (int x = 0; x < windowSize; x++)
    {
        for (int y = 0; y < windowSize; y++)
        {
            blockDCT[x][y] = blockDCT[x][y] * blockDCT[x][y];
        }
    }

    // store squared transformed block for further analysis
    squared_dct_grain_block_list.push_back(blockDCT);
}

// check edges
int FGAnalyser::count_edges(PicYuv *buffer, int windowSize, uint8_t compID, int offsetX, int offsetY)
{
    for (int x = 0; x < windowSize; x++)
    {
        for (int y = 0; y < windowSize; y++)
        {
            int stride = !compID ? buffer->m_stride : buffer->m_strideC;
            int idx = (offsetY + y) * stride + (offsetX + x);
            if (buffer->m_picOrg[compID][idx])
            {
                return 0;
            }
        }
    }
    return 1;
}

// calulate mean and variance for windowSize x windowSize block
template <typename T>
int FGAnalyser::meanVar(T *buffer, int windowSize, int stride, int offsetX, int offsetY, bool getVar)
{
    double m = 0, v = 0;

    for (int x = 0; x < windowSize; x++)
    {
        for (int y = 0; y < windowSize; y++)
        {
            int idx = (offsetY + y) * stride + (offsetX + x);
            m += buffer[idx];
            v += buffer[idx] * buffer[idx];
        }
    }

    m = m / (windowSize * windowSize);
    if (getVar)
    {
        return (int)(v / (windowSize * windowSize) - m * m + .5);
    }

    return (int)(m + .5);
}

// Fit data to a function using n-th order polynomial interpolation
bool FGAnalyser::fit_function(std::vector<int> &data_x, std::vector<int> &data_y, std::vector<double> &coeffs,
                              std::vector<double> &scalingVec, int order, int bitDepth, bool second_pass, int compID)
{
    PelMatrixLongDouble a(MAXPAIRS + 1, std::vector<long double>(MAXPAIRS + 1));
    PelVectorLongDouble B(MAXPAIRS + 1), C(MAXPAIRS + 1), S(MAXPAIRS + 1);
    long double         A1, A2, Y1, m, S1, x1;
    long double         xscale, yscale;
    long double         xmin = 0.0, xmax = 0.0, ymin = 0.0, ymax = 0.0;
    long double         polycoefs[MAXORDER + 1];

    int i, j, k, L, R;

    // several data filtering and data manipulations before fitting the function
    // create interval points for function fitting
    int              INTENSITY_INTERVAL_NUMBER = (1 << bitDepth) / INTERVAL_SIZE;
    std::vector<int> vec_mean_intensity(INTENSITY_INTERVAL_NUMBER, 0);
    std::vector<int> vec_variance_intensity(INTENSITY_INTERVAL_NUMBER, 0);
    std::vector<int> element_number_per_interval(INTENSITY_INTERVAL_NUMBER, 0);
    std::vector<int> tmp_data_x;
    std::vector<int> tmp_data_y;
    double           mn = 0.0, sd = 0.0;

    if (second_pass)   // in second pass, filter based on the variance of the data_y. remove all high and low points
    {
        xmin = scalingVec.back();
        scalingVec.pop_back();
        xmax = scalingVec.back();
        scalingVec.pop_back();
        int n = (int) data_y.size();
        if (n != 0)
        {
            mn = accumulate(data_y.begin(), data_y.end(), 0.0) / n;
            for (int cnt = 0; cnt < n; cnt++)
            {
                sd += (data_y[cnt] - mn) * (data_y[cnt] - mn);
            }
            sd /= n;
            sd = sqrt(sd);
        }
    }

    for (unsigned int cnt = 0; cnt < data_x.size(); cnt++)
    {
        if (second_pass)
        {
            if (data_x[cnt] >= xmin && data_x[cnt] <= xmax)
            {
                if ((data_y[cnt] < scalingVec[data_x[cnt] - (int) xmin] + sd * VAR_SCALE_UP) && (data_y[cnt] > scalingVec[data_x[cnt] - (int) xmin] - sd * VAR_SCALE_DOWN))
                {
                    int block_index = data_x[cnt] / INTERVAL_SIZE;
                    vec_mean_intensity[block_index] += data_x[cnt];
                    vec_variance_intensity[block_index] += data_y[cnt];
                    element_number_per_interval[block_index]++;
                }
            }
        }
        else
        {
            int block_index = data_x[cnt] / INTERVAL_SIZE;
            vec_mean_intensity[block_index] += data_x[cnt];
            vec_variance_intensity[block_index] += data_y[cnt];
            element_number_per_interval[block_index]++;
        }
    }
    if (!stored_vec_mean_intensity[compID].empty() && !stored_vec_variance_intensity[compID].empty())
    {
        for (int block_idx = 0; block_idx < INTENSITY_INTERVAL_NUMBER; block_idx++)
        {
            element_number_per_interval[block_idx] += stored_element_number_per_interval[compID][block_idx];
            vec_mean_intensity[block_idx] += stored_vec_mean_intensity[compID][block_idx];
            vec_variance_intensity[block_idx] += stored_vec_variance_intensity[compID][block_idx];
        }
    }

    // create a points per intensity interval
    for (int block_idx = 0; block_idx < INTENSITY_INTERVAL_NUMBER; block_idx++)
    {
        if (element_number_per_interval[block_idx] >= MIN_ELEMENT_NUMBER_PER_INTENSITY_INTERVAL)
        {
            tmp_data_x.push_back(vec_mean_intensity[block_idx] / element_number_per_interval[block_idx]);
            tmp_data_y.push_back(vec_variance_intensity[block_idx] / element_number_per_interval[block_idx]);
        }
    }

    if (second_pass)
    {
        // save data for fitting function in the next frames (to get better estimation by agregating estimation over different frames)
        stored_vec_mean_intensity[compID].resize(0);
        stored_vec_variance_intensity[compID].resize(0);
        stored_element_number_per_interval[compID].resize(0);
        stored_vec_mean_intensity[compID] = vec_mean_intensity;
        stored_vec_variance_intensity[compID] = vec_variance_intensity;
        stored_element_number_per_interval[compID] = element_number_per_interval;
    }

    // There needs to be at least ORDER+1 points to fit the function
    if ((int)tmp_data_x.size() < (order + 1))
    {
        return false;   // if there is no enough blocks to estimate film grain parameters, default or previously estimated
                        // parameters are used
    }

    for (i = 0; i < (int)tmp_data_x.size(); i++) // remove single points before extending and fitting
    {
        int check = 0;
        for (j = -WINDOW; j <= WINDOW; j++)
        {
            int idx = i + j;
            if (idx >= 0 && idx < (int)tmp_data_x.size() && j != 0)
            {
                check += abs(tmp_data_x[i] / INTERVAL_SIZE - tmp_data_x[idx] / INTERVAL_SIZE) <= WINDOW ? 1 : 0;
            }
        }

        if (check < NBRS)
        {
            tmp_data_x.erase(tmp_data_x.begin() + i);
            tmp_data_y.erase(tmp_data_y.begin() + i);
            i--;
        }
    }

    extend_points(tmp_data_x, tmp_data_y, bitDepth);   // find the most left and the most right point, and extend edges

    X265_CHECK(tmp_data_x.size() <= MAXPAIRS, "Maximum dataset size exceeded.");

    // fitting the function starts here
    xmin = tmp_data_x[0];
    xmax = tmp_data_x[0];
    ymin = tmp_data_y[0];
    ymax = tmp_data_y[0];
    for (i = 0; i < (int)tmp_data_x.size(); i++)
    {
        if (tmp_data_x[i] < xmin)
        {
            xmin = tmp_data_x[i];
        }
        if (tmp_data_x[i] > xmax)
        {
            xmax = tmp_data_x[i];
        }
        if (tmp_data_y[i] < ymin)
        {
            ymin = tmp_data_y[i];
        }
        if (tmp_data_y[i] > ymax)
        {
            ymax = tmp_data_y[i];
        }
    }

    long double xlow = xmax;
    long double ylow = ymax;

    int data_pairs = (int) tmp_data_x.size();

    PelMatrixDouble data_array(2, std::vector<double>(MAXPAIRS + 1));

    for (i = 0; i < data_pairs; i++)
    {
        data_array[0][i + 1] = (double) tmp_data_x[i];
        data_array[1][i + 1] = (double) tmp_data_y[i];
    }

    // release memory for data_x and data_y, and clear previous vectors
    std::vector<int>().swap(tmp_data_x);
    std::vector<int>().swap(tmp_data_y);
    if (second_pass)
    {
        std::vector<int>().swap(data_x);
        std::vector<int>().swap(data_y);
        std::vector<double>().swap(coeffs);
        std::vector<double>().swap(scalingVec);
    }

    for (i = 1; i <= data_pairs; i++)
    {
        if (data_array[0][i] < xlow && data_array[0][i] != 0)
        {
            xlow = data_array[0][i];
        }
        if (data_array[1][i] < ylow && data_array[1][i] != 0)
        {
            ylow = data_array[1][i];
        }
    }

    if (xlow < .001 && xmax < 1000)
    {
        xscale = 1 / xlow;
    }
    else if (xmax > 1000 && xlow > .001)
    {
        xscale = 1 / xmax;
    }
    else
    {
        xscale = 1;
    }

    if (ylow < .001 && ymax < 1000)
    {
        yscale = 1 / ylow;
    }
    else if (ymax > 1000 && ylow > .001)
    {
        yscale = 1 / ymax;
    }
    else
    {
        yscale = 1;
    }

    // initialise array variables
    for (i = 0; i <= MAXPAIRS; i++)
    {
        B[i] = 0;
        C[i] = 0;
        S[i] = 0;
        for (j = 0; j < MAXPAIRS; j++)
        {
            a[i][j] = 0;
        }
    }

    for (i = 0; i <= MAXORDER; i++)
    {
        polycoefs[i] = 0;
    }

    Y1 = 0;
    for (j = 1; j <= data_pairs; j++)
    {
        for (i = 1; i <= order; i++)
        {
            B[i] = B[i] + data_array[1][j] * yscale * ldpow(data_array[0][j] * xscale, i);
            if (B[i] == LDBL_MAX)
            {
                return false;
            }
            for (k = 1; k <= order; k++)
            {
                a[i][k] = a[i][k] + ldpow(data_array[0][j] * xscale, (i + k));
                if (a[i][k] == LDBL_MAX)
                {
                    return false;
                }
            }
            S[i] = S[i] + ldpow(data_array[0][j] * xscale, i);
            if (S[i] == LDBL_MAX)
            {
                return false;
            }
        }
        Y1 = Y1 + data_array[1][j] * yscale;
        if (Y1 == LDBL_MAX)
        {
            return false;
        }
    }

    for (i = 1; i <= order; i++)
    {
        for (j = 1; j <= order; j++)
        {
            a[i][j] = a[i][j] - S[i] * S[j] / (long double) data_pairs;
            if (a[i][j] == LDBL_MAX)
            {
                return false;
            }
        }
        B[i] = B[i] - Y1 * S[i] / (long double) data_pairs;
        if (B[i] == LDBL_MAX)
        {
            return false;
        }
    }

    for (k = 1; k <= order; k++)
    {
        R  = k;
        A1 = 0;
        for (L = k; L <= order; L++)
        {
            A2 = fabsl(a[L][k]);
            if (A2 > A1)
            {
                A1 = A2;
                R  = L;
            }
        }
        if (A1 == 0)
        {
            return false;
        }
        if (R != k)
        {
            for (j = k; j <= order; j++)
            {
                x1      = a[R][j];
                a[R][j] = a[k][j];
                a[k][j] = x1;
            }
            x1   = B[R];
            B[R] = B[k];
            B[k] = x1;
        }
        for (i = k; i <= order; i++)
        {
            m = a[i][k];
            for (j = k; j <= order; j++)
            {
                if (i == k)
                {
                    a[i][j] = a[i][j] / m;
                }
                else
                {
                    a[i][j] = a[i][j] - m * a[k][j];
                }
            }
            if (i == k)
            {
                B[i] = B[i] / m;
            }
            else
            {
                B[i] = B[i] - m * B[k];
            }
        }
    }

    polycoefs[order] = B[order];
    for (k = 1; k <= order - 1; k++)
    {
        i  = order - k;
        S1 = 0;
        for (j = 1; j <= order; j++)
        {
            S1 = S1 + a[i][j] * polycoefs[j];
            if (S1 == LDBL_MAX)
            {
                return false;
            }
        }
        polycoefs[i] = B[i] - S1;
    }

    S1 = 0;
    for (i = 1; i <= order; i++)
    {
        S1 = S1 + polycoefs[i] * S[i] / (long double) data_pairs;
        if (S1 == LDBL_MAX)
        {
            return false;
        }
    }
    polycoefs[0] = (Y1 / (long double) data_pairs - S1);

    // zero all coeficient values smaller than +/- .00000000001 (avoids -0)
    for (i = 0; i <= order; i++)
    {
        if (fabsl(polycoefs[i] * 100000000000) < 1)
        {
            polycoefs[i] = 0;
        }
    }

    // rescale parameters
    for (i = 0; i <= order; i++)
    {
        polycoefs[i] = (1 / yscale) * polycoefs[i] * ldpow(xscale, i);
        coeffs.push_back(polycoefs[i]);
    }

    // create fg scaling function. interpolation based on coeffs which returns lookup table from 0 - 2^B-1. 
    // n-th order polinomial regression
    for (i = (int) xmin; i <= (int) xmax; i++)
    {
        double val = coeffs[0];
        for (j = 1; j < (int)coeffs.size(); j++)
        {
            val += (coeffs[j] * ldpow(i, j));
        }

        val = x265_clip3(0.0, (double) (1 << bitDepth) - 1, val);
        scalingVec.push_back(val);
    }

    // save in scalingVec min and max value for further use
    scalingVec.push_back(xmax);
    scalingVec.push_back(xmin);

    return true;
}

// avg scaling vector with previous result to smooth transition betweeen frames
void FGAnalyser::avg_scaling_vec(std::vector<double> &scalingVec, uint8_t compID, int bitDepth)
{
    int xmin = (int) scalingVec.back();
    scalingVec.pop_back();
    int xmax = (int) scalingVec.back();
    scalingVec.pop_back();

    static std::vector<std::vector<double> > scalingVecAvg(MAX_NUM_COMPONENT, std::vector<double>((int)(1<<bitDepth)));
    static bool                isFirstScalingEst[MAX_NUM_COMPONENT] = { true, true, true };

    if (isFirstScalingEst[compID])
    {
        for (int i = xmin; i <= xmax; i++)
        {
            scalingVecAvg[compID][i] = scalingVec[i - xmin];
        }

        isFirstScalingEst[compID] = false;
    }
    else
    {
        for (unsigned int i = 0; i < scalingVec.size(); i++)
        {
            scalingVecAvg[compID][i + xmin] += scalingVec[i];
        }
        for (unsigned int i = 0; i < scalingVecAvg[compID].size(); i++)
        {
            scalingVecAvg[compID][i] /= 2;
        }
    }

    // re-init scaling vec and add new min and max to be used in other functions
    int index = 0;
    for (; index < (int)scalingVecAvg[compID].size(); index++)
    {
        if (scalingVecAvg[compID][index])
        {
            break;
        }
    }
    xmin = index;

    index = (int) scalingVecAvg[compID].size() - 1;
    for (; index >=0 ; index--)
    {
        if (scalingVecAvg[compID][index])
        {
            break;
        }
    }
    xmax = index;

    scalingVec.resize(xmax - xmin + 1);
    for (int i = xmin; i <= xmax; i++)
    {
        scalingVec[i - xmin] = scalingVecAvg[compID][i];
    }

    scalingVec.push_back(xmax);
    scalingVec.push_back(xmin);
}

// Lloyd Max quantizer
bool FGAnalyser::lloyd_max(std::vector<double> &scalingVec, std::vector<int> &quantizedVec, double &distortion, 
                           int numQuantizedLevels, int bitDepth)
{
    X265_CHECK(scalingVec.size() > 0, "Empty training dataset.");

    int xmin = (int) scalingVec.back();
    scalingVec.pop_back();
    scalingVec.pop_back();   // dummy pop_back ==> int xmax = (int)scalingVec.back();

    double ymin          = 0.0;
    double ymax          = 0.0;
    double init_training = 0.0;
    double tolerance     = 0.0000001;
    double last_distor   = 0.0;
    double rel_distor    = 0.0;

    std::vector<double> codebook(numQuantizedLevels);
    std::vector<double> partition(numQuantizedLevels - 1);

    std::vector<double> tmpVec(scalingVec.size(), 0.0);
    distortion = 0.0;

    ymin = scalingVec[0];
    ymax = scalingVec[0];
    for (int i = 0; i < (int)scalingVec.size(); i++)
    {
        if (scalingVec[i] < ymin)
        {
            ymin = scalingVec[i];
        }
        if (scalingVec[i] > ymax)
        {
            ymax = scalingVec[i];
        }
    }

    init_training = (ymax - ymin) / numQuantizedLevels;

    if (init_training <= 0)
    {
        // msg(WARNING, "Invalid training dataset. Film grain parameter estimation is not performed.
        // Default or previously estimated parameters are reused.\n");
        return false;
    }

    // initial codebook
    double step = init_training / 2;
    for (int i = 0; i < numQuantizedLevels; i++)
    {
        codebook[i] = ymin + i * init_training + step;
    }

    // initial partition
    for (int i = 0; i < numQuantizedLevels - 1; i++)
    {
        partition[i] = (codebook[i] + codebook[i + 1]) / 2;
    }

    // quantizer initialization
    quantize(scalingVec, tmpVec, distortion, partition, codebook);

    double tolerance2 = DBL_EPSILON * ymax;
    if (distortion > tolerance2)
    {
        rel_distor = abs(distortion - last_distor) / distortion;
    }
    else
    {
        rel_distor = distortion;
    }

    // optimization: find optimal codebook and partition
    while ((rel_distor > tolerance) && (rel_distor > tolerance2))
    {
        for (int i = 0; i < numQuantizedLevels; i++)
        {
            int    count = 0;
            double sum   = 0.0;

            for (unsigned int j = 0; j < tmpVec.size(); j++)
            {
                if (codebook[i] == tmpVec[j])
                {
                    count++;
                    sum += scalingVec[j];
                }
            }

            if (count)
            {
                codebook[i] = sum / (double) count;
            }
            else
            {
                sum   = 0.0;
                count = 0;
                if (i == 0)
                {
                    for (unsigned int j = 0; j < tmpVec.size(); j++)
                    {
                        if (scalingVec[j] <= partition[i])
                        {
                            count++;
                            sum += scalingVec[j];
                        }
                    }
                    if (count)
                    {
                        codebook[i] = sum / (double) count;
                    }
                    else
                    {
                        codebook[i] = (partition[i] + ymin) / 2;
                    }
                }
                else if (i == numQuantizedLevels - 1)
                {
                    for (unsigned int j = 0; j < tmpVec.size(); j++)
                    {
                        if (scalingVec[j] >= partition[i - 1])
                        {
                            count++;
                            sum += scalingVec[j];
                        }
                    }
                    if (count)
                    {
                        codebook[i] = sum / (double) count;
                    }
                    else
                    {
                        codebook[i] = (partition[i - 1] + ymax) / 2;
                    }
                }
                else
                {
                    for (unsigned int j = 0; j < tmpVec.size(); j++)
                    {
                        if (scalingVec[j] >= partition[i - 1] && scalingVec[j] <= partition[i])
                        {
                            count++;
                            sum += scalingVec[j];
                        }
                    }
                    if (count)
                    {
                        codebook[i] = sum / (double) count;
                    }
                    else
                    {
                        codebook[i] = (partition[i - 1] + partition[i]) / 2;
                    }
                }
            }
        }

        // compute and sort partition
        for (int i = 0; i < numQuantizedLevels - 1; i++)
        {
            partition[i] = (codebook[i] + codebook[i + 1]) / 2.0;
        }
        std::sort(partition.begin(), partition.end());

        // final quantization - testing condition
        last_distor = distortion;
        quantize(scalingVec, tmpVec, distortion, partition, codebook);

        if (distortion > tolerance2)
        {
            rel_distor = abs(distortion - last_distor) / distortion;
        }
        else
        {
            rel_distor = distortion;
        }
    }

    // fill the final quantized vector
    quantizedVec.resize((int) (1 << bitDepth), 0);
    for (unsigned int i = 0; i < tmpVec.size(); i++)
    {
        quantizedVec[i + xmin] = x265_clip3(0, MAX_STANDARD_DEVIATION << (bitDepth - BIT_DEPTH_8), (int) (tmpVec[i] + .5));
    }

    return true;
}

void FGAnalyser::quantize(std::vector<double> &scalingVec, std::vector<double> &quantizedVec, double &distortion, std::vector<double> partition, std::vector<double> codebook)
{
    X265_CHECK(partition.size() > 0 || codebook.size() > 0, "Check partitions and codebook.");

    // reset previous quantizedVec to 0 and distortion to 0
    std::fill(quantizedVec.begin(), quantizedVec.end(), 0.0);
    distortion = 0.0;

    // quantize input vector
    for (unsigned int i = 0; i < scalingVec.size(); i++)
    {
        for (unsigned int j = 0; j < partition.size(); j++)
        {
            quantizedVec[i] =
            quantizedVec[i] + (scalingVec[i] > partition[j]);   // partition need to be sorted in acceding order
        }
        quantizedVec[i] = codebook[(int) quantizedVec[i]];
    }

    // compute distortion - mse
    for (unsigned int i = 0; i < scalingVec.size(); i++)
    {
        distortion += ((scalingVec[i] - quantizedVec[i]) * (scalingVec[i] - quantizedVec[i]));
    }
    distortion /= scalingVec.size();
}

// Set correctlly SEI parameters based on the quantized curve
void FGAnalyser::setEstimatedParameters(std::vector<int> &quantizedVec, unsigned int bitDepth, uint8_t compID)
{
    std::vector<std::vector<int> > finalIntervalsandScalingFactors(3);   // lower_bound, upper_bound, scaling_factor

    int cutoff_horizontal = 8;
    int cutoff_vertical   = 8;

    // calculate intervals and scaling factors
    define_intervals_and_scalings(finalIntervalsandScalingFactors, quantizedVec, bitDepth);

    // merge small intervals with left or right interval
    for (unsigned int i = 0; i < finalIntervalsandScalingFactors[2].size(); i++)
    {
        int tmp1 = finalIntervalsandScalingFactors[1][i] - finalIntervalsandScalingFactors[0][i];

        if (tmp1 < (2 << (bitDepth - BIT_DEPTH_8)))
        {
            int diffRight =
            (i == (finalIntervalsandScalingFactors[2].size() - 1)) || (finalIntervalsandScalingFactors[2][i + 1] == 0)
                            ? INT_MAX
                            : abs(finalIntervalsandScalingFactors[2][i] - finalIntervalsandScalingFactors[2][i + 1]);
            int diffLeft = (i == 0) || (finalIntervalsandScalingFactors[2][i - 1] == 0)
                            ? INT_MAX
                            : abs(finalIntervalsandScalingFactors[2][i] - finalIntervalsandScalingFactors[2][i - 1]);

            if (diffLeft < diffRight)   // merge with left
            {
                int tmp2     = finalIntervalsandScalingFactors[1][i - 1] - finalIntervalsandScalingFactors[0][i - 1];
                int newScale = (tmp2 * finalIntervalsandScalingFactors[2][i - 1] + tmp1 * finalIntervalsandScalingFactors[2][i]) / (tmp2 + tmp1);

                finalIntervalsandScalingFactors[1][i - 1] = finalIntervalsandScalingFactors[1][i];
                finalIntervalsandScalingFactors[2][i - 1] = newScale;
                for (int j = 0; j < 3; j++)
                {
                    finalIntervalsandScalingFactors[j].erase(finalIntervalsandScalingFactors[j].begin() + i);
                }
                i--;
            }
            else   // merge with right
            {
                int tmp2     = finalIntervalsandScalingFactors[1][i + 1] - finalIntervalsandScalingFactors[0][i + 1];
                int newScale = (tmp2 * finalIntervalsandScalingFactors[2][i + 1] + tmp1 * finalIntervalsandScalingFactors[2][i]) / (tmp2 + tmp1);

                finalIntervalsandScalingFactors[1][i] = finalIntervalsandScalingFactors[1][i + 1];
                finalIntervalsandScalingFactors[2][i] = newScale;
                for (int j = 0; j < 3; j++)
                {
                    finalIntervalsandScalingFactors[j].erase(finalIntervalsandScalingFactors[j].begin() + i + 1);
                }
                i--;
            }
        }
    }

    // scale to 8-bit range as supported by current sei and rdd5
    scale_down(finalIntervalsandScalingFactors, bitDepth);

    // because of scaling in previous step, some intervals may overlap. Check intervals for errors.
    confirm_intervals(finalIntervalsandScalingFactors);

    // set number of intervals; exculde intervals with scaling factor 0.
    m_compModel[compID].m_filmGrainNumIntensityIntervalMinus1 =
        (int) finalIntervalsandScalingFactors[2].size()
        - (int) count(finalIntervalsandScalingFactors[2].begin(), finalIntervalsandScalingFactors[2].end(), 0);

    if (m_compModel[compID].m_filmGrainNumIntensityIntervalMinus1 == 0)
    {   // check if all intervals are 0, and if yes set presentFlag to false
        m_compModel[compID].bPresentFlag = false;
        return;
    }


    // set final interval boundaries and scaling factors. check if some interval has scaling factor 0, and do not encode
    // them within SEI.
    int j = 0;
    for (unsigned int i = 0; i < finalIntervalsandScalingFactors[2].size(); i++)
    {
        if (finalIntervalsandScalingFactors[2][i] != 0)
        {
            m_compModel[compID].intensityValues[j].intensityIntervalLowerBound = finalIntervalsandScalingFactors[0][i];
            m_compModel[compID].intensityValues[j].intensityIntervalUpperBound = finalIntervalsandScalingFactors[1][i];
            m_compModel[compID].intensityValues[j].compModelValue[0]           = finalIntervalsandScalingFactors[2][i];
            m_compModel[compID].intensityValues[j].compModelValue[1]           = cutoff_horizontal;
            m_compModel[compID].intensityValues[j].compModelValue[2]           = cutoff_vertical;
            j++;
        }
    }
    X265_CHECK(j == m_compModel[compID].m_filmGrainNumIntensityIntervalMinus1, "Check film grain intensity levels");
    m_compModel[compID].m_filmGrainNumIntensityIntervalMinus1 -= 1;
}

long double FGAnalyser::ldpow(long double n, unsigned p)
{
    long double x = 1;
    unsigned    i;

    for (i = 0; i < p; i++)
    {
        x = x * n;
    }

    return x;
}

// find bounds of intensity intervals and scaling factors for each interval
void FGAnalyser::define_intervals_and_scalings(std::vector<std::vector<int> > &parameters, std::vector<int> &quantizedVec, int bitDepth)
{
    parameters[0].push_back(0);
    parameters[2].push_back(quantizedVec[0]);
    for (unsigned int i = 0; i < quantizedVec.size() - 1; i++)
    {
        if (quantizedVec[i] != quantizedVec[i + 1])
        {
            parameters[0].push_back(i + 1);
            parameters[1].push_back(i);
            parameters[2].push_back(quantizedVec[i + 1]);
        }
    }
    parameters[1].push_back((1 << bitDepth) - 1);
}

// scale everything to 8-bit ranges as supported by SEI message
void FGAnalyser::scale_down(std::vector<std::vector<int> > &parameters, int bitDepth)
{
    for (unsigned int i = 0; i < parameters[2].size(); i++)
    {
        parameters[0][i] >>= (bitDepth - BIT_DEPTH_8);
        parameters[1][i] >>= (bitDepth - BIT_DEPTH_8);
        parameters[2][i] <<= m_log2ScaleFactor;
        parameters[2][i] >>= (bitDepth - BIT_DEPTH_8);
    }
}

// check if intervals are properly set after scaling to 8-bit representation
void FGAnalyser::confirm_intervals(std::vector<std::vector<int> > &parameters)
{
    std::vector<int> tmp;
    for (unsigned int i = 0; i < parameters[2].size(); i++)
    {
        tmp.push_back(parameters[0][i]);
        tmp.push_back(parameters[1][i]);
    }
    for (unsigned int i = 0; i < tmp.size() - 1; i++)
    {
        if (tmp[i] == tmp[i + 1])
        {
            tmp[i + 1]++;
        }
    }
    for (unsigned int i = 0; i < parameters[2].size(); i++)
    {
        parameters[0][i] = tmp[2 * i];
        parameters[1][i] = tmp[2 * i + 1];
    }
}

void FGAnalyser::extend_points(std::vector<int> &data_x, std::vector<int> &data_y, int bitDepth)
{
    int xmin = data_x[0];
    int xmax = data_x[0];
    int ymin = data_y[0];
    int ymax = data_y[0];
    for (unsigned int i = 0; i < data_x.size(); i++)
    {
        if (data_x[i] < xmin)
        {
            xmin = data_x[i];
            ymin = data_y[i];   // not real ymin
        }
        if (data_x[i] > xmax)
        {
            xmax = data_x[i];
            ymax = data_y[i];   // not real ymax
        }
    }

    // extend points to the left
    int    step  = POINT_STEP;
    double scale = POINT_SCALE;
    int num_extra_point_left  = MAX_NUM_POINT_TO_EXTEND;
    int num_extra_point_right = MAX_NUM_POINT_TO_EXTEND;
    while (xmin >= step && ymin > 1 && num_extra_point_left > 0)
    {
        xmin -= step;
        ymin = static_cast<int>(ymin / scale);
        data_x.push_back(xmin);
        data_y.push_back(ymin);
        num_extra_point_left--;
    }

    // extend points to the right
    while (xmax + step <= ((1 << bitDepth) - 1) && ymax > 1 && num_extra_point_right > 0)
    {
        xmax += step;
        ymax = static_cast<int>(ymax / scale);
        data_x.push_back(xmax);
        data_y.push_back(ymax);
        num_extra_point_right--;
    }
    for (unsigned int i = 0; i < data_x.size(); i++)
    {
        if (data_x[i] < MIN_INTENSITY || data_x[i] > MAX_INTENSITY)
        {
            data_x.erase(data_x.begin() + i);
            data_y.erase(data_y.begin() + i);
            i--;
        }
    }
}

void FGAnalyser::set_film_grain_parameters()
{
    /* Write to the model file */
    filmgrain.m_filmGrainCharacteristicsCancelFlag = 0;
    filmgrain.m_filmGrainCharacteristicsPersistenceFlag = 0;
    filmgrain.m_filmGrainModelId = 0;
    filmgrain.m_separateColourDescriptionPresentFlag = 0; // Always set to 0
    filmgrain.m_blendingModeId = 0;
    filmgrain.m_log2ScaleFactor = m_log2ScaleFactor;
    filmgrain.m_compModel[0] = &m_compModel[0];
    filmgrain.m_compModel[1] = &m_compModel[1];
    filmgrain.m_compModel[2] = &m_compModel[2];
    printf("===== Frequency Model Parameters =====\n");
    printf("CancelFlag=%d\n", filmgrain.m_filmGrainCharacteristicsCancelFlag);
    printf("PersistenceFlag=%d\n", filmgrain.m_filmGrainCharacteristicsPersistenceFlag);
    printf("ModelId=%u\n", filmgrain.m_filmGrainModelId);
    printf("SeparateColourDescriptionPresentFlag=%d\n", filmgrain.m_separateColourDescriptionPresentFlag);
    printf("BlendingModeId=%u\n", filmgrain.m_blendingModeId);
    printf("log2ScaleFactor=%u\n", filmgrain.m_log2ScaleFactor);

    for (int i = 0; i < 3; i++) {
        printf("CompModel[%d].bPresentFlag = %d\n", i, filmgrain.m_compModel[i]->bPresentFlag);
        if (filmgrain.m_compModel[i]->bPresentFlag) {
            printf("  NumIntensityIntervalsMinus1=%u\n", filmgrain.m_compModel[i]->m_filmGrainNumIntensityIntervalMinus1);
            printf("  NumModelValues=%u\n", filmgrain.m_compModel[i]->numModelValues);
            for (int j = 0; j <= filmgrain.m_compModel[i]->m_filmGrainNumIntensityIntervalMinus1; j++) {
                printf("    Interval[%d] = [%u,%u] Values:",
                       j,
                       filmgrain.m_compModel[i]->intensityValues[j].intensityIntervalLowerBound,
                       filmgrain.m_compModel[i]->intensityValues[j].intensityIntervalUpperBound);
                for (int k = 0; k < filmgrain.m_compModel[i]->numModelValues; k++)
                    printf(" %d", filmgrain.m_compModel[i]->intensityValues[j].compModelValue[k]);
                printf("\n");
            }
        }
    }
    //memcpy(filmgrain.m_compModel, m_compModel, sizeof(m_compModel));
}

x265_FilmGrainCharacteristics* FGAnalyser::get_film_grain_parameters()
{
    return &filmgrain;
}

// delete picture buffers
void FGAnalyser::destroy()
{
    if (m_originalBuf != NULL)
    {
        m_originalBuf->destroy();
        delete m_originalBuf;
        m_originalBuf = NULL;
    }
    if (m_workingBuf != NULL)
    {
        m_workingBuf->destroy();
        delete m_workingBuf;
        m_workingBuf = NULL;
    }
    if (m_maskBuf != NULL)
    {
        m_maskBuf->destroy();
        delete m_maskBuf;
        m_maskBuf = NULL;
    }
}