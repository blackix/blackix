// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
//
#include "HMDPrivatePCH.h"
#include "OculusRiftHMD.h"

#if OCULUS_RIFT_SUPPORTED_PLATFORMS

#if defined(OVR_D3D_VERSION) && (OVR_D3D_VERSION == 11)

#include "D3D11RHIPrivate.h"
#include "D3D11Util.h"

#ifndef WINDOWS_PLATFORM_TYPES_GUARD
#include "AllowWindowsPlatformTypes.h"
#endif
#include "OVR_CAPI_D3D.h"

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "PostProcess/PostProcessHMD.h"
#include "ScreenRendering.h"

#include "SlateBasics.h"


//-------------------------------------------------------------------------------------------------
// FD3D11Texture2DSet
//-------------------------------------------------------------------------------------------------

class FD3D11Texture2DSet : public FD3D11Texture2D
{
public:
	FD3D11Texture2DSet(
		class FD3D11DynamicRHI* InD3DRHI,
		ID3D11Texture2D* InResource,
		ID3D11ShaderResourceView* InShaderResourceView,
		bool bInCreatedRTVsPerSlice,
		int32 InRTVArraySize,
		const TArray<TRefCountPtr<ID3D11RenderTargetView> >& InRenderTargetViews,
		TRefCountPtr<ID3D11DepthStencilView>* InDepthStencilViews,
		uint32 InSizeX,
		uint32 InSizeY,
		uint32 InSizeZ,
		uint32 InNumMips,
		uint32 InNumSamples,
		EPixelFormat InFormat,
		bool bInCubemap,
		uint32 InFlags,
		bool bInPooled
		)
		: FD3D11Texture2D(
		InD3DRHI,
		InResource,
		InShaderResourceView,
		bInCreatedRTVsPerSlice,
		InRTVArraySize,
		InRenderTargetViews,
		InDepthStencilViews,
		InSizeX,
		InSizeY,
		InSizeZ,
		InNumMips,
		InNumSamples,
		InFormat,
		bInCubemap,
		InFlags,
		bInPooled,
		FClearValueBinding::None
		)
	{
		TextureSet = nullptr;
	}

	void ReleaseResources(ovrSession InOvrSession);
	void SwitchToNextElement();
	void AddTexture(ID3D11Texture2D*, ID3D11ShaderResourceView*, TArray<TRefCountPtr<ID3D11RenderTargetView> >* = nullptr);

	ovrSwapTextureSet* GetTextureSet() const { return TextureSet; }

	static FD3D11Texture2DSet* D3D11CreateTexture2DSet(
		FD3D11DynamicRHI* InD3D11RHI,
		ovrSwapTextureSet* InTextureSet,
		const D3D11_TEXTURE2D_DESC& InDsDesc,
		EPixelFormat InFormat,
		uint32 InFlags
		);
protected:
	void InitWithCurrentElement();

	struct TextureElement
	{
		TRefCountPtr<ID3D11Texture2D> Texture;
		TRefCountPtr<ID3D11ShaderResourceView> SRV;
		TArray<TRefCountPtr<ID3D11RenderTargetView> > RTVs;
	};
	TArray<TextureElement> Textures;

	ovrSwapTextureSet* TextureSet;
};

void FD3D11Texture2DSet::AddTexture(ID3D11Texture2D* InTexture, ID3D11ShaderResourceView* InSRV, TArray<TRefCountPtr<ID3D11RenderTargetView> >* InRTVs)
{
	TextureElement element;
	element.Texture = InTexture;
	element.SRV = InSRV;
	if (InRTVs)
	{
		element.RTVs.Empty(InRTVs->Num());
		for (int32 i = 0; i < InRTVs->Num(); ++i)
		{
			element.RTVs.Add((*InRTVs)[i]);
		}
	}
	Textures.Push(element);
}

void FD3D11Texture2DSet::SwitchToNextElement()
{
	check(TextureSet);
	check(TextureSet->TextureCount == Textures.Num());

	TextureSet->CurrentIndex = (TextureSet->CurrentIndex + 1) % TextureSet->TextureCount;
	InitWithCurrentElement();
}

void FD3D11Texture2DSet::InitWithCurrentElement()
{
	check(TextureSet);
	check(TextureSet->TextureCount == Textures.Num());

	Resource = Textures[TextureSet->CurrentIndex].Texture;
	ShaderResourceView = Textures[TextureSet->CurrentIndex].SRV;

	RenderTargetViews.Empty(Textures[TextureSet->CurrentIndex].RTVs.Num());
	for (int32 i = 0; i < Textures[TextureSet->CurrentIndex].RTVs.Num(); ++i)
	{
		RenderTargetViews.Add(Textures[TextureSet->CurrentIndex].RTVs[i]);
	}
}

void FD3D11Texture2DSet::ReleaseResources(ovrSession InOvrSession)
{
	if (TextureSet)
	{
		UE_LOG(LogHMD, Log, TEXT("Freeing textureSet 0x%p"), TextureSet);
		ovr_DestroySwapTextureSet(InOvrSession, TextureSet);
		TextureSet = nullptr;
	}
	Textures.Empty(0);
}

FD3D11Texture2DSet* FD3D11Texture2DSet::D3D11CreateTexture2DSet(
	FD3D11DynamicRHI* InD3D11RHI,
	ovrSwapTextureSet* InTextureSet,
	const D3D11_TEXTURE2D_DESC& InDsDesc,
	EPixelFormat InFormat,
	uint32 InFlags
	)
{
	check(InTextureSet);

	TArray<TRefCountPtr<ID3D11RenderTargetView> > TextureSetRenderTargetViews;
	FD3D11Texture2DSet* NewTextureSet = new FD3D11Texture2DSet(
		InD3D11RHI,
		nullptr,
		nullptr,
		false,
		1,
		TextureSetRenderTargetViews,
		/*DepthStencilViews=*/ NULL,
		InDsDesc.Width,
		InDsDesc.Height,
		0,
		InDsDesc.MipLevels,
		InDsDesc.SampleDesc.Count,
		InFormat,
		/*bInCubemap=*/ false,
		InFlags,
		/*bPooledTexture=*/ false
		);

	const uint32 TexCount = InTextureSet->TextureCount;
	const bool bSRGB = (InFlags & TexCreate_SRGB) != 0;

	const DXGI_FORMAT PlatformResourceFormat = (DXGI_FORMAT)GPixelFormats[InFormat].PlatformFormat;
	const DXGI_FORMAT PlatformShaderResourceFormat = FindShaderResourceDXGIFormat(PlatformResourceFormat, bSRGB);
	const DXGI_FORMAT PlatformRenderTargetFormat = FindShaderResourceDXGIFormat(PlatformResourceFormat, bSRGB);
	D3D11_RTV_DIMENSION RenderTargetViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	if (InDsDesc.SampleDesc.Count > 1)
	{
		RenderTargetViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
	}
	for (uint32 i = 0; i < TexCount; ++i)
	{
		ovrD3D11Texture D3DTex;
		D3DTex.Texture = InTextureSet->Textures[i];

		TArray<TRefCountPtr<ID3D11RenderTargetView> > RenderTargetViews;
		if (InFlags & TexCreate_RenderTargetable)
		{
			// Create a render target view for each mip
			for (uint32 MipIndex = 0; MipIndex < InDsDesc.MipLevels; MipIndex++)
			{
				check(!(InFlags & TexCreate_TargetArraySlicesIndependently)); // not supported
				D3D11_RENDER_TARGET_VIEW_DESC RTVDesc;
				FMemory::Memzero(&RTVDesc, sizeof(RTVDesc));
				RTVDesc.Format = PlatformRenderTargetFormat;
				RTVDesc.ViewDimension = RenderTargetViewDimension;
				RTVDesc.Texture2D.MipSlice = MipIndex;

				TRefCountPtr<ID3D11RenderTargetView> RenderTargetView;
				VERIFYD3D11RESULT(InD3D11RHI->GetDevice()->CreateRenderTargetView(D3DTex.D3D11.pTexture, &RTVDesc, RenderTargetView.GetInitReference()));
				RenderTargetViews.Add(RenderTargetView);
			}
		}

		TRefCountPtr<ID3D11ShaderResourceView> ShaderResourceView = D3DTex.D3D11.pSRView;

		// Create a shader resource view for the texture.
		if (!ShaderResourceView && (InFlags & TexCreate_ShaderResource))
		{
			D3D11_SRV_DIMENSION ShaderResourceViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
			SRVDesc.Format = PlatformShaderResourceFormat;

			SRVDesc.ViewDimension = ShaderResourceViewDimension;
			SRVDesc.Texture2D.MostDetailedMip = 0;
			SRVDesc.Texture2D.MipLevels = InDsDesc.MipLevels;

			VERIFYD3D11RESULT(InD3D11RHI->GetDevice()->CreateShaderResourceView(D3DTex.D3D11.pTexture, &SRVDesc, ShaderResourceView.GetInitReference()));

			check(IsValidRef(ShaderResourceView));
		}

		NewTextureSet->AddTexture(D3DTex.D3D11.pTexture, ShaderResourceView, &RenderTargetViews);
	}

	NewTextureSet->TextureSet = InTextureSet;
	NewTextureSet->InitWithCurrentElement();
	return NewTextureSet;
}

static FD3D11Texture2D* D3D11CreateTexture2DAlias(
	FD3D11DynamicRHI* InD3D11RHI,
	ID3D11Texture2D* InResource,
	ID3D11ShaderResourceView* InShaderResourceView,
	uint32 InSizeX,
	uint32 InSizeY,
	uint32 InSizeZ,
	uint32 InNumMips,
	uint32 InNumSamples,
	EPixelFormat InFormat,
	uint32 InFlags)
{
	const bool bSRGB = (InFlags & TexCreate_SRGB) != 0;

	const DXGI_FORMAT PlatformResourceFormat = (DXGI_FORMAT)GPixelFormats[InFormat].PlatformFormat;
	const DXGI_FORMAT PlatformShaderResourceFormat = FindShaderResourceDXGIFormat(PlatformResourceFormat, bSRGB);
	const DXGI_FORMAT PlatformRenderTargetFormat = FindShaderResourceDXGIFormat(PlatformResourceFormat, bSRGB);
	D3D11_RTV_DIMENSION RenderTargetViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	if (InNumSamples > 1)
	{
		RenderTargetViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
	}

	TArray<TRefCountPtr<ID3D11RenderTargetView> > RenderTargetViews;

	if (InFlags & TexCreate_RenderTargetable)
	{
		// Create a render target view for each mip
		for (uint32 MipIndex = 0; MipIndex < InNumMips; MipIndex++)
		{
			check(!(InFlags & TexCreate_TargetArraySlicesIndependently)); // not supported
			D3D11_RENDER_TARGET_VIEW_DESC RTVDesc;
			FMemory::Memzero(&RTVDesc, sizeof(RTVDesc));
			RTVDesc.Format = PlatformRenderTargetFormat;
			RTVDesc.ViewDimension = RenderTargetViewDimension;
			RTVDesc.Texture2D.MipSlice = MipIndex;

			TRefCountPtr<ID3D11RenderTargetView> RenderTargetView;
			VERIFYD3D11RESULT(InD3D11RHI->GetDevice()->CreateRenderTargetView(InResource, &RTVDesc, RenderTargetView.GetInitReference()));
			RenderTargetViews.Add(RenderTargetView);
		}
	}

	TRefCountPtr<ID3D11ShaderResourceView> ShaderResourceView;

	// Create a shader resource view for the texture.
	if (!InShaderResourceView && (InFlags & TexCreate_ShaderResource))
	{
		D3D11_SRV_DIMENSION ShaderResourceViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
		SRVDesc.Format = PlatformShaderResourceFormat;

		SRVDesc.ViewDimension = ShaderResourceViewDimension;
		SRVDesc.Texture2D.MostDetailedMip = 0;
		SRVDesc.Texture2D.MipLevels = InNumMips;

		VERIFYD3D11RESULT(InD3D11RHI->GetDevice()->CreateShaderResourceView(InResource, &SRVDesc, ShaderResourceView.GetInitReference()));

		check(IsValidRef(ShaderResourceView));
	}
	else
	{
		ShaderResourceView = InShaderResourceView;
	}

	FD3D11Texture2D* NewTexture = new FD3D11Texture2D(
		InD3D11RHI,
		InResource,
		ShaderResourceView,
		false,
		1,
		RenderTargetViews,
		/*DepthStencilViews=*/ NULL,
		InSizeX,
		InSizeY,
		InSizeZ,
		InNumMips,
		InNumSamples,
		InFormat,
		/*bInCubemap=*/ false,
		InFlags,
		/*bPooledTexture=*/ false,
		FClearValueBinding::None
		);

	return NewTexture;
}


//-------------------------------------------------------------------------------------------------
// FLateLatchingFrameD3D
//-------------------------------------------------------------------------------------------------

FLateLatchingFrameD3D::FLateLatchingFrameD3D(FLateLatchingD3D* InLateLatchingD3D) :
	FLateLatchingFrame(InLateLatchingD3D)
{
	check(IsInRenderingThread());

	InitRHISucceeded = false;
	InitResource();
}


FLateLatchingFrameD3D::~FLateLatchingFrameD3D()
{
	check(IsInRenderingThread());

	ReleaseResource();
}


void FLateLatchingFrameD3D::InitRHI()
{
	check(IsInRenderingThread());

	HRESULT hr;
	
	// Device
	ID3D11Device* Device = (ID3D11Device*) RHIGetNativeDevice();
		
	if(!Device)
	{
		return;
	}
	
	// DeviceContext
	{
		check(!DeviceContext);

		Device->GetImmediateContext(DeviceContext.GetInitReference());
	}
	
	// RingBufferCPU
	{
		check(!RingBufferCPU);

		D3D11_BUFFER_DESC BufferDesc;
		BufferDesc.ByteWidth = sizeof(FRingBuffer);
		BufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		BufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		BufferDesc.MiscFlags = 0;
		BufferDesc.StructureByteStride = 0;

		if(FAILED(hr = Device->CreateBuffer(&BufferDesc, nullptr, RingBufferCPU.GetInitReference())))
		{
			UE_LOG(LogHMD, Error, TEXT("FLateLatchingFrameD3D::InitRHI: CreateBuffer failed (0x%0.8x)"), hr);
			ReleaseRHI();
			return;
		}
	}

	// RingBuffer
	{
		check(!RingBuffer);

		D3D11_BUFFER_DESC BufferDesc;
		BufferDesc.ByteWidth = sizeof(FRingBuffer);
		BufferDesc.Usage = D3D11_USAGE_DEFAULT;
		BufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		BufferDesc.CPUAccessFlags = 0;
		BufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		BufferDesc.StructureByteStride = sizeof(FVector4);

		if(FAILED(hr = Device->CreateBuffer(&BufferDesc, nullptr, RingBuffer.GetInitReference())))
		{
			UE_LOG(LogHMD, Error, TEXT("FLateLatchingFrameD3D::InitRHI: CreateBuffer failed (0x%0.8x)"), hr);
			ReleaseRHI();
			return;
		}
	}

	// RingBufferSRV
	{
		check(!RingBufferSRV);

		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
		SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		SRVDesc.Buffer.FirstElement = 0;
		SRVDesc.Buffer.NumElements = sizeof(FRingBuffer) / sizeof(FVector4);

		if(FAILED(hr = Device->CreateShaderResourceView(RingBuffer, &SRVDesc, RingBufferSRV.GetInitReference())))
		{
			UE_LOG(LogHMD, Error, TEXT("FLateLatchingFrameD3D::InitRHI: CreateShaderResourceView failed (0x%0.8x)"), hr);
			ReleaseRHI();
			return;
		}
	}

	// DataBuffer
	{
		check(!DataBuffer);

		D3D11_BUFFER_DESC BufferDesc;
		BufferDesc.ByteWidth = sizeof(FRingBufferData);
		BufferDesc.Usage = D3D11_USAGE_DEFAULT;
		BufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
		BufferDesc.CPUAccessFlags = 0;
		BufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		BufferDesc.StructureByteStride = sizeof(FVector4);

		if(FAILED(hr = Device->CreateBuffer(&BufferDesc, nullptr, DataBuffer.GetInitReference())))
		{
			UE_LOG(LogHMD, Error, TEXT("FLateLatchingFrameD3D::InitRHI: CreateBuffer failed (0x%0.8x)"), hr);
			ReleaseRHI();
			return;
		}
	}

	// DataBufferUAV
	{
		check(!DataBufferUAV);

		D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc;
		UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
		UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		UAVDesc.Buffer.FirstElement = 0;
		UAVDesc.Buffer.NumElements = sizeof(FRingBufferData) / sizeof(FVector4);
		UAVDesc.Buffer.Flags = 0;

		if(FAILED(hr = Device->CreateUnorderedAccessView(DataBuffer, &UAVDesc, DataBufferUAV.GetInitReference())))
		{
			UE_LOG(LogHMD, Error, TEXT("FLateLatchingFrameD3D::InitRHI: CreateUnorderedAccessView failed (0x%0.8x)"), hr);
			ReleaseRHI();
			return;
		}
	}

	// DebugBuffer
	{
		check(!DebugBuffer);

		D3D11_BUFFER_DESC BufferDesc;
		BufferDesc.ByteWidth = sizeof(FRingBufferDebug);
		BufferDesc.Usage = D3D11_USAGE_STAGING;
		BufferDesc.BindFlags = 0;
		BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		BufferDesc.MiscFlags = 0;
		BufferDesc.StructureByteStride = 0;

		if(FAILED(hr = Device->CreateBuffer(&BufferDesc, nullptr, DebugBuffer.GetInitReference())))
		{
			UE_LOG(LogHMD, Error, TEXT("FLateLatchingFrameD3D::InitRHI: CreateBuffer failed (0x%0.8x)"), hr);
			ReleaseRHI();
			return;
		}
	}

	// QueryBegin, QueryLatch
	{
		check(!QueryBegin && !QueryLatch);

		D3D11_QUERY_DESC QueryDesc;
		QueryDesc.Query = D3D11_QUERY_TIMESTAMP;
		QueryDesc.MiscFlags = 0;

		if( FAILED(hr = Device->CreateQuery(&QueryDesc, QueryBegin.GetInitReference())) ||
			FAILED(hr = Device->CreateQuery(&QueryDesc, QueryLatch.GetInitReference())))
		{
			UE_LOG(LogHMD, Error, TEXT("FLateLatchingFrameD3D::InitRHI: CreateQuery failed (0x%0.8x)"), hr);
			ReleaseRHI();
			return;
		}
	}

	// QueryDisjoint
	{
		check(!QueryDisjoint);

		D3D11_QUERY_DESC QueryDesc;
		QueryDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
		QueryDesc.MiscFlags = 0;

		if(FAILED(hr = Device->CreateQuery(&QueryDesc, QueryDisjoint.GetInitReference())))
		{
			UE_LOG(LogHMD, Error, TEXT("FLateLatchingFrameD3D::InitRHI: CreateQuery failed (0x%0.8x)"), hr);
			ReleaseRHI();
			return;
		}
	}

	// PoseBuffer
	// UNDONE Remove this when we get PoseBuffer from LibOVR
	{
		check(!PoseBuffer);

		D3D11_BUFFER_DESC BufferDesc;
		BufferDesc.ByteWidth = sizeof(FMatrix) * 2;
		BufferDesc.Usage = D3D11_USAGE_DEFAULT;
		BufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		BufferDesc.CPUAccessFlags = 0;
		BufferDesc.MiscFlags = 0;
		BufferDesc.StructureByteStride = 0;

		if(FAILED(hr = Device->CreateBuffer(&BufferDesc, nullptr, PoseBuffer.GetInitReference())))
		{
			UE_LOG(LogHMD, Error, TEXT("FLateLatchingFrameD3D::InitRHI: CreateBuffer failed (0x%0.8x)"), hr);
			ReleaseRHI();
			return;
		}
	}
	
	InitRHISucceeded = true;
}


void FLateLatchingFrameD3D::ReleaseRHI()
{	
	check(IsInRenderingThread());

	{
		FScopeLock ScopeLock(&LateLatching->Critsec);
		InitRHISucceeded = false;
	}
	
	
	PoseBuffer = nullptr; // UNDONE Remove this when we get PoseBuffer from LibOVR
	QueryBegin = nullptr;
	QueryLatch = nullptr;
	QueryDisjoint = nullptr;
	DebugBuffer = nullptr;
	DataBufferUAV = nullptr;
	DataBuffer = nullptr;
	RingBufferSRV = nullptr;
	RingBuffer = nullptr;
	RingBufferCPU = nullptr;
	DeviceContext = nullptr;
}


void FLateLatchingFrameD3D::BeginFrame(FGameFrame* CurrentFrame, FSceneViewFamily& SceneViewFamily)
{
	check(IsInRenderingThread());

	ETWEnabled = ((FLateLatchingD3D*) LateLatching)->IsETWEnabled();

	// Get PoseBuffer
	// UNDONE Get PoseBuffer from LibOVR
	
	// Get ViewBuffers
	for(uint32 EyeIndex = 0; EyeIndex < 2; EyeIndex++)
	{
		const FSceneView* SceneView = SceneViewFamily.Views[EyeIndex];

		if(IsValidRef(SceneView->UniformBuffer))
			ViewBuffer[EyeIndex] = ((FD3D11UniformBuffer*) SceneView->UniformBuffer.GetReference())->Resource;
	}

	// Get RingBufferData pointer
	if(InitRHISucceeded)
	{
		D3D11_MAPPED_SUBRESOURCE MappedSubresource;

		if(SUCCEEDED(DeviceContext->Map(RingBufferCPU, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &MappedSubresource)))
		{
			RingBufferData = (FRingBuffer*) MappedSubresource.pData;
			DeviceContext->Unmap(RingBufferCPU, 0);				
		}
	}

	// Initialize RingBufferData contents and query BeginFrame times
	{
		OnBeginFrame(CurrentFrame, SceneViewFamily);

		if(InitRHISucceeded && ETWEnabled)
		{
			DeviceContext->Begin(QueryDisjoint);
			DeviceContext->End(QueryBegin);
			QueryBeginResult = S_FALSE;
		}
	}
}


void FLateLatchingFrameD3D::LatchFrame()
{
	check(IsInRenderingThread());

	FLateLatchingD3D* LateLatchingD3D = (FLateLatchingD3D*) LateLatching;
	
	if(InitRHISucceeded && LateLatchingD3D->InitRHISucceeded && RingBufferData)
	{
		// Schedule copy of RingBuffer from CPU to GPU
		{
			DeviceContext->CopyResource(RingBuffer, RingBufferCPU);
		}

		// Use ComputeShader to copy data from RingBuffer to DataBuffer
		{
			ID3D11ComputeShader* ComputeShaderOld = nullptr;
			ID3D11ShaderResourceView* ShaderResourceViewOld = nullptr;

			DeviceContext->CSGetShader(&ComputeShaderOld, nullptr, nullptr);
			DeviceContext->CSGetShaderResources(0, 1, &ShaderResourceViewOld);

			ID3D11ShaderResourceView* ShaderResourceView = RingBufferSRV;
			ID3D11UnorderedAccessView* UnorderedAccessView = DataBufferUAV;

			DeviceContext->CSSetShader(LateLatchingD3D->ComputeShader, nullptr, 0);
			DeviceContext->CSSetShaderResources(0, 1, &ShaderResourceView);
			DeviceContext->CSSetUnorderedAccessViews(0, 1, &UnorderedAccessView, nullptr);
			DeviceContext->Dispatch(1, 1, 1);

			DeviceContext->CSSetShader(ComputeShaderOld, nullptr, 0);
			DeviceContext->CSSetShaderResources(0, 1, &ShaderResourceViewOld);

			if(ShaderResourceViewOld)
				ShaderResourceViewOld->Release();
			if(ComputeShaderOld)
				ComputeShaderOld->Release();
		}

		// Schedule copies from DataBuffer to DebugBuffer
		if(DebugBuffer && ETWEnabled)
		{
			D3D11_BOX DebugBufferBox;
			DebugBufferBox.left = offsetof(FRingBufferData, Debug);
			DebugBufferBox.top = 0;
			DebugBufferBox.front = 0;
			DebugBufferBox.right = DebugBufferBox.left + sizeof(FRingBufferDebug);
			DebugBufferBox.bottom = 1;
			DebugBufferBox.back = 1;

			DeviceContext->CopySubresourceRegion(DebugBuffer, 0, 0, 0, 0, DataBuffer, 0, &DebugBufferBox);
		}

		// Schedule copy from DataBuffer to PoseBuffer
		if(PoseBuffer)
		{
			D3D11_BOX PoseBufferBox;
			PoseBufferBox.left = offsetof(FRingBufferData, PoseMatrix);
			PoseBufferBox.top = 0;
			PoseBufferBox.front = 0;
			PoseBufferBox.right = PoseBufferBox.left + sizeof(FMatrix[2]);
			PoseBufferBox.bottom = 1;
			PoseBufferBox.back = 1;

			DeviceContext->CopySubresourceRegion(PoseBuffer, 0, 0, 0, 0, DataBuffer, 0, &PoseBufferBox);
		}

		// Schedule copies from DataBuffer to ViewBuffers
		for(uint32 EyeIndex = 0; EyeIndex < 2; EyeIndex++)
		{
			if(ViewBuffer[EyeIndex])
			{
				D3D11_BOX ViewBufferBox;
				ViewBufferBox.left = offsetof(FRingBufferData, UniformShaderParameters[EyeIndex]);
				ViewBufferBox.top = 0;
				ViewBufferBox.front = 0;
				ViewBufferBox.right = ViewBufferBox.left + sizeof(FViewUniformShaderParameters);
				ViewBufferBox.bottom = 1;
				ViewBufferBox.back = 1;

				DeviceContext->CopySubresourceRegion(ViewBuffer[EyeIndex], 0, 0, 0, 0, DataBuffer, 0, &ViewBufferBox);
			}
		}
	}

	// Query LatchFrame times
	{
		OnLatchFrame();
	
		if(InitRHISucceeded)
		{
			DeviceContext->End(QueryLatch);
			QueryLatchResult = S_FALSE;

			if(ETWEnabled)
			{
				DeviceContext->End(QueryDisjoint);
				QueryDisjointResult = S_FALSE;
			}
		}
	}
}


bool FLateLatchingFrameD3D::IsFrameInFlight()
{
	check(IsInRenderingThread());

	if(InitRHISucceeded)
	{
		if(ETWEnabled)
		{
			if(S_FALSE == QueryBeginResult)
			{
				QueryBeginResult = DeviceContext->GetData(QueryBegin, &QueryBeginData, sizeof(QueryBeginData), D3D11_ASYNC_GETDATA_DONOTFLUSH);

				if(S_FALSE == QueryBeginResult)
				{
					return true;
				}
			}
		}

		if(S_FALSE == QueryLatchResult)
		{
			QueryLatchResult = DeviceContext->GetData(QueryLatch, &QueryLatchData, sizeof(QueryLatchData), D3D11_ASYNC_GETDATA_DONOTFLUSH);

			if(S_FALSE == QueryLatchResult)
			{
				return true;
			}
		}

		if(ETWEnabled)
		{
			if(S_FALSE == QueryDisjointResult)
			{
				QueryDisjointResult = DeviceContext->GetData(QueryDisjoint, &QueryDisjointData, sizeof(QueryDisjointData), D3D11_ASYNC_GETDATA_DONOTFLUSH);

				if(S_FALSE == QueryDisjointResult)
				{
					return true;
				}
			}
		}
	}
	
	return false;
}


void FLateLatchingFrameD3D::ReleaseFrame()
{
	check(IsInRenderingThread());

	OnReleaseFrame();

	// Release PoseBuffer
	// UNDONE Release PoseBuffer obtained from LibOVR
	
	// Release ViewBuffers
	for(uint32 EyeIndex = 0; EyeIndex < 2; EyeIndex++)
	{
		ViewBuffer[EyeIndex] = nullptr;
	}
	
	RingBufferData = nullptr;


	// ETW Trace
	if(InitRHISucceeded && ETWEnabled && S_OK == QueryBeginResult && S_OK == QueryLatchResult && S_OK == QueryDisjointResult && !QueryDisjointData.Disjoint)
	{
		double CPULatchTime = TimeLatch - TimeBegin;
		UINT32 CPULatchTimeMicroseconds = (UINT32) (CPULatchTime * 1000000.0);
		double GPULatchTime = (double) (QueryLatchData - QueryBeginData) / (double) QueryDisjointData.Frequency;
		UINT32 GPULatchTimeMicroseconds = (UINT32) (GPULatchTime * 1000000.0);

		D3D11_MAPPED_SUBRESOURCE MappedSubresource;
		
		if(SUCCEEDED(DeviceContext->Map(DebugBuffer, 0, D3D11_MAP_READ, 0, &MappedSubresource)))
		{
			const FRingBufferDebug& DebugLatch = *(const FRingBufferDebug*) MappedSubresource.pData;

			FVector HeadPositionDelta = DebugLatch.HeadPosition - DebugBegin.HeadPosition;
			UINT32 UpdateIndexDelta = DebugLatch.UpdateIndex - DebugBegin.UpdateIndex;
			
			((FLateLatchingD3D*) LateLatching)->TraceETW(CPULatchTimeMicroseconds, GPULatchTimeMicroseconds, HeadPositionDelta, UpdateIndexDelta);
			DeviceContext->Unmap(DebugBuffer, 0);				
		}
	}
}


//-------------------------------------------------------------------------------------------------
// FLateLatchingD3D
//-------------------------------------------------------------------------------------------------

FLateLatchingD3D::FLateLatchingD3D(FOculusRiftHMD* InOculusRiftHMD) : 
	FLateLatching(InOculusRiftHMD),
	FRenderResource(ERHIFeatureLevel::SM5)
{
	InitETW();
	InitRHISucceeded = false;	
	BeginInitResource(this);
}


FLateLatchingD3D::~FLateLatchingD3D()
{
	ReleaseResourceAndFlush(this);
	ReleaseETW();
}


void FLateLatchingD3D::InitRHI()
{
	check(IsInRenderingThread());

	HRESULT hr;
	
	// Device
	ID3D11Device* Device = (ID3D11Device*) RHIGetNativeDevice();
		
	if(!Device)
	{
		return;
	}

	// ComputeShader
	{
		static const DWORD ComputeShaderBytecode[] =
		{
			/*
				StructuredBuffer<float4> RingBuffer;
				RWStructuredBuffer<float4> DataBuffer;

				[numthreads(1,1,1)]
				void main()
				{
					const uint index = asuint(RingBuffer[0].x);
					const uint count = asuint(RingBuffer[0].y);
					const uint offset = index * count + 1;

					for(uint i = 0; i < count; i++)
						DataBuffer[i] = RingBuffer[offset + i];
				}
			*/

			// fxc cs.hlsl /T cs_5_0 /Fx

			/* 0000: */  0x43425844,  0xcfaa410e,  0xec4de396,  0x21a5bfec,  /* DXBC.A....M....! */
			/* 0010: */  0x884d6986,  0x00000001,  0x000003e0,  0x00000005,  /* .iM..___..__.___ */
			/* 0020: */  0x00000034,  0x000001b8,  0x000001c8,  0x000001d8,  /* 4___..__..__..__ */
			/* 0030: */  0x00000344,  0x46454452,  0x0000017c,  0x00000002,  /* D.__RDEF|.__.___ */
			/* 0040: */  0x00000094,  0x00000002,  0x0000003c,  0x43530500,  /* .___.___<____.SC */
			/* 0050: */  0x00000100,  0x00000148,  0x31314452,  0x0000003c,  /* _.__H.__RD11<___ */
			/* 0060: */  0x00000018,  0x00000020,  0x00000028,  0x00000024,  /* .___ ___(___$___ */
			/* 0070: */  0x0000000c,  0x00000000,  0x0000007c,  0x00000005,  /* ._______|___.___ */
			/* 0080: */  0x00000006,  0x00000001,  0x00000010,  0x00000000,  /* .___.___._______ */
			/* 0090: */  0x00000001,  0x00000000,  0x00000087,  0x00000006,  /* ._______.___.___ */
			/* 00a0: */  0x00000006,  0x00000001,  0x00000010,  0x00000000,  /* .___.___._______ */
			/* 00b0: */  0x00000001,  0x00000000,  0x676e6952,  0x66667542,  /* ._______RingBuff */
			/* 00c0: */  0x44007265,  0x42617461,  0x65666675,  0xabab0072,  /* er_DataBuffer_.. */
			/* 00d0: */  0x0000007c,  0x00000001,  0x000000c4,  0x00000010,  /* |___.___.___.___ */
			/* 00e0: */  0x00000000,  0x00000003,  0x00000087,  0x00000001,  /* ____.___.___.___ */
			/* 00f0: */  0x00000120,  0x00000010,  0x00000000,  0x00000003,  /*  .__._______.___ */
			/* 0100: */  0x000000ec,  0x00000000,  0x00000010,  0x00000002,  /* ._______.___.___ */
			/* 0110: */  0x000000fc,  0x00000000,  0xffffffff,  0x00000000,  /* ._______....____ */
			/* 0120: */  0xffffffff,  0x00000000,  0x656c4524,  0x746e656d,  /* ....____$Element */
			/* 0130: */  0x6f6c6600,  0x00347461,  0x00030001,  0x00040001,  /* _float4_._._._._ */
			/* 0140: */  0x00000000,  0x00000000,  0x00000000,  0x00000000,  /* ________________ */
			/* 0150: */  0x00000000,  0x00000000,  0x000000f5,  0x000000ec,  /* ________.___.___ */
			/* 0160: */  0x00000000,  0x00000010,  0x00000002,  0x000000fc,  /* ____.___.___.___ */
			/* 0170: */  0x00000000,  0xffffffff,  0x00000000,  0xffffffff,  /* ____....____.... */
			/* 0180: */  0x00000000,  0x7263694d,  0x666f736f,  0x52282074,  /* ____Microsoft (R */
			/* 0190: */  0x4c482029,  0x53204c53,  0x65646168,  0x6f432072,  /* ) HLSL Shader Co */
			/* 01a0: */  0x6c69706d,  0x36207265,  0x392e332e,  0x2e303036,  /* mpiler 6.3.9600. */
			/* 01b0: */  0x38333631,  0xabab0034,  0x4e475349,  0x00000008,  /* 16384_..ISGN.___ */
			/* 01c0: */  0x00000000,  0x00000008,  0x4e47534f,  0x00000008,  /* ____.___OSGN.___ */
			/* 01d0: */  0x00000000,  0x00000008,  0x58454853,  0x00000164,  /* ____.___SHEXd.__ */
			/* 01e0: */  0x00050050,  0x00000059,  0x0100086a,  0x040000a2,  /* P_._Y___j._..__. */
			/* 01f0: */  0x00107000,  0x00000000,  0x00000010,  0x0400009e,  /* _p._____.___.__. */
			/* 0200: */  0x0011e000,  0x00000000,  0x00000010,  0x02000068,  /* _.._____.___h__. */
			/* 0210: */  0x00000002,  0x0400009b,  0x00000001,  0x00000001,  /* .___.__..___.___ */
			/* 0220: */  0x00000001,  0x8b0000a7,  0x80008302,  0x00199983,  /* .___.__..._...._ */
			/* 0230: */  0x00100032,  0x00000000,  0x00004001,  0x00000000,  /* 2_._____.@______ */
			/* 0240: */  0x00004001,  0x00000000,  0x00107046,  0x00000000,  /* .@______Fp._____ */
			/* 0250: */  0x09000023,  0x00100012,  0x00000000,  0x0010000a,  /* #__.._._____._._ */
			/* 0260: */  0x00000000,  0x0010001a,  0x00000000,  0x00004001,  /* ____._._____.@__ */
			/* 0270: */  0x00000001,  0x05000036,  0x00100042,  0x00000000,  /* .___6__.B_._____ */
			/* 0280: */  0x00004001,  0x00000000,  0x01000030,  0x07000050,  /* .@______0__.P__. */
			/* 0290: */  0x00100082,  0x00000000,  0x0010002a,  0x00000000,  /* ._._____*_._____ */
			/* 02a0: */  0x0010001a,  0x00000000,  0x03040003,  0x0010003a,  /* ._._____._..:_._ */
			/* 02b0: */  0x00000000,  0x0700001e,  0x00100082,  0x00000000,  /* ____.__.._._____ */
			/* 02c0: */  0x0010002a,  0x00000000,  0x0010000a,  0x00000000,  /* *_._____._._____ */
			/* 02d0: */  0x8b0000a7,  0x80008302,  0x00199983,  0x001000f2,  /* .__..._...._._._ */
			/* 02e0: */  0x00000001,  0x0010003a,  0x00000000,  0x00004001,  /* .___:_._____.@__ */
			/* 02f0: */  0x00000000,  0x00107e46,  0x00000000,  0x090000a8,  /* ____F~._____.__. */
			/* 0300: */  0x0011e0f2,  0x00000000,  0x0010002a,  0x00000000,  /* ..._____*_._____ */
			/* 0310: */  0x00004001,  0x00000000,  0x00100e46,  0x00000001,  /* .@______F.._.___ */
			/* 0320: */  0x0700001e,  0x00100042,  0x00000000,  0x0010002a,  /* .__.B_._____*_._ */
			/* 0330: */  0x00000000,  0x00004001,  0x00000001,  0x01000016,  /* ____.@__.___.__. */
			/* 0340: */  0x0100003e,  0x54415453,  0x00000094,  0x0000000c,  /* >__.STAT.___.___ */
			/* 0350: */  0x00000002,  0x00000000,  0x00000000,  0x00000000,  /* ._______________ */
			/* 0360: */  0x00000003,  0x00000001,  0x00000001,  0x00000001,  /* .___.___.___.___ */
			/* 0370: */  0x00000000,  0x00000000,  0x00000000,  0x00000000,  /* ________________ */
			/* 0380: */  0x00000000,  0x00000000,  0x00000002,  0x00000000,  /* ________._______ */
			/* 0390: */  0x00000000,  0x00000000,  0x00000001,  0x00000000,  /* ________._______ */
			/* 03a0: */  0x00000000,  0x00000000,  0x00000000,  0x00000000,  /* ________________ */
			/* 03b0: */  0x00000000,  0x00000000,  0x00000000,  0x00000000,  /* ________________ */
			/* 03c0: */  0x00000000,  0x00000000,  0x00000000,  0x00000000,  /* ________________ */
			/* 03d0: */  0x00000000,  0x00000000,  0x00000000,  0x00000001,  /* ____________.___ */
		};

		check(!ComputeShader);

		if(FAILED(hr = Device->CreateComputeShader(ComputeShaderBytecode, sizeof(ComputeShaderBytecode), nullptr, ComputeShader.GetInitReference())))
		{
			UE_LOG(LogHMD, Error, TEXT("FLateLatchingD3D::InitRHI: CreateComputeShader failed (0x%0.8x)"), hr);
			ReleaseRHI();
			return;
		}
	}
	
	InitRHISucceeded = true;
}


void FLateLatchingD3D::ReleaseRHI()
{
	check(IsInRenderingThread());

	{
		FScopeLock ScopeLock(&Critsec);
		InitRHISucceeded = false;
	}
	
	ComputeShader = nullptr;
}


FLateLatchingFrame* FLateLatchingD3D::CreateFrame()
{
	check(IsInRenderingThread());

	return (FLateLatchingFrame*) new FLateLatchingFrameD3D(this);
}


void FLateLatchingD3D::InitETW()
{
	// {B3E9FB28-DD14-477C-8FEC-24FE806D32CF}
	static const GUID ETWControlGuid = 
	{ 
		0xb3e9fb28, 0xdd14, 0x477c, { 0x8f, 0xec, 0x24, 0xfe, 0x80, 0x6d, 0x32, 0xcf } 
	};

	ETWRegistrationHandle = 0;
	ETWSessionHandle = 0;

	::RegisterTraceGuids(FLateLatchingD3D::ETWControlCallback, this, &ETWControlGuid, 0, NULL, NULL, NULL, &ETWRegistrationHandle);
}


bool FLateLatchingD3D::IsETWEnabled()
{
	return ETWSessionHandle != 0;
}


void FLateLatchingD3D::TraceETW(UINT32 CPULatchTimeMicroseconds, UINT32 GPULatchTimeMicroseconds, const FVector& HeadPositionDelta, UINT32 UpdateIndexDelta)
{
	// {12F1299A-E1ED-4DA6-98AC-81AEFADE9132}
	static const GUID ETWTraceGuid = 
	{
		0x12f1299a, 0xe1ed, 0x4da6, { 0x98, 0xac, 0x81, 0xae, 0xfa, 0xde, 0x91, 0x32 }
	};

	if(IsETWEnabled())
	{
		struct SEventTrace
		{
			EVENT_TRACE_HEADER Header;
			MOF_FIELD CPULatchTimeMicroseconds;
			MOF_FIELD GPULatchTimeMicroseconds;
			MOF_FIELD HeadPositionDelta;
			MOF_FIELD UpdateIndexDelta;
		};

		SEventTrace EventTrace;
		FMemory::Memzero(EventTrace);

		EventTrace.Header.Size = sizeof(SEventTrace);
		EventTrace.Header.Flags = WNODE_FLAG_TRACED_GUID | WNODE_FLAG_USE_GUID_PTR | WNODE_FLAG_USE_MOF_PTR;
		EventTrace.Header.GuidPtr = (ULONG64) &ETWTraceGuid;
		EventTrace.Header.Class.Type = EVENT_TRACE_TYPE_INFO;
		EventTrace.Header.Class.Level = TRACE_LEVEL_INFORMATION;
		EventTrace.CPULatchTimeMicroseconds.DataPtr = (UINT64) &CPULatchTimeMicroseconds;
		EventTrace.CPULatchTimeMicroseconds.Length = sizeof(CPULatchTimeMicroseconds);
		EventTrace.GPULatchTimeMicroseconds.DataPtr = (UINT64) &GPULatchTimeMicroseconds;
		EventTrace.GPULatchTimeMicroseconds.Length = sizeof(GPULatchTimeMicroseconds);
		EventTrace.HeadPositionDelta.DataPtr = (UINT64) &HeadPositionDelta;
		EventTrace.HeadPositionDelta.Length = sizeof(HeadPositionDelta);
		EventTrace.UpdateIndexDelta.DataPtr = (UINT64) &UpdateIndexDelta;
		EventTrace.UpdateIndexDelta.Length = sizeof(UpdateIndexDelta);

		::TraceEvent(ETWSessionHandle, &EventTrace.Header);
	}
}


void FLateLatchingD3D::ReleaseETW()
{
	if(ETWRegistrationHandle)
	{
		::UnregisterTraceGuids(ETWRegistrationHandle);
	}
}


ULONG WINAPI FLateLatchingD3D::ETWControlCallback(WMIDPREQUESTCODE RequestCode, PVOID Context, ULONG* Reserved, PVOID Header)
{
	FLateLatchingD3D* LateLatchingD3D = (FLateLatchingD3D*) Context;

	switch(RequestCode)
	{
	case WMI_ENABLE_EVENTS:
		{
			TRACEHANDLE ETWSessionHandle = ::GetTraceLoggerHandle(Header);

			if(INVALID_HANDLE_VALUE == (HANDLE) ETWSessionHandle)
			{
				return ::GetLastError();
			}

			if(LateLatchingD3D->ETWSessionHandle == 0)
			{
				LateLatchingD3D->ETWSessionHandle = ETWSessionHandle;
			}
		}
		break;

	case WMI_DISABLE_EVENTS:
		{
			TRACEHANDLE ETWSessionHandle = ::GetTraceLoggerHandle(Header);

			if(INVALID_HANDLE_VALUE == (HANDLE) ETWSessionHandle)
			{
				return ::GetLastError();
			}

			if(LateLatchingD3D->ETWSessionHandle == ETWSessionHandle)
			{
				LateLatchingD3D->ETWSessionHandle = 0;
			}
		}
		break;

	default:
		return ERROR_INVALID_PARAMETER;
	}

	return ERROR_SUCCESS;
}


//-------------------------------------------------------------------------------------------------
// FOculusRiftHMD
//-------------------------------------------------------------------------------------------------

#pragma comment(lib, "dxgi")

void FOculusRiftHMD::PreInit()
{
	// Find the adapterIndex where the HMD is connected
	ovrHmd hmd;
	ovrGraphicsLuid luid;

	if(OVR_SUCCESS(ovr_Create(&hmd, &luid)))
	{
		SetHmdGraphicsAdapter(luid);
		ovr_Destroy(hmd);
	}
}

void FOculusRiftHMD::SetHmdGraphicsAdapter(const ovrGraphicsLuid& luid)
{
	TRefCountPtr<IDXGIFactory> DXGIFactory;

	if(SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**) DXGIFactory.GetInitReference())))
	{
		for(int32 adapterIndex = 0;; adapterIndex++)
		{
			TRefCountPtr<IDXGIAdapter> DXGIAdapter;
			DXGI_ADAPTER_DESC DXGIAdapterDesc;

			if( FAILED(DXGIFactory->EnumAdapters(adapterIndex, DXGIAdapter.GetInitReference())) ||
				FAILED(DXGIAdapter->GetDesc(&DXGIAdapterDesc)) )
			{
				break;
			}

			if(!FMemory::Memcmp(&luid, &DXGIAdapterDesc.AdapterLuid, sizeof(LUID)))
			{
				// Remember this adapterIndex so we use the right adapter, even when we startup without HMD connected
				IConsoleVariable* CVarHmdGraphicsAdapter = IConsoleManager::Get().FindConsoleVariable(L"r.HmdGraphicsAdapter");

				if(CVarHmdGraphicsAdapter)
				{
					if(adapterIndex != CVarHmdGraphicsAdapter->GetInt())
						CVarHmdGraphicsAdapter->Set(adapterIndex);
				}

				break;
			}
		}
	}
}

bool FOculusRiftHMD::IsRHIUsingHmdGraphicsAdapter(const ovrGraphicsLuid& luid)
{
	if (IsPCPlatform(GMaxRHIShaderPlatform) && !IsOpenGLPlatform(GMaxRHIShaderPlatform))
	{
		TRefCountPtr<ID3D11Device> Device;

		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			GetNativeDevice,
			TRefCountPtr<ID3D11Device>&, DeviceRef, Device,
			{
				DeviceRef = (ID3D11Device*) RHIGetNativeDevice();
			});

		FlushRenderingCommands();

		if(Device)
		{
			TRefCountPtr<IDXGIDevice> DXGIDevice;
			TRefCountPtr<IDXGIAdapter> DXGIAdapter;
			DXGI_ADAPTER_DESC DXGIAdapterDesc;

			if( SUCCEEDED(Device->QueryInterface(__uuidof(IDXGIDevice), (void**) DXGIDevice.GetInitReference())) &&
				SUCCEEDED(DXGIDevice->GetAdapter(DXGIAdapter.GetInitReference())) &&
				SUCCEEDED(DXGIAdapter->GetDesc(&DXGIAdapterDesc)) )
			{
				return !FMemory::Memcmp(&luid, &DXGIAdapterDesc.AdapterLuid, sizeof(LUID));
			}
		}
	}

	// Not enough information.  Assume that we are using the correct adapter.
	return true;
}


//-------------------------------------------------------------------------------------------------
// FOculusRiftHMD::D3D11Bridge
//-------------------------------------------------------------------------------------------------

FOculusRiftHMD::D3D11Bridge::D3D11Bridge(ovrSession InOvrSession)
	: FCustomPresent()
{
	Init(InOvrSession);
}

void FOculusRiftHMD::D3D11Bridge::SetHmd(ovrSession InOvrSession)
{
	if (InOvrSession != OvrSession)
	{
		Reset();
		Init(InOvrSession);
		bNeedReAllocateTextureSet = true;
		bNeedReAllocateMirrorTexture = true;
	}
}

void FOculusRiftHMD::D3D11Bridge::Init(ovrSession InOvrSession)
{
	OvrSession = InOvrSession;
	bInitialized = true;
}

bool FOculusRiftHMD::D3D11Bridge::AllocateRenderTargetTexture(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 InFlags, uint32 TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples)
{
	check(SizeX != 0 && SizeY != 0);

	if (!ColorTextureSet || (ColorTextureSet->GetSizeX() != SizeX || ColorTextureSet->GetSizeY() != SizeY || ColorTextureSet->GetFormat() != Format))
	{
		bNeedReAllocateTextureSet = true;
	}

	if (OvrSession && bNeedReAllocateTextureSet)
	{
		auto D3D11RHI = static_cast<FD3D11DynamicRHI*>(GDynamicRHI);
		if (ColorTextureSet)
		{
			ColorTextureSet->ReleaseResources(OvrSession);
			ColorTextureSet = nullptr;
		}
		ID3D11Device* D3DDevice = D3D11RHI->GetDevice();

		const DXGI_FORMAT PlatformResourceFormat = (DXGI_FORMAT)GPixelFormats[Format].PlatformFormat;

		D3D11_TEXTURE2D_DESC dsDesc;
		dsDesc.Width = SizeX;
		dsDesc.Height = SizeY;
		dsDesc.MipLevels = 1;
		dsDesc.ArraySize = 1;

		// just make sure the proper format is used; if format is different then we might
		// need to make some changes here.
		check(PlatformResourceFormat == DXGI_FORMAT_B8G8R8A8_TYPELESS);

		dsDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB; // use SRGB for compositor
		dsDesc.SampleDesc.Count = 1;
		dsDesc.SampleDesc.Quality = 0;
		dsDesc.Usage = D3D11_USAGE_DEFAULT;
		dsDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		dsDesc.CPUAccessFlags = 0;
		dsDesc.MiscFlags = 0;

		ovrSwapTextureSet* textureSet;
		ovrResult res = ovr_CreateSwapTextureSetD3D11(OvrSession, D3DDevice, &dsDesc, ovrSwapTextureSetD3D11_Typeless, &textureSet);
		if (!textureSet || res != ovrSuccess)
		{
			UE_LOG(LogHMD, Error, TEXT("Can't create swap texture set (size %d x %d), error = %d"), SizeX, SizeY, res);
			if (res == ovrError_DisplayLost)
			{
				bNeedReAllocateMirrorTexture = bNeedReAllocateTextureSet = true;
				FPlatformAtomics::InterlockedExchange(&NeedToKillHmd, 1);
			}
			return false;
		}

		// set the proper format for RTV & SRV
		dsDesc.Format = PlatformResourceFormat; //DXGI_FORMAT_B8G8R8A8_UNORM;

		bNeedReAllocateTextureSet = false;
		bNeedReAllocateMirrorTexture = true;
		UE_LOG(LogHMD, Log, TEXT("Allocated a new swap texture set (size %d x %d)"), SizeX, SizeY);

		ColorTextureSet = FD3D11Texture2DSet::D3D11CreateTexture2DSet(
			D3D11RHI,
			textureSet,
			dsDesc,
			EPixelFormat(Format),
			TexCreate_RenderTargetable | TexCreate_ShaderResource
			);
	}
	if (ColorTextureSet)
	{
		OutTargetableTexture = ColorTextureSet->GetTexture2D();
		OutShaderResourceTexture = ColorTextureSet->GetTexture2D();
		return true;
	}
	return false;
}

void FOculusRiftHMD::D3D11Bridge::BeginRendering(FHMDViewExtension& InRenderContext, const FTexture2DRHIRef& RT)
{
	SCOPE_CYCLE_COUNTER(STAT_BeginRendering);

	check(IsInRenderingThread());

	SetRenderContext(&InRenderContext);

	FGameFrame* CurrentFrame = GetRenderFrame();
	check(CurrentFrame);
	FSettings* FrameSettings = CurrentFrame->GetSettings();
	check(FrameSettings);

	const uint32 RTSizeX = RT->GetSizeX();
	const uint32 RTSizeY = RT->GetSizeY();

	const FVector2D ActualMirrorWindowSize = CurrentFrame->WindowSize;
	// detect if mirror texture needs to be re-allocated or freed
	if (OvrSession && MirrorTextureRHI && (bNeedReAllocateMirrorTexture || OvrSession != RenderContext->OvrSession ||
		(FrameSettings->Flags.bMirrorToWindow && (
		FrameSettings->MirrorWindowMode != FSettings::eMirrorWindow_Distorted ||
		ActualMirrorWindowSize != FVector2D(MirrorTextureRHI->GetSizeX(), MirrorTextureRHI->GetSizeY()))) ||
		!FrameSettings->Flags.bMirrorToWindow ))
	{
		check(MirrorTexture);
		ovr_DestroyMirrorTexture(OvrSession, MirrorTexture);
		MirrorTexture = nullptr;
		MirrorTextureRHI = nullptr;
		bNeedReAllocateMirrorTexture = false;
	}

	// need to allocate a mirror texture?
	if (FrameSettings->Flags.bMirrorToWindow && FrameSettings->MirrorWindowMode == FSettings::eMirrorWindow_Distorted && !MirrorTextureRHI &&
		ActualMirrorWindowSize.X != 0 && ActualMirrorWindowSize.Y != 0)
	{
		D3D11_TEXTURE2D_DESC dsDesc;
		dsDesc.Width = (UINT)ActualMirrorWindowSize.X;
		dsDesc.Height = (UINT)ActualMirrorWindowSize.Y;
		dsDesc.MipLevels = 1;
		dsDesc.ArraySize = 1;
		dsDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB; // SRGB is required for the compositor
		dsDesc.SampleDesc.Count = 1;
		dsDesc.SampleDesc.Quality = 0;
		dsDesc.Usage = D3D11_USAGE_DEFAULT;
		dsDesc.BindFlags = 0;// D3D11_BIND_SHADER_RESOURCE; //can't even use D3DMirrorTexture.D3D11.pSRView since we need one w/o SRGB set
		dsDesc.CPUAccessFlags = 0;
		dsDesc.MiscFlags = 0;

		ID3D11Device* D3DDevice = (ID3D11Device*)RHIGetNativeDevice();

		ovrResult res = ovr_CreateMirrorTextureD3D11(OvrSession, D3DDevice, &dsDesc, ovrSwapTextureSetD3D11_Typeless, &MirrorTexture);
		if (!MirrorTexture || res != ovrSuccess)
		{
			UE_LOG(LogHMD, Error, TEXT("Can't create a mirror texture, error = %d"), res);
			return;
		}

		UE_LOG(LogHMD, Log, TEXT("Allocated a new mirror texture (size %d x %d)"), (int)ActualMirrorWindowSize.X, (int)ActualMirrorWindowSize.Y);
		ovrD3D11Texture D3DMirrorTexture;
		D3DMirrorTexture.Texture = *MirrorTexture;
		MirrorTextureRHI = D3D11CreateTexture2DAlias(
			static_cast<FD3D11DynamicRHI*>(GDynamicRHI),
			D3DMirrorTexture.D3D11.pTexture,
			nullptr,// can't use D3DMirrorTexture.D3D11.pSRView since we need one w/o SRGB set
			dsDesc.Width,
			dsDesc.Height,
			0,
			dsDesc.MipLevels,
			/*ActualMSAACount=*/ 1,
			(EPixelFormat)PF_B8G8R8A8,
			TexCreate_ShaderResource);
		bNeedReAllocateMirrorTexture = false;
	}
}

void FOculusRiftHMD::D3D11Bridge::FinishRendering()
{
	SCOPE_CYCLE_COUNTER(STAT_FinishRendering);

	check(IsInRenderingThread());
	
	check(RenderContext.IsValid());

	if (RenderContext->bFrameBegun && ColorTextureSet)
	{
		if (!ColorTextureSet)
		{
			UE_LOG(LogHMD, Warning, TEXT("Skipping frame: TextureSet is null ?"));
		}
		else
		{
#if 0 // !UE_BUILD_SHIPPING
			// Debug code
			static int _cnt = 0;
			_cnt++;
			if (_cnt % 10 == 0)
			{
				FGameFrame* CurrentFrame = GetRenderFrame();
				UE_LOG(LogHMD, Log, TEXT("Time from BeginFrame (GT) to SubmitFrame (RT) is %f"), ovr_GetTimeInSeconds() - CurrentFrame->BeginFrameTimeInSec);
			}
#endif

			// Finish the frame and let OVR do buffer swap (Present) and flush/sync.
			FSettings* FrameSettings = RenderContext->GetFrameSettings();

			check(ColorTextureSet->GetTextureSet());
			FrameSettings->EyeLayer.EyeFov.ColorTexture[0] = ColorTextureSet->GetTextureSet();
			FrameSettings->EyeLayer.EyeFov.ColorTexture[1] = ColorTextureSet->GetTextureSet();

			ovrLayerHeader* LayerList[1];
			LayerList[0] = &FrameSettings->EyeLayer.EyeFov.Header;

			// Set up positional data.
			ovrViewScaleDesc viewScaleDesc;
			viewScaleDesc.HmdSpaceToWorldScaleInMeters = 1.0f;
			viewScaleDesc.HmdToEyeViewOffset[0] = FrameSettings->EyeRenderDesc[0].HmdToEyeViewOffset;
			viewScaleDesc.HmdToEyeViewOffset[1] = FrameSettings->EyeRenderDesc[1].HmdToEyeViewOffset;

			ovrResult res = ovr_SubmitFrame(RenderContext->OvrSession, RenderContext->RenderFrame->FrameNumber, &viewScaleDesc, LayerList, 1);
			if (res != ovrSuccess)
			{
				UE_LOG(LogHMD, Warning, TEXT("Error at SubmitFrame, err = %d"), int(res));
				
				if (res == ovrError_DisplayLost)
				{
					bNeedReAllocateMirrorTexture = bNeedReAllocateTextureSet = true;
					FPlatformAtomics::InterlockedExchange(&NeedToKillHmd, 1);
				}
			}

			if (RenderContext->ShowFlags.Rendering)
			{
				ColorTextureSet->SwitchToNextElement();
			}

			// Update frame stats
#if STATS
			struct 
			{
				float LatencyRender;
				float LatencyTimewarp;
				float LatencyPostPresent;
				float ErrorRender;
				float ErrorTimewarp;
			} DK2Latency;

			const unsigned int DK2LatencyCount = sizeof(DK2Latency) / sizeof(float);

			if (ovr_GetFloatArray(RenderContext->OvrSession, "DK2Latency", (float*) &DK2Latency, DK2LatencyCount) == DK2LatencyCount)
			{
				SET_FLOAT_STAT(STAT_LatencyRender, DK2Latency.LatencyRender * 1000.0f);
				SET_FLOAT_STAT(STAT_LatencyTimewarp, DK2Latency.LatencyTimewarp * 1000.0f);
				SET_FLOAT_STAT(STAT_LatencyPostPresent, DK2Latency.LatencyPostPresent * 1000.0f);
				SET_FLOAT_STAT(STAT_ErrorRender, DK2Latency.ErrorRender * 1000.0f);
				SET_FLOAT_STAT(STAT_ErrorTimewarp, DK2Latency.ErrorTimewarp * 1000.0f);
			}
#endif
		}
	}
	else
	{
		UE_LOG(LogHMD, Warning, TEXT("Skipping frame: FinishRendering called with no corresponding BeginRendering (was BackBuffer re-allocated?)"));
	}
	RenderContext->bFrameBegun = false;
	SetRenderContext(nullptr);
}

void FOculusRiftHMD::D3D11Bridge::Reset_RenderThread()
{
	if (MirrorTexture)
	{
		ovr_DestroyMirrorTexture(OvrSession, MirrorTexture);
		MirrorTextureRHI = nullptr;
		MirrorTexture = nullptr;
	}
	if (ColorTextureSet)
	{
		ColorTextureSet->ReleaseResources(OvrSession);
		ColorTextureSet = nullptr;
	}
	OvrSession = nullptr;

	if (RenderContext.IsValid())
	{
		RenderContext->bFrameBegun = false;
		SetRenderContext(nullptr);
	}
}

void FOculusRiftHMD::D3D11Bridge::Reset()
{
	if (IsInGameThread())
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(ResetD3D,
		FOculusRiftHMD::D3D11Bridge*, Bridge, this,
		{
			Bridge->Reset_RenderThread();
		});
		// Wait for all resources to be released
		FlushRenderingCommands();
	}
	else
	{
		Reset_RenderThread();
	}

	bInitialized = false;
}

#if PLATFORM_WINDOWS
	// It is required to undef WINDOWS_PLATFORM_TYPES_GUARD for any further D3D11 / GL private includes
	#undef WINDOWS_PLATFORM_TYPES_GUARD
#endif

#endif // #if defined(OVR_D3D_VERSION) && (OVR_D3D_VERSION == 11)

#endif // OCULUS_RIFT_SUPPORTED_PLATFORMS