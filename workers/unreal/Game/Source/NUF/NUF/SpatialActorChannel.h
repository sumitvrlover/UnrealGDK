// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include <improbable/worker.h>

#include "Engine/ActorChannel.h"
#include "EntityId.h"
#include "SpatialOSCommandResult.h"
#include "Commander.h"
#include "improbable/worker.h"
#include "improbable/standard_library.h"
#include "SpatialTypeBinding.h"
#include "SpatialNetDriver.h"
#include "SpatialActorChannel.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSpatialOSActorChannel, Log, All);

class USpatialNetDriver;

// A replacement actor channel that plugs into the Engine's replication system and works with SpatialOS
UCLASS(Transient)
class NUF_API USpatialActorChannel : public UActorChannel
{
	GENERATED_BODY()

public:
	USpatialActorChannel(const FObjectInitializer & ObjectInitializer = FObjectInitializer::Get());

	// SpatialOS Entity ID.
	FORCEINLINE worker::EntityId GetEntityId() const
	{
		return ActorEntityId;
	}

	FORCEINLINE bool IsReadyForReplication() const
	{
		// Wait until we've reserved an entity ID.		
		return ActorEntityId != worker::EntityId{};
	}

	// Called on the client when receiving an update.
	FORCEINLINE bool IsClientAutonomousProxy(worker::ComponentId ServerRPCsComponentId)
	{
		check(SpatialNetDriver->GetNetMode() == NM_Client);
		TSharedPtr<worker::View> View = WorkerView.Pin();
		if (View.Get())
		{
			// This will never fail because we can't have an actor channel without having checked out the entity.
			auto& EntityAuthority = View->ComponentAuthority[ActorEntityId];
			auto ComponentIterator = EntityAuthority.find(ServerRPCsComponentId);
			if (ComponentIterator != EntityAuthority.end())
			{
				return (*ComponentIterator).second == worker::Authority::kAuthoritative;
			}
		}
		return false;
	}

	FORCEINLINE FPropertyChangeState GetChangeState(const TArray<uint16>& Changed) const
	{
		return{
			Changed,
			(uint8*)Actor,
			ActorReplicator->RepLayout->Cmds,
			ActorReplicator->RepLayout->BaseHandleToCmdIndex,
		};
	}

	// UChannel interface
	virtual void Init(UNetConnection * connection, int32 channelIndex, bool bOpenedLocally) override;
	//Requires source changes to be virtual in base class.
	virtual bool ReplicateActor() override;
	virtual void SetChannelActor(AActor* InActor) override;

	// Distinguishes between channels created for actors that went through the "old" pipeline vs actors that are triggered through SpawnActor() calls.
	//In the future we may not use an actor channel for non-core actors.
	UPROPERTY(transient)
	bool bCoreActor;

protected:
	// UChannel interface
	virtual bool CleanUp(const bool bForDestroy) override;
private:
	void BindToSpatialView();
	void UnbindFromSpatialView() const;

	void OnReserveEntityIdResponse(const worker::ReserveEntityIdResponseOp& Op);
	void OnCreateEntityResponse(const worker::CreateEntityResponseOp& Op);
	TWeakPtr<worker::Connection> WorkerConnection;
	TWeakPtr<worker::View> WorkerView;
	worker::EntityId ActorEntityId;

	worker::Dispatcher::CallbackKey ReserveEntityCallback;
	worker::Dispatcher::CallbackKey CreateEntityCallback;
	
	worker::RequestId<worker::ReserveEntityIdRequest> ReserveEntityIdRequestId;
	worker::RequestId<worker::CreateEntityRequest> CreateEntityRequestId;

	UPROPERTY(transient)
	USpatialNetDriver* SpatialNetDriver;

	FVector LastSpatialPosition;

	// If this actor channel is responsible for creating a new entity, this will be set to true during initial replication.
	UPROPERTY(Transient)
	bool bCreatingNewEntity;

private:
	void UpdateSpatialPosition(bool SendComponentUpdate);
};