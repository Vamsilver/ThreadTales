#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "ThreadTalesDecalLibrary.generated.h"

UCLASS()
class THREADTALES_API UThreadTalesDecalLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Searches both sides of a spline sample and returns the nearest blocking surface. */
    UFUNCTION(BlueprintCallable, Category="ThreadTales|Decals", meta=(WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", DisplayName="Trace Nearest Wall", AdvancedDisplay="TraceColor,TraceHitColor,DrawTime"))
    static bool TraceNearestWall(const UObject* WorldContextObject, FVector Start, FVector End,
        ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore,
        EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf,
        FLinearColor TraceColor=FLinearColor::Red, FLinearColor TraceHitColor=FLinearColor::Green,
        float DrawTime=5.0f);
};
