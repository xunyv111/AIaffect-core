#pragma once

#include "CoreMinimal.h"
#include "AffectRuntime.h"
#include "AffectAutoRule.generated.h"

/** Maps one newly-added Actor Tag to one bounded affect update. */
USTRUCT(BlueprintType)
struct MYAITOWNAFFECTCORE_API FAffectAutoRule
{
    GENERATED_BODY()

    /** Actor tag which represents the game event, for example Affect.Event.Conflict. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affect")
    FName TriggerActorTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affect")
    EAffectSourceKind SourceKind = EAffectSourceKind::SocialEvent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affect")
    int32 ValenceDelta = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affect")
    int32 ArousalDelta = 0;

    /** Optional stable cause identifier shown only to internal code. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affect")
    FString CauseEventId;

    /** Only additions are observed. Removing a tag never applies a reverse update. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affect")
    bool bApplyOnAdded = true;
};
