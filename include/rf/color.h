#pragma once
#include "rf_common.h"


int    const LAMBDA_MIN = 360;
int    const LAMBDA_MAX = 830;
real32 const LAMBDA_R = 680.0f;
real32 const LAMBDA_G = 550.0f;
real32 const LAMBDA_B = 440.0f;

// Conversion factor between watts and lumens
real32 const MAX_LUMINOUS_EFFICACY = 683.f;

// Values from "CIE (1931) 2-deg color matching functions", see
// "http://web.archive.org/web/20081228084047/
//    http://www.cvrl.org/database/data/cmfs/ciexyz31.txt".
extern const real32 CIE_2_DEG_COLOR_MATCHING_FUNCTIONS[380];

// The conversion matrix from XYZ to linear sRGB color spaces.
// Values from https://en.wikipedia.org/wiki/SRGB.
extern const real32 XYZ_TO_SRGB[9];


// Match a wavelength to a CIE color tabulated value
real32 CieColorMatchingFunctionTableValue(real32 Wavelength, int Col);

// Returns the Wavelength spectrum value associated with the given Wavelength index
/// @param Wavelengths : array of wavelength indices (usually integers from LAMBDA_MIN to LAMBDA_MAX)
/// @param WavelengthFunctions : array of wavelength spectrum values associated with each index
/// @param N : number of wavelength indices/values in those arrays
/// @param Wavelength : The queried FP index
real32 Interpolate(real32 const *Wavelengths, real32 const *WavelengthFunctions, int N, real32 Wavelength);

/// Returns the sRGB color channels for a given set of wavelength functions (i.e., wavelength values associated to each wavelength index)
/// Uses Interpolate() above internally for each R,G,B color channel
/// @param Scale : constant scale for each three R,G,B channels
vec3f ConvertSpectrumToSRGB(real32 const *Wavelengths, real32 const *WavelengthFunctions, int N, real32 Scale);

/// Converts a function of wavelength to linear sRGB.
/// Wavelengths and Spectrum arrays have the same size N
vec3f ConvertSpectrumToLuminanceFactors(real32 const *Wavelengths, real32 *Spectrum, int N);