/************************************************************************************

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

Licensed under the Oculus VR Rift SDK License Version 3.2 (the "License");
you may not use the Oculus VR Rift SDK except in compliance with the License,
which is provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at

http://www.oculusvr.com/licenses/LICENSE-3.2

Unless required by applicable law or agreed to in writing, the Oculus VR SDK
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/

#ifndef OVR_Plugin_MixedReality_h
#define OVR_Plugin_MixedReality_h

#include "OVR_Plugin_Types.h"

#if OVRP_MIXED_REALITY_PRIVATE
#include "OVR_Plugin_MixedReality_Private.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

//////////////////// Tracked Camera //////////////////////////

/// Initialize Mixed Reality functionalities
OVRP_EXPORT ovrpResult ovrp_InitializeMixedReality();

/// Shutdown Mixed Reality functionalities
OVRP_EXPORT ovrpResult ovrp_ShutdownMixedReality();

/// Check whether Mixed Reality functionalities has been initialized
OVRP_EXPORT ovrpBool ovrp_GetMixedRealityInitialized();

/// Update external camera. Need to be called before accessing the camera count or individual camera information
OVRP_EXPORT ovrpResult ovrp_UpdateExternalCamera();

/// Get the number of external cameras
OVRP_EXPORT ovrpResult ovrp_GetExternalCameraCount(int* cameraCount);

/// Get the name of an external camera
OVRP_EXPORT ovrpResult ovrp_GetExternalCameraName(int cameraId, char cameraName[OVRP_EXTERNAL_CAMERA_NAME_SIZE]);

/// Get intrinsics of an external camera
OVRP_EXPORT ovrpResult ovrp_GetExternalCameraIntrinsics(int cameraId, ovrpCameraIntrinsics* cameraIntrinsics);

/// Get extrinsics of an external camera
OVRP_EXPORT ovrpResult ovrp_GetExternalCameraExtrinsics(int cameraId, ovrpCameraExtrinsics* cameraExtrinsics);


//////////////////// Camera Devices //////////////////////////

/// Retrieve all supported camera devices
OVRP_EXPORT ovrpResult
ovrp_EnumerateAllCameraDevices(ovrpCameraDevice* deviceArray, int deviceArraySize, int* deviceCount);

/// Retrive all supported camera devices which is also available
OVRP_EXPORT ovrpResult
ovrp_EnumerateAvailableCameraDevices(ovrpCameraDevice* deviceArray, int deviceArraySize, int* deviceCount);

/// Update all the opened cameras. Should be called on each frame from the main thread
OVRP_EXPORT ovrpResult ovrp_UpdateCameraDevices();

/// Check the camera device availablity
OVRP_EXPORT ovrpResult ovrp_IsCameraDeviceAvailable2(ovrpCameraDevice camera, ovrpBool* available);

/// The PreferredColorFrameSize is only a hint. The final ColorFrameSize could be different
OVRP_EXPORT ovrpResult
ovrp_SetCameraDevicePreferredColorFrameSize(ovrpCameraDevice camera, ovrpSizei preferredColorFrameSize);

/// Open the camera device
OVRP_EXPORT ovrpResult ovrp_OpenCameraDevice(ovrpCameraDevice camera);

/// Close the camera device
OVRP_EXPORT ovrpResult ovrp_CloseCameraDevice(ovrpCameraDevice camera);

/// Check if the camera device has been opened
OVRP_EXPORT ovrpResult ovrp_HasCameraDeviceOpened2(ovrpCameraDevice camera, ovrpBool* opened);

/// Check if there the color frame is available for the camera device
OVRP_EXPORT ovrpResult ovrp_IsCameraDeviceColorFrameAvailable2(ovrpCameraDevice camera, ovrpBool* available);

/// Retrieve the dimension of the current color frame
OVRP_EXPORT ovrpResult ovrp_GetCameraDeviceColorFrameSize(ovrpCameraDevice camera, ovrpSizei* colorFrameSize);

/// Retrieve the raw data of the currect color frame (in BGRA arrangement)
OVRP_EXPORT ovrpResult ovrp_GetCameraDeviceColorFrameBgraPixels(
    ovrpCameraDevice camera,
    const ovrpByte** colorFrameBgraPixels,
    int* colorFrameRowPitch);

#ifdef __cplusplus
}
#endif

#endif