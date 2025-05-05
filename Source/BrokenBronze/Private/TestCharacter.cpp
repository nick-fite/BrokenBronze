// Fill out your copyright notice in the Description page of Project Settings.


#include "TestCharacter.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Generation/MarchingCubeObject.h"
#include "NavigationSystem.h"


ATestCharacter::ATestCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    bCanScope = true;
    bIsScoping = false;
    ScopeRotationProgress = 0.f;
}

void ATestCharacter::BeginPlay()
{
    Super::BeginPlay();
}

void ATestCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bIsScoping)
    {
        ProcessScopeRotation(DeltaTime);
    }
}

void ATestCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    PlayerInputComponent->BindAxis("MoveForward", this, &ATestCharacter::MoveForward);
    PlayerInputComponent->BindAxis("MoveRight", this, &ATestCharacter::MoveRight);
    PlayerInputComponent->BindAxis("Turn", this, &ATestCharacter::AddControllerYawInput);
    PlayerInputComponent->BindAxis("LookUp", this, &ATestCharacter::AddControllerPitchInput);
    PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ATestCharacter::Jump);
    PlayerInputComponent->BindAction("StopTime", IE_Pressed, this, &ATestCharacter::StartSlowTime);
    PlayerInputComponent->BindAction("StopTime", IE_Released, this, &ATestCharacter::StopSlowTime);
    PlayerInputComponent->BindAction("360Scope", IE_Pressed, this, &ATestCharacter::Doing360Scope);
    PlayerInputComponent->BindAction("RightMouseClick", IE_Pressed, this, &ATestCharacter::ShootDestructionBall);
    PlayerInputComponent->BindAction("LeftMouseClick", IE_Pressed, this, &ATestCharacter::ShootAI);
}

void ATestCharacter::MoveForward(float AxisVal)
{
    AddMovementInput(GetActorForwardVector() * AxisVal);
}

void ATestCharacter::MoveRight(float AxisVal)
{
    AddMovementInput(GetActorRightVector() * AxisVal);
}

void ATestCharacter::StartSlowTime()
{
    if (GetWorld())
    {
        GetWorld()->GetWorldSettings()->SetTimeDilation(TimeDilationFactor);
    }
}

void ATestCharacter::StopSlowTime()
{
    if (GetWorld())
    {
        GetWorld()->GetWorldSettings()->SetTimeDilation(1.0f);
    }
}

void ATestCharacter::Doing360Scope()
{
    if (!bCanScope || bIsScoping) return;

    UWorld* World = GetWorld();
    if (!World) return;

    if (GetCharacterMovement())
    {
        LaunchCharacter(FVector(0, 0, ScopeJumpForce), false, false);

        if (APlayerController* PC = Cast<APlayerController>(GetController()))
        {
            ScopeStartRotation = PC->GetControlRotation();
            bIsScoping = true;
            ScopeRotationProgress = 0.f;
        }

        bCanScope = false;
        World->GetTimerManager().SetTimer(
            ScopeCooldownTimer,
            this,
            &ATestCharacter::ResetScopeCooldown,
            ScopeCooldown,
            false
        );
    }
}

void ATestCharacter::ProcessScopeRotation(float DeltaTime)
{
    if (!GetController() || !bIsScoping) return;

    //float EaseFactor = 1.0f - FMath::Square(1.0f - (ScopeRotationProgress / ScopeRotationDuration));

    // we figured that 720 made it turn 5 rounds, so...
    float RotationThisFrame = ((720.f / 5.f) / ScopeRotationDuration) * DeltaTime; 

    AddControllerYawInput(RotationThisFrame);

    ScopeRotationProgress += DeltaTime;

    if (ScopeRotationProgress >= ScopeRotationDuration)
    {
        bIsScoping = false;
        ScopeRotationProgress = 0.f;

        FRotator FinalRot = GetControlRotation();
        FinalRot.Yaw = FRotator::NormalizeAxis(FinalRot.Yaw);
        GetController()->SetControlRotation(FinalRot);
    }
}

void ATestCharacter::ResetScopeCooldown()
{
    bCanScope = true;
}
void ATestCharacter::ShootDestructionBall()
{
      UWorld* World = GetWorld();
    if (!World) return;
    
    // Get the player's view point
    FVector Location;
    FRotator Rotation;
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        PC->GetPlayerViewPoint(Location, Rotation);
    }
    else
    {
        Location = GetActorLocation();
        Rotation = GetActorRotation();
    }
    
    FVector Direction = Rotation.Vector();
    FVector End = Location + (Direction * 10000.0f);
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    
    // Perform the raycast
    FHitResult HitResult;
    bool bHit = World->LineTraceSingleByChannel(
        HitResult,
        Location,
        End,
        ECC_Visibility,
        QueryParams
    );
    
    if (bHit)
    {
        // Check if we hit a marching cube object
        AMarchingCubeObject* MarchingCube = Cast<AMarchingCubeObject>(HitResult.GetActor());
        if (MarchingCube)
        {
            // Call MakeHole with the impact position
            MarchingCube->MakeHole(HitResult.ImpactPoint, Radius);
        }
    }
}

void ATestCharacter::ShootAI()
{
    UWorld* World = GetWorld();
    if (!World) return;
    
    // Get the player's view point
    FVector Location;
    FRotator Rotation;
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        PC->GetPlayerViewPoint(Location, Rotation);
    }
    else
    {
        Location = GetActorLocation();
        Rotation = GetActorRotation();
    }
    
    FVector Direction = Rotation.Vector();
    FVector End = Location + (Direction * 10000.0f);
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    
    // Perform the raycast
    FHitResult HitResult;
    bool bHit = World->LineTraceSingleByChannel(
        HitResult,
        Location,
        End,
        ECC_Pawn,
        QueryParams
    );
    
    if (bHit)
    {
        // Check if hit actor is controlled by AI
        AActor* OwnerActor = HitResult.GetComponent()->GetOwner();
        APawn* HitPawn = Cast<APawn>(OwnerActor);
        if (HitPawn)
        {
            UE_LOG(LogTemp, Warning, TEXT("Hit a pawn!"));
            AAIController* AIController = Cast<AAIController>(HitPawn->GetController());
            if (AIController)
            {
                // Access the blackboard component
                UBlackboardComponent* BlackboardComp = AIController->GetBlackboardComponent();
                if (BlackboardComp)
                {
                    // Get current health and reduce it
                    float CurrentHealth = BlackboardComp->GetValueAsFloat("Health");
                    float DamageAmount = 25.0f; // Can be made configurable
                    float NewHealth = FMath::Max(0.0f, CurrentHealth - DamageAmount);
                    
                    // Update the health value in blackboard
                    BlackboardComp->SetValueAsFloat("Health", NewHealth);
                    
                    UE_LOG(LogTemp, Warning, TEXT("Hit AI! Health reduced from %.1f to %.1f"), 
                        CurrentHealth, NewHealth);
                }
            }
        }
    }
}
