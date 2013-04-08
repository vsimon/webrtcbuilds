/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.videoengine;

import java.io.IOException;
import java.util.Locale;
import java.util.concurrent.locks.ReentrantLock;

import org.webrtc.videoengine.CaptureCapabilityAndroid;
import org.webrtc.videoengine.VideoCaptureDeviceInfoAndroid.AndroidVideoCaptureDevice;

import android.graphics.ImageFormat;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.SurfaceTexture;
import android.graphics.YuvImage;
import android.hardware.Camera;
import android.hardware.Camera.PreviewCallback;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceHolder.Callback;

public class VideoCaptureAndroid implements PreviewCallback, Callback {

    private final static String TAG = "WEBRTC-JC";

    private Camera camera;
    private AndroidVideoCaptureDevice currentDevice = null;
    public ReentrantLock previewBufferLock = new ReentrantLock();
    // This lock takes sync with StartCapture and SurfaceChanged
    private ReentrantLock captureLock = new ReentrantLock();
    private int PIXEL_FORMAT = ImageFormat.NV21;
    PixelFormat pixelFormat = new PixelFormat();
    // True when the C++ layer has ordered the camera to be started.
    private boolean isCaptureStarted = false;
    private boolean isCaptureRunning = false;
    private boolean isSurfaceReady = false;

    private final int numCaptureBuffers = 3;
    private int expectedFrameSize = 0;
    private int orientation = 0;
    private int id = 0;
    // C++ callback context variable.
    private long context = 0;
    private SurfaceHolder localPreview = null;
    private SurfaceTexture dummySurfaceTexture = null;
    // True if this class owns the preview video buffers.
    private boolean ownsBuffers = false;

    private int mCaptureWidth = -1;
    private int mCaptureHeight = -1;
    private int mCaptureFPS = -1;

    public static
    void DeleteVideoCaptureAndroid(VideoCaptureAndroid captureAndroid) {
        Log.d(TAG, "DeleteVideoCaptureAndroid");
        if (captureAndroid.camera == null) {
            return;
        }

        captureAndroid.StopCapture();
        captureAndroid.camera.release();
        captureAndroid.camera = null;
        captureAndroid.context = 0;
    }

    public VideoCaptureAndroid(int in_id, long in_context, Camera in_camera,
            AndroidVideoCaptureDevice in_device) {
        id = in_id;
        context = in_context;
        camera = in_camera;
        currentDevice = in_device;
    }

    private int tryStartCapture(int width, int height, int frameRate) {
        if (camera == null) {
            Log.e(TAG, "Camera not initialized %d" + id);
            return -1;
        }

        Log.d(TAG, "tryStartCapture: " + width +
            "x" + height +", frameRate: " + frameRate +
            ", isCaptureRunning: " + isCaptureRunning +
            ", isSurfaceReady: " + isSurfaceReady +
            ", isCaptureStarted: " + isCaptureStarted);

        if (isCaptureRunning || !isCaptureStarted) {
            return 0;
        }

        CaptureCapabilityAndroid currentCapability =
                new CaptureCapabilityAndroid();
        currentCapability.width = width;
        currentCapability.height = height;
        currentCapability.maxFPS = frameRate;
        PixelFormat.getPixelFormatInfo(PIXEL_FORMAT, pixelFormat);

        Camera.Parameters parameters = camera.getParameters();
        parameters.setPreviewSize(currentCapability.width,
                currentCapability.height);
        parameters.setPreviewFormat(PIXEL_FORMAT);
        parameters.setPreviewFrameRate(currentCapability.maxFPS);
        camera.setParameters(parameters);

        int bufSize = width * height * pixelFormat.bitsPerPixel / 8;
        byte[] buffer = null;
        for (int i = 0; i < numCaptureBuffers; i++) {
            buffer = new byte[bufSize];
            camera.addCallbackBuffer(buffer);
        }
        camera.setPreviewCallbackWithBuffer(this);
        ownsBuffers = true;

        camera.startPreview();
        previewBufferLock.lock();
        expectedFrameSize = bufSize;
        isCaptureRunning = true;
        previewBufferLock.unlock();

        return 0;
    }

    public int StartCapture(int width, int height, int frameRate) {
        Log.d(TAG, "StartCapture width " + width +
                " height " + height +" frame rate " + frameRate);
        // Get the local preview SurfaceHolder from the static render class
        localPreview = ViERenderer.GetLocalRenderer();
        if (localPreview != null) {
            if (localPreview.getSurface() != null &&
                localPreview.getSurface().isValid()) {
                surfaceCreated(localPreview);
            }
            localPreview.addCallback(this);
        } else {
          // No local renderer.  Camera won't capture without
          // setPreview{Texture,Display}, so we create a dummy SurfaceTexture
          // and hand it over to Camera, but never listen for frame-ready
          // callbacks, and never call updateTexImage on it.
          captureLock.lock();
          try {
            dummySurfaceTexture = new SurfaceTexture(42);
            camera.setPreviewTexture(dummySurfaceTexture);
          } catch (IOException e) {
            throw new RuntimeException(e);
          }
          captureLock.unlock();
        }

        captureLock.lock();
        isCaptureStarted = true;
        mCaptureWidth = width;
        mCaptureHeight = height;
        mCaptureFPS = frameRate;

        int res = tryStartCapture(mCaptureWidth, mCaptureHeight, mCaptureFPS);

        captureLock.unlock();
        return res;
    }

    public int StopCapture() {
        Log.d(TAG, "StopCapture");
        try {
            previewBufferLock.lock();
            isCaptureRunning = false;
            previewBufferLock.unlock();
            camera.stopPreview();
            camera.setPreviewCallbackWithBuffer(null);
        } catch (RuntimeException e) {
            Log.e(TAG, "Failed to stop camera", e);
            return -1;
        }

        isCaptureStarted = false;
        return 0;
    }

    native void ProvideCameraFrame(byte[] data, int length, long captureObject);

    public void onPreviewFrame(byte[] data, Camera camera) {
        previewBufferLock.lock();

        // The following line is for debug only
        // Log.v(TAG, "preview frame length " + data.length +
        //            " context" + context);
        if (isCaptureRunning) {
            // If StartCapture has been called but not StopCapture
            // Call the C++ layer with the captured frame
            if (data.length == expectedFrameSize) {
                ProvideCameraFrame(data, expectedFrameSize, context);
                if (ownsBuffers) {
                    // Give the video buffer to the camera service again.
                    camera.addCallbackBuffer(data);
                }
            }
        }
        previewBufferLock.unlock();
    }

    // Sets the rotation of the preview render window.
    // Does not affect the captured video image.
    public void SetPreviewRotation(int rotation) {
        Log.v(TAG, "SetPreviewRotation:" + rotation);

        if (camera == null) {
            return;
        }

        int resultRotation = 0;
        if (currentDevice.frontCameraType ==
            VideoCaptureDeviceInfoAndroid.FrontFacingCameraType.Android23) {
            // this is a 2.3 or later front facing camera.
            // SetDisplayOrientation will flip the image horizontally
            // before doing the rotation.
            resultRotation = ( 360 - rotation ) % 360; // compensate the mirror
        }
        else {
            // Back facing or 2.2 or previous front camera
            resultRotation = rotation;
        }
        camera.setDisplayOrientation(resultRotation);
    }

    public void surfaceChanged(SurfaceHolder holder,
                               int format, int width, int height) {
        Log.d(TAG, "VideoCaptureAndroid::surfaceChanged");
    }

    public void surfaceCreated(SurfaceHolder holder) {
        Log.d(TAG, "VideoCaptureAndroid::surfaceCreated");
        captureLock.lock();
        try {
            camera.setPreviewDisplay(holder);
        } catch (IOException e) {
            Log.e(TAG, "Failed to set preview surface!", e);
        }
        captureLock.unlock();
    }

    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.d(TAG, "VideoCaptureAndroid::surfaceDestroyed");
        captureLock.lock();
        try {
            if (camera != null) {
                camera.setPreviewDisplay(null);
            }
        } catch (IOException e) {
            Log.e(TAG, "Failed to clear preview surface!", e);
        }
        captureLock.unlock();
    }
}
