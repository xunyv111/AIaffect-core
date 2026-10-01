#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AffectRuntime.h"
#include "AffectWorldSubsystem.generated.h"

class UAffectProfileComponent;

/** Discovers registered profiles in one world without exposing numeric state. */
UCLASS()
class MYAITOWNAFFECTCORE_API UAffectWorldSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    void RegisterProfile(UAffectProfileComponent* Profile);
    void UnregisterProfile(UAffectProfileComponent* Profile);

    UFUNCTION(BlueprintPure, Category = "Affect")
    bool FindPresentation(const FString& CharacterId, FAffectPresentation& OutPresentation) const;

private:
    TMap<FString, TWeakObjectPtr<UAffectProfileComponent>> Profiles;
};
