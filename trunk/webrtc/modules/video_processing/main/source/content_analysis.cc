/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "content_analysis.h"
#include "tick_util.h"
#include "system_wrappers/interface/cpu_features_wrapper.h"

#include <math.h>
#include <stdlib.h>

namespace webrtc {

VPMContentAnalysis::VPMContentAnalysis(bool runtime_cpu_detection):
_origFrame(NULL),
_prevFrame(NULL),
_width(0),
_height(0),
_skipNum(1),
_border(8),
_motionMagnitude(0.0f),
_spatialPredErr(0.0f),
_spatialPredErrH(0.0f),
_spatialPredErrV(0.0f),
_firstFrame(true),
_CAInit(false),
_cMetrics(NULL)
{
    ComputeSpatialMetrics = &VPMContentAnalysis::ComputeSpatialMetrics_C;
    TemporalDiffMetric = &VPMContentAnalysis::TemporalDiffMetric_C;

    if (runtime_cpu_detection)
    {
#if defined(WEBRTC_ARCH_X86_FAMILY)
        if (WebRtc_GetCPUInfo(kSSE2))
        {
            ComputeSpatialMetrics =
                          &VPMContentAnalysis::ComputeSpatialMetrics_SSE2;
            TemporalDiffMetric = &VPMContentAnalysis::TemporalDiffMetric_SSE2;
        }
#endif
    }

    Release();
}

VPMContentAnalysis::~VPMContentAnalysis()
{
    Release();
}


VideoContentMetrics*
VPMContentAnalysis::ComputeContentMetrics(const I420VideoFrame& inputFrame)
{
    if (inputFrame.IsZeroSize())
    {
        return NULL;
    }

    // Init if needed (native dimension change)
    if (_width != inputFrame.width() || _height != inputFrame.height())
    {
        if (VPM_OK != Initialize(inputFrame.width(), inputFrame.height()))
        {
            return NULL;
        }
    }
    // Only interested in the Y plane.
    _origFrame = inputFrame.buffer(kYPlane);

    // compute spatial metrics: 3 spatial prediction errors
    (this->*ComputeSpatialMetrics)();

    // compute motion metrics
    if (_firstFrame == false)
        ComputeMotionMetrics();

    // saving current frame as previous one: Y only
    memcpy(_prevFrame, _origFrame, _width * _height);

    _firstFrame =  false;
    _CAInit = true;

    return ContentMetrics();
}

WebRtc_Word32
VPMContentAnalysis::Release()
{
    if (_cMetrics != NULL)
    {
        delete _cMetrics;
       _cMetrics = NULL;
    }

    if (_prevFrame != NULL)
    {
        delete [] _prevFrame;
        _prevFrame = NULL;
    }

    _width = 0;
    _height = 0;
    _firstFrame = true;

    return VPM_OK;
}

WebRtc_Word32
VPMContentAnalysis::Initialize(int width, int height)
{
   _width = width;
   _height = height;
   _firstFrame = true;

    // skip parameter: # of skipped rows: for complexity reduction
    //  temporal also currently uses it for column reduction.
    _skipNum = 1;

    // use skipNum = 2 for 4CIF, WHD
    if ( (_height >=  576) && (_width >= 704) )
    {
        _skipNum = 2;
    }
    // use skipNum = 4 for FULLL_HD images
    if ( (_height >=  1080) && (_width >= 1920) )
    {
        _skipNum = 4;
    }

    if (_cMetrics != NULL)
    {
        delete _cMetrics;
    }

    if (_prevFrame != NULL)
    {
        delete [] _prevFrame;
    }

    // Spatial Metrics don't work on a border of 8.  Minimum processing
    // block size is 16 pixels.  So make sure the width and height support this.
    if (_width <= 32 || _height <= 32)
    {
        _CAInit = false;
        return VPM_PARAMETER_ERROR;
    }

    _cMetrics = new VideoContentMetrics();
    if (_cMetrics == NULL)
    {
        return VPM_MEMORY;
    }

    _prevFrame = new WebRtc_UWord8[_width * _height] ; // Y only
    if (_prevFrame == NULL)
    {
        return VPM_MEMORY;
    }

    return VPM_OK;
}


// Compute motion metrics: magnitude over non-zero motion vectors,
//  and size of zero cluster
WebRtc_Word32
VPMContentAnalysis::ComputeMotionMetrics()
{

    // Motion metrics: only one is derived from normalized
    //  (MAD) temporal difference
    (this->*TemporalDiffMetric)();

    return VPM_OK;
}

// Normalized temporal difference (MAD): used as a motion level metric
// Normalize MAD by spatial contrast: images with more contrast
//  (pixel variance) likely have larger temporal difference
// To reduce complexity, we compute the metric for a reduced set of points.
WebRtc_Word32
VPMContentAnalysis::TemporalDiffMetric_C()
{
    // size of original frame
    int sizei = _height;
    int sizej = _width;

    WebRtc_UWord32 tempDiffSum = 0;
    WebRtc_UWord32 pixelSum = 0;
    WebRtc_UWord64 pixelSqSum = 0;

    WebRtc_UWord32 numPixels = 0; // counter for # of pixels

    const int width_end = ((_width - 2*_border) & -16) + _border;

    for(int i = _border; i < sizei - _border; i += _skipNum)
    {
        for(int j = _border; j < width_end; j++)
        {
            numPixels += 1;
            int ssn =  i * sizej + j;

            WebRtc_UWord8 currPixel  = _origFrame[ssn];
            WebRtc_UWord8 prevPixel  = _prevFrame[ssn];

            tempDiffSum += (WebRtc_UWord32)
                            abs((WebRtc_Word16)(currPixel - prevPixel));
            pixelSum += (WebRtc_UWord32) currPixel;
            pixelSqSum += (WebRtc_UWord64) (currPixel * currPixel);
        }
    }

    // default
    _motionMagnitude = 0.0f;

    if (tempDiffSum == 0)
    {
        return VPM_OK;
    }

    // normalize over all pixels
    float const tempDiffAvg = (float)tempDiffSum / (float)(numPixels);
    float const pixelSumAvg = (float)pixelSum / (float)(numPixels);
    float const pixelSqSumAvg = (float)pixelSqSum / (float)(numPixels);
    float contrast = pixelSqSumAvg - (pixelSumAvg * pixelSumAvg);

    if (contrast > 0.0)
    {
        contrast = sqrt(contrast);
       _motionMagnitude = tempDiffAvg/contrast;
    }

    return VPM_OK;

}

// Compute spatial metrics:
// To reduce complexity, we compute the metric for a reduced set of points.
// The spatial metrics are rough estimates of the prediction error cost for
//  each QM spatial mode: 2x2,1x2,2x1
// The metrics are a simple estimate of the up-sampling prediction error,
// estimated assuming sub-sampling for decimation (no filtering),
// and up-sampling back up with simple bilinear interpolation.
WebRtc_Word32
VPMContentAnalysis::ComputeSpatialMetrics_C()
{
    //size of original frame
    const int sizei = _height;
    const int sizej = _width;

    // pixel mean square average: used to normalize the spatial metrics
    WebRtc_UWord32 pixelMSA = 0;

    WebRtc_UWord32 spatialErrSum = 0;
    WebRtc_UWord32 spatialErrVSum = 0;
    WebRtc_UWord32 spatialErrHSum = 0;

    // make sure work section is a multiple of 16
    const int width_end = ((sizej - 2*_border) & -16) + _border;

    for(int i = _border; i < sizei - _border; i += _skipNum)
    {
        for(int j = _border; j < width_end; j++)
        {

            int ssn1=  i * sizej + j;
            int ssn2 = (i + 1) * sizej + j; // bottom
            int ssn3 = (i - 1) * sizej + j; // top
            int ssn4 = i * sizej + j + 1;   // right
            int ssn5 = i * sizej + j - 1;   // left

            WebRtc_UWord16 refPixel1  = _origFrame[ssn1] << 1;
            WebRtc_UWord16 refPixel2  = _origFrame[ssn1] << 2;

            WebRtc_UWord8 bottPixel = _origFrame[ssn2];
            WebRtc_UWord8 topPixel = _origFrame[ssn3];
            WebRtc_UWord8 rightPixel = _origFrame[ssn4];
            WebRtc_UWord8 leftPixel = _origFrame[ssn5];

            spatialErrSum  += (WebRtc_UWord32) abs((WebRtc_Word16)(refPixel2
                            - (WebRtc_UWord16)(bottPixel + topPixel
                                             + leftPixel + rightPixel)));
            spatialErrVSum += (WebRtc_UWord32) abs((WebRtc_Word16)(refPixel1
                            - (WebRtc_UWord16)(bottPixel + topPixel)));
            spatialErrHSum += (WebRtc_UWord32) abs((WebRtc_Word16)(refPixel1
                            - (WebRtc_UWord16)(leftPixel + rightPixel)));

            pixelMSA += _origFrame[ssn1];
        }
    }

    // normalize over all pixels
    const float spatialErr  = (float)(spatialErrSum >> 2);
    const float spatialErrH = (float)(spatialErrHSum >> 1);
    const float spatialErrV = (float)(spatialErrVSum >> 1);
    const float norm = (float)pixelMSA;

    // 2X2:
    _spatialPredErr = spatialErr / norm;

    // 1X2:
    _spatialPredErrH = spatialErrH / norm;

    // 2X1:
    _spatialPredErrV = spatialErrV / norm;

    return VPM_OK;
}

VideoContentMetrics*
VPMContentAnalysis::ContentMetrics()
{
    if (_CAInit == false)
    {
        return NULL;
    }

    _cMetrics->spatial_pred_err = _spatialPredErr;
    _cMetrics->spatial_pred_err_h = _spatialPredErrH;
    _cMetrics->spatial_pred_err_v = _spatialPredErrV;
    // Motion metric: normalized temporal difference (MAD)
    _cMetrics->motion_magnitude = _motionMagnitude;

    return _cMetrics;

}

} // namespace
