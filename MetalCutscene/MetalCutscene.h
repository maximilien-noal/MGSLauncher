// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the METALCUTSCENE_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// METALCUTSCENE_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef METALCUTSCENE_EXPORTS
#define METALCUTSCENE_API __declspec(dllexport)
#else
#define METALCUTSCENE_API __declspec(dllimport)
#endif

// This class is exported from the MetalCutscene.dll
class METALCUTSCENE_API CMetalCutscene {
public:
	CMetalCutscene(void);
	// TODO: add your methods here.
};

METALCUTSCENE_API int fnMetalCutscene(void);
