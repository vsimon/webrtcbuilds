/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * video_processing.h
 * This header file contains the API required for the video
 * processing module class.
 */


#ifndef WEBRTC_MODULES_INTERFACE_VIDEO_PROCESSING_H
#define WEBRTC_MODULES_INTERFACE_VIDEO_PROCESSING_H

#include "module.h"
#include "module_common_types.h"
#include "video_processing_defines.h"

/**
   The module is largely intended to process video streams, except functionality 
   provided by static functions which operate independent of previous frames. It
   is recommended, but not required that a unique instance be used for each 
   concurrently processed stream. Similarly, it is recommended to call Reset()
   before switching to a new stream, but this is not absolutely required.
   
   The module provides basic thread safety by permitting only a single function to
   execute concurrently.
*/

namespace webrtc {

class VideoProcessingModule : public Module
{
public:
    /**
       Structure to hold frame statistics. Populate it with GetFrameStats().
    */
    struct FrameStats
    {
        FrameStats() :
            mean(0),
            sum(0),
            numPixels(0),
            subSamplWidth(0),
            subSamplHeight(0)
        {
            memset(hist, 0, sizeof(hist));
        }

        WebRtc_UWord32 hist[256];      /**< Histogram of frame */
        WebRtc_UWord32 mean;           /**< Mean value of frame */
        WebRtc_UWord32 sum;            /**< Sum of frame */
        WebRtc_UWord32 numPixels;      /**< Number of pixels */
        WebRtc_UWord8  subSamplWidth;  /**< Subsampling rate of width in powers of 2 */
        WebRtc_UWord8  subSamplHeight; /**< Subsampling rate of height in powers of 2 */
    };

    /**
       Specifies the warning types returned by BrightnessDetection().
    */
    enum BrightnessWarning 
    {
        kNoWarning,                /**< Frame has acceptable brightness */
        kDarkWarning,              /**< Frame is too dark */
        kBrightWarning            /**< Frame is too bright */
    };

    /*
       Creates a VPM object.
      
       \param[in] id
           Unique identifier of this object.
      
       \return Pointer to a VPM object.
    */
    static VideoProcessingModule* Create(WebRtc_Word32 id);

    /**
       Destroys a VPM object.
      
       \param[in] module
           Pointer to the VPM object to destroy.
    */
    static void Destroy(VideoProcessingModule* module);

    /**
       Retrieves the version of the module and its components.
      
       \param[in,out] version
           Buffer to which the null-terminated version string will be copied.
      
       \param[in,out] remainingBufferInBytes 
           Bytes remaining between the supplied position and the end of the buffer.
           This will reflect the new remaining size at return.
       
       \param[in,out] position
           Position in bytes within the buffer to place the version string. This 
           will reflect the first byte position following the version string at 
           return. 
      
       \return 0 on success, -1 on failure.
    */
    static WebRtc_Word32 GetVersion(WebRtc_Word8* version,
                                  WebRtc_UWord32& remainingBufferInBytes,
                                  WebRtc_UWord32& position);

    /**
       Not supported.
    */
    virtual WebRtc_Word32 TimeUntilNextProcess() { return -1; }

    /**
       Not supported.
    */
    virtual WebRtc_Word32 Process() { return -1; }

    /**
       Resets all processing components to their initial states. This should be
       called whenever a new video stream is started.
    */
    virtual void Reset() = 0;

    /**
       Retrieves statistics for the input frame. This function must be used to
       prepare a FrameStats struct for use in certain VPM functions.
      
       \param[out] stats
           The frame statistics will be stored here on return.
      
       \param[in]  frame
           Pointer to the video frame.
      
       \param[in]  width
           Frame width in pixels.
      
       \param[in]  height
           Frame height in pixels.
      
       \return 0 on success, -1 on failure.
    */
    static WebRtc_Word32 GetFrameStats(FrameStats& stats,
                                     const WebRtc_UWord8* frame,
                                     WebRtc_UWord32 width,
                                     WebRtc_UWord32 height);

    /**
       \overload
    */
     static WebRtc_Word32 GetFrameStats(FrameStats& stats,
                                     const VideoFrame& frame);

    /**
       Checks the validity of a FrameStats struct. Currently, valid implies only
       that is had changed from its initialized state.
      
       \param[in] stats
           Frame statistics.
      
       \return True on valid stats, false on invalid stats.
    */
    static bool ValidFrameStats(const FrameStats& stats);

    /**
       Returns a FrameStats struct to its intialized state.
      
       \param[in,out] stats
           Frame statistics.
    */
    static void ClearFrameStats(FrameStats& stats);

    /**
       Enhances the color of an image through a constant mapping. Only the 
       chrominance is altered. Has a fixed-point implementation.
      
       \param[in,out] frame
           Pointer to the video frame.
      
       \param[in]     width
           Frame width in pixels.
      
       \param[in]     height
           Frame height in pixels.
      
       \return 0 on success, -1 on failure.
    */
    static WebRtc_Word32 ColorEnhancement(WebRtc_UWord8* frame,
                                        WebRtc_UWord32 width,
                                        WebRtc_UWord32 height);

    /**
       \overload
    */
    static WebRtc_Word32 ColorEnhancement(VideoFrame& frame);

    /**
       Increases/decreases the luminance value.

       \param[in,out] frame
           Pointer to the video frame buffer.

       \param[in]     width
           Frame width in pixels.

       \param[in]     height
           Frame height in pixels.

      \param[in] delta
           The amount to change the chrominance value of every single pixel.
           Can be < 0 also.

       \return 0 on success, -1 on failure.
    */
    static WebRtc_Word32 Brighten(WebRtc_UWord8* frame,
                                  int width, int height, int delta);
    /**
       \overload
    */
    static WebRtc_Word32 Brighten(VideoFrame& frame, int delta);

    /**
       Detects and removes camera flicker from a video stream. Every frame from the
       stream must be passed in. A frame will only be altered if flicker has been
       detected. Has a fixed-point implementation.
      
       \param[in,out] frame
           Pointer to the video frame.
      
       \param[in]     width
           Frame width in pixels.
      
       \param[in]     height
           Frame height in pixels.
      
       \param[in]     timestamp
           Frame timestamp in 90 kHz format.
      
       \param[in,out] stats
           Frame statistics provided by GetFrameStats(). On return the stats will
           be reset to zero if the frame was altered. Call GetFrameStats() again
           if the statistics for the altered frame are required.
      
       \return 0 on success, -1 on failure.
    */
    virtual WebRtc_Word32 Deflickering(WebRtc_UWord8* frame,
                                     WebRtc_UWord32 width,
                                     WebRtc_UWord32 height,
                                     WebRtc_UWord32 timestamp,
                                     FrameStats& stats) = 0;
    
    /**
       \overload
    */
    virtual WebRtc_Word32 Deflickering(VideoFrame& frame,
                                     FrameStats& stats) = 0;

    /**
       Denoises a video frame. Every frame from the stream should be passed in.
       Has a fixed-point implementation.
      
       \param[in,out] frame
           Pointer to the video frame.
      
       \param[in]     width
           Frame width in pixels.
      
       \param[in]     height
           Frame height in pixels.
      
       \return The number of modified pixels on success, -1 on failure.
    */
    virtual WebRtc_Word32 Denoising(WebRtc_UWord8* frame,
                                  WebRtc_UWord32 width,
                                  WebRtc_UWord32 height) = 0;
    
    /**
       \overload
    */
    virtual WebRtc_Word32 Denoising(VideoFrame& frame) = 0;

    /**
       Detects if a video frame is excessively bright or dark. Returns a warning if
       this is the case. Multiple frames should be passed in before expecting a 
       warning. Has a floating-point implementation.
      
       \param[in] frame
           Pointer to the video frame.
      
       \param[in]     width
           Frame width in pixels.
      
       \param[in]     height
           Frame height in pixels.
      
       \param[in] stats
           Frame statistics provided by GetFrameStats().
      
       \return A member of BrightnessWarning on success, -1 on error
    */
    virtual WebRtc_Word32 BrightnessDetection(const WebRtc_UWord8* frame,
                                            WebRtc_UWord32 width,
                                            WebRtc_UWord32 height,
                                            const FrameStats& stats) = 0;

    /**
       \overload
    */
    virtual WebRtc_Word32 BrightnessDetection(const VideoFrame& frame,
                                            const FrameStats& stats) = 0;


    /**
    The following functions refer to the pre-processor unit within VPM. The pre-processor
    perfoms spatial/temporal decimation and content analysis on the frames prior to encoding.
    */
	
    /**
    Enable/disable temporal decimation

    \param[in] enable when true, temporal decimation is enabled
    */
    virtual void EnableTemporalDecimation(bool enable) = 0;
	
    /**
   Set target resolution
    
   \param[in] width
   Target width
    
   \param[in] height
   Target height
    
    \param[in] frameRate
    Target frameRate
           
    \return VPM_OK on success, a negative value on error (see error codes)

    */
    virtual WebRtc_Word32 SetTargetResolution(WebRtc_UWord32 width, WebRtc_UWord32 height, WebRtc_UWord32 frameRate) = 0;
    
    /**
    Set max frame rate
    \param[in] maxFrameRate: maximum frame rate (limited to native frame rate)

    \return VPM_OK on success, a negative value on error (see error codes)
    */
    virtual WebRtc_Word32 SetMaxFrameRate(WebRtc_UWord32 maxFrameRate) = 0;

    /**
    Get decimated(target) frame rate
    */
    virtual WebRtc_UWord32 DecimatedFrameRate() = 0;
	
    /**
    Get decimated(target) frame width
    */
    virtual WebRtc_UWord32 DecimatedWidth() const = 0;

    /**
    Get decimated(target) frame height
    */
    virtual WebRtc_UWord32 DecimatedHeight() const = 0 ;

    /**
    Set the spatial resampling settings of the VPM: The resampler may either be disabled or one of the following:
    scaling to a close to target dimension followed by crop/pad

    \param[in] resamplingMode
    Set resampling mode (a member of VideoFrameResampling)
    */
    virtual void SetInputFrameResampleMode(VideoFrameResampling resamplingMode) = 0;
  
    /**
    Get Processed (decimated) frame
	  
    \param[in] frame pointer to the video frame.
	  
	  \param[in] processedFrame pointer (double) to the processed frame
    
    \return VPM_OK on success, a negative value on error (see error codes)
    */
    virtual WebRtc_Word32 PreprocessFrame(const VideoFrame* frame, VideoFrame** processedFrame) = 0;

    /**
    Return content metrics for the last processed frame
    */
    virtual VideoContentMetrics* ContentMetrics() const = 0 ;

    /**
    Enable content analysis
    */
    virtual void EnableContentAnalysis(bool enable) = 0;

};

} //namespace

#endif
