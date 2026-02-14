#ifndef __ENTITY_EDITOR_H__
#define __ENTITY_EDITOR_H__

#include "Editor.h"

class idEntity;

class hcEntityEditor : public hcEditor {
public:
						hcEntityEditor( void );
	virtual				~hcEntityEditor( void );

	virtual void		Init( const idDict* spawnArgs = nullptr ) override;
	virtual void		Shutdown( void ) override;
	virtual void		Draw( void ) override;
	virtual bool		IsVisible( void ) const override;
	virtual void		SetVisible( bool visible ) override;
	virtual const char*	GetName( void ) const override { return "Entity Editor"; }

private:
	// Editor state
	bool				visible;
	bool				showSpawnArgsSection;
	bool				showTransformSection;
	float				propertyTableWidth;
	char				spawnArgFilter[128];

	// Entity state
	idEntity*			selectedEntity;
	idVec3				entityOrigin;
	idMat3				entityAxis;
	int					gizmoOperation;
	int					gizmoMode;

	void				CheckSelectedEntity( void );
	void				RefreshEntityData( void );
	void				ApplyEntityChanges( void );
	void				DrawEntityInfo( void );
	void				DrawGizmoControls( void );
	void				DrawGizmo( void );
	void				DrawTransformSection( void );
	void				DrawSpawnArgsSection( void );
	void				DrawActionsSection( void );
};

extern hcEntityEditor*	entityEditor;

#endif /* !__ENTITY_EDITOR_H__ */
