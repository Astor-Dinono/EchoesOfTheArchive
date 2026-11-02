#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "ArchiveRules.h"
#include "ArchiveGame.generated.h"

class UCameraComponent;
class USpotLightComponent;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UPointLightComponent;
class USoundBase;
class USoundAttenuation;
class AArchiveMaze;
class AArchiveCurator;

UCLASS()
class ECHOESOFTHEARCHIVE_API AArchiveGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AArchiveGameMode();
    virtual void BeginPlay() override;
    ArchiveRules::Progress Progress;
    UPROPERTY(BlueprintReadOnly) AArchiveMaze* Maze = nullptr;
    UPROPERTY(BlueprintReadOnly) AArchiveCurator* Curator = nullptr;
    FString Notice;
    float NoticeUntil = 0.f;
    void Tell(const FString& Text, float Seconds = 4.f);
    void Lose();
    void Win();
};

UCLASS()
class ECHOESOFTHEARCHIVE_API AArchivePlayer : public ACharacter
{
    GENERATED_BODY()
public:
    AArchivePlayer();
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UCameraComponent* Camera;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) USpotLightComponent* Flashlight;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balance") float WalkSpeed = 340.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balance") float SprintSpeed = 530.f;
    UPROPERTY(BlueprintReadOnly) float Stamina = 5.f;
    bool bWantsSprint = false;
    bool bSprinting = false;
    bool bLightOn = true;
    float PulseReadyAt = 0.f;
    float NextFootstep = 0.f;
    void MoveForward(float Value);
    void MoveRight(float Value);
    void LookYaw(float Value);
    void LookPitch(float Value);
    void SprintOn();
    void SprintOff();
    void ToggleLight();
    void Interact();
    void Decoy();
    void Pulse();
    void RestartRun();
};

UCLASS()
class ECHOESOFTHEARCHIVE_API AArchivePickup : public AActor
{
    GENERATED_BODY()
public:
    AArchivePickup();
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(VisibleAnywhere) UStaticMeshComponent* Mesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bShard = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PickupId = 0;
    FVector BasePosition;
    void Configure(bool IsShard, int32 Id);
};

UCLASS()
class ECHOESOFTHEARCHIVE_API AArchiveMaze : public AActor
{
    GENERATED_BODY()
public:
    AArchiveMaze();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(VisibleAnywhere) UInstancedStaticMeshComponent* Walls;
    UPROPERTY(VisibleAnywhere) UInstancedStaticMeshComponent* Floors;
    UPROPERTY(VisibleAnywhere) UInstancedStaticMeshComponent* Trim;
    UPROPERTY(VisibleAnywhere) UInstancedStaticMeshComponent* Books;
    UPROPERTY(VisibleAnywhere) UStaticMeshComponent* PowerGate;
    UPROPERTY(VisibleAnywhere) UStaticMeshComponent* Shelf;
    UPROPERTY(VisibleAnywhere) UStaticMeshComponent* ExitArch;
    UPROPERTY(VisibleAnywhere) UPointLightComponent* ExitLight;
    UPROPERTY() USoundAttenuation* CueAttenuation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bPowerOpen = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bShelfOpen = false;
    FVector DecoyPosition = FVector::ZeroVector;
    float DecoyUntil = 0.f;
    float NextDecoySound = 0.f;
    FVector PowerControl, ShelfControl;
    static FVector Position(ArchiveRules::Cell C, float Z=88.f);
    static ArchiveRules::Cell CellAt(FVector P);
    void Build();
    void UseNearby(AArchivePlayer* Player);
    FString InteractionPrompt(const AArchivePlayer* Player) const;
    bool ClearSight(FVector A, FVector B, const AActor* Ignore=nullptr) const;
    void PlayCue(const TCHAR* Asset, FVector Location, float Volume=0.4f);
private:
    bool bExitLit = false;
    void Label(const FString& Text, FVector P, FRotator Rotation, FColor Color, float Size=25.f);
    void Light(FVector P, FLinearColor Color, float Intensity);
    bool DoorOccupied(ArchiveRules::Cell Door) const;
};

UENUM(BlueprintType)
enum class ECuratorState : uint8 { Patrol, Investigate, Chase, Search, Stunned };

UCLASS()
class ECHOESOFTHEARCHIVE_API AArchiveCurator : public AActor
{
    GENERATED_BODY()
public:
    AArchiveCurator();
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(BlueprintReadOnly) ECuratorState State = ECuratorState::Patrol;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balance") float PatrolSpeed = 225.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balance") float ChaseSpeed = 390.f;
    float StunUntil = 0.f;
    float SenseAt = 0.f;
    float MemoryUntil = 0.f;
    FVector TargetPosition;
    FVector Waypoint;
    bool bHasWaypoint = false;
    int32 PatrolIndex = 0;
    bool bSeesPlayer = false;
    bool bMaterialInitialized = false;
    bool bStunMaterial = false;
    UPROPERTY() UStaticMeshComponent* Body;
    void Stun(float Seconds);
    FString StateName() const;
private:
    void Sense(AArchiveGameMode* GM, AArchivePlayer* Player);
};

UCLASS()
class ECHOESOFTHEARCHIVE_API AArchiveHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
};

