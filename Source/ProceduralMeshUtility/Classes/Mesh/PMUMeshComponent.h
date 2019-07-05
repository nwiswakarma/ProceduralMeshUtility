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
#include "PMUMeshComponent.generated.h"

class FPrimitiveSceneProxy;
class UBodySetup;

/**
*	Component that allows you to specify custom triangle mesh geometry
*	Beware! This feature is experimental and may be substantially changed in future releases.
*/
UCLASS(hidecategories=(Object, LOD), meta=(BlueprintSpawnableComponent), ClassGroup=Rendering)
class PROCEDURALMESHUTILITY_API UPMUMeshComponent : public UMeshComponent, public IInterface_CollisionDataProvider
{
	GENERATED_UCLASS_BODY()

	friend class FPMUMeshSceneProxy;

    typedef TArray<int32> FSectionIdGroup;

public:

	/** 
	 * Controls whether the single material is used for all mesh sections.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Procedural Mesh")
	bool bUseSingleMaterial;

	/** 
	 * Controls whether the complex (Per poly) geometry should be treated as 'simple' collision. 
	 * Should be set to false if this component is going to be given simple collision and simulated.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Procedural Mesh")
	bool bUseComplexAsSimpleCollision;

	/**
	* Controls whether the physics cooking should be done off the game thread.
    * Should be used when collision geometry doesn't have to be immediately up to date (For example streaming in far away objects)
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Procedural Mesh")
	bool bUseAsyncCooking;

	/** Collision data */
	UPROPERTY(Instanced)
	UBodySetup* MeshBodySetup;

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	//~ End UObject Interface.

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual UBodySetup* GetBodySetup() override;
	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const override;
	virtual UMaterialInterface* GetMaterialFromCollisionFaceIndex(int32 FaceIndex, int32& SectionIndex) const override;
	//~ End UPrimitiveComponent Interface.

	//~ Begin UMeshComponent Interface.
	virtual int32 GetNumMaterials() const override;
	//~ End UMeshComponent Interface.

	//~ Begin Interface_CollisionDataProvider Interface
	virtual bool GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData) override;
	virtual bool ContainsPhysicsTriMeshData(bool InUseAllTriData) const override;
	virtual bool WantsNegXTriMesh() override{ return false; }
	//~ End Interface_CollisionDataProvider Interface

	FPMUMeshSceneProxy* GetSceneProxy();

	int32 CreateNewSection(const FPMUMeshSectionRef& Section, int32 GroupIndex = -1);
	void AssignSectionGroup(int32 SectionIndex, int32 GroupIndex);

	/** Request render state update */
	UFUNCTION(BlueprintCallable, Category="Components|PMU Mesh")
	void UpdateRenderState();

	UFUNCTION(BlueprintCallable, Category="Components|PMU Mesh", meta=(DisplayName="Create Section From Section Reference"))
	void K2_CreateSectionFromRef(int32 SectionIndex, const FPMUMeshSectionRef& Section) { CreateSection(SectionIndex, Section); }
	void CreateSection(int32 SectionIndex, const FPMUMeshSectionRef& Section);

	/**
	 *	Create/replace a section for this procedural mesh component.
	 *	@param	SectionIndex		Index of the section to create or replace.
	 *	@param	Positions			Vertex buffer of all vertex positions to use for this mesh section.
	 *	@param	Indices			    Index buffer indicating which vertices make up each triangle. Length must be a multiple of 3.
	 *	@param	Normals				Optional array of normal vectors for each vertex. If supplied, must be same length as Vertices array.
	 *	@param	Tangents			Optional array of tangent vector for each vertex. If supplied, must be same length as Vertices array.
	 *	@param	UVs					Optional array of texture co-ordinates for each vertex. If supplied, must be same length as Vertices array.
	 *	@param	LinearColors		Optional array of colors for each vertex. If supplied, must be same length as Vertices array.
	 *	@param	bCreateCollision	Indicates whether collision should be created for this section. This adds significant cost.
	 */
	UFUNCTION(BlueprintCallable, Category="Components|PMU Mesh", meta=(DisplayName="Create Mesh Section", AutoCreateRefTerm="Normals,Tangents,UVs,LinearColors"))
	void CreateMeshSection_LinearColor(
        int32 SectionIndex,
        const TArray<FVector>& Positions,
        const TArray<int32>& Indices,
        const TArray<FVector>& Normals,
        const TArray<FVector>& Tangents,
        const TArray<FVector2D>& UVs,
        const TArray<FLinearColor>& LinearColors,
        FBox SectionBounds,
        bool bCreateCollision = false,
        bool bCalculateBounds = true
        );

	UFUNCTION(BlueprintCallable, Category="Components|PMU Mesh")
	void CopyMeshSection(
        int32 SectionIndex,
        const FPMUMeshSection& SourceSection,
        bool bCalculateBounds = true
        );

	// Expand section bounds
	UFUNCTION(BlueprintCallable, Category = "Components|PMU Mesh")
	void ExpandSectionBounds(int32 SectionIndex, const FVector& NegativeExpand, const FVector& PositiveExpand);

	/** Clear a section of the procedural mesh. Other sections do not change index. */
	UFUNCTION(BlueprintCallable, Category = "Components|PMU Mesh")
	void ClearSection(int32 SectionIndex);

	/** Clear all mesh sections and reset to empty state */
	UFUNCTION(BlueprintCallable, Category = "Components|PMU Mesh")
	void ClearAllSections();

	/** Returns whether a particular section is currently visible */
	UFUNCTION(BlueprintCallable, Category = "Components|PMU Mesh")
	bool IsValidSection(int32 SectionIndex) const;

	/** Returns whether a particular section is currently visible */
	UFUNCTION(BlueprintCallable, Category = "Components|PMU Mesh")
	bool IsValidNonEmptySection(int32 SectionIndex) const;

	/** Returns whether a particular section is currently visible */
	UFUNCTION(BlueprintCallable, Category = "Components|PMU Mesh")
	bool IsSectionVisible(int32 SectionIndex) const;

	/** Returns number of sections currently created for this component */
	UFUNCTION(BlueprintCallable, Category = "Components|PMU Mesh")
	int32 GetNumSections() const;

	UFUNCTION(BlueprintCallable, Category="Components|PMU Mesh")
	FPMUMeshSectionRef GetSectionRef(int32 SectionIndex);

	UFUNCTION(BlueprintCallable, Category="Components|PMU Mesh")
	void GetAllNonEmptySectionIndices(TArray<int32>& SectionIndices);

	/** Add simple collision convex to this component */
	UFUNCTION(BlueprintCallable, Category = "Components|PMU Mesh")
	void AddCollisionConvexMesh(TArray<FVector> ConvexVerts);

	/** Add simple collision convex to this component */
	UFUNCTION(BlueprintCallable, Category = "Components|PMU Mesh")
	void ClearCollisionConvexMeshes();

private:

	/** Array of sections of mesh */
	UPROPERTY()
	TArray<FPMUMeshSection> Sections;

	/** Mesh sections groups */
	TMap<int32, FSectionIdGroup> SectionGroupMap;

	/** Local space bounds of mesh */
	UPROPERTY()
	FBoxSphereBounds LocalBounds;

	/** Convex shapes used for simple collision */
	UPROPERTY()
	TArray<FKConvexElem> CollisionConvexElems;
	
	/** Queue for async body setups that are being cooked */
	UPROPERTY(transient)
	TArray<UBodySetup*> AsyncBodySetupQueue;

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ End USceneComponent Interface.

	/** Update LocalBounds member from the local box of each section */
	void UpdateLocalBounds();

	/** Helper to create new body setup objects */
	UBodySetup* CreateBodySetupHelper();

	/** Ensure MeshBodySetup is allocated and configured */
	void CreateMeshBodySetup();

	/** Mark collision data as dirty, and re-create on instance if necessary */
	void UpdateCollision();

	/** Once async physics cook is done, create needed state */
	void FinishPhysicsAsyncCook(bool bSuccess, UBodySetup* FinishedBodySetup);
};
