/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <string.h>
#include <android/log.h>

#include "org_webrtc_videoengineapp_vie_android_java_api.h"

#include "voe_base.h"
#include "voe_codec.h"
#include "voe_file.h"
#include "voe_network.h"
#include "voe_audio_processing.h"
#include "voe_volume_control.h"
#include "voe_hardware.h"

#include "vie_base.h"
#include "vie_codec.h"
#include "vie_capture.h"
#include "vie_network.h"
#include "vie_render.h"
#include "vie_rtp_rtcp.h"

#include "common_types.h"

#define WEBRTC_LOG_TAG "*WEBRTCN*"
#define VALIDATE_BASE_POINTER                                           \
  if (!veData.base)                                                     \
  {                                                                     \
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,              \
                        "Base pointer doesn't exist");                  \
    return -1;                                                          \
  }
#define VALIDATE_CODEC_POINTER                                          \
  if (!veData.codec)                                                    \
  {                                                                     \
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,              \
                        "Codec pointer doesn't exist");                 \
    return -1;                                                          \
  }
#define VALIDATE_FILE_POINTER                                           \
  if (!veData.file)                                                     \
  {                                                                     \
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,              \
                        "File pointer doesn't exist");                  \
    return -1;                                                          \
  }
#define VALIDATE_APM_POINTER                                            \
  if (!veData.codec)                                                    \
  {                                                                     \
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,              \
                        "Apm pointer doesn't exist");                   \
    return -1;                                                          \
  }
#define VALIDATE_HARDWARE_POINTER                                       \
  if (!veData.hardware)                                                 \
  {                                                                     \
    __android_log_write(                                                \
        ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,                              \
        "Hardware pointer doesn't exist");                              \
    return -1;                                                          \
  }
#define VALIDATE_VOLUME_POINTER                                         \
  if (!veData.volume)                                                   \
  {                                                                     \
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,              \
                        "Volume pointer doesn't exist");                \
    return -1;                                                          \
  }

using namespace webrtc;

//Forward declaration.
class VideoCallbackAndroid;

// VoiceEngine data struct
typedef struct
{
  // VoiceEngine
  VoiceEngine* ve;
  // Sub-APIs
  VoEBase* base;
  VoECodec* codec;
  VoEFile* file;
  VoENetwork* netw;
  VoEAudioProcessing* apm;
  VoEVolumeControl* volume;
  VoEHardware* hardware;
  JavaVM* jvm;
} VoiceEngineData;

class AndroidVideoRenderCallback;
// VideoEngine data struct
typedef struct
{
  VideoEngine* vie;
  ViEBase* base;
  ViECodec* codec;
  ViENetwork* netw;
  ViERTP_RTCP* rtp;
  ViERender* render;
  ViECapture* capture;
  VideoCallbackAndroid* callback;

} VideoEngineData;

// Global variables
JavaVM* webrtcGlobalVM;

// Global variables visible in this file
static VoiceEngineData veData;
static VideoEngineData vieData;

// "Local" functions (i.e. not Java accessible)
#define WEBRTC_TRACE_MAX_MESSAGE_SIZE 1024
static bool VE_GetSubApis();
static bool VE_ReleaseSubApis();

#define CHECK_API_RETURN(ret)                                           \
  if (ret!=0)                                                           \
  {                                                                     \
    __android_log_print(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,              \
                        "Return error %d",ret);                         \
    break;                                                              \
  }

class VideoCallbackAndroid: public ViEDecoderObserver,
                            public ViEEncoderObserver
{

    // Implements ViEDecoderObserver
    virtual void IncomingRate(const int videoChannel,
                              const unsigned int framerate,
                              const unsigned int bitrate)
    {
        // Let's print out the network statistics from this call back as well
        unsigned short fraction_lost;
        unsigned int dummy;
        int intdummy;
        _vieData.rtp->GetReceivedRTCPStatistics(videoChannel, fraction_lost,
                                                dummy, dummy, dummy, intdummy);
        unsigned short packetLossRate = 0;
        if (fraction_lost > 0)
        {
            // Change from frac to %
            packetLossRate = (fraction_lost * 100) >> 8;
        }

        JNIEnv* threadEnv = NULL;
        int ret = webrtcGlobalVM->AttachCurrentThread(&threadEnv, NULL);
        // Get the JNI env for this thread
        if ((ret < 0) || !threadEnv)
        {
            __android_log_print(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG,
                                "Could not attach thread to JVM (%d, %p)", ret,
                                threadEnv);
            return;
        }
        threadEnv->CallIntMethod(_callbackObj, _callbackId, framerate, bitrate,
                                 packetLossRate, _frameRateO, _bitRateO);
        webrtcGlobalVM->DetachCurrentThread();
    }
    ;

    virtual void IncomingCodecChanged(const int videoChannel,
                                      const webrtc::VideoCodec& videoCodec)
    {
    }
    ;

    virtual void RequestNewKeyFrame(const int videoChannel)
    {
    }
    ;

    virtual void OutgoingRate(const int videoChannel,
                              const unsigned int framerate,
                              const unsigned int bitrate)
    {
        _frameRateO = framerate;
        _bitRateO = bitrate;
        //__android_log_print(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG,
        // "SendRate frameRate %d bitrate %d\n",frameRate,bitrate);
    }
    ;

public:
  VideoEngineData& _vieData;
  JNIEnv * _env;
  jobject _callbackObj;
  jclass _callbackCls;
  jmethodID _callbackId;
  int _frameRateO, _bitRateO;
  VideoCallbackAndroid(VideoEngineData& vieData, JNIEnv * env,
                       jobject callback) :
      _vieData(vieData), _env(env), _callbackObj(callback),
      _frameRateO(0), _bitRateO(0) {
    _callbackCls = _env->GetObjectClass(_callbackObj);
    _callbackId
        = _env->GetMethodID(_callbackCls, "UpdateStats", "(IIIII)I");
    if (_callbackId == NULL) {
      __android_log_print(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                          "Failed to get jid");
    }
    _callbackObj = _env->NewGlobalRef(_callbackObj);
  }
};

// JNI_OnLoad
jint JNI_OnLoad(JavaVM* vm, void* /*reserved*/) {
  webrtcGlobalVM = vm;
  if (!webrtcGlobalVM)
  {
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                        "JNI_OnLoad did not receive a valid VM pointer");
    return -1;
  }

  // Get JNI
  JNIEnv* env;
  if (JNI_OK != vm->GetEnv(reinterpret_cast<void**> (&env),
                           JNI_VERSION_1_4)) {
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                        "JNI_OnLoad could not get JNI env");
    return -1;
  }

  // Init VoiceEngine data
  memset(&veData, 0, sizeof(veData));
  // Store the JVM
  veData.jvm = vm;

  // Init VideoEngineData data
  memset(&vieData, 0, sizeof(vieData));

  return JNI_VERSION_1_4;
}

// Class:     org_webrtc_videoengineapp_ViEAndroidJavaAPI
// Method:    NativeInit
// Signature: (Landroid/content/Context;)Z
JNIEXPORT jboolean JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_NativeInit(
    JNIEnv * env,
    jobject,
    jobject context)
{
  return true;
}

// VideoEngine API wrapper functions
// Class:     org_webrtc_videoengineapp_ViEAndroidJavaAPI
// Method:    GetVideoEngine
// Signature: ()I
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_GetVideoEngine(
    JNIEnv *,
    jobject context) {

  __android_log_write(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG, "GetVideoEngine");

  VideoEngine::SetAndroidObjects(webrtcGlobalVM, context);

  // Check if already got
  if (vieData.vie) {
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                        "ViE already got");
    return -1;
  }

  // Create
  vieData.vie = VideoEngine::Create();
  if (!vieData.vie) {
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG, "Get ViE failed");
    return -1;
  }
  vieData.base = ViEBase::GetInterface(vieData.vie);
  if (!vieData.base) {
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                        "Get base sub-API failed");
    return -1;
  }

  vieData.codec = ViECodec::GetInterface(vieData.vie);
  if (!vieData.codec) {
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                        "Get codec sub-API failed");
    return -1;
  }

  vieData.netw = ViENetwork::GetInterface(vieData.vie);
  if (!vieData.netw) {
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                        "Get network sub-API failed");
    return -1;
  }

  vieData.rtp = ViERTP_RTCP::GetInterface(vieData.vie);
  if (!vieData.rtp) {
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                        "Get RTP sub-API failed");
    return -1;
  }

  vieData.render = ViERender::GetInterface(vieData.vie);
  if (!vieData.render) {
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                        "Get Render sub-API failed");
    return -1;
  }

  vieData.capture = ViECapture::GetInterface(vieData.vie);
  if (!vieData.capture) {
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                        "Get Capture sub-API failed");
    return -1;
  }

  return 0;
}

// Class:     org_webrtc_videoengineapp_ViEAndroidJavaAPI
// Method:    Init
// Signature: (IIIZ)I
JNIEXPORT jint JNICALL Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_Init(
    JNIEnv *,
    jobject,
    jboolean enableTrace)
{
    if (vieData.vie) {
      __android_log_write(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG, "Init");

      int ret = vieData.base->Init();
      __android_log_print(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG,
                            "Init return %d", ret);
        if (enableTrace)
        {
            __android_log_write(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG,
                                "SetTraceFile");
            if (0 != vieData.vie->SetTraceFile(("/sdcard/trace.txt"), false))
            {
                __android_log_print(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                                    "Video Engine could not enable trace");
            }

            __android_log_write(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG,
                                "SetTraceFilter");
            if (0 != vieData.vie->SetTraceFilter(webrtc::kTraceDefault))
            {
                __android_log_write(ANDROID_LOG_WARN, WEBRTC_LOG_TAG,
                                    "Could not set trace filter");
            }
        }
        else
        {
            if (0 != vieData.vie->SetTraceFilter(webrtc::kTraceNone))
            {
                __android_log_write(ANDROID_LOG_WARN, WEBRTC_LOG_TAG,
                                    "Could not set trace filter");
            }
        }
        if (veData.ve) // VoiceEngine is enabled
        {
            __android_log_write(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG,
                                "SetVoiceEngine");
            if (0 != vieData.base->SetVoiceEngine(veData.ve))
            {
                __android_log_write(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG,
                                    "SetVoiceEngine failed");
            }
        }
        return ret;
    }
    else
    {
        return -1;
    }
}

/*
 * Class:     org_webrtc_videoengineapp_ViEAndroidJavaAPI
 * Method:    Terminate
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_Terminate(
    JNIEnv *,
    jobject)
{
  __android_log_write(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG, "Terminate");

  if (vieData.vie) {
    if (!vieData.rtp || vieData.rtp->Release() != 0) {
      __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                          "Failed to release RTP sub-API");
    }

    if (!vieData.netw || vieData.netw->Release() != 0) {
      __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                          "Failed to release Network sub-API");
    }

    if (!vieData.codec || vieData.codec->Release() != 0) {
      __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                                "Failed to release Codec sub-API");
    }

    if (!vieData.render || vieData.render->Release()) {
      __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                          "Failed to release Render sub-API");
    }

    if (!vieData.capture || vieData.capture->Release()) {
      __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                          "Failed to release Capture sub-API");
    }

    if (!vieData.base || vieData.base->Release() != 0) {
      __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                          "Failed to release Base sub-API");
    }

    // Delete Vie
    if (!VideoEngine::Delete(vieData.vie)) {
      __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                          "Failed to delete ViE ");
      return -1;
    }
    memset(&vieData, 0, sizeof(vieData));
    VideoEngine::SetAndroidObjects(NULL, NULL);
    return 0;
  }
  else {
    return -1;
  }
}

// Class:     org_webrtc_videoengineapp_ViEAndroidJavaAPI
// Method:    StartSend
// Signature: (I)I
JNIEXPORT jint JNICALL Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_StartSend(
    JNIEnv *,
    jobject,
    jint channel)
{
  __android_log_write(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG, "StartSend");

  if (vieData.base) {
    int ret = vieData.base->StartSend(channel);
    return ret;
  }
  else {
    return -1;
  }
}

// Class:     org_webrtc_videoengineapp_ViEAndroidJavaAPI
// Method:    StopRender
// Signature: (I)I
JNIEXPORT jint JNICALL Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_StopRender(
                                                                            JNIEnv *,
                                                                            jobject,
                                                                            jint channel)
{
    __android_log_write(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG, "StopRender");

    if (vieData.render) {
        return vieData.render->StopRender(channel);
    }
    else {
        return -1;
    }
}

// Class:     org_webrtc_videoengineapp_ViEAndroidJavaAPI
// Method:    Stop
// Signature: ()I
JNIEXPORT jint JNICALL Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_StopSend(
    JNIEnv *,
    jobject,
    jint channel)
{
  __android_log_write(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG, "StopSend");

  if (vieData.base) {
    return vieData.base->StopSend(channel);
  }
  else {
    return -1;
  }
}

// Class:     org_webrtc_videoengineapp_ViEAndroidJavaAPI
// Method:    StartReceive
// Signature: ()I
JNIEXPORT jint JNICALL Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_StartReceive(
    JNIEnv *,
    jobject,
    jint channel)
{
  __android_log_write(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG, "StartReceive");

  if (vieData.base) {
    return vieData.base->StartReceive(channel);
  }
  else {
    return -1;
  }
}

// Class:     org_webrtc_videoengineapp_ViEAndroidJavaAPI
// Method:    StopReceive
// Signature: ()I
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_StopReceive(
    JNIEnv *,
    jobject,
    jint channel)
{
  __android_log_write(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG, "StopReceive");
  if (vieData.base) {
    return vieData.base->StopReceive(channel);
  }
  else {
    return -1;
  }
}

// Class:     org_webrtc_videoengineapp_ViEAndroidJavaAPI
// Method:    CreateChannel
// Signature: ()I
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_CreateChannel(
    JNIEnv *,
    jobject,
    jint voiceChannel)
{
  __android_log_write(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG, "CreateChannel");

  if (vieData.vie) {
    int channel = 0;
    if (vieData.base->CreateChannel(channel) != 0) {
      return -1;
    }
    if (voiceChannel >= 0) {
      vieData.base->ConnectAudioChannel(channel, voiceChannel);
    }

    return channel;
  }
  else {
    return -1;
  }
}

// Class:     org_webrtc_videoengineapp_ViEAndroidJavaAPI
// Method:    SetLocalReceiver
// Signature: (II)I
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_SetLocalReceiver(
    JNIEnv *,
    jobject,
    jint channel,
    jint port)
{
  __android_log_write(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG, "SetLocalReceiver");

  if (vieData.vie) {
    int ret = vieData.netw->SetLocalReceiver(channel, port);
    return ret;
  }
  else {
    return -1;
  }
}

// Class:     org_webrtc_videoengineapp_ViEAndroidJavaAPI
// Method:    SetSendDestination
// Signature: (II[B)I
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_SetSendDestination(
    JNIEnv * env,
    jobject,
    jint channel,
    jint port,
    jbyteArray ipadr)
{

  if (NULL == vieData.vie)
    return -1;

  char ip[64];
  jsize len = env->GetArrayLength(ipadr);
  if ((len >= 64) || (len == 0))
    return -1;
  env->GetByteArrayRegion(ipadr, 0, len, (jbyte*) ip);
  ip[len] = '\0';

  __android_log_print(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG,
                      "SetSendDestination: channel=%d, port=%d, ip=%s\n",
                      channel, port, ip);

  return vieData.netw->SetSendDestination(channel, ip, port);
}


// Class:     org_webrtc_videoengineapp_ViEAndroidJavaAPI
// Method:    SetReceiveCodec
// Signature: (IIIIII)I
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_SetReceiveCodec(
    JNIEnv *,
    jobject,
    jint channel,
    jint codecNum,
    jint intbitRate,
    jint width,
    jint height,
    jint frameRate)
{
  if (NULL == vieData.codec)
    return -1;

  //Create codec
  webrtc::VideoCodec codec;
  vieData.codec->GetCodec(codecNum, codec);

  __android_log_print(
      ANDROID_LOG_DEBUG,
      WEBRTC_LOG_TAG,
      "SetReceiveCodec %s, pltype=%d, bitRate=%d, maxBitRate=%d,"
      " width=%d, height=%d, frameRate=%d, codecSpecific=%d \n",
      codec.plName, codec.plType, codec.startBitrate,
      codec.maxBitrate, codec.width, codec.height,
      codec.maxFramerate, codec.codecSpecific);
  int ret = vieData.codec->SetReceiveCodec(channel, codec);
  __android_log_print(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG,
                      "SetReceiveCodec return %d", ret);
  return ret;
}

// Class:     org_webrtc_videoengineapp_ViEAndroidJavaAPI
// Method:    SetSendCodec
// Signature: (IIIIII)I
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_SetSendCodec(
    JNIEnv *,
    jobject,
    jint channel,
    jint codecNum,
    jint intbitRate,
    jint width,
    jint height,
    jint frameRate)
{
  if (NULL == vieData.codec)
    return -1;

  //Create codec
  webrtc::VideoCodec codec;
  vieData.codec->GetCodec(codecNum, codec);
  codec.startBitrate = intbitRate;
  codec.maxBitrate = 600;
  codec.width = width;
  codec.height = height;
  codec.maxFramerate = frameRate;

  for (int i = 0; i < vieData.codec->NumberOfCodecs(); ++i) {
    webrtc::VideoCodec codecToList;
    vieData.codec->GetCodec(i, codecToList);
    __android_log_print(
        ANDROID_LOG_DEBUG,
        WEBRTC_LOG_TAG,
        "Codec list %s, pltype=%d, bitRate=%d, maxBitRate=%d,"
        " width=%d, height=%d, frameRate=%d\n",
        codecToList.plName, codecToList.plType,
        codecToList.startBitrate, codecToList.maxBitrate,
        codecToList.width, codecToList.height,
        codecToList.maxFramerate);
  }
  __android_log_print(
      ANDROID_LOG_DEBUG,
      WEBRTC_LOG_TAG,
      "SetSendCodec %s, pltype=%d, bitRate=%d, maxBitRate=%d, "
      "width=%d, height=%d, frameRate=%d\n",
      codec.plName, codec.plType, codec.startBitrate,
      codec.maxBitrate, codec.width, codec.height,
      codec.maxFramerate);

  return vieData.codec->SetSendCodec(channel, codec);
}

// Class:     org_webrtc_videoengineapp_ViEAndroidJavaAPI
// Method:    AddRemoteRenderer
// Signature: (ILandroid/view/SurfaceView;)I
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_AddRemoteRenderer(
    JNIEnv *,
    jobject,
    jint channel,
    jobject glSurface)
{
  __android_log_write(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG, "AddRemoteRenderer");
  if (vieData.vie) {
    return vieData.render->AddRenderer(channel, glSurface, 0, 0, 0, 1, 1);
  }
  else {
    return -1;
  }
}

// Class:     org_webrtc_videoengineapp_ViEAndroidJavaAPI
// Method:    RemoveRemoteRenderer
// Signature: (I)I
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_RemoveRemoteRenderer(
    JNIEnv *,
    jobject,
    jint channel)
{
  __android_log_write(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG, "RemoveRemoteRenderer");

  if (vieData.vie) {
    return vieData.render->RemoveRenderer(channel);
  }
  else {
    return -1;
  }
  return 0;
}

// Class:     org_webrtc_videoengineapp_ViEAndroidJavaAPI
// Method:    StartRender
// Signature: (I)I
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_StartRender(
    JNIEnv *,
    jobject,
    jint channel)
{
  __android_log_write(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG, "StartRender");

  if (vieData.render) {
    return vieData.render->StartRender(channel);
  }
  else {
    return -1;
  }
}

// Class:     org_webrtc_videoengineapp_ViEAndroidJavaAPI
// Method:    StarteCamera
// Signature: (II)I
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_StartCamera(
    JNIEnv * env,
    jobject,
    jint channel,
    jint cameraNum)
{
  if (NULL == vieData.vie)
    return -1;

  int i = 0;
  char deviceName[64];
  char deviceUniqueName[64];
  int re;
  do {
      re = vieData.capture->GetCaptureDevice(i, deviceName,
                                             sizeof(deviceName),
                                             deviceUniqueName,
                                             sizeof(deviceUniqueName));
      __android_log_print(
          ANDROID_LOG_DEBUG,
          WEBRTC_LOG_TAG,
          "GetCaptureDevice ret %d devicenum %d deviceUniqueName %s",
          re, i, deviceUniqueName);
      i++;
  } while (re == 0);

  int ret;
  int cameraId;
  vieData.capture->GetCaptureDevice(cameraNum, deviceName,
                                    sizeof(deviceName), deviceUniqueName,
                                    sizeof(deviceUniqueName));
  vieData.capture->AllocateCaptureDevice(deviceUniqueName,
                                         sizeof(deviceUniqueName), cameraId);

  if (cameraId >= 0) { //Connect the
    ret = vieData.capture->ConnectCaptureDevice(cameraId, channel);
    __android_log_print(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG,
                        "ConnectCaptureDevice ret %d ", ret);

    ret = vieData.capture->StartCapture(cameraId);
    __android_log_print(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG,
                            "StartCapture ret %d ", ret);
  }

  return cameraId;
}

// Class:     org_webrtc_videoengineapp_ViEAndroidJavaAPI
// Method:    StopCamera
// Signature: ()I
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_StopCamera(
    JNIEnv *,
    jobject,
    jint cameraId)
{
  if (NULL == vieData.capture)
    return -1;

  int ret = vieData.capture->StopCapture(cameraId);
  __android_log_print(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG,
                      "StopCapture  ret %d ", ret);
  ret = vieData.capture->ReleaseCaptureDevice(cameraId);
  __android_log_print(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG,
                      "ReleaseCaptureDevice  ret %d ", ret);

  return ret;
}

// Class:     org_webrtc_videoengineapp_ViEAndroidJavaAPI
// Method:    GetCameraOrientation
// Signature: (I)I
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_GetCameraOrientation(
    JNIEnv *,
    jobject,
    jint cameraNum)
{
  char deviceName[64];
  char deviceUniqueName[64];
  int ret;

  ret = vieData.capture->GetCaptureDevice(cameraNum, deviceName,
                                          sizeof(deviceName),
                                          deviceUniqueName,
                                          sizeof(deviceUniqueName));
  if (ret != 0) {
    return -1;
  }

  RotateCapturedFrame orientation;
  ret = vieData.capture->GetOrientation(deviceUniqueName, orientation);
  __android_log_print(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG,
                      "GetOrientation  ret %d orientation %d", ret,
                      orientation);

  return (jint) orientation;

}

// Class:     org_webrtc_videoengineapp_ViEAndroidJavaAPI
// Method:    SetRotation
// Signature: (II)I
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_SetRotation(
    JNIEnv *,
    jobject,
    jint captureId,
    jint degrees)
{

  if (NULL == vieData.capture)
    return -1;
  RotateCapturedFrame rotation = RotateCapturedFrame_0;
  if (degrees == 90)
    rotation = RotateCapturedFrame_90;
  else if (degrees == 180)
    rotation = RotateCapturedFrame_180;
  else if (degrees == 270)
    rotation = RotateCapturedFrame_270;

  int ret = vieData.capture->SetRotateCapturedFrames(captureId, rotation);
  return ret;
}

// Class:     org_webrtc_videoengineapp_ViEAndroidJavaAPI
// Method:    EnableNACK
// Signature: (IZ)I
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_EnableNACK(
                                                                JNIEnv *,
                                                                jobject,
                                                                jint channel,
                                                                jboolean enable)
{
  if (NULL == vieData.rtp)
    return -1;

  if (enable)
    __android_log_write(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG,
                            "EnableNACK enable");
  else
    __android_log_write(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG,
                            "EnableNACK disable");

  int ret = vieData.rtp->SetNACKStatus(channel, enable);
  return ret;
}

// Class:     org_webrtc_videoengineapp_ViEAndroidJavaAPI
// Method:    EnablePLI
// Signature: (IZ)I
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_EnablePLI(
    JNIEnv *,
    jobject,
    jint channel,
    jboolean enable)
{
  if (NULL == vieData.rtp)
    return -1;

  if (enable)
    __android_log_write(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG,
                        "EnablePLI enable");
  else
    __android_log_write(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG,
                        "EnablePLI disable");

  int ret = vieData.rtp->SetKeyFrameRequestMethod(channel,
                                                  kViEKeyFrameRequestPliRtcp);
  return ret;
}

// Class:     org_webrtc_videoengineapp_ViEAndroidJavaAPI
// Method:    SetCallback
// Signature: (ILorg/webrtc/videoengineapp/IViEAndroidCallback;)I
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_SetCallback(
    JNIEnv * env,
    jobject,
    jint channel,
    jobject callback)
{
  __android_log_write(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG, "SetCallback");

  if (NULL == vieData.codec)
    return -1;
  if (vieData.callback == NULL) {
    vieData.callback = new VideoCallbackAndroid(vieData, env, callback);
  }
  else if (vieData.codec) {
    vieData.codec->DeregisterDecoderObserver(channel); // Wrong channel?
    vieData.codec->DeregisterEncoderObserver(channel);
  }

  vieData.codec->RegisterDecoderObserver(channel, *vieData.callback);
  vieData.codec->RegisterEncoderObserver(channel, *vieData.callback);

  return 0;
}

//
// VoiceEngine API wrapper functions
//

// Create VoiceEngine instance
JNIEXPORT jboolean JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_VE_Create(
    JNIEnv *env,
    jobject,
    jobject context)
{
  __android_log_write(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG, "Create");

  // Check if already created
  if (veData.ve) {
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                        "VoE already created");
    return false;
  }

  // Init Android Object
  VoiceEngine::SetAndroidObjects(veData.jvm, env, context);
  // Create
  veData.ve = VoiceEngine::Create();
  if (!veData.ve) {
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                        "Create VoE failed");
    return false;
  }

  // Get sub-APIs
  if (!VE_GetSubApis()) {
    // If not OK, release all sub-APIs and delete VoE
    VE_ReleaseSubApis();
    if (!VoiceEngine::Delete(veData.ve)) {
      __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                          "Delete VoE failed");
    }
    return false;
  }

  return true;
}

// Delete VoiceEngine instance
JNIEXPORT jboolean JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_VE_Delete(
    JNIEnv *,
    jobject)
{
  // Check if exists
  if (!veData.ve) {
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                        "VoE does not exist");
    return false;
  }

  // Release sub-APIs
  VE_ReleaseSubApis();

  // Delete
  if (!VoiceEngine::Delete(veData.ve)) {
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                        "Delete VoE failed");
    return false;
  }

  veData.ve = NULL;

  // Clear instance independent Java objects
  VoiceEngine::SetAndroidObjects(NULL, NULL, NULL);

  return true;
}

// [Base] Initialize VoiceEngine
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_VE_Init(
    JNIEnv *,
    jobject,
    jboolean enableTrace,
    jboolean useExtTrans)
{
  __android_log_write(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG, "VE_Init");

  VALIDATE_BASE_POINTER;

  if (useExtTrans) {
    // Not implemented
    return -1;
  }

  return veData.base->Init();
}

// [Base] Terminate VoiceEngine
JNIEXPORT jint
JNICALL Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_VE_Terminate(
    JNIEnv *,
    jobject)
{
  VALIDATE_BASE_POINTER;

  jint retVal = veData.base->Terminate();
  return retVal;
}

// [Base] Create channel
JNIEXPORT jint JNICALL Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_VE_CreateChannel(
    JNIEnv *,
    jobject)
{
  VALIDATE_BASE_POINTER;

  webrtc::CodecInst voiceCodec;
  int numOfVeCodecs = veData.codec->NumOfCodecs();

  //enum all the supported codec
  __android_log_print(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG,
                      "Supported Voice Codec:\n");
  for (int i = 0; i < numOfVeCodecs; ++i) {
    if (veData.codec->GetCodec(i, voiceCodec) != -1) {
      __android_log_print(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG,
                          "num: %d name: %s\n", i, voiceCodec.plname);
    }
  }

  jint channel = veData.base->CreateChannel();

  return channel;
}

// [Base] Delete channel
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_VE_DeleteChannel(
    JNIEnv *,
    jobject,
    jint channel)
{
  VALIDATE_BASE_POINTER;
  return veData.base->DeleteChannel(channel);
}

// [Base] SetLocalReceiver
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_VE_SetLocalReceiver(
    JNIEnv *,
    jobject,
    jint channel,
    jint port)
{
  __android_log_write(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG, "SetLocalReceiver");
  VALIDATE_BASE_POINTER;
  return veData.base->SetLocalReceiver(channel, port);
}

// [Base] SetSendDestination
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_VE_SetSendDestination(
    JNIEnv *env,
    jobject,
    jint channel,
    jint port,
    jstring ipaddr)
{
  __android_log_write(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG, "SetSendDestination");
  VALIDATE_BASE_POINTER;

  const char* ipaddrNative = env->GetStringUTFChars(ipaddr, NULL);
  if (!ipaddrNative) {
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                        "Could not get UTF string");
    return -1;
  }
  jint retVal = veData.base->SetSendDestination(channel, port, ipaddrNative);
  env->ReleaseStringUTFChars(ipaddr, ipaddrNative);
  return retVal;
}

// [Base] StartListen
JNIEXPORT jint JNICALL Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_VE_StartListen(
    JNIEnv *,
    jobject,
    jint channel)
{
  __android_log_write(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG, "StartListen");
  VALIDATE_BASE_POINTER;
  return veData.base->StartReceive(channel);
}

// [Base] Start playout
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_VE_StartPlayout(
    JNIEnv *,
    jobject,
    jint channel)
{
  __android_log_write(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG, "StartPlayout");
  VALIDATE_BASE_POINTER;
  return veData.base->StartPlayout(channel);
}

// [Base] Start send
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_VE_StartSend(
    JNIEnv *,
    jobject,
    jint channel)
{
  __android_log_write(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG, "StartSend");
  VALIDATE_BASE_POINTER;
  return veData.base->StartSend(channel);
}

// [Base] Stop listen
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_VE_StopListen(
    JNIEnv *,
    jobject,
    jint channel)
{
  VALIDATE_BASE_POINTER;
  return veData.base->StartReceive(channel);
}

// [Base] Stop playout
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_VE_StopPlayout(
    JNIEnv *,
    jobject,
    jint channel)
{
  VALIDATE_BASE_POINTER;
  return veData.base->StopPlayout(channel);
}

// [Base] Stop send
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_VE_StopSend(
    JNIEnv *,
    jobject,
    jint channel)
{
  VALIDATE_BASE_POINTER;
  return veData.base->StopSend(channel);
}

// [codec] Number of codecs
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_VE_NumOfCodecs(
    JNIEnv *,
    jobject)
{
  VALIDATE_CODEC_POINTER;
  return veData.codec->NumOfCodecs();
}

// [codec] Set send codec
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_VE_SetSendCodec(
    JNIEnv *,
    jobject,
    jint channel,
    jint index)
{
  VALIDATE_CODEC_POINTER;

  webrtc::CodecInst codec;

  for (int i = 0; i < veData.codec->NumOfCodecs(); ++i) {
    webrtc::CodecInst codecToList;
    veData.codec->GetCodec(i, codecToList);
    __android_log_print(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG,
                        "VE Codec list %s, pltype=%d\n",
                        codecToList.plname, codecToList.pltype);
  }

  if (veData.codec->GetCodec(index, codec) != 0) {
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                        "Failed to get codec");
    return -1;
  }
  __android_log_print(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG, "SetSendCodec %s\n",
                      codec.plname);

  return veData.codec->SetSendCodec(channel, codec);
}

// [audioprocessing] SetNSStatus
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_VE_SetNSStatus(
    JNIEnv *,
    jobject,
    jboolean enable,
    jint mode)
{
  //TODO implement
  return -1;
}

// [audioprocessing] SetAGCStatus
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_VE_SetAGCStatus(
    JNIEnv *,
    jobject,
    jboolean enable,
    jint mode)
{
  //TODO implement
  return -1;
}

// [audioprocessing] SetECStatus
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_VE_SetECStatus(
    JNIEnv *,
    jobject,
    jboolean enable,
    jint mode,
    jint AESmode,
    jint AESattenuation)
{
  //TODO implement
  return -1;
}

// [File] Start play file locally
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_VE_StartPlayingFileLocally(
    JNIEnv * env,
    jobject,
    jint channel,
    jstring fileName,
    jboolean loop)
{
  VALIDATE_FILE_POINTER;

  const char* fileNameNative = env->GetStringUTFChars(fileName, NULL);
  if (!fileNameNative) {
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                        "Could not get UTF string");
    return -1;
  }

  jint retVal = veData.file->StartPlayingFileLocally(channel,
                                                     fileNameNative,
                                                     loop);

  env->ReleaseStringUTFChars(fileName, fileNameNative);

  return retVal;
}

// [File] Stop play file locally
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_VE_StopPlayingFileLocally(
    JNIEnv *,
    jobject,
    jint channel)
{
  VALIDATE_FILE_POINTER;
  return veData.file->StopPlayingFileLocally(channel);
}

// [File] Start playing file as microphone
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_VE_StartPlayingFileAsMicrophone(
    JNIEnv *env,
    jobject,
    jint channel,
    jstring fileName,
    jboolean loop)
{
  VALIDATE_FILE_POINTER;

  const char* fileNameNative = env->GetStringUTFChars(fileName, NULL);
  if (!fileNameNative) {
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                        "Could not get UTF string");
    return -1;
  }

  jint retVal = veData.file->StartPlayingFileAsMicrophone(channel,
                                                          fileNameNative,
                                                          loop);

  env->ReleaseStringUTFChars(fileName, fileNameNative);

  return retVal;
}

// [File] Stop playing file as microphone
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_VE_StopPlayingFileAsMicrophone(
    JNIEnv *,
    jobject,
    jint channel)
{
  VALIDATE_FILE_POINTER;
  return veData.file->StopPlayingFileAsMicrophone(channel);
}

// [Volume] Set speaker volume
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_VE_SetSpeakerVolume(
    JNIEnv *,
    jobject,
    jint level)
{
  VALIDATE_VOLUME_POINTER;

  if (veData.volume->SetSpeakerVolume(level) != 0) {
    return -1;
  }

  unsigned int storedVolume = 0;
  if (veData.volume->GetSpeakerVolume(storedVolume) != 0) {
    return -1;
  }

  if (storedVolume != level) {
    return -1;
  }

  return 0;
}

// [Hardware] Set speaker volume
JNIEXPORT jint JNICALL
Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_VE_SetLoudspeakerStatus(
    JNIEnv *,
    jobject,
    jboolean enable)
{
  VALIDATE_HARDWARE_POINTER;

  if (veData.hardware->SetLoudspeakerStatus(enable) != 0) {
    return -1;
  }

  return 0;
}

//
// local function
//

// Get all sub-APIs
bool VE_GetSubApis() {
  bool getOK = true;

  // Base
  veData.base = VoEBase::GetInterface(veData.ve);
  if (!veData.base) {
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                        "Get base sub-API failed");
    getOK = false;
  }

  // Codec
  veData.codec = VoECodec::GetInterface(veData.ve);
  if (!veData.codec) {
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                        "Get codec sub-API failed");
    getOK = false;
  }

  // File
  veData.file = VoEFile::GetInterface(veData.ve);
  if (!veData.file) {
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                        "Get file sub-API failed");
    getOK = false;
  }

  // Network
  veData.netw = VoENetwork::GetInterface(veData.ve);
  if (!veData.netw) {
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                        "Get network sub-API failed");
    getOK = false;
  }

  // audioprocessing
  veData.apm = VoEAudioProcessing::GetInterface(veData.ve);
  if (!veData.apm) {
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                        "Get VoEAudioProcessing sub-API failed");
    getOK = false;
  }

  // Volume
  veData.volume = VoEVolumeControl::GetInterface(veData.ve);
  if (!veData.volume) {
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                        "Get volume sub-API failed");
    getOK = false;
  }

  // Hardware
  veData.hardware = VoEHardware::GetInterface(veData.ve);
  if (!veData.hardware) {
    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                        "Get hardware sub-API failed");
    getOK = false;
  }

  return getOK;
}

// Release all sub-APIs
bool VE_ReleaseSubApis() {
  bool releaseOK = true;

  // Base
  if (veData.base) {
    if (0 != veData.base->Release()) {
      __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                          "Release base sub-API failed");
      releaseOK = false;
    }
    else {
      veData.base = NULL;
    }
  }

  // Codec
  if (veData.codec) {
    if (0 != veData.codec->Release()) {
      __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                          "Release codec sub-API failed");
      releaseOK = false;
    }
    else {
      veData.codec = NULL;
    }
  }

  // File
  if (veData.file) {
    if (0 != veData.file->Release()) {
      __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                          "Release file sub-API failed");
      releaseOK = false;
    }
    else {
      veData.file = NULL;
    }
  }

  // Network
  if (veData.netw) {
    if (0 != veData.netw->Release()) {
      __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                          "Release network sub-API failed");
      releaseOK = false;
    }
    else {
      veData.netw = NULL;
    }
  }

  // apm
  if (veData.apm) {
    if (0 != veData.apm->Release()) {
      __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                          "Release apm sub-API failed");
      releaseOK = false;
    }
    else {
      veData.apm = NULL;
    }
  }

  // Volume
  if (veData.volume) {
    if (0 != veData.volume->Release()) {
      __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                          "Release volume sub-API failed");
      releaseOK = false;
    }
    else {
      veData.volume = NULL;
    }
  }

  // Hardware
  if (veData.hardware) {
    if (0 != veData.hardware->Release()) {
      __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                          "Release hardware sub-API failed");
      releaseOK = false;
    }
    else {
      veData.hardware = NULL;
    }
  }

  return releaseOK;
}
