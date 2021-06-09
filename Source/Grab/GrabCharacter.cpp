// Copyright Epic Games, Inc. All Rights Reserved.

#include "GrabCharacter.h"

#include "DrawDebugHelpers.h"
#include "GrabProjectile.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/InputSettings.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "MotionControllerComponent.h"
#include "XRMotionControllerBase.h" // for FXRMotionControllerBase::RightHandSourceId

DEFINE_LOG_CATEGORY_STATIC(LogFPChar, Warning, All);

//////////////////////////////////////////////////////////////////////////
// AGrabCharacter

AGrabCharacter::AGrabCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Create a CameraComponent	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-39.56f, 1.75f, 64.f)); // Position the camera
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	Mesh1P->SetRelativeRotation(FRotator(1.9f, -19.19f, 5.2f));
	Mesh1P->SetRelativeLocation(FVector(-0.5f, -4.4f, -155.7f));

	// Create a gun mesh component
	FP_Gun = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FP_Gun"));
	FP_Gun->SetOnlyOwnerSee(false);			// otherwise won't be visible in the multiplayer
	FP_Gun->bCastDynamicShadow = false;
	FP_Gun->CastShadow = false;
	// FP_Gun->SetupAttachment(Mesh1P, TEXT("GripPoint"));
	FP_Gun->SetupAttachment(RootComponent);

	// FP_MuzzleLocation = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzleLocation"));
	// FP_MuzzleLocation->SetupAttachment(FP_Gun);
	// FP_MuzzleLocation->SetRelativeLocation(FVector(0.2f, 48.4f, -10.6f));

	// Default offset from the character location for projectiles to spawn
	// GunOffset = FVector(100.0f, 0.0f, 10.0f);

	///////////////////////////////////////////////////////////////////////////////////////////
	// Create a definition for object pickup location
	PickedObjectLocation = CreateDefaultSubobject<USceneComponent>(TEXT("PickedUpObjectLocation"));
	PickedObjectLocation->SetupAttachment(FP_Gun);  // Stick this component to First person character's gun.
}

void AGrabCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	//Attach gun mesh component to Skeleton, doing it here because the skeleton is not yet created in the constructor
	FP_Gun->AttachToComponent(Mesh1P, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true), TEXT("GripPoint"));

}

//////////////////////////////////////////////////////////////////////////
// Input

void AGrabCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// set up gameplay key bindings
	check(PlayerInputComponent);

	// Bind jump events
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	// Bind fire event
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &AGrabCharacter::OnFire);
	PlayerInputComponent->BindAction("Fire", IE_Released, this, &AGrabCharacter::AfterFire);

	// Bind movement events
	PlayerInputComponent->BindAxis("MoveForward", this, &AGrabCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AGrabCharacter::MoveRight);

	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &AGrabCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AGrabCharacter::LookUpAtRate);
}

void AGrabCharacter::Tick(float DeltaSeconds)
{
	if(LeftButtonCLicked)																								/// check if on fire is enabled.
	{
		FVector CameraStart = FirstPersonCameraComponent->GetComponentLocation();
		FVector CameraEnd = (FirstPersonCameraComponent->GetForwardVector() * 2000.0f) + CameraStart;
		FHitResult Hit;
		FCollisionQueryParams Params("GravityGun", false, this);
		Params.AddIgnoredActor(this);
		if(GetWorld()->LineTraceSingleByChannel(Hit,CameraStart,CameraEnd,ECC_Visibility,Params))			/// perform a line trace, I got no idea how to do line trace like the video, this is just an 
		{																												/// attempt to achieve that.
			FVector ObjectLocation = Hit.Actor->GetActorLocation();
			FVector PlayerCrossHairDrag = (this->GetActorForwardVector() * Hit.Distance);								
			DrawDebugLine(GetWorld(),ObjectLocation, PlayerCrossHairDrag,FColor::White,false,0.2,2,4);
		}
	}
}

void AGrabCharacter::OnFire()
{
	LeftButtonCLicked = true;																							////// I'm using this boolean so that a bebug line will be draw immediately by Tick() event
	float TraceLineLength = 2500.0f;
	FVector TraceStart = FirstPersonCameraComponent->GetComponentLocation();
	FVector TraceEnd = (FirstPersonCameraComponent->GetForwardVector() * TraceLineLength) + TraceStart;
	FHitResult Hit;
	FCollisionQueryParams QueryParams("GrabbingObjectTrace",false,this);
	if(GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))		////// Perform a line trace
	{
		if(UPrimitiveComponent* PrimComp = Hit.GetComponent())															///// Check if we hit an object. We need to change SimulatingPhysics() property to objects only.
		{																												///// Can't let the player drag air.
			if(PrimComp->IsSimulatingPhysics())	
			{
				PickTheObject(PrimComp);																				//// If the line trace hit an object and it also happens to generate physics, call PickTheObject.
			}																											//// PickTheObject will take the object that the line trace hit and change it's physics property.
		}
	}
}

void AGrabCharacter::AfterFire()
{
	LeftButtonCLicked = false;
	if(PickedObject)																									//// check if player has alrady picked up an object.
	{
		float ReleaseStrength = 2000.f;
		const FVector ReleaseVelocity = FirstPersonCameraComponent->GetForwardVector() * ReleaseStrength;				//// declare some shooting speed

		PickedObject->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);							/// since player has picked up an object, he shall release it				
		PickedObject->SetSimulatePhysics(true);																			/// After releasing the object, enable the physics so that the object can spin and fall.
		PickedObject->AddImpulse(ReleaseVelocity, NAME_None, true);											/// Add some force to the released object.

		PickTheObject(nullptr);																							/// clear the previous object data. It is required so that if the character doesn't
																														/// select another object in the next click, previous object will get affected.
																														/// like previously released cube spinning if the plyer shoots in the sky.
		
	}
	
	/////// I disabled the sound.
	// if (FireSound != nullptr)
	// {
	// 	UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetActorLocation());
	// }

	// try and play a firing animation if specified
	if (FireAnimation != nullptr)
	{
		// Get the animation object for the arms mesh
		UAnimInstance* AnimInstance = Mesh1P->GetAnimInstance();
		if (AnimInstance != nullptr)
		{
			AnimInstance->Montage_Play(FireAnimation, 1.f);
		}
	}
}


void AGrabCharacter::PickTheObject(UPrimitiveComponent* PickThisObject)
{
	PickedObject = PickThisObject;
	if(PickedObject)									
	{
		PickedObject->SetSimulatePhysics(true);																			/// if this is set to false, you could grab the object while it is still in air
	}
}

void AGrabCharacter::MoveForward(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorForwardVector(), Value);
	}
}

void AGrabCharacter::MoveRight(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorRightVector(), Value);
	}
}

void AGrabCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AGrabCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}