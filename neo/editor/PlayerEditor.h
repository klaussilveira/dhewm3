#ifndef __PLAYER_EDITOR_H__
#define __PLAYER_EDITOR_H__

#include "Editor.h"

class hcPlayerEditor : public hcEditor {
public:
						hcPlayerEditor( void );
	virtual				~hcPlayerEditor( void );

	virtual void		Init( const idDict* spawnArgs = nullptr ) override;
	virtual void		Shutdown( void ) override;
	virtual void		Draw( void ) override;
	virtual bool		IsVisible( void ) const override;
	virtual void		SetVisible( bool visible ) override;
	virtual const char*	GetName( void ) const override { return "Player Editor"; }

private:
	bool				initialized;
	bool				visible;
	bool				showMovementSection;
	bool				showBoundsSection;
	bool				showStaminaSection;
	bool				showBobSection;
	bool				showThirdPersonSection;
	bool				showWeaponSection;
	float				propertyTableWidth;

	void				DrawMovementSection( void );
	void				DrawBoundsSection( void );
	void				DrawStaminaSection( void );
	void				DrawBobSection( void );
	void				DrawThirdPersonSection( void );
	void				DrawWeaponSection( void );
};

extern hcPlayerEditor*	playerEditor;

#endif /* !__PLAYER_EDITOR_H__ */
