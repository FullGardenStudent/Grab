// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GrabCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class USceneComponent;
class UCameraComponent;
class UMotionControllerComponent;
class UAnimMontage;
class USoundBase;

UCLASS(config=Game)
class AGrabCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Pawn mesh: 1st person view (arms; seen only by self) */
	UPROPERTY(VisibleDefaultsOnly, Category=Mesh)
	USkeletalMeshComponent* Mesh1P;

	/** Gun mesh: 1st person view (seen only by self) */
	UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
	USkeletalMeshComponent* FP_Gun;

	/** Location on gun mesh where projectiles should spawn. */
	// UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
	// USceneComponent* FP_MuzzleLocation;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

	UPROPERTY(EditAnywhere, Category="Components")
	USceneComponent* PickedObjectLocation;

public:
	AGrabCharacter();

protected:

public:
	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseTurnRate;

	/** Base look up/down rate, in deg/sec. Other scaling may affect final rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseLookUpRate;

	/** Gun muzzle's offset from the characters location */
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Gameplay)
	// FVector GunOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Gameplay)
	USoundBase* FireSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Gameplay)
	UAnimMontage* FireAnimation;

	/// we use physics handle component to drag and drop the actors/objects
	UPROPERTY(EditDefaultsOnly, Category = Components)
	class UPhysicsHandleComponent* PhysicsHandleComponent = nullptr;

protected:

	/// stuff to do when left mouse button is pressed.
	void OnFire();

	/// stuff to do after left mouse button is released.
	void AfterFire();

	void MoveForward(float Val);

	void MoveRight(float Val);

	void TurnAtRate(float Rate);

	void LookUpAtRate(float Rate);

	/// We need a scene component to attach an actor to component and hold it so that we could
	/// check whether the actor/object is simulating physics or now and stuff like that.
	UPROPERTY()
	UPrimitiveComponent* PickupObject;

	/// PlayerController is a child class of AController. We will use the mouse pointer from this class.
	APlayerController* MyPlayerController;

	/// get the center of screen at BeginPlay() and fix the mouse pointer's position at these
	/// points so that the pointer will stay at the center.
	float CenterX;
	float CenterY;

	/// Grab object's distance from mouse pointer
	float ObjectDistance;
	
	FVector MouseLocation, MouseDirection;


protected:

	virtual void BeginPlay() override;
	// APawn interface
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	// End of APawn interface

	/// create a tick event constructor.
	virtual void Tick(float DeltaSeconds) override;


public:
	/** Returns Mesh1P subobject **/
	USkeletalMeshComponent* GetMesh1P() const { return Mesh1P; }
	/** Returns FirstPersonCameraComponent subobject **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

};

