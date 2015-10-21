/********************************************************************************//**
\file      OVR_Version.h
\brief     This header provides LibOVR version identification.
\copyright Copyright 2014 Oculus VR, LLC All Rights reserved.
*************************************************************************************/

#ifndef OVR_Version_h
#define OVR_Version_h


//---BEGIN REDACTION SECTION---//
// The build strip tool will remove this section.

/// OVR_USES_REDACTION
///
/// Use OVR_USES_REDACTION at the top of files that use OVR_INTERNAL_CODE or OVR_PARTNER_CODE 
/// to make sure that this header is included properly.
#define OVR_USES_REDACTION

/// OVR_INTERNAL_CODE, OVR_PARTNER_CODE
///
/// Note that you can also modify these in an internal build to simulate a partner or
/// public build without using a special script to strip these out. You should comment
/// out the OVR_INTERNAL_CODE and OVR_PARTNER_CODE sections below to simulate a public
/// build, and comment out just the OVR_INTERNAL_CODE section below to simulate a partner build.
///
/// An internal build is one we use only internally to Oculus during development (both debug 
/// and release). It includes sections of code within OVR_INTERNAL_CODE and OVR_PARTNER_CODE.
/// A partner build is one that's sent only to NDA'd developers which we work closely with. It includes
/// sections of code that are within OVR_PARTNER_CODE.
/// A public build is one that we ship. It doesn't include code within OVR_INTERNAL_CODE or OVR_PARTNER_CODE
///
/// When writing code to use either OVR_INTERNAL_CODE or OVR_PARTNER_CODE, use #ifdef rather than #if, 
/// and make the #ifdef start on the first column of the line. 
///
/// Example usage:
///     OVR_USES_REDACTION
///     [...]
///     int Function()
///     {
///         int value = 0;
///
///     #ifdef OVR_INTERNAL_CODE
///         value++;
///     #endif
/// 
///         return value;
///     }  
///
#ifndef OVR_INTERNAL_CODE
    #define OVR_INTERNAL_CODE sizeof(NoCompile) == BuiltForInternalOnly
#endif
#ifndef OVR_PARTNER_CODE
    #define OVR_PARTNER_CODE sizeof(NoCompile) == BuiltForPartnersOrInternal
#endif

#ifdef OVR_INTERNAL_CODE
    /// Use OVR_INTERNAL_FILE to have the source file removed Partner and Public builds.
    #define OVR_INTERNAL_FILE

    /// Use OVR_PARTNER_FILE to have the source file removed from Public builds.
    #define OVR_PARTNER_FILE
#else // OVR_INTERNAL_CODE
    #ifdef OVR_PARTNER_CODE
        /// Use OVR_PARTNER_FILE to have the source file removed from Public builds.
        #define OVR_PARTNER_FILE
    #endif // OVR_PARTNER_CODE
#endif // OVR_INTERNAL_CODE

//---END REDACTION SECTION---//

/// Conventional string-ification macro.
#if !defined(OVR_STRINGIZE)
    #define OVR_STRINGIZEIMPL(x) #x
    #define OVR_STRINGIZE(x)     OVR_STRINGIZEIMPL(x)
#endif


// We are on major version 6 of the beta pre-release SDK. At some point we will
// transition to product version 1 and reset the major version back to 1 (first
// product release, version 1.0).
#define OVR_PRODUCT_VERSION 0
#define OVR_MAJOR_VERSION   8
#define OVR_MINOR_VERSION   0
#define OVR_PATCH_VERSION   0
#define OVR_BUILD_NUMBER    0

// This is the major version of the service that the DLL is compatible with.
// When we backport changes to old versions of the DLL we update the old DLLs
// to move this version number up to the latest version.
// The DLL is responsible for checking that the service is the version it supports
// and returning an appropriate error message if it has not been made compatible.
#define OVR_DLL_COMPATIBLE_MAJOR_VERSION 8

#ifdef OVR_INTERNAL_CODE
// Feature version means whether it is internal, partner, or public code.
// 2 = internal, 1 = partner, 0 = public code.
#define OVR_FEATURE_VERSION 2
#else // OVR_INTERNAL_CODE
#ifdef OVR_PARTNER_CODE
#define OVR_FEATURE_VERSION 1
#else // OVR_PARTNER_CODE
#define OVR_FEATURE_VERSION 0
#endif // OVR_PARTNER_CODE
#endif // OVR_INTERNAL_CODE


/// "Product.Major.Minor.Patch"
#if !defined(OVR_VERSION_STRING)
    #define OVR_VERSION_STRING  OVR_STRINGIZE(OVR_PRODUCT_VERSION.OVR_MAJOR_VERSION.OVR_MINOR_VERSION.OVR_PATCH_VERSION)
#endif


/// "Product.Major.Minor.Patch.Build"
#if !defined(OVR_DETAILED_VERSION_STRING)
    #define OVR_DETAILED_VERSION_STRING OVR_STRINGIZE(OVR_PRODUCT_VERSION.OVR_MAJOR_VERSION.OVR_MINOR_VERSION.OVR_PATCH_VERSION.OVR_BUILD_NUMBER)
#endif

// This is the product version for the Oculus Display Driver. A continuous
// process will propagate this value to all dependent files
#define OVR_DISPLAY_DRIVER_PRODUCT_VERSION "1.2.8.0"

// This is the product version for the Oculus Position Tracker Driver. A
// continuous process will propagate this value to all dependent files
#define OVR_POSITIONAL_TRACKER_DRIVER_PRODUCT_VERSION "1.0.14.0"

/// \brief file description for version info
/// This appears in the user-visible file properties. It is intended to convey publicly
/// available additional information such as feature builds.
#if !defined(OVR_FILE_DESCRIPTION_STRING)
    #if defined(_DEBUG)
        #define OVR_FILE_DESCRIPTION_STRING "dev build debug"
    #else
        #define OVR_FILE_DESCRIPTION_STRING "dev build"
    #endif
#endif


#endif // OVR_Version_h
