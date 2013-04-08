/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//
//  video_capture_qtkit_info_objc.h
//
//

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_QTKIT_VIDEO_CAPTURE_QTKIT_INFO_OBJC_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_QTKIT_VIDEO_CAPTURE_QTKIT_INFO_OBJC_H_

#import <QTKit/QTKit.h>
#import <Foundation/Foundation.h>
#include "video_capture_qtkit_utility.h"
#include "video_capture_qtkit_info.h"

@interface VideoCaptureMacQTKitInfoObjC : NSObject{
    bool                                _OSSupportedInfo;
    NSArray*                            _captureDevicesInfo;
    NSAutoreleasePool*                    _poolInfo;
    int                                    _captureDeviceCountInfo;

}

/**************************************************************************
 *
 *   The following functions are considered to be private
 *
 ***************************************************************************/

- (NSNumber*)getCaptureDevices;
- (NSNumber*)initializeVariables;
- (void)checkOSSupported;


/**************************************************************************
 *
 *   The following functions are considered to be public and called by VideoCaptureMacQTKitInfo class
 *
 ***************************************************************************/

- (NSNumber*)getCaptureDeviceCount;

- (NSNumber*)getDeviceNamesFromIndex:(WebRtc_UWord32)index
    DefaultName:(char*)deviceName
    WithLength:(WebRtc_UWord32)deviceNameLength
    AndUniqueID:(char*)deviceUniqueID
    WithLength:(WebRtc_UWord32)deviceUniqueIDLength
    AndProductID:(char*)deviceProductID
    WithLength:(WebRtc_UWord32)deviceProductIDLength;

- (NSNumber*)displayCaptureSettingsDialogBoxWithDevice:
        (const char*)deviceUniqueIdUTF8
    AndTitle:(const char*)dialogTitleUTF8
    AndParentWindow:(void*) parentWindow AtX:(WebRtc_UWord32)positionX
    AndY:(WebRtc_UWord32) positionY;
@end

#endif  // WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_QTKIT_VIDEO_CAPTURE_QTKIT_INFO_OBJC_H_
