#ifndef __PARTICLE_EDITOR_H__
#define __PARTICLE_EDITOR_H__

#include "Editor.h"
#include "framework/DeclParticle.h"

#ifndef IMGUI_DISABLE
#include "../libs/imgui/ImSequencer.h"
#endif

// Forward declarations
class idEntity;
class idDeclParticle;
class idParticleStage;

class hcParticleSequence : public ImSequencer::SequenceInterface {
public:
							hcParticleSequence( void );

	void					SetParticle( idDeclParticle* p ) { particle = p; }
	idDeclParticle*			GetParticle( void ) const { return particle; }

	virtual int				GetFrameMin( void ) const override;
	virtual int				GetFrameMax( void ) const override;
	virtual int				GetItemCount( void ) const override;
	virtual int				GetItemTypeCount( void ) const override { return 1; }
	virtual const char*		GetItemTypeName( int typeIndex ) const override { return "Stage"; }
	virtual const char*		GetItemLabel( int index ) const override;
	virtual void			Get( int index, int** start, int** end, int* type, unsigned int* color ) override;
	virtual void			BeginEdit( int index ) override;
	virtual void			EndEdit( void ) override;
	virtual void			DoubleClick( int index ) override;

private:
	void					UpdateStageTiming( int index, int startFrame, int endFrame );

	idDeclParticle*			particle;
	int						editingStage;
	int						cachedStart[64];
	int						cachedEnd[64];
};

class hcParticleEditor : public hcEditor {
public:
							hcParticleEditor( void );
	virtual					~hcParticleEditor( void );

	virtual void			Init( const idDict* spawnArgs = nullptr ) override;
	virtual void			Shutdown( void ) override;
	virtual void			Draw( void ) override;
	virtual bool			IsVisible( void ) const override;
	virtual void			SetVisible( bool visible ) override;
	virtual const char*		GetName( void ) const override { return "Particle Editor"; }

private:
	// Editor state
	bool					initialized;
	bool					visible;
	bool					showNewParticleDialog;
	bool					showSaveAsDialog;
	char					newParticleName[256];
	char					newParticleFile[256];
	bool					copyCurrentParticle;
	char					saveAsName[256];
	char					saveAsFile[256];
	bool					showDropDialog;
	char					dropEntityName[256];
	float					propertyTableWidth;
	idEntity*				lastSelectedEntity;

	// Entity state
	int						currentParticleIndex;
	int						currentStageIndex;
	idList<idStr>			particleNames;
	idList<int>				particleDeclIndices;

	enum VisualizationMode {
		VIS_TESTMODEL = 0,
		VIS_IMPACT,
		VIS_MUZZLE,
		VIS_FLIGHT,
		VIS_SELECTED
	};

	int						visualizationMode;

	// Timeline state
	hcParticleSequence		particleSequence;
	int						timelineCurrentFrame;
	bool					timelineExpanded;
	int						timelineSelectedEntry;
	int						timelineFirstFrame;

	// Material browser
	bool					showMaterialBrowser;
	char					materialFilter[256];
	idList<idStr>			materialNames;
	int						materialBrowserScrollToIdx;
	int						materialBrowserTab;
	float					materialThumbnailSize;
	idList<int>				filteredMaterialIndices;
	char					lastMaterialFilter[256];

	// Gizmo
	bool					mapModified;
	bool					showGizmo;
	int						gizmoOperation;
	int						gizmoMode;

	void					EnumParticles( void );
	void					EnumMaterials( void );
	void					RebuildFilteredMaterialList( void );
	idDeclParticle*			GetCurrentParticle( void );
	idParticleStage*		GetCurrentStage( void );
	bool					SelectParticleByName( const char* name );
	void					CheckSelectedParticleEntity( void );
	void					SetParticleView( void );
	void					MoveSelectedEntities( float x, float y, float z );
	void					OpenNewParticleDialog( void );
	void					OpenSaveAsDialog( void );
	void					OpenDropDialog( void );
	void					OpenMaterialBrowser( void );
	void					DrawParticleGizmo( void );
	void					DrawEntityControls( void );
	void					DrawParticleSelector( void );
	void					DrawVisualizationControls( void );
	void					DrawTimeline( void );
	void					DrawStageList( void );
	void					DrawStageProperties( void );
	void					DrawSaveControls( void );
	void					DrawNewParticleDialog( void );
	void					DrawSaveAsDialog( void );
	void					DrawDropDialog( void );
	void					DrawMaterialBrowser( void );
	void					DrawMaterialBrowserListTab( idParticleStage* stage );
	void					DrawMaterialBrowserVisualTab( idParticleStage* stage );
};

extern hcParticleEditor*	particleEditor;

#endif /* !__PARTICLE_EDITOR_H__ */
