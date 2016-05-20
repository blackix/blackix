
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "RendererPrivate.h"
#include "WideCustomResolveShaders.h"
#include "ShaderParameterUtils.h"


IMPLEMENT_SHADER_TYPE(, FWideCustomResolveVS, TEXT("WideCustomResolveShaders"), TEXT("WideCustomResolveVS"), SF_Vertex);

#define IMPLEMENT_RESOLVE_SHADER(Width, MSAA) \
	typedef FWideCustomResolvePS<Width,MSAA> FWideCustomResolve##Width##_##MSAA##xPS; \
	IMPLEMENT_SHADER_TYPE(template<>, FWideCustomResolve##Width##_##MSAA##xPS, TEXT("WideCustomResolveShaders"), TEXT("WideCustomResolvePS"), SF_Pixel)

IMPLEMENT_RESOLVE_SHADER(0, 1);
IMPLEMENT_RESOLVE_SHADER(2, 0);
IMPLEMENT_RESOLVE_SHADER(2, 1);
IMPLEMENT_RESOLVE_SHADER(2, 2);
IMPLEMENT_RESOLVE_SHADER(2, 3);
IMPLEMENT_RESOLVE_SHADER(4, 0);
IMPLEMENT_RESOLVE_SHADER(4, 1);
IMPLEMENT_RESOLVE_SHADER(4, 2);
IMPLEMENT_RESOLVE_SHADER(4, 3);