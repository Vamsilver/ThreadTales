#include "ThreadTalesDecalLibrary.h"

bool UThreadTalesDecalLibrary::TraceNearestWall(const UObject* WorldContextObject, FVector Start,
    FVector End, ETraceTypeQuery TraceChannel, bool bTraceComplex,
    const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType,
    FHitResult& OutHit, bool bIgnoreSelf, FLinearColor TraceColor,
    FLinearColor TraceHitColor, float DrawTime)
{
    OutHit = FHitResult();
    if ((End - Start).IsNearlyZero()) return false;
    FHitResult ForwardHit, BackwardHit;
    const bool bForward = UKismetSystemLibrary::LineTraceSingle(WorldContextObject, Start, End,
        TraceChannel, bTraceComplex, ActorsToIgnore, DrawDebugType, ForwardHit, bIgnoreSelf,
        TraceColor, TraceHitColor, DrawTime);
    const bool bBackward = UKismetSystemLibrary::LineTraceSingle(WorldContextObject, Start,
        Start - (End - Start), TraceChannel, bTraceComplex, ActorsToIgnore, DrawDebugType,
        BackwardHit, bIgnoreSelf, TraceColor, TraceHitColor, DrawTime);
    if (!bForward && !bBackward) return false;
    OutHit = bBackward && (!bForward || BackwardHit.Distance < ForwardHit.Distance)
        ? BackwardHit : ForwardHit;
    return true;
}
