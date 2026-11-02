#include "ArchiveGame.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "DrawDebugHelpers.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    AArchiveGameMode* Game(const UObject* Context)
    { return Cast<AArchiveGameMode>(UGameplayStatics::GetGameMode(Context)); }

    UMaterialInterface* Material(const TCHAR* Name)
    { return LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("/Game/Materials/M_%s.M_%s"), Name, Name)); }

    void Apply(UStaticMeshComponent* Mesh, const TCHAR* Name)
    { if (UMaterialInterface* M = Material(Name)) Mesh->SetMaterial(0, M); }

    const TCHAR* ShardText[6] = {
        TEXT("RECORD 01 / We archived the missing names. Then someone erased the index."),
        TEXT("RECORD 02 / The Curator was ordered to protect the evidence."),
        TEXT("RECORD 03 / The archive was sealed while the witnesses were still inside."),
        TEXT("RECORD 04 / The coins were visitor tokens. The records were never for sale."),
        TEXT("RECORD 05 / Six fragments reconstruct the suppressed testimony."),
        TEXT("RECORD 06 / The truth is complete. Carry it out. The exit is now unlocked.")
    };
}

AArchiveGameMode::AArchiveGameMode()
{
    DefaultPawnClass = AArchivePlayer::StaticClass();
    HUDClass = AArchiveHUD::StaticClass();
}

void AArchiveGameMode::BeginPlay()
{
    Super::BeginPlay();
    for (TActorIterator<AArchiveMaze> It(GetWorld()); It; ++It) { Maze = *It; break; }
    if (!Maze) Maze = GetWorld()->SpawnActor<AArchiveMaze>();
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Curator = GetWorld()->SpawnActor<AArchiveCurator>(AArchiveMaze::Position(ArchiveRules::EnemyStart), FRotator(0,180,0), Params);
    if (AArchivePlayer* P = Cast<AArchivePlayer>(UGameplayStatics::GetPlayerPawn(this, 0)))
    {
        P->SetActorLocation(AArchiveMaze::Position(ArchiveRules::Start));
        if (P->Controller) P->Controller->SetControlRotation(FRotator::ZeroRotator);
    }
    Tell(TEXT("Find all 6 cyan shards, then reach the EXIT. Gold coins earn score and rewards."), 10.f);
}

void AArchiveGameMode::Tell(const FString& Text, float Seconds)
{ Notice = Text; NoticeUntil = GetWorld()->GetTimeSeconds()+Seconds; }

void AArchiveGameMode::Lose()
{
    if (!Progress.Active()) return;
    Progress.bLost = true;
    Tell(TEXT("THE CURATOR FOUND YOU / Press R to start a fresh run."), 10000.f);
    if (Maze) Maze->PlayCue(TEXT("Capture"), Curator ? Curator->GetActorLocation() : FVector::ZeroVector, 0.7f);
}

void AArchiveGameMode::Win()
{
    if (Progress.TryEscape())
    {
        Tell(TEXT("ARCHIVE RECOVERED / The suppressed testimony is safe. Press R to play again."), 10000.f);
        if (Maze) Maze->PlayCue(TEXT("Victory"), AArchiveMaze::Position(ArchiveRules::Exit), 0.6f);
    }
}

AArchivePlayer::AArchivePlayer()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    Camera->SetupAttachment(GetCapsuleComponent());
    Camera->SetRelativeLocation(FVector(0,0,62));
    Camera->bUsePawnControlRotation = true;
    Camera->FieldOfView = 85.f;
    Flashlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Flashlight"));
    Flashlight->SetupAttachment(Camera);
    Flashlight->SetRelativeLocation(FVector(15,8,-10));
    Flashlight->SetIntensity(12000.f);
    Flashlight->SetAttenuationRadius(2200.f);
    Flashlight->SetInnerConeAngle(16.f);
    Flashlight->SetOuterConeAngle(34.f);
    Flashlight->SetLightColor(FLinearColor(1.f,0.89f,0.68f));
    Flashlight->SetCastShadows(true);
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
    GetCharacterMovement()->JumpZVelocity = 0.f;
    GetCharacterMovement()->MaxStepHeight = 30.f;
}

void AArchivePlayer::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    Input->BindAxis("MoveForward", this, &AArchivePlayer::MoveForward);
    Input->BindAxis("MoveRight", this, &AArchivePlayer::MoveRight);
    Input->BindAxis("LookYaw", this, &AArchivePlayer::LookYaw);
    Input->BindAxis("LookPitch", this, &AArchivePlayer::LookPitch);
    Input->BindAction("Sprint", IE_Pressed, this, &AArchivePlayer::SprintOn);
    Input->BindAction("Sprint", IE_Released, this, &AArchivePlayer::SprintOff);
    Input->BindAction("Flashlight", IE_Pressed, this, &AArchivePlayer::ToggleLight);
    Input->BindAction("Interact", IE_Pressed, this, &AArchivePlayer::Interact);
    Input->BindAction("Decoy", IE_Pressed, this, &AArchivePlayer::Decoy);
    Input->BindAction("Pulse", IE_Pressed, this, &AArchivePlayer::Pulse);
    Input->BindAction("Restart", IE_Pressed, this, &AArchivePlayer::RestartRun);
}

void AArchivePlayer::MoveForward(float V)
{ if (Game(this) && Game(this)->Progress.Active()) AddMovementInput(GetActorForwardVector(), V); }
void AArchivePlayer::MoveRight(float V)
{ if (Game(this) && Game(this)->Progress.Active()) AddMovementInput(GetActorRightVector(), V); }
void AArchivePlayer::LookYaw(float V) { AddControllerYawInput(V); }
void AArchivePlayer::LookPitch(float V) { AddControllerPitchInput(V); }
void AArchivePlayer::SprintOn() { bWantsSprint = true; }
void AArchivePlayer::SprintOff() { bWantsSprint = false; }
void AArchivePlayer::ToggleLight()
{ if (Game(this) && Game(this)->Progress.Active()) { bLightOn = !bLightOn; Flashlight->SetVisibility(bLightOn); } }
void AArchivePlayer::Interact()
{ if (Game(this) && Game(this)->Progress.Active() && Game(this)->Maze) Game(this)->Maze->UseNearby(this); }
void AArchivePlayer::RestartRun()
{ UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this))); }

void AArchivePlayer::Tick(float Dt)
{
    Super::Tick(Dt);
    AArchiveGameMode* GM = Game(this);
    if (!GM || !GM->Progress.Active()) { GetCharacterMovement()->StopMovementImmediately(); return; }
    bSprinting = bWantsSprint && Stamina > 0.f && GetVelocity().Size2D() > 10.f;
    if (bSprinting) { Stamina = FMath::Max(0.f, Stamina-Dt); if (Stamina == 0.f) bWantsSprint = false; }
    else Stamina = FMath::Min(5.f, Stamina + Dt*0.8f);
    GetCharacterMovement()->MaxWalkSpeed = bSprinting ? SprintSpeed : WalkSpeed;
    const float Time = GetWorld()->GetTimeSeconds();
    if (GetVelocity().Size2D() > 30.f && Time > NextFootstep)
    {
        NextFootstep = Time + (bSprinting ? 0.3f : 0.52f);
        if (GM->Maze) GM->Maze->PlayCue(TEXT("Footstep"), GetActorLocation(), bSprinting ? 0.3f : 0.12f);
    }
    if (FVector::Dist2D(GetActorLocation(), AArchiveMaze::Position(ArchiveRules::Exit)) < 120.f) GM->Win();
}

void AArchivePlayer::Decoy()
{
    AArchiveGameMode* GM = Game(this);
    if (!GM || !GM->Progress.Active() || !GM->Maze) return;
    if (!GM->Progress.bDecoyUnlocked) { GM->Tell(TEXT("Sound decoys unlock at 200 points / 20 coins.")); return; }
    if (GM->Progress.Decoys <= 0) { GM->Tell(TEXT("No decoys left this run.")); return; }
    if (GM->Maze->DecoyUntil > GetWorld()->GetTimeSeconds()) { GM->Tell(TEXT("Your current decoy is still active.")); return; }
    --GM->Progress.Decoys;
    GM->Maze->DecoyPosition = GetActorLocation();
    GM->Maze->DecoyUntil = GetWorld()->GetTimeSeconds()+6.f;
    GM->Tell(TEXT("Decoy placed / Move away quietly. It cannot distract an enemy watching you."));
}

void AArchivePlayer::Pulse()
{
    AArchiveGameMode* GM = Game(this);
    if (!GM || !GM->Progress.Active()) return;
    const float Time = GetWorld()->GetTimeSeconds();
    if (!GM->Progress.bPulseUnlocked) { GM->Tell(TEXT("Memory Pulse unlocks at 400 points / 40 coins.")); return; }
    if (GM->Progress.Pulses <= 0) { GM->Tell(TEXT("No pulses left this run.")); return; }
    if (Time < PulseReadyAt) { GM->Tell(TEXT("Memory Pulse is cooling down.")); return; }
    if (!GM->Curator || FVector::Dist(GetActorLocation(), GM->Curator->GetActorLocation()) > ArchiveRules::PulseRange ||
        !GM->Maze->ClearSight(Camera->GetComponentLocation(), GM->Curator->GetActorLocation(), this))
    { GM->Tell(TEXT("No clear target within 8 metres / Pulse not spent.")); return; }
    --GM->Progress.Pulses;
    PulseReadyAt = Time+ArchiveRules::PulseCooldown;
    GM->Curator->Stun(ArchiveRules::PulseDuration);
    GM->Maze->PlayCue(TEXT("Pulse"), GetActorLocation(), 0.55f);
    DrawDebugSphere(GetWorld(), GetActorLocation(), 400.f, 24, FColor::Cyan, false, 0.6f, 0, 4.f);
    GM->Tell(TEXT("MEMORY PULSE / Curator stopped for 3 seconds. Run!"));
}

AArchivePickup::AArchivePickup()
{
    PrimaryActorTick.bCanEverTick = true;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pickup"));
    RootComponent = Mesh;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Ball(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    Mesh->SetStaticMesh(Ball.Object);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetCastShadow(false);
}

void AArchivePickup::Configure(bool IsShard, int32 Id)
{
    bShard = IsShard; PickupId = Id; BasePosition = GetActorLocation();
    if (bShard)
    {
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
        Mesh->SetWorldScale3D(FVector(0.3f,0.3f,0.65f));
        Mesh->SetRelativeRotation(FRotator(20,45,15));
        Apply(Mesh, TEXT("Shard"));
    }
    else { Mesh->SetWorldScale3D(FVector(0.40f,0.12f,0.40f)); Apply(Mesh,TEXT("Gold")); }
}

void AArchivePickup::Tick(float Dt)
{
    Super::Tick(Dt);
    AArchiveGameMode* GM = Game(this);
    if (!GM || !GM->Progress.Active()) return;
    AddActorLocalRotation(FRotator(0,75.f*Dt,0));
    SetActorLocation(BasePosition+FVector(0,0,8.f*FMath::Sin(GetWorld()->TimeSeconds*2.f+PickupId)));
    AArchivePlayer* P = Cast<AArchivePlayer>(UGameplayStatics::GetPlayerPawn(this,0));
    if (!P || FVector::Dist2D(P->GetActorLocation(),BasePosition)>85.f) return;
    const bool WasDecoy = GM->Progress.bDecoyUnlocked, WasPulse = GM->Progress.bPulseUnlocked;
    const bool Collected = bShard ? GM->Progress.CollectShard(PickupId) : GM->Progress.CollectCoin(PickupId);
    if (!Collected) return;
    if (bShard) GM->Tell(ShardText[PickupId],8.f);
    else if (!WasPulse && GM->Progress.bPulseUnlocked) GM->Tell(TEXT("400 POINTS / Memory Pulse unlocked: 2 uses. Press SPACE near the Curator."),7.f);
    else if (!WasDecoy && GM->Progress.bDecoyUnlocked) GM->Tell(TEXT("200 POINTS / Sound decoys unlocked: 3 uses. Press Q to place one."),7.f);
    if (GM->Maze) GM->Maze->PlayCue(bShard ? TEXT("Shard") : TEXT("Coin"), BasePosition, 0.35f);
    Destroy();
}

AArchiveMaze::AArchiveMaze()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    Walls = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Walls"));
    Floors = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Floors"));
    Trim = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ArchiveFrames"));
    Books = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ArchiveBoxes"));
    UInstancedStaticMeshComponent* Groups[] = {Walls,Floors,Trim,Books};
    for (auto* G : Groups) { G->SetupAttachment(RootComponent); G->SetStaticMesh(Cube.Object); G->SetCollisionProfileName(TEXT("BlockAll")); }
    Trim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Books->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PowerGate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PowerGate"));
    Shelf = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RotatingShelf"));
    ExitArch = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExitMarker"));
    for (UStaticMeshComponent* M : {PowerGate,Shelf,ExitArch})
    { M->SetupAttachment(RootComponent); M->SetStaticMesh(Cube.Object); M->SetMobility(EComponentMobility::Movable); M->SetCollisionProfileName(TEXT("BlockAll")); }
    ExitArch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ExitLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("ExitLight"));
    ExitLight->SetupAttachment(RootComponent);
    ExitLight->SetIntensity(4500.f);
    ExitLight->SetAttenuationRadius(600.f);
    ExitLight->SetCastShadows(false);
}

FVector AArchiveMaze::Position(ArchiveRules::Cell C, float Z)
{ return FVector(C.X*ArchiveRules::CellSize,C.Y*ArchiveRules::CellSize,Z); }
ArchiveRules::Cell AArchiveMaze::CellAt(FVector P)
{ return {FMath::RoundToInt(P.X/ArchiveRules::CellSize), FMath::RoundToInt(P.Y/ArchiveRules::CellSize)}; }

void AArchiveMaze::BeginPlay()
{
    Super::BeginPlay();
    CueAttenuation = NewObject<USoundAttenuation>(this);
    CueAttenuation->Attenuation.bAttenuate = true;
    CueAttenuation->Attenuation.bSpatialize = true;
    CueAttenuation->Attenuation.AttenuationShapeExtents = FVector(150.f);
    CueAttenuation->Attenuation.FalloffDistance = 1600.f;
    Build();
}

void AArchiveMaze::Label(const FString& Text, FVector P, FRotator Rotation, FColor Color, float Size)
{
    UTextRenderComponent* T = NewObject<UTextRenderComponent>(this);
    T->SetupAttachment(RootComponent); T->RegisterComponent();
    T->SetWorldLocation(P); T->SetWorldRotation(Rotation);
    T->SetText(FText::FromString(Text)); T->SetTextRenderColor(Color);
    T->SetHorizontalAlignment(EHTA_Center); T->SetWorldSize(Size);
}

void AArchiveMaze::Light(FVector P, FLinearColor Color, float Intensity)
{
    UPointLightComponent* L = NewObject<UPointLightComponent>(this);
    L->SetupAttachment(RootComponent); L->RegisterComponent(); L->SetWorldLocation(P);
    L->SetLightColor(Color); L->SetIntensity(Intensity); L->SetAttenuationRadius(900.f); L->SetCastShadows(false);
}

void AArchiveMaze::Build()
{
    Apply(Walls,TEXT("Wall")); Apply(Floors,TEXT("Floor")); Apply(Trim,TEXT("Trim")); Apply(Books,TEXT("Paper"));
    Apply(PowerGate,TEXT("Metal")); Apply(Shelf,TEXT("Trim")); Apply(ExitArch,TEXT("Locked"));
    const float Unit = ArchiveRules::CellSize;
    for (int Y=0; Y<ArchiveRules::Size; ++Y) for (int X=0; X<ArchiveRules::Size; ++X)
    {
        const ArchiveRules::Cell C{X,Y};
        Floors->AddInstance(FTransform(FRotator::ZeroRotator,Position(C,-15),FVector(Unit/100,Unit/100,0.3f)));
        const char V = ArchiveRules::Layout[Y][X];
        if (V == '#')
        {
            Walls->AddInstance(FTransform(FRotator::ZeroRotator,Position(C,175),FVector(Unit/100,Unit/100,3.5f)));
            const ArchiveRules::Cell Neighbors[4] = {{X+1,Y},{X-1,Y},{X,Y+1},{X,Y-1}};
            for (int D=0; D<4; ++D) if (ArchiveRules::Walkable(Neighbors[D],false,false))
            {
                const FVector Normal = (Position(Neighbors[D])-Position(C)).GetSafeNormal();
                const FVector Side(-Normal.Y,Normal.X,0);
                for (int Level=0; Level<3; ++Level)
                {
                    const FVector P = Position(C,55+Level*95)+Normal*202.f;
                    const FRotator R(0,D<2 ? 90.f : 0.f,0);
                    Trim->AddInstance(FTransform(R,P,FVector(3.7f,0.12f,0.1f)));
                    for (int Box=0; Box<4; ++Box)
                        Books->AddInstance(FTransform(R,P+Side*(Box*75-112)+FVector(0,0,32),FVector(0.55f,0.08f,0.52f)));
                }
            }
        }
        else if (V != 'G' && V != 'H')
        {
            Floors->AddInstance(FTransform(FRotator::ZeroRotator,Position(C,370),FVector(Unit/100,Unit/100,0.15f)));
            if ((X+Y*3)%7==0) Light(Position(C,285),FLinearColor(1.f,0.70f,0.35f),6000.f);
        }
    }
    PowerGate->SetWorldLocation(Position(ArchiveRules::PowerDoor,175));
    PowerGate->SetWorldScale3D(FVector(0.3f,4.f,3.5f));
    Shelf->SetWorldLocation(Position(ArchiveRules::ShelfDoor,175));
    Shelf->SetWorldScale3D(FVector(0.4f,4.f,3.5f));
    PowerControl = Position({5,7},100);
    ShelfControl = Position({9,13},100);
    Label(TEXT("POWER CONTROL [E]"),PowerControl+FVector(140,0,105),FRotator(0,180,0),FColor(255,198,95),22);
    Label(TEXT("ROTATE SHELF [E]"),ShelfControl+FVector(140,0,105),FRotator(0,180,0),FColor(255,198,95),22);
    Label(TEXT("EXIT / 6 SHARDS"),Position(ArchiveRules::Exit,220)+FVector(0,180,0),FRotator(0,-90,0),FColor(92,235,225),30);
    Label(TEXT("ECHOES OF THE ARCHIVE"),Position({3,1},240)+FVector(0,-195,0),FRotator(0,90,0),FColor(245,212,143),25);
    Light(Position(ArchiveRules::Start,270),FLinearColor(0.5f,0.75f,1.f),6000.f);
    ExitArch->SetWorldLocation(Position(ArchiveRules::Exit,15));
    ExitArch->SetWorldScale3D(FVector(2.f,2.f,0.15f));
    ExitLight->SetWorldLocation(Position(ArchiveRules::Exit,210));
    ExitLight->SetLightColor(FLinearColor(1.f,0.15f,0.08f));
    ArchiveRules::Cell Coins[ArchiveRules::CoinCount];
    const int Count = ArchiveRules::CoinCells(Coins);
    for (int I=0; I<Count; ++I)
    {
        AArchivePickup* P = GetWorld()->SpawnActor<AArchivePickup>(Position(Coins[I],100),FRotator::ZeroRotator);
        if (P) P->Configure(false,I);
    }
    for (int I=0; I<ArchiveRules::ShardCount; ++I)
    {
        AArchivePickup* P = GetWorld()->SpawnActor<AArchivePickup>(Position(ArchiveRules::Shards[I],110),FRotator::ZeroRotator);
        if (P) P->Configure(true,I);
        Light(Position(ArchiveRules::Shards[I],180),FLinearColor(0.04f,0.85f,1.f),3200.f);
    }
}

bool AArchiveMaze::ClearSight(FVector A, FVector B, const AActor* Ignore) const
{
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ArchiveSight),false);
    if (Ignore) Params.AddIgnoredActor(Ignore);
    const bool Blocked = GetWorld()->LineTraceSingleByChannel(Hit,A,B,ECC_Visibility,Params);
    const AActor* HitActor=Hit.GetActor();
    const bool TargetBody=HitActor && (HitActor->IsA(AArchivePlayer::StaticClass()) || HitActor->IsA(AArchiveCurator::StaticClass()));
    return !Blocked || (TargetBody && FVector::Dist(Hit.ImpactPoint,B)<100.f);
}

bool AArchiveMaze::DoorOccupied(ArchiveRules::Cell Door) const
{
    const FVector P = Position(Door);
    AArchiveGameMode* GM = Game(this);
    const APawn* Player = UGameplayStatics::GetPlayerPawn(this,0);
    auto Near = [&P](const AActor* A)
    { return A && FMath::Abs(A->GetActorLocation().X-P.X)<250.f && FMath::Abs(A->GetActorLocation().Y-P.Y)<250.f; };
    return Near(Player) || (GM && Near(GM->Curator));
}

FString AArchiveMaze::InteractionPrompt(const AArchivePlayer* P) const
{
    if (!P) return FString();
    if (FVector::Dist2D(P->GetActorLocation(),PowerControl)<260.f && ClearSight(P->Camera->GetComponentLocation(),PowerControl,P))
        return bPowerOpen ? TEXT("[E] Close power shortcut") : TEXT("[E] Power on / open shortcut");
    if (FVector::Dist2D(P->GetActorLocation(),ShelfControl)<260.f && ClearSight(P->Camera->GetComponentLocation(),ShelfControl,P))
        return bShelfOpen ? TEXT("[E] Rotate shelf / close shortcut") : TEXT("[E] Rotate shelf / open shortcut");
    return FString();
}

void AArchiveMaze::UseNearby(AArchivePlayer* P)
{
    AArchiveGameMode* GM = Game(this);
    if (!GM || InteractionPrompt(P).IsEmpty()) return;
    const bool Power = FVector::Dist2D(P->GetActorLocation(),PowerControl)<260.f;
    bool& Open = Power ? bPowerOpen : bShelfOpen;
    const ArchiveRules::Cell Door = Power ? ArchiveRules::PowerDoor : ArchiveRules::ShelfDoor;
    if (Open && DoorOccupied(Door)) { GM->Tell(TEXT("Doorway occupied / Cannot close this shortcut.")); return; }
    Open = !Open;
    UStaticMeshComponent* M = Power ? PowerGate : Shelf;
    M->SetCollisionEnabled(Open ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
    M->SetWorldRotation(FRotator(0,(!Power && Open)?90.f:0.f,0));
    M->SetWorldLocation(Position(Door,Power && Open ? 520.f : 175.f)+FVector(0,(!Power && Open)?190.f:0.f,0));
    GM->Tell(Open ? TEXT("Shortcut opened / The Curator can use it too.") : TEXT("Shortcut closed / Other routes remain open."));
    PlayCue(TEXT("Switch"),P->GetActorLocation());
}

void AArchiveMaze::PlayCue(const TCHAR* Asset, FVector Location, float Volume)
{
    const FString Path = FString::Printf(TEXT("/Game/Audio/A_%s.A_%s"),Asset,Asset);
    if (USoundBase* Sound = LoadObject<USoundBase>(nullptr,*Path)) UGameplayStatics::PlaySoundAtLocation(this,Sound,Location,Volume,1.f,0.f,CueAttenuation);
}

void AArchiveMaze::Tick(float Dt)
{
    Super::Tick(Dt);
    AArchiveGameMode* GM = Game(this);
    if (!GM || !GM->Progress.Active()) return;
    if (!bExitLit && GM->Progress.ShardsHeld == ArchiveRules::ShardCount)
    {
        bExitLit = true; Apply(ExitArch,TEXT("Shard"));
        ExitLight->SetLightColor(FLinearColor(0.02f,1.f,0.65f));
        GM->Tell(TEXT("ALL 6 SHARDS / Exit unlocked. Reach the green exit pad to win."),8.f);
    }
    const float Time = GetWorld()->GetTimeSeconds();
    if (DecoyUntil > Time)
    {
        DrawDebugSphere(GetWorld(),DecoyPosition-FVector(0,0,60),22,10,FColor::Yellow,false,-1,0,2);
        if (Time > NextDecoySound) { PlayCue(TEXT("Decoy"),DecoyPosition,0.45f); NextDecoySound = Time+0.8f; }
    }
}

AArchiveCurator::AArchiveCurator()
{
    PrimaryActorTick.bCanEverTick = true;
    UCapsuleComponent* Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Collision"));
    Capsule->InitCapsuleSize(42,80); Capsule->SetCollisionProfileName(TEXT("Pawn")); RootComponent = Capsule;
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CuratorBody"));
    Body->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Shape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    Body->SetStaticMesh(Shape.Object); Body->SetRelativeScale3D(FVector(0.85f,0.85f,1.8f));
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    UPointLightComponent* Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("DangerGlow"));
    Glow->SetupAttachment(RootComponent); Glow->SetRelativeLocation(FVector(0,0,60));
    Glow->SetLightColor(FLinearColor(1.f,0.02f,0.01f)); Glow->SetIntensity(4000); Glow->SetAttenuationRadius(500); Glow->SetCastShadows(false);
}

FString AArchiveCurator::StateName() const
{
    switch (State) {
        case ECuratorState::Investigate: return TEXT("INVESTIGATING");
        case ECuratorState::Chase: return TEXT("PURSUIT");
        case ECuratorState::Search: return TEXT("SEARCHING");
        case ECuratorState::Stunned: return TEXT("STUNNED");
        default: return TEXT("PATROL");
    }
}

void AArchiveCurator::Stun(float Seconds)
{ StunUntil = GetWorld()->GetTimeSeconds()+Seconds; State = ECuratorState::Stunned; }

void AArchiveCurator::Sense(AArchiveGameMode* GM, AArchivePlayer* P)
{
    const float Time = GetWorld()->GetTimeSeconds();
    FVector Offset = P->GetActorLocation()-GetActorLocation(); Offset.Z = 0;
    const float Dist = Offset.Size();
    const float Range = P->bLightOn ? 1900.f : 950.f;
    const bool InView = FVector::DotProduct(GetActorForwardVector(),Offset.GetSafeNormal()) > 0.30f || Dist < 220.f;
    bSeesPlayer = Dist < Range && InView && GM->Maze->ClearSight(GetActorLocation()+FVector(0,0,50),P->Camera->GetComponentLocation(),this);
    if (bSeesPlayer)
    {
        if (State != ECuratorState::Chase) GM->Maze->PlayCue(TEXT("Alert"),GetActorLocation(),0.5f);
        State = ECuratorState::Chase; TargetPosition = P->GetActorLocation(); MemoryUntil = Time+5.f;
    }
    else if (State == ECuratorState::Chase) { State = ECuratorState::Search; }
    else if (GM->Maze->DecoyUntil > Time && FVector::Dist2D(GetActorLocation(),GM->Maze->DecoyPosition)<2200.f)
    { State = ECuratorState::Investigate; TargetPosition = GM->Maze->DecoyPosition; MemoryUntil = GM->Maze->DecoyUntil; }
    else if (P->bSprinting && Dist < 1000.f)
    { State = ECuratorState::Investigate; TargetPosition = P->GetActorLocation(); MemoryUntil = Time+4.f; }
    else if (Time > MemoryUntil) State = ECuratorState::Patrol;
    if (State == ECuratorState::Patrol)
    {
        TargetPosition = AArchiveMaze::Position(ArchiveRules::Shards[PatrolIndex]);
        if (FVector::Dist2D(GetActorLocation(),TargetPosition)<70.f) PatrolIndex = (PatrolIndex+1)%ArchiveRules::ShardCount;
    }
}

void AArchiveCurator::Tick(float Dt)
{
    Super::Tick(Dt);
    AArchiveGameMode* GM = Game(this);
    AArchivePlayer* P = Cast<AArchivePlayer>(UGameplayStatics::GetPlayerPawn(this,0));
    if (!GM || !P || !GM->Maze || !GM->Progress.Active()) return;
    const float Time = GetWorld()->GetTimeSeconds();
    if (Time < StunUntil)
    {
        if (!bMaterialInitialized || !bStunMaterial) Apply(Body,TEXT("Shard"));
        bMaterialInitialized=true; bStunMaterial=true; return;
    }
    if (!bMaterialInitialized || bStunMaterial) Apply(Body,TEXT("Enemy"));
    bMaterialInitialized=true; bStunMaterial=false;
    if (State == ECuratorState::Stunned) { State = ECuratorState::Search; MemoryUntil=Time+3.f; }
    if (Time >= SenseAt) { Sense(GM,P); SenseAt=Time+0.15f; }
    if (FVector::Dist2D(GetActorLocation(),P->GetActorLocation())<100.f && GM->Maze->ClearSight(GetActorLocation(),P->GetActorLocation(),this))
    { GM->Lose(); return; }
    if (!bHasWaypoint || FVector::Dist2D(GetActorLocation(),Waypoint)<4.f)
    {
        ArchiveRules::Cell Next;
        const auto From = AArchiveMaze::CellAt(GetActorLocation());
        const auto To = AArchiveMaze::CellAt(TargetPosition);
        bHasWaypoint = ArchiveRules::NextStep(From,To,GM->Maze->bPowerOpen,GM->Maze->bShelfOpen,Next);
        if (bHasWaypoint)
        {
            Waypoint = AArchiveMaze::Position(Next);
            if (From == To && GM->Maze->ClearSight(GetActorLocation(),TargetPosition,this))
                Waypoint = FVector(TargetPosition.X,TargetPosition.Y,88.f);
        }
    }
    if (bHasWaypoint)
    {
        if (!ArchiveRules::Walkable(AArchiveMaze::CellAt(Waypoint),GM->Maze->bPowerOpen,GM->Maze->bShelfOpen)) { bHasWaypoint=false; return; }
        FVector D = Waypoint-GetActorLocation(); D.Z=0;
        if (D.Size2D()<1.f) { bHasWaypoint=false; return; }
        const float Step = FMath::Min(D.Size2D(),(State==ECuratorState::Chase ? ChaseSpeed : PatrolSpeed)*Dt);
        SetActorRotation(D.Rotation());
        FHitResult Hit;
        SetActorLocation(GetActorLocation()+D.GetSafeNormal()*Step,true,&Hit);
        if (Hit.bBlockingHit)
        {
            bHasWaypoint=false;
        }
    }
}

void AArchiveHUD::DrawHUD()
{
    Super::DrawHUD();
    AArchiveGameMode* GM = Game(this);
    AArchivePlayer* P = Cast<AArchivePlayer>(GetOwningPawn());
    if (!GM || !P || !Canvas) return;
    const float W = Canvas->SizeX, H = Canvas->SizeY;
    const float S = FMath::Clamp(W/1440.f,0.65f,1.5f);
    const FLinearColor Ivory(0.94f,0.92f,0.84f), Cyan(0.22f,0.88f,0.90f), Gold(1.f,0.73f,0.30f);
    auto Text = [this,S](const FString& T,FLinearColor C,float X,float Y,float Scale=1.f)
    { DrawText(T,C,X,Y,GEngine->GetMediumFont(),S*Scale*1.2f,false); };
    DrawRect(FLinearColor(0.012f,0.020f,0.027f,0.88f),0,0,W,115*S);
    Text(TEXT("E C H O E S  /  T H E  A R C H I V E"),Ivory,32*S,17*S,1.0f);
    Text(FString::Printf(TEXT("SCORE  %03d     COINS  %02d / 60"),GM->Progress.Score,GM->Progress.Coins),Gold,32*S,52*S);
    Text(FString::Printf(TEXT("MEMORY SHARDS  %d / 6"),GM->Progress.ShardsHeld),Cyan,32*S,82*S);
    Text(GM->Progress.ShardsHeld==6 ? TEXT("EXIT UNLOCKED / Return to the green pad") : TEXT("OBJECTIVE / Find all 6 cyan memory shards"),Ivory,W-460*S,24*S,0.92f);
    Text(P->bLightOn ? TEXT("FLASHLIGHT ON / More visible") : TEXT("FLASHLIGHT OFF / Less visible"),Gold,W-460*S,56*S,0.85f);
    if (GM->Curator && FVector::Dist2D(P->GetActorLocation(),GM->Curator->GetActorLocation())<2000.f)
        Text(TEXT("CURATOR / ")+GM->Curator->StateName(),FLinearColor(1,0.32f,0.24f),W-460*S,83*S,0.85f);
    DrawRect(FLinearColor(0.012f,0.020f,0.027f,0.9f),0,H-109*S,W,109*S);
    const FString D = GM->Progress.bDecoyUnlocked ? FString::Printf(TEXT("[Q] DECOY  %d uses"),GM->Progress.Decoys) : TEXT("[Q] DECOY  Locked / 200 pts");
    const float Cooldown = FMath::Max(0.f,P->PulseReadyAt-GetWorld()->GetTimeSeconds());
    const FString U = GM->Progress.bPulseUnlocked ? FString::Printf(TEXT("[SPACE] PULSE  %d uses / %s"),GM->Progress.Pulses,Cooldown>0 ? *FString::Printf(TEXT("%.0fs"),Cooldown) : TEXT("READY")) : TEXT("[SPACE] PULSE  Locked / 400 pts");
    Text(D,Gold,32*S,H-92*S,0.85f); Text(U,Cyan,380*S,H-92*S,0.85f);
    Text(TEXT("WASD Move   Mouse Look   SHIFT Sprint   F Light   E Interact   R Restart"),Ivory,32*S,H-31*S,0.77f);
    DrawRect(FLinearColor(0.15f,0.19f,0.21f),32*S,H-54*S,210*S,6*S);
    DrawRect(Cyan,32*S,H-54*S,210*S*P->Stamina/5.f,6*S);
    Text(TEXT("STAMINA"),Ivory,255*S,H-60*S,0.63f);
    DrawRect(Ivory,W/2-2,H/2-2,4,4);
    if (GM->Maze)
    {
        const FString Prompt=GM->Maze->InteractionPrompt(P);
        if (!Prompt.IsEmpty()) Text(Prompt,Gold,W/2-160*S,H/2+50*S,0.9f);
    }
    if (GM->NoticeUntil>GetWorld()->GetTimeSeconds())
    {
        DrawRect(FLinearColor(0.02f,0.03f,0.04f,0.94f),20*S,H-175*S,W-40*S,46*S);
        Text(GM->Notice,Ivory,32*S,H-161*S,0.79f);
    }
    if (!GM->Progress.Active())
    {
        DrawRect(FLinearColor(0.005f,0.01f,0.018f,0.82f),0,120*S,W,H-305*S);
        Text(GM->Progress.bWon ? TEXT("ARCHIVE RECOVERED") : TEXT("CAUGHT BY THE CURATOR"),GM->Progress.bWon ? Cyan : FLinearColor(1,0.3f,0.2f),W/2-260*S,H/2-60*S,1.8f);
        Text(FString::Printf(TEXT("Score %d  /  Shards %d of 6  /  Press R to restart"),GM->Progress.Score,GM->Progress.ShardsHeld),Ivory,W/2-240*S,H/2+10*S,1.f);
    }
}
