// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TestCharacter.generated.h"

UCLASS()
class ATestCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ATestCharacter();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    void MoveForward(float AxisVal);
    void MoveRight(float AxisVal);

    void StartSlowTime();
    void StopSlowTime();


    void Doing360Scope();
    void ResetScopeCooldown();
    void ProcessScopeRotation(float DeltaTime);

    void ShootDestructionBall();
    void ShootAI();

    UPROPERTY(EditAnywhere, Category = "360 Scope")
    float ScopeJumpForce = 300.0f;

    UPROPERTY(EditAnywhere, Category = "360 Scope")
    float ScopeCooldown = 1.0f;

    UPROPERTY(EditAnywhere, Category = "360 Scope")
    float ScopeRotationDuration = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "360 Scope")
    float ScopeRotationSpeed = 180.f; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "360 Scope")
    float ScopeRotationSmoothing = 2.f;

    UPROPERTY(EditAnywhere, Category = "Time Control")
    float TimeDilationFactor = 0.3f;

private:
    
    bool bCanScope = true;
    bool bIsScoping = false;
    float ScopeRotationProgress = 0.f;
    FRotator ScopeStartRotation;
    FTimerHandle ScopeCooldownTimer;
    bool bIsRotating;
    float CurrentRotation;
    FRotator InitialRotation;
    float RotationSpeed;

    UPROPERTY(EditDefaultsOnly, Category = "Destruction")
    float Radius = 3.0f;
public:
    float Health = 100;
};
