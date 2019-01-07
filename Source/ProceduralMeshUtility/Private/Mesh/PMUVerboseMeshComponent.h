////////////////////////////////////////////////////////////////////////////////
//
// MIT License
// 
// Copyright (c) 2018-2019 Nuraga Wiswakarma
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
////////////////////////////////////////////////////////////////////////////////
// 

#pragma once

#include "CoreMinimal.h"
#include "Components/MeshComponent.h"
#include "Interfaces/Interface_CollisionDataProvider.h"
#include "PhysicsEngine/ConvexElem.h"
#include "UObject/ObjectMacros.h"
#include "Mesh/PMUMeshTypes.h"
#include "Mesh/Simplify/PMUMeshSimplifier.h"
#include "PMUVerboseMeshComponent.generated.h"

class FPrimitiveSceneProxy;
class UBodySetup;

/**
*	Component that allows you to specify custom triangle mesh geometry
*	Beware! This feature is experimental and may be substantially changed in future releases.
*/
UCLASS(hidecategories=(Object, LOD), meta=(BlueprintSpawnableComponent), ClassGroup=Rendering)
class PROCEDURALMESHUTILITY_API UPMUVerboseMeshComponent : public UMeshComponent, public IInterface_CollisionDataProvider
{
	GENERATED_UCLASS_BODY()

public:

	/** 
	 *	Controls whether single material is used for all mesh sections
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Procedural Mesh")
	bool bUseSingleMaterial;

	/** 
	 *	Controls whether the complex (Per poly) geometry should be treated as 'simple' collision. 
	 *	Should be set to false if this component is going to be given simple collision and simulated.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Procedural Mesh")
	bool bUseComplexAsSimpleCollision;

	/**
	 *	Controls whether the physics cooking should be done off the game thread.
     *	This should be used when collision geometry doesn't have to be immediately up to date (For example streaming in far away objects)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Procedural Mesh")
	bool bUseAsyncCooking;

	// SECTION FUNCTIONS

	// Get the number of sections currently created for this component
	UFUNCTION(BlueprintCallable, Category="Components|ProceduralMesh")
	int32 GetNumSections() const;

	// Clear the geometry of a section
	UFUNCTION(BlueprintCallable, Category="Components|ProceduralMesh")
	void ClearMeshSectionGeometry(int32 SectionIndex);

	// Clear all mesh sections
	UFUNCTION(BlueprintCallable, Category="Components|ProceduralMesh")
	void ClearAllMeshSections();

	// Returns whether a particular section is currently visible
	UFUNCTION(BlueprintCallable, Category="Components|ProceduralMesh")
	bool IsMeshSectionVisible(int32 SectionIndex) const;

    // Get point to a mesh section
	FPMUMeshSectionBufferData* GetMeshSection(int32 SectionIndex);

	// Resizes section containers to hold at least the specified count
	UFUNCTION(BlueprintCallable, Category="Components|ProceduralMesh")
	void SetNumMeshSections(int32 SectionCount);

	// Replace a section with new section geometry
	UFUNCTION(BlueprintCallable, Category="Components|ProceduralMesh")
	void SetMeshSection(int32 SectionIndex, const FPMUMeshSectionBufferData& Section, bool bUpdateRenderState = true);

	// Request render state update
	UFUNCTION(BlueprintCallable, Category="Components|ProceduralMesh")
	void UpdateRenderState();

	UFUNCTION(BlueprintCallable, Category="Components|ProceduralMesh")
	void SimplifySection(int32 SectionIndex, const FPMUMeshSimplifierOptions& Options);

	// Add simple collision convex to this component
	UFUNCTION(BlueprintCallable, Category="Components|ProceduralMesh")
	void AddCollisionConvexMesh(TArray<FVector> ConvexVerts);

	// Add simple collision convex to this component
	UFUNCTION(BlueprintCallable, Category="Components|ProceduralMesh")
	void ClearCollisionConvexMeshes();

	// Replace all collision mesh with the specified meshes
	void SetCollisionConvexMeshes(const TArray< TArray<FVector> >& ConvexMeshes);

	//~ Begin Interface_CollisionDataProvider Interface
	virtual bool GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData) override;
	virtual bool ContainsPhysicsTriMeshData(bool InUseAllTriData) const override;
	virtual bool WantsNegXTriMesh() override{ return false; }
	//~ End Interface_CollisionDataProvider Interface

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual UBodySetup* GetBodySetup() override;
	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const override;
	virtual UMaterialInterface* GetMaterialFromCollisionFaceIndex(int32 FaceIndex, int32& SectionIndex) const override;
	//~ End UPrimitiveComponent Interface.

	//~ Begin UMeshComponent Interface.
	virtual int32 GetNumMaterials() const override;
	//~ End UMeshComponent Interface.

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	//~ End UObject Interface.

private:

	friend class FPMUVMeshSceneProxy;

	// Array of sections of mesh
	UPROPERTY()
	TArray<FPMUMeshSectionBufferData> MeshSections;

	// Convex shapes used for simple collision
	UPROPERTY()
	TArray<FKConvexElem> CollisionConvexElems;

	// Local space bounds of mesh
	UPROPERTY()
	FBoxSphereBounds LocalBounds;

	// Collision data
	UPROPERTY(Instanced)
	UBodySetup* MeshBodySetup;
	
	// Queue for async body setups that are being cooked
	UPROPERTY(transient)
	TArray<UBodySetup*> AsyncBodySetupQueue;

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ End USceneComponent Interface.

	// Update LocalBounds member from the local box of each section
	void UpdateLocalBounds();

	// Ensure MeshBodySetup is allocated and configured
	void CreateMeshBodySetup();

	// Mark collision data as dirty, and re-create on instance if necessary
	void UpdateCollision();

	// Once async physics cook is done, create needed state
	void FinishPhysicsAsyncCook(UBodySetup* FinishedBodySetup);

	// Helper to create new body setup objects
	UBodySetup* CreateBodySetupHelper();
};
