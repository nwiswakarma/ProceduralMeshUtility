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

public:

	/** 
	 *	Controls whether the single material is used for all mesh sections.
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
	*	Controls whether the physics cooking should be done off the game thread. This should be used when collision geometry doesn't have to be immediately up to date (For example streaming in far away objects)
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Procedural Mesh")
	bool bUseAsyncCooking;

	/** Collision data */
	UPROPERTY(Instanced)
	UBodySetup* MeshBodySetup;

	/**
	 *	Create/replace a section for this procedural mesh component.
	 *	This function is deprecated for Blueprints because it uses the unsupported 'Color' type. Use new 'Create Mesh Section' function which uses LinearColor instead.
	 *	@param	SectionIndex		Index of the section to create or replace.
	 *	@param	Vertices			Vertex buffer of all vertex positions to use for this mesh section.
	 *	@param	Triangles			Index buffer indicating which vertices make up each triangle. Length must be a multiple of 3.
	 *	@param	Normals				Optional array of normal vectors for each vertex. If supplied, must be same length as Vertices array.
	 *	@param	UV0					Optional array of texture co-ordinates for each vertex. If supplied, must be same length as Vertices array.
	 *	@param	VertexColors		Optional array of colors for each vertex. If supplied, must be same length as Vertices array.
	 *	@param	Tangents			Optional array of tangent vector for each vertex. If supplied, must be same length as Vertices array.
	 *	@param	bCreateCollision	Indicates whether collision should be created for this section. This adds significant cost.
	 */
	UFUNCTION(BlueprintCallable, Category="Components|ProceduralMesh", meta=(DisplayName="Create Mesh Section FColor", AutoCreateRefTerm="Normals,UV0,VertexColors,Tangents"))
	void CreateMeshSection(
        int32 SectionIndex,
        const TArray<FVector>& Vertices,
        const TArray<int32>& Triangles,
        const TArray<FVector>& Normals,
        const TArray<FVector2D>& UV0,
        const TArray<FColor>& VertexColors,
        const TArray<FPMUMeshTangent>& Tangents,
        bool bCreateCollision
        );

	/**
	 *	Create/replace a section for this procedural mesh component.
	 *	@param	SectionIndex		Index of the section to create or replace.
	 *	@param	Vertices			Vertex buffer of all vertex positions to use for this mesh section.
	 *	@param	Triangles			Index buffer indicating which vertices make up each triangle. Length must be a multiple of 3.
	 *	@param	Normals				Optional array of normal vectors for each vertex. If supplied, must be same length as Vertices array.
	 *	@param	UV0					Optional array of texture co-ordinates for each vertex. If supplied, must be same length as Vertices array.
	 *	@param	VertexColors		Optional array of colors for each vertex. If supplied, must be same length as Vertices array.
	 *	@param	Tangents			Optional array of tangent vector for each vertex. If supplied, must be same length as Vertices array.
	 *	@param	bCreateCollision	Indicates whether collision should be created for this section. This adds significant cost.
	 */
	UFUNCTION(BlueprintCallable, Category="Components|ProceduralMesh", meta=(DisplayName="Create Mesh Section", AutoCreateRefTerm="Normals,UV0,VertexColors,Tangents"))
	void CreateMeshSection_LinearColor(
        int32 SectionIndex,
        const TArray<FVector>& Vertices,
        const TArray<int32>& Triangles,
        const TArray<FVector>& Normals,
        const TArray<FVector2D>& UV0,
        const TArray<FLinearColor>& VertexColors,
        const TArray<FPMUMeshTangent>& Tangents,
        bool bCreateCollision
        );


	/**
	 *	Updates a section of this procedural mesh component. This is faster than CreateMeshSection, but does not let you change topology.
     *	Collision info is also updated.
	 *	This function is deprecated for Blueprints because it uses the unsupported 'Color' type. Use new 'Create Mesh Section' function which uses LinearColor instead.
	 *	@param	Vertices			Vertex buffer of all vertex positions to use for this mesh section.
	 *	@param	Normals				Optional array of normal vectors for each vertex. If supplied, must be same length as Vertices array.
	 *	@param	UV0					Optional array of texture co-ordinates for each vertex. If supplied, must be same length as Vertices array.
	 *	@param	VertexColors		Optional array of colors for each vertex. If supplied, must be same length as Vertices array.
	 *	@param	Tangents			Optional array of tangent vector for each vertex. If supplied, must be same length as Vertices array.
	 */
	UFUNCTION(BlueprintCallable, Category="Components|ProceduralMesh", meta=(DisplayName="Update Mesh Section FColor", AutoCreateRefTerm="Normals,UV0,VertexColors,Tangents"))
	void UpdateMeshSection(
        int32 SectionIndex,
        const TArray<FVector>& Vertices,
        const TArray<FVector>& Normals,
        const TArray<FVector2D>& UV0,
        const TArray<FColor>& VertexColors,
        const TArray<FPMUMeshTangent>& Tangents
        );

	/**
	 *	Updates a section of this procedural mesh component. This is faster than CreateMeshSection, but does not let you change topology. Collision info is also updated.
	 *	@param	Vertices			Vertex buffer of all vertex positions to use for this mesh section.
	 *	@param	Normals				Optional array of normal vectors for each vertex. If supplied, must be same length as Vertices array.
	 *	@param	UV0					Optional array of texture co-ordinates for each vertex. If supplied, must be same length as Vertices array.
	 *	@param	VertexColors		Optional array of colors for each vertex. If supplied, must be same length as Vertices array.
	 *	@param	Tangents			Optional array of tangent vector for each vertex. If supplied, must be same length as Vertices array.
	 */
	UFUNCTION(BlueprintCallable, Category="Components|ProceduralMesh", meta=(DisplayName="Update Mesh Section", AutoCreateRefTerm="Normals,UV0,VertexColors,Tangents"))
	void UpdateMeshSection_LinearColor(
        int32 SectionIndex,
        const TArray<FVector>& Vertices,
        const TArray<FVector>& Normals,
        const TArray<FVector2D>& UV0,
        const TArray<FLinearColor>& VertexColors,
        const TArray<FPMUMeshTangent>& Tangents
        );


	/** Clear a section of the procedural mesh. Other sections do not change index. */
	UFUNCTION(BlueprintCallable, Category="Components|ProceduralMesh")
	void ClearMeshSectionGeometry(int32 SectionIndex);

	/** Clear all mesh sections and reset to empty state */
	UFUNCTION(BlueprintCallable, Category="Components|ProceduralMesh")
	void ClearAllMeshSections();

	/** Control visibility of a particular section */
	UFUNCTION(BlueprintCallable, Category="Components|ProceduralMesh")
	void SetMeshSectionVisible(int32 SectionIndex, bool bNewVisibility);

	/** Returns whether a particular section is currently visible */
	UFUNCTION(BlueprintCallable, Category="Components|ProceduralMesh")
	bool IsMeshSectionVisible(int32 SectionIndex) const;

	/** Returns number of sections currently created for this component */
	UFUNCTION(BlueprintCallable, Category="Components|ProceduralMesh")
	int32 GetNumSections() const;

	/** 
	 *	Get pointer to internal data for one section of this procedural mesh component. 
	 *	Note that pointer will becomes invalid if sections are added or removed.
	 */
	FPMUMeshSection* GetMeshSection(int32 SectionIndex);

	UFUNCTION(BlueprintCallable, Category="Components|ProceduralMesh")
	FPMUMeshSectionResourceRef GetSectionResourceRef(int32 SectionIndex);

	UFUNCTION(BlueprintCallable, Category="Components|ProceduralMesh", meta=(DisplayName="Create Mesh Section From Section Resource"))
	FPMUMeshSection CreateSectionFromSectionResource(int32 SectionIndex);

	UFUNCTION(BlueprintCallable, Category="Components|ProceduralMesh", meta=(DisplayName="Create Mesh Section From Section Resource (Raw Buffers)"))
	void CreateSectionBuffersFromSectionResource(int32 SectionIndex, TArray<FVector>& Positions, TArray<FVector>& Normals, TArray<int32>& Indices, FBox& LocalBounds);

	/** Resizes section containers to hold at least the specified count */
	UFUNCTION(BlueprintCallable, Category="Components|ProceduralMesh")
	void SetNumMeshSections(int32 SectionCount, bool bAllowShrinking = true);

	/** Resizes section resource containers to hold at least the specified count */
	UFUNCTION(BlueprintCallable, Category="Components|ProceduralMesh")
	void SetNumSectionResources(int32 SectionCount, bool bAllowShrinking = true);

	/** Assign a mesh section */
	UFUNCTION(BlueprintCallable, Category="Components|ProceduralMesh")
	void SetMeshSection(int32 SectionIndex, const FPMUMeshSection& Section, bool bUpdateRenderState = true);

	/** Assign a mesh section (Direct ) */
	UFUNCTION(BlueprintCallable, Category="Components|ProceduralMesh")
	void SetSectionResource(int32 SectionIndex, const FPMUMeshSectionResource& Section, bool bUpdateRenderState = true);

	/** Request render state update */
	UFUNCTION(BlueprintCallable, Category="Components|ProceduralMesh")
	void UpdateRenderState();

	UFUNCTION(BlueprintCallable, Category="Components|ProceduralMesh")
	void SimplifySection(int32 SectionIndex, const FPMUMeshSimplifierOptions& Options);

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

	/** Add simple collision convex to this component */
	UFUNCTION(BlueprintCallable, Category="Components|ProceduralMesh")
	void AddCollisionConvexMesh(TArray<FVector> ConvexVerts);

	/** Add simple collision convex to this component */
	UFUNCTION(BlueprintCallable, Category="Components|ProceduralMesh")
	void ClearCollisionConvexMeshes();

	/** Function to replace _all_ simple collision in one go */
	void SetCollisionConvexMeshes(const TArray< TArray<FVector> >& ConvexMeshes);

	//~ Begin Interface_CollisionDataProvider Interface
	virtual bool GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData) override;
	virtual bool ContainsPhysicsTriMeshData(bool InUseAllTriData) const override;
	virtual bool WantsNegXTriMesh() override{ return false; }
	//~ End Interface_CollisionDataProvider Interface

private:

	friend class FPMUMeshSceneProxy;

	/** Array of sections of mesh */
	UPROPERTY()
	TArray<FPMUMeshSection> MeshSections;

	/** Array of sections of mesh (section resource) */
	UPROPERTY()
	TArray<FPMUMeshSectionResource> SectionResources;

	/** Convex shapes used for simple collision */
	UPROPERTY()
	TArray<FKConvexElem> CollisionConvexElems;

	/** Local space bounds of mesh */
	UPROPERTY()
	FBoxSphereBounds LocalBounds;
	
	/** Queue for async body setups that are being cooked */
	UPROPERTY(transient)
	TArray<UBodySetup*> AsyncBodySetupQueue;

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ End USceneComponent Interface.

	/** Update LocalBounds member from the local box of each section */
	void UpdateLocalBounds();

	/** Ensure MeshBodySetup is allocated and configured */
	void CreateMeshBodySetup();

	/** Mark collision data as dirty, and re-create on instance if necessary */
	void UpdateCollision();

	/** Once async physics cook is done, create needed state */
	void FinishPhysicsAsyncCook(bool bSuccess, UBodySetup* FinishedBodySetup);

	/** Helper to create new body setup objects */
	UBodySetup* CreateBodySetupHelper();
};
