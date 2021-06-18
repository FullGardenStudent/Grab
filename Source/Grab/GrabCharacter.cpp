// Copyright Epic Games, Inc. All Rights Reserved.

#include "GrabCharacter.h"

#include "DrawDebugHelpers.h"
#include "GrabProjectile.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/InputSettings.h"
#include "Kismet/GameplayStatics.h"
#include "MotionControllerComponent.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPChar, Warning, All);

AGrabCharacter::AGrabCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);

	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-39.56f, 1.75f, 64.f)); // Position the camera
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	Mesh1P->SetRelativeRotation(FRotator(1.9f, -19.19f, 5.2f));
	Mesh1P->SetRelativeLocation(FVector(-0.5f, -4.4f, -155.7f));

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

	PickedObjectLocation = CreateDefaultSubobject<USceneComponent>(TEXT("PickedUpObjectLocation"));
	PickedObjectLocation->SetupAttachment(FP_Gun);  // Stick this component to First person character's gun.
}

void AGrabCharacter::BeginPlay()
{
	Super::BeginPlay();
	FP_Gun->AttachToComponent(Mesh1P, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true), TEXT("GripPoint"));
	PhysicsHandleComponent = NewObject<UPhysicsHandleComponent>(this, TEXT("PhysicsHandle"));			/// Create a new physicshandlecomopnent object
	PhysicsHandleComponent->RegisterComponent();							/// since PhysicsHandle is a componet, it's declared a bit differently, as per my understanding.
	FVector2D ScreenSize;													/// get the screen size so that we can set the crosshair at the center later
	GEngine->GameViewport->GetViewportSize(ScreenSize);					/// with this, ScreenSize 2 dimesion vector will have the size of screen stored in it.
	CenterX = ScreenSize.X /2;												/// do some math to get the center point of the screen
	CenterY = ScreenSize.Y /2;
	MyPlayerController = GetWorld()->GetFirstPlayerController();			/// GetFirstPlayerController() will return whatever is the default player controller
																			/// now, our Playercontroller is not null and holds a PlayerController
	if(MyPlayerController)													/// if the GetFirstPlayerController() returned null, out game will crash. to avoid if, we perform a check
	{
		MyPlayerController->bShowMouseCursor = true;						///setting this false will not show any mouse cursor
		MyPlayerController->CurrentMouseCursor= EMouseCursor::Crosshairs;	/// https://docs.unrealengine.com/4.26/en-US/Resources/ContentExamples/MouseInterface/SettingsAndCursors/index.html
	}
}

//////////////////////////////////////////////////////////////////////////
// Input

void AGrabCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	check(PlayerInputComponent);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &AGrabCharacter::OnFire);
	PlayerInputComponent->BindAction("Fire", IE_Released, this, &AGrabCharacter::AfterFire);

	PlayerInputComponent->BindAxis("MoveForward", this, &AGrabCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AGrabCharacter::MoveRight);

	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &AGrabCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AGrabCharacter::LookUpAtRate);
}

void AGrabCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if(MyPlayerController)																								/// check to avoid crashed from null pointer reference.
	{
		MyPlayerController->SetMouseLocation(CenterX,CenterY);															/// set mouse cursor location to center of screen
		MyPlayerController->DeprojectMousePositionToWorld(MouseLocation, MouseDirection);							/// this will assign cursor position in the world, 
																														/// location and direction to MouseLocation and MouseDirection
		MyPlayerController->SetInputMode(FInputModeGameOnly());													/// set input mode to this so that character can move freely.
		if(PickupObject)																								/// Check if after clicking left mosue button, the object is pickeable.
		{																												/// An object is assigned to PickupObject variable only if the object is movable.	
			if(PickupObject->IsSimulatingPhysics())																		/// Check if the picked object is simulating physics
			{
				if(PhysicsHandleComponent)																				/// make sure our PhysicsHandleComponent is not null
				{
					/// Draw a debug line(white colour) from picked object to mouse crsor's direction + that object's distance from cursor.
					DrawDebugLine(GetWorld(), PickupObject->GetComponentLocation(), MouseLocation+ MouseDirection*ObjectDistance, FColor::White, false, 0,0,3);
					PhysicsHandleComponent->SetTargetLocation(MouseLocation + (MouseDirection*ObjectDistance));			/// set the picked object's location to mousecursor's location + that object's distance from cursor
					PhysicsHandleComponent->SetInterpolationSpeed(4.0f);												/// changing this number will change the object grab delay speed
				}																										/// you can speed up or slow it down by chaning this 4.0f value
			}
		}
	}
}

void AGrabCharacter::OnFire()
{
	if(MyPlayerController)																								/// we will be using player Controller, so if it happens to be null at any point it'll crash. 
	{																													/// so, we'll check it before doing anything first
		FHitResult HitResult;
		FCollisionQueryParams Parameters;
		Parameters.AddIgnoredActor(this);																				/// just a straight line trace from mouse cursor
		if(GetWorld()->LineTraceSingleByChannel(HitResult,MouseLocation, (MouseDirection*5000) + MouseLocation, ECollisionChannel::ECC_Visibility, Parameters))
		{
			ObjectDistance = HitResult.Distance;																		/// if the linetrace hits something, get it's distance from cursor.
				if(HitResult.GetComponent()->Mobility == EComponentMobility::Movable)									/// check if the hit component has movable mobility. We can't move static objects.
				{
					PickupObject = HitResult.GetComponent();															/// if it's movable, assign it to PickupObject so that we can use it in Tick()
					if(PickupObject->IsSimulatingPhysics())																/// check if the picked object is simulating physics
					{
						MyPlayerController->CurrentMouseCursor = EMouseCursor::GrabHandClosed;							/// if it's simulating physics and left mouse is clicked, we want a closed hand cursor, just like in the video
						if(PhysicsHandleComponent)																		/// checking if PhysicsHandleComponet is null or not
						{
							PhysicsHandleComponent->GrabComponentAtLocation(PickupObject, NAME_None, HitResult.Location);	/// grab the component from that location. grab at the hit point, not from the object's origin.
						}																								/// if the object is hit at a corner, we will grab it a that corner, instead of from it's origin.
					}
					
				}
			
		}
	}
}

void AGrabCharacter::AfterFire()
{
	if(MyPlayerController)																							/// check if player controller is valid
	{
		MyPlayerController->CurrentMouseCursor = EMouseCursor::Crosshairs;											/// change the cursor to crosshair. 
	}
	if(PickupObject)																								/// make sure that PhysicsHandleComponent only releases objects if an object is picked first.
	{
		PhysicsHandleComponent->ReleaseComponent();																	/// release the object from the physics handle that we grabbed at left mouse click
	}
	PickupObject = nullptr;																							/// this is very important. If the PickupObject is not make a null pointer, that 
}																													/// first hit object will always stay as PickupObject throughout the game.


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