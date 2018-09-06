#include "OculusMR_CastingSceneViewport.h"
#include "OculusMRPrivate.h"
#include "Slate/SlateTextures.h"
#include "Widgets/SViewport.h"
#include "Framework/Application/SlateApplication.h"

FOculusMR_CastingSceneViewport::FOculusMR_CastingSceneViewport(FViewportClient* InViewportClient, TSharedPtr<SViewport> InViewportWidget, int32 InMaximumBufferedFrames)
	: FSceneViewport(InViewportClient, InViewportWidget)
{
#if WITH_OCULUS_PRIVATE_CODE
	bUseSeparateRenderTarget = true;
	NumBufferedFrames = InMaximumBufferedFrames;
#endif
	MaximumBufferedFrames = InMaximumBufferedFrames;
	ExpectedLantencyInSeconds = 0.0;
}

void FOculusMR_CastingSceneViewport::EnqueueBeginRenderFrame(const bool bShouldPresent)
{
	Super::EnqueueBeginRenderFrame(bShouldPresent);
#if WITH_OCULUS_PRIVATE_CODE
	BufferedFrameStartTimes[CurrentBufferedTargetIndex] = FPlatformTime::Seconds();
#endif
}

int32 FOculusMR_CastingSceneViewport::GetPresentBufferredTargetIndex() const
{
#if WITH_OCULUS_PRIVATE_CODE
	double CurrentFrameStartTime = BufferedFrameStartTimes[CurrentBufferedTargetIndex];
	if (CurrentFrameStartTime < 0.0)
	{
		return CurrentBufferedTargetIndex;
	}
	double PresentFrameStartTime = CurrentFrameStartTime - ExpectedLantencyInSeconds;
	int LatencyFrames = 0;
	while (LatencyFrames < MaximumBufferedFrames)
	{
		int FrameIndex = (CurrentBufferedTargetIndex + MaximumBufferedFrames - LatencyFrames) % MaximumBufferedFrames;
		if (BufferedFrameStartTimes[FrameIndex] <= PresentFrameStartTime)
		{
			if (BufferedFrameStartTimes[FrameIndex] < 0.0)
			{
				// this frame is invalid, use the oldest frame
				FrameIndex = (FrameIndex + 1) % MaximumBufferedFrames;
				check(BufferedFrameStartTimes[FrameIndex] >= 0.0);
			}
			return FrameIndex;
		}
		++LatencyFrames;
	}
	return (CurrentBufferedTargetIndex + 1) % MaximumBufferedFrames;
#else
	return 0;
#endif
}

FSlateShaderResource* FOculusMR_CastingSceneViewport::GetViewportRenderTargetTexture() const
{
#if WITH_OCULUS_PRIVATE_CODE
	check(IsThreadSafeForSlateRendering());
	if (BufferedSlateHandles.Num() == 0)
	{
		return nullptr;
	}
	else
	{
		int32 PresentBufferredTargetIndex = GetPresentBufferredTargetIndex();
		return BufferedSlateHandles[PresentBufferredTargetIndex];
	}
#else
	return nullptr;
#endif
}

FSlateShaderResource* FOculusMR_CastingSceneViewport::GetViewportRenderTargetTexture()
{
#if WITH_OCULUS_PRIVATE_CODE
	if (IsInRenderingThread())
	{
		return RenderThreadSlateTexture;
	}
	if (BufferedSlateHandles.Num() == 0)
	{
		return nullptr;
	}
	else
	{
		int32 PresentBufferredTargetIndex = GetPresentBufferredTargetIndex();
		return BufferedSlateHandles[PresentBufferredTargetIndex];
	}
#else
	return nullptr;
#endif
}

void FOculusMR_CastingSceneViewport::InitDynamicRHI()
{
#if WITH_OCULUS_PRIVATE_CODE
	check(NumBufferedFrames == MaximumBufferedFrames);      // NumBufferedFrames should never be modified
	check(!bRequiresHitProxyStorage);
	RTTSize = FIntPoint(0, 0);

	FSlateRenderer* Renderer = FSlateApplication::Get().GetRenderer();
	uint32 TexSizeX = SizeX, TexSizeY = SizeY;
	check(UseSeparateRenderTarget());
	check(BufferedSlateHandles.Num() == BufferedRenderTargetsRHI.Num() && BufferedSlateHandles.Num() == BufferedShaderResourceTexturesRHI.Num() && BufferedSlateHandles.Num() == BufferedFrameStartTimes.Num());

	//clear existing entries
	for (int32 i = 0; i < BufferedSlateHandles.Num(); ++i)
	{
		if (!BufferedSlateHandles[i])
		{
			BufferedSlateHandles[i] = new FSlateRenderTargetRHI(nullptr, 0, 0);
		}
		BufferedRenderTargetsRHI[i] = nullptr;
		BufferedShaderResourceTexturesRHI[i] = nullptr;
		BufferedFrameStartTimes[i] = -1.0;
	}

	if (BufferedSlateHandles.Num() < NumBufferedFrames)
	{
		//add sufficient entires for buffering.
		for (int32 i = BufferedSlateHandles.Num(); i < NumBufferedFrames; i++)
		{
			BufferedSlateHandles.Add(new FSlateRenderTargetRHI(nullptr, 0, 0));
			BufferedRenderTargetsRHI.Add(nullptr);
			BufferedShaderResourceTexturesRHI.Add(nullptr);
			BufferedFrameStartTimes.Add(-1.0);
		}
	}
	else if (BufferedSlateHandles.Num() > NumBufferedFrames)
	{
		BufferedSlateHandles.SetNum(NumBufferedFrames);
		BufferedRenderTargetsRHI.SetNum(NumBufferedFrames);
		BufferedShaderResourceTexturesRHI.SetNum(NumBufferedFrames);
		BufferedFrameStartTimes.SetNum(NumBufferedFrames);
	}
	check(BufferedSlateHandles.Num() == BufferedRenderTargetsRHI.Num() && BufferedSlateHandles.Num() == BufferedShaderResourceTexturesRHI.Num() && BufferedSlateHandles.Num() == BufferedFrameStartTimes.Num());

	FRHIResourceCreateInfo CreateInfo;
	FTexture2DRHIRef BufferedRTRHI;
	FTexture2DRHIRef BufferedSRVRHI;

	for (int32 i = 0; i < NumBufferedFrames; ++i)
	{
		RHICreateTargetableShaderResource2D(TexSizeX, TexSizeY, PF_B8G8R8A8, 1, TexCreate_None, TexCreate_RenderTargetable, false, CreateInfo, BufferedRTRHI, BufferedSRVRHI);
		BufferedRenderTargetsRHI[i] = BufferedRTRHI;
		BufferedShaderResourceTexturesRHI[i] = BufferedSRVRHI;

		if (BufferedSlateHandles[i])
		{
			BufferedSlateHandles[i]->SetRHIRef(BufferedShaderResourceTexturesRHI[i], TexSizeX, TexSizeY);
		}
	}

	// clear out any extra entries we have hanging around
	for (int32 i = NumBufferedFrames; i < BufferedSlateHandles.Num(); ++i)
	{
		if (BufferedSlateHandles[i])
		{
			BufferedSlateHandles[i]->SetRHIRef(nullptr, 0, 0);
		}
		BufferedRenderTargetsRHI[i] = nullptr;
		BufferedShaderResourceTexturesRHI[i] = nullptr;
	}

	CurrentBufferedTargetIndex = 0;
	NextBufferedTargetIndex = (CurrentBufferedTargetIndex + 1) % BufferedSlateHandles.Num();
	RenderTargetTextureRHI = BufferedShaderResourceTexturesRHI[CurrentBufferedTargetIndex];

	//how is this useful at all?  Pinning a weakptr to get a non-threadsafe shared ptr?  Pinning a weakptr is supposed to be protecting me from my weakptr dying underneath me...
	TSharedPtr<SWidget> PinnedViewport = ViewportWidget.Pin();
	if (PinnedViewport.IsValid())
	{

		FWidgetPath WidgetPath;
		TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(PinnedViewport.ToSharedRef(), WidgetPath);

		WindowRenderTargetUpdate(Renderer, Window.Get());
		if (UseSeparateRenderTarget())
		{
			RTTSize = FIntPoint(TexSizeX, TexSizeY);
		}
	}
#endif
}

#if WITH_OCULUS_PRIVATE_CODE
void FOculusMR_CastingSceneViewport::ReleaseDynamicRHI()
{
	Super::ReleaseDynamicRHI();
}
#endif

void FOculusMR_CastingSceneViewport::SetExpectedLantencyInSeconds(double InLatency)
{
	if (ExpectedLantencyInSeconds != InLatency) 
	{ 
		ExpectedLantencyInSeconds = InLatency; 
	}
}

bool FOculusMR_CastingSceneViewport::IsStereoRenderingAllowed() const
{
	return false;
}
