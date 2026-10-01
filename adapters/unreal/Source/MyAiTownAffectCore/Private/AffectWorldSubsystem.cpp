#include "AffectWorldSubsystem.h"

#include "AffectProfileComponent.h"

void UAffectWorldSubsystem::RegisterProfile(UAffectProfileComponent* Profile)
{
    if (IsValid(Profile))
    {
        const FString CharacterId = Profile->GetResolvedCharacterId();
        if (!Profiles.Contains(CharacterId))
        {
            Profiles.Add(CharacterId, Profile);
        }
    }
}

void UAffectWorldSubsystem::UnregisterProfile(UAffectProfileComponent* Profile)
{
    if (!IsValid(Profile))
    {
        return;
    }

    const FString CharacterId = Profile->GetResolvedCharacterId();
    if (const TWeakObjectPtr<UAffectProfileComponent>* Existing = Profiles.Find(CharacterId); Existing != nullptr && Existing->Get() == Profile)
    {
        Profiles.Remove(CharacterId);
    }
}

bool UAffectWorldSubsystem::FindPresentation(const FString& CharacterId, FAffectPresentation& OutPresentation) const
{
    const TWeakObjectPtr<UAffectProfileComponent>* Profile = Profiles.Find(CharacterId);
    return Profile != nullptr && Profile->IsValid() && Profile->Get()->GetCurrentPresentation(OutPresentation);
}
