/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "tb_capture_device.h"

tbCaptureDevice::tbCaptureDevice(tbInterfaces& Engine, int& nrOfErrors) :
    captureId(-1),
    numberOfErrors(nrOfErrors),
    ViE(Engine),
    vcpm_(NULL)
{
    const unsigned int KMaxDeviceNameLength = 128;
    const unsigned int KMaxUniqueIdLength = 256;
    WebRtc_UWord8 deviceName[KMaxDeviceNameLength];
    memset(deviceName, 0, KMaxDeviceNameLength);
    WebRtc_UWord8 uniqueId[KMaxUniqueIdLength];
    memset(uniqueId, 0, KMaxUniqueIdLength);

    int error;
    bool captureDeviceSet = false;

    webrtc::VideoCaptureModule::DeviceInfo* devInfo =
        webrtc::VideoCaptureModule::CreateDeviceInfo(0);
    for (size_t captureIdx = 0;
        captureIdx < devInfo->NumberOfDevices();
        captureIdx++)
    {
        error = devInfo->GetDeviceName(captureIdx, deviceName,
                                       KMaxDeviceNameLength, uniqueId,
                                       KMaxUniqueIdLength);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        vcpm_ = webrtc::VideoCaptureModule::Create(captureIdx, uniqueId);
        if (vcpm_ == NULL) // Failed to open this device. Try next.
        {
            continue;
        }

        error = ViE.ptrViECapture->AllocateCaptureDevice(*vcpm_, captureId);
        if (error == 0)
        {
            ViETest::Log("Using capture device: %s, captureId: %d", deviceName,
                         captureId);
            captureDeviceSet = true;
            break;
        }
    }
    webrtc::VideoCaptureModule::DestroyDeviceInfo(devInfo);
    numberOfErrors += ViETest::TestError(
        captureDeviceSet, "ERROR: %s at line %d - Could not set capture device",
        __FUNCTION__, __LINE__);

    ViETest::Log("Starting capture device %s with captureId %d\n", deviceName,
                 captureId);

    error = ViE.ptrViECapture->StartCapture(captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
}

tbCaptureDevice::~tbCaptureDevice(void)
{
    ViETest::Log("Stopping capture device with id %d\n", captureId);
    int error;
    error = ViE.ptrViECapture->StopCapture(captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViECapture->ReleaseCaptureDevice(captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    webrtc::VideoCaptureModule::Destroy(vcpm_);

}

void tbCaptureDevice::ConnectTo(int videoChannel)
{
    int error;
    error = ViE.ptrViECapture->ConnectCaptureDevice(captureId, videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
}

void tbCaptureDevice::Disconnect(int videoChannel)
{
    int error = 0;
    error = ViE.ptrViECapture->DisconnectCaptureDevice(videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
}
