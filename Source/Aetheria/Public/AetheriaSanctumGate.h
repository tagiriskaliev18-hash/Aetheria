#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AetheriaSanctumGate.generated.h"

class UStaticMeshComponent;
class USphereComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAetheriaGateOpened);

UCLASS()
class AETHERIA_API AAetheriaSanctumGate : public AActor
{
	GENERATED_BODY()

public:
	AAetheriaSanctumGate();

	virtual void Tick(float DeltaSeconds) override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;

	UFUNCTION(BlueprintCallable, Category = "Aetheria|Gate")
	void TryOpen();

	UFUNCTION(BlueprintPure, Category = "Aetheria|Gate")
	bool IsOpen() const { return bIsOpen; }

	UPROPERTY(BlueprintAssignable, Category = "Aetheria|Gate")
	FOnAetheriaGateOpened OnGateOpened;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Gate")
	TObjectPtr<UStaticMeshComponent> GateMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Gate")
	TObjectPtr<USphereComponent> InteractSphere;

protected:
	virtual void BeginPlay() override;

	bool bIsOpen = false;
	bool bPlayerNearby = false;
	float OpenAlpha = 0.0f;
	FRotator ClosedRot = FRotator::ZeroRotator;
};
