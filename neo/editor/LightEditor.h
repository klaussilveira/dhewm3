#ifndef __LIGHT_EDITOR_H__
#define __LIGHT_EDITOR_H__

#include "Editor.h"

// Forward declarations
class idEntity;

class hcLightEditor : public hcEditor {
public:
							hcLightEditor( void );
	virtual					~hcLightEditor( void );

	virtual void			Init( const idDict* spawnArgs = nullptr ) override;
	virtual void			Shutdown( void ) override;
	virtual void			Draw( void ) override;
	virtual bool			IsVisible( void ) const override;
	virtual void			SetVisible( bool visible ) override;
	virtual const char*		GetName( void ) const override { return "Light Editor"; }

private:
	// Editor state
	bool					initialized;
	bool					visible;
	float					propertyTableWidth;

	// Texture browser
	idList<idStr>			textureNames;
	int						currentTextureIndex;
	idStr					lightTexture;
	bool					showTextureBrowser;
	char					textureFilter[256];
	idList<int>				filteredTextureIndices;
	char					lastTextureFilter[256];
	int						textureBrowserTab;
	float					textureThumbnailSize;
	int						textureBrowserScrollToIdx;

	// Entity state
	idEntity*				selectedLight;
	int						gizmoOperation;
	int						gizmoMode;
	idVec3					lightOrigin;
	idMat3					lightAxis;
	idVec3					lightColor;
	idVec3					lightRadius;
	bool					isPointLight;
	bool					isParallel;
	bool					noShadows;
	bool					noSpecular;
	bool					noDiffuse;

	int						falloffMode;
	float					falloff;

	bool					hasCenter;
	idVec3					lightCenter;

	idVec3					lightTarget;
	idVec3					lightRight;
	idVec3					lightUp;
	idVec3					lightStart;
	idVec3					lightEnd;
	bool					explicitStartEnd;

	void					LoadLightTextures( void );
	int						FindTextureIndex( const char* textureName );
	void					RebuildFilteredTextureList( void );
	void					OpenTextureBrowser( void );
	void					RefreshLightData( void );
	void					ApplyLightChanges( void );
	void					CheckSelectedLight( void );
	void					DrawGizmo( void );
	void					DrawGizmoControls( void );
	void					DrawLightTypeSection( void );
	void					DrawTransformSection( void );
	void					DrawColorSection( void );
	void					DrawPointLightSection( void );
	void					DrawProjectedLightSection( void );
	void					DrawOptionsSection( void );
	void					DrawInfoSection( void );
	void					DrawTextureBrowser( void );
	void					DrawTextureBrowserListTab( void );
	void					DrawTextureBrowserVisualTab( void );
};

extern hcLightEditor*		lightEditor;

#endif /* !__LIGHT_EDITOR_H__ */
