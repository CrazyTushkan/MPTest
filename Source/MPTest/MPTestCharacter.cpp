// Copyright Epic Games, Inc. All Rights Reserved.

#include "MPTestCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"

//////////////////////////////////////////////////////////////////////////
// AMPTestCharacter

AMPTestCharacter::AMPTestCharacter():
	CreateSessionComplete(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreateSessionComplete)),
	RecreateSessionAfterDestroy(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnRecreateSessionAfterDestroy)),
	FindSessionsComplete(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFindSessionsComplete))
{

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// set our turn rate for input
	TurnRateGamepad = 50.f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character)
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)


	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem) {
		SessionInterface = OnlineSubsystem->GetSessionInterface();

		if (GEngine) {
			GEngine->AddOnScreenDebugMessage(
				-1, 15.f, FColor::Blue, FString::Printf(TEXT("Found subsystem %s"), *OnlineSubsystem->GetSubsystemName().ToString()));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Input

void AMPTestCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAxis("Move Forward / Backward", this, &AMPTestCharacter::MoveForward);
	PlayerInputComponent->BindAxis("Move Right / Left", this, &AMPTestCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn Right / Left Mouse", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("Turn Right / Left Gamepad", this, &AMPTestCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("Look Up / Down Mouse", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("Look Up / Down Gamepad", this, &AMPTestCharacter::LookUpAtRate);

	// handle touch devices
	PlayerInputComponent->BindTouch(IE_Pressed, this, &AMPTestCharacter::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &AMPTestCharacter::TouchStopped);
}

void AMPTestCharacter::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (this->IsSessionInterfaceValid()) {
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
	}

	if (GEngine) {
		GEngine->AddOnScreenDebugMessage(
			-1, 15.f, bWasSuccessful ? FColor::Blue : FColor::Red, FString::Printf(TEXT("Session %s created: %d"), *SessionName.ToString(), bWasSuccessful));
	}
}

void AMPTestCharacter::OnFindSessionsComplete(bool bWasSuccessful)
{
	for (auto Result : SessionSearch->SearchResults) {
		FString Id = Result.GetSessionIdStr();
		FString User = Result.Session.OwningUserName;

		if (GEngine) {
			GEngine->AddOnScreenDebugMessage(
				-1, 15.f, FColor::Blue, FString::Printf(TEXT("Id: %s, User: %s"), *Id, *User));
		}	}
}

void AMPTestCharacter::OnRecreateSessionAfterDestroy(FName SessionName, bool bDestroyWasSuccessful)
{
	if (this->IsSessionInterfaceValid()) {
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(RecreateSessionAfterDestroyHandle);
		if (bDestroyWasSuccessful) {
			this->CreateSession();
		}
		/* TODO: Add handling for the case when session was failed to destroy */
	}
}

void AMPTestCharacter::CreateSession()
{
	if (this->IsSessionInterfaceValid()) {
		auto ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
		if (ExistingSession != nullptr) {
			RecreateSessionAfterDestroyHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(RecreateSessionAfterDestroy);
			SessionInterface->DestroySession(NAME_GameSession);

			return;
		}

		TSharedPtr<FOnlineSessionSettings> SessionSettings = MakeShareable(new FOnlineSessionSettings());
		SessionSettings->bIsLANMatch = false;
		SessionSettings->NumPublicConnections = 4;
		SessionSettings->bAllowJoinInProgress = true;
		SessionSettings->bAllowJoinViaPresence = true;
		SessionSettings->bShouldAdvertise = true;
		SessionSettings->bUsesPresence = true;
		SessionSettings->bUseLobbiesIfAvailable = true;

		const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();

		CreateSessionCompleteHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionComplete);
		SessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, *SessionSettings);
	}

}

void AMPTestCharacter::JoinSession()
{
	if (IsSessionInterfaceValid()) {
		SessionSearch = MakeShareable(new FOnlineSessionSearch());
		SessionSearch->MaxSearchResults = 10000;
		SessionSearch->bIsLanQuery = false;
		SessionSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);

		const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();

		FindSessionsCompleteHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsComplete);
		SessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), SessionSearch.ToSharedRef());
	}
}

void AMPTestCharacter::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
	Jump();
}

void AMPTestCharacter::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
	StopJumping();
}

void AMPTestCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * TurnRateGamepad * GetWorld()->GetDeltaSeconds());
}

void AMPTestCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * TurnRateGamepad * GetWorld()->GetDeltaSeconds());
}

void AMPTestCharacter::MoveForward(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void AMPTestCharacter::MoveRight(float Value)
{
	if ( (Controller != nullptr) && (Value != 0.0f) )
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get right vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}

bool AMPTestCharacter::IsSessionInterfaceValid() {
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();

	if (OnlineSubsystem) {
		if (SessionInterface.IsValid()) {
			return true;
		}
	}

	return false;
}
