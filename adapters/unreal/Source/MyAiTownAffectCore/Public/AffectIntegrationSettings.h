#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AffectAutoRule.h"
#include "AffectIntegrationSettings.generated.h"

/** Project-wide defaults for the optional Actor Tag integration layer. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "My AI Town Affect"))
class MYAITOWNAFFECTCORE_API UAffectIntegrationSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    /** How often attached profiles inspect their owning Actor's Tags. */
    UPROPERTY(Config, EditAnywhere, Category = "Automatic integration", meta = (ClampMin = "0.05"))
    float DefaultPollingIntervalSeconds = 0.5f;

    /** Rules used by every profile before that profile's local rules. */
    UPROPERTY(Config, EditAnywhere, Category = "Automatic integration")
    TArray<FAffectAutoRule> DefaultActorTagRules;
};
