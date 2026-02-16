#ifndef __SCENE_INSPECTOR_H__
#define __SCENE_INSPECTOR_H__

#include "Editor.h"

class hcSceneInspector : public hcEditor {
public:
					hcSceneInspector( void );
	virtual			~hcSceneInspector( void );

	virtual void		Init( const idDict* spawnArgs = nullptr ) override;
	virtual void		Shutdown( void ) override;
	virtual void		Draw( void ) override;
	virtual bool		IsVisible( void ) const override;
	virtual void		SetVisible( bool visible ) override;
	virtual const char*	GetName( void ) const override { return "Scene Inspector"; }

private:
	bool			visible;
	char			filterBuffer[256];
	int				selectedIndex;
	bool			needsRefresh;
	int				lastEntityCount;

	struct EntityInfo {
		idStr		name;
		idStr		classname;
		idVec3		origin;
	};
	idList<EntityInfo>	entityList;

	void			RefreshEntityList( void );
	void			TeleportToEntity( const EntityInfo& info );
	void			DrawEntityList( void );
};

extern hcSceneInspector*	sceneInspector;

#endif /* !__SCENE_INSPECTOR_H__ */
