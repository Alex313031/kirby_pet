#ifndef PET_LIB_DLL_EXPORT_H_
#define PET_LIB_DLL_EXPORT_H_

// Consumers: don't import this directly, import pet_lib.h instead!

// How pet_lib's public functions are declared, by how the lib is being built
// or consumed. Two defines decide it:
//   PET_LIB_SHARED - selects the .dll, on BOTH sides: the DLL build sets it,
//                    and so does any consumer linking pet_lib.dll.
//   PET_LIB_       - the lib's own private define (BUILD.gn / Makefile set
//                    it for the lib's sources only, never for a consumer).
//
//   PET_LIB_SHARED + PET_LIB_  -> building pet_lib.dll itself: export
//   PET_LIB_SHARED alone       -> a consumer of pet_lib.dll:   import
//   neither                    -> the static lib, either side: plain functions
//
// PET_LIB_DLL comes out defined only inside the DLL build; pet.cc keys its
// DllMain on it and pet_lib.rc its VFT_DLL file type.
#if defined(PET_LIB_SHARED)
 #if defined(PET_LIB_)
  #define LIB_API __declspec(dllexport)
  #define PET_LIB_DLL
 #else
  #define LIB_API __declspec(dllimport)
 #endif
#else
 #define LIB_API
#endif

#endif // PET_LIB_DLL_EXPORT_H_
