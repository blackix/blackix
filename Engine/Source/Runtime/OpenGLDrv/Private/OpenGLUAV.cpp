// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "OpenGLDrvPrivate.h"

FShaderResourceViewRHIRef FOpenGLDynamicRHI::RHICreateShaderResourceView(FVertexBufferRHIParamRef VertexBufferRHI, uint32 Stride, uint8 Format)
{
	GLuint TextureID = 0;
	if ( FOpenGL::SupportsResourceView() )
	{
		DYNAMIC_CAST_OPENGLRESOURCE(VertexBuffer,VertexBuffer);

		const uint32 FormatBPP = GPixelFormats[Format].BlockBytes;

		if (FormatBPP != Stride)
		{
			UE_LOG(LogRHI, Fatal,TEXT("OpenGL 3.2 RHI supports only tightly packed texture buffers!"));
			return new FOpenGLShaderResourceView(this, 0, GL_TEXTURE_BUFFER);
		}

		const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[Format];

		FOpenGL::GenTextures(1,&TextureID);

		// Use a texture stage that's not likely to be used for draws, to avoid waiting
		CachedSetupTextureStage(GetContextStateForCurrentContext(), FOpenGL::GetMaxCombinedTextureImageUnits() - 1, GL_TEXTURE_BUFFER, TextureID, -1, 1);
		FOpenGL::TexBuffer(GL_TEXTURE_BUFFER, GLFormat.InternalFormat[0], VertexBuffer->Resource);
	}

	// No need to restore texture stage; leave it like this,
	// and the next draw will take care of cleaning it up; or
	// next operation that needs the stage will switch something else in on it.

	return new FOpenGLShaderResourceView(this,TextureID,GL_TEXTURE_BUFFER,VertexBufferRHI,Format);
}

FOpenGLShaderResourceView::~FOpenGLShaderResourceView()
{
	if (Resource && OwnsResource)
	{
		OpenGLRHI->InvalidateTextureResourceInCache( Resource );
		FOpenGL::DeleteTextures(1, &Resource);
	}
}

FUnorderedAccessViewRHIRef FOpenGLDynamicRHI::RHICreateUnorderedAccessView(FStructuredBufferRHIParamRef StructuredBufferRHI, bool bUseUAVCounter, bool bAppendBuffer)
{
	DYNAMIC_CAST_OPENGLRESOURCE(StructuredBuffer,StructuredBuffer);
	UE_LOG(LogRHI, Fatal,TEXT("%s not implemented yet"),ANSI_TO_TCHAR(__FUNCTION__)); 
	return new FOpenGLUnorderedAccessView();
}

FUnorderedAccessViewRHIRef FOpenGLDynamicRHI::RHICreateUnorderedAccessView(FTextureRHIParamRef TextureRHI)
{
	DYNAMIC_CAST_OPENGLRESOURCE(Texture,Texture);
	check(Texture->GetFlags() & TexCreate_UAV);
	return new FOpenGLTextureUnorderedAccessView(TextureRHI);
}


FOpenGLTextureUnorderedAccessView::FOpenGLTextureUnorderedAccessView( FTextureRHIParamRef InTextureRHI):
	TextureRHI(InTextureRHI)
{
	VERIFY_GL_SCOPE();
	
	FOpenGLTextureBase* Texture = GetOpenGLTextureFromRHITexture(TextureRHI);
	const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[TextureRHI->GetFormat()];

	this->Resource = Texture->Resource;
	this->Format = GLFormat.InternalFormat[0];
}


FOpenGLVertexBufferUnorderedAccessView::FOpenGLVertexBufferUnorderedAccessView(	FOpenGLDynamicRHI* InOpenGLRHI, FVertexBufferRHIParamRef InVertexBufferRHI, uint8 Format):
	VertexBufferRHI(InVertexBufferRHI),
	OpenGLRHI(InOpenGLRHI)
{
	VERIFY_GL_SCOPE();
	DYNAMIC_CAST_OPENGLRESOURCE(VertexBuffer, InVertexBuffer);


	const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[Format];

	GLuint TextureID = 0;
	FOpenGL::GenTextures(1,&TextureID);

	// Use a texture stage that's not likely to be used for draws, to avoid waiting
	OpenGLRHI->CachedSetupTextureStage(OpenGLRHI->GetContextStateForCurrentContext(), FOpenGL::GetMaxCombinedTextureImageUnits() - 1, GL_TEXTURE_BUFFER, TextureID, -1, 1);
	FOpenGL::TexBuffer(GL_TEXTURE_BUFFER, GLFormat.InternalFormat[0], InVertexBuffer->Resource);

	// No need to restore texture stage; leave it like this,
	// and the next draw will take care of cleaning it up; or
	// next operation that needs the stage will switch something else in on it.

	this->Resource = TextureID;
	this->Format = GLFormat.InternalFormat[0];
}

FOpenGLVertexBufferUnorderedAccessView::~FOpenGLVertexBufferUnorderedAccessView()
{
	if (Resource)
	{
		OpenGLRHI->InvalidateTextureResourceInCache( Resource );
		FOpenGL::DeleteTextures(1, &Resource);
	}
}


FUnorderedAccessViewRHIRef FOpenGLDynamicRHI::RHICreateUnorderedAccessView(FVertexBufferRHIParamRef VertexBufferRHI,uint8 Format)
{
	DYNAMIC_CAST_OPENGLRESOURCE(VertexBuffer,VertexBuffer);
	return new FOpenGLVertexBufferUnorderedAccessView(this, VertexBufferRHI, Format);
}

FShaderResourceViewRHIRef FOpenGLDynamicRHI::RHICreateShaderResourceView(FStructuredBufferRHIParamRef StructuredBufferRHI)
{
	DYNAMIC_CAST_OPENGLRESOURCE(StructuredBuffer,StructuredBuffer);
	UE_LOG(LogRHI, Fatal,TEXT("OpenGL RHI doesn't support RHICreateShaderResourceView yet!"));
	return new FOpenGLShaderResourceView(this,0,GL_TEXTURE_BUFFER);
}

void FOpenGLDynamicRHI::RHIClearUAV(FUnorderedAccessViewRHIParamRef UnorderedAccessViewRHI, const uint32* Values)
{
	UE_LOG(LogRHI, Fatal,TEXT("OpenGL RHI doesn't support RHIClearUAV."));
	GPUProfilingData.RegisterGPUWork(1);
}
