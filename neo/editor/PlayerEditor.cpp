#ifndef IMGUI_DISABLE

#include "sys/platform.h"

#include "framework/Common.h"
#include "framework/CVarSystem.h"
#include "PlayerEditor.h"

#include "sys/sys_imgui.h"

hcPlayerEditor* playerEditor = nullptr;

hcPlayerEditor::hcPlayerEditor( void ) {
	initialized = false;
	visible = false;
	showMovementSection = true;
	showBoundsSection = true;
	showStaminaSection = true;
	showBobSection = false;
	showThirdPersonSection = false;
	showWeaponSection = true;
	propertyTableWidth = 140.0f;
}

hcPlayerEditor::~hcPlayerEditor( void ) {
	Shutdown();
}

void hcPlayerEditor::Init( const idDict* spawnArgs ) {
	initialized = true;
	visible = true;
}

void hcPlayerEditor::Shutdown( void ) {
	visible = false;
}

bool hcPlayerEditor::IsVisible( void ) const {
	return visible;
}

void hcPlayerEditor::SetVisible( bool vis ) {
	if ( vis && !visible ) {
		Init( nullptr );
	} else if ( !vis && visible ) {
		Shutdown();
	}

	visible = vis;
}

void hcPlayerEditor::DrawMovementSection( void ) {
	if ( !ImGui::CollapsingHeader( "Movement", showMovementSection ? ImGuiTreeNodeFlags_DefaultOpen : 0 ) ) {
		return;
	}
	showMovementSection = true;

	ImGui::PropertyTable( "movement_props", propertyTableWidth, [&]() {
		ImGui::LabeledWidget( "Walk Speed", [&]() {
			float value = cvarSystem->GetCVarFloat( "pm_walkspeed" );
			if ( ImGui::SliderFloatWidget( "##pm_walkspeed", &value, 50.0f, 300.0f, "Speed the player can move while walking" ) ) {
				cvarSystem->SetCVarFloat( "pm_walkspeed", value );
			}
		});

		ImGui::LabeledWidget( "Run Speed", [&]() {
			float value = cvarSystem->GetCVarFloat( "pm_runspeed" );
			if ( ImGui::SliderFloatWidget( "##pm_runspeed", &value, 100.0f, 500.0f, "Speed the player can move while running" ) ) {
				cvarSystem->SetCVarFloat( "pm_runspeed", value );
			}
		});

		ImGui::LabeledWidget( "Crouch Speed", [&]() {
			float value = cvarSystem->GetCVarFloat( "pm_crouchspeed" );
			if ( ImGui::SliderFloatWidget( "##pm_crouchspeed", &value, 20.0f, 200.0f, "Speed the player can move while crouched" ) ) {
				cvarSystem->SetCVarFloat( "pm_crouchspeed", value );
			}
		});

		ImGui::LabeledWidget( "Noclip Speed", [&]() {
			float value = cvarSystem->GetCVarFloat( "pm_noclipspeed" );
			if ( ImGui::SliderFloatWidget( "##pm_noclipspeed", &value, 50.0f, 1000.0f, "Speed the player can move while in noclip mode" ) ) {
				cvarSystem->SetCVarFloat( "pm_noclipspeed", value );
			}
		});

		ImGui::LabeledWidget( "Spectate Speed", [&]() {
			float value = cvarSystem->GetCVarFloat( "pm_spectatespeed" );
			if ( ImGui::SliderFloatWidget( "##pm_spectatespeed", &value, 100.0f, 1000.0f, "Speed the player can move while spectating" ) ) {
				cvarSystem->SetCVarFloat( "pm_spectatespeed", value );
			}
		});

		ImGui::LabeledWidget( "Jump Height", [&]() {
			float value = cvarSystem->GetCVarFloat( "pm_jumpheight" );
			if ( ImGui::SliderFloatWidget( "##pm_jumpheight", &value, 0.0f, 200.0f, "Approximate height the player can jump" ) ) {
				cvarSystem->SetCVarFloat( "pm_jumpheight", value );
			}
		});

		ImGui::LabeledWidget( "Step Size", [&]() {
			float value = cvarSystem->GetCVarFloat( "pm_stepsize" );
			if ( ImGui::SliderFloatWidget( "##pm_stepsize", &value, 0.0f, 32.0f, "Maximum height the player can step up without jumping" ) ) {
				cvarSystem->SetCVarFloat( "pm_stepsize", value );
			}
		});

		ImGui::LabeledWidget( "Min View Pitch", [&]() {
			float value = cvarSystem->GetCVarFloat( "pm_minviewpitch" );
			if ( ImGui::SliderFloatWidget( "##pm_minviewpitch", &value, -89.0f, 0.0f, "Amount player's view can look up (negative values are up)" ) ) {
				cvarSystem->SetCVarFloat( "pm_minviewpitch", value );
			}
		});

		ImGui::LabeledWidget( "Max View Pitch", [&]() {
			float value = cvarSystem->GetCVarFloat( "pm_maxviewpitch" );
			if ( ImGui::SliderFloatWidget( "##pm_maxviewpitch", &value, 0.0f, 89.0f, "Amount player's view can look down" ) ) {
				cvarSystem->SetCVarFloat( "pm_maxviewpitch", value );
			}
		});

		ImGui::LabeledWidget( "Air Time (ms)", [&]() {
			int value = cvarSystem->GetCVarInteger( "pm_air" );
			if ( ImGui::SliderIntWidget( "##pm_air", &value, 0, 10000, "How long in milliseconds the player can go without air before taking damage" ) ) {
				cvarSystem->SetCVarInteger( "pm_air", value );
			}
		});
	});
}

void hcPlayerEditor::DrawBoundsSection( void ) {
	if ( !ImGui::CollapsingHeader( "Bounds & Heights", showBoundsSection ? ImGuiTreeNodeFlags_DefaultOpen : 0 ) ) {
		return;
	}
	showBoundsSection = true;

	ImGui::PropertyTable( "bounds_props", propertyTableWidth, [&]() {
		ImGui::LabeledWidget( "Bbox Width", [&]() {
			float value = cvarSystem->GetCVarFloat( "pm_bboxwidth" );
			if ( ImGui::SliderFloatWidget( "##pm_bboxwidth", &value, 16.0f, 64.0f, "X/Y size of player's bounding box" ) ) {
				cvarSystem->SetCVarFloat( "pm_bboxwidth", value );
			}
		});

		ImGui::LabeledWidget( "Spectate Bbox", [&]() {
			float value = cvarSystem->GetCVarFloat( "pm_spectatebbox" );
			if ( ImGui::SliderFloatWidget( "##pm_spectatebbox", &value, 8.0f, 64.0f, "Size of the spectator bounding box" ) ) {
				cvarSystem->SetCVarFloat( "pm_spectatebbox", value );
			}
		});

		ImGui::TextDisabled( "Standing" );

		ImGui::LabeledWidget( "Normal Height", [&]() {
			float value = cvarSystem->GetCVarFloat( "pm_normalheight" );
			if ( ImGui::SliderFloatWidget( "##pm_normalheight", &value, 32.0f, 128.0f, "Height of player's bounding box while standing" ) ) {
				cvarSystem->SetCVarFloat( "pm_normalheight", value );
			}
		});

		ImGui::LabeledWidget( "Normal View Height", [&]() {
			float value = cvarSystem->GetCVarFloat( "pm_normalviewheight" );
			if ( ImGui::SliderFloatWidget( "##pm_normalviewheight", &value, 32.0f, 128.0f, "Height of player's view while standing" ) ) {
				cvarSystem->SetCVarFloat( "pm_normalviewheight", value );
			}
		});

		ImGui::TextDisabled( "Crouching" );

		ImGui::LabeledWidget( "Crouch Height", [&]() {
			float value = cvarSystem->GetCVarFloat( "pm_crouchheight" );
			if ( ImGui::SliderFloatWidget( "##pm_crouchheight", &value, 16.0f, 64.0f, "Height of player's bounding box while crouched" ) ) {
				cvarSystem->SetCVarFloat( "pm_crouchheight", value );
			}
		});

		ImGui::LabeledWidget( "Crouch View Height", [&]() {
			float value = cvarSystem->GetCVarFloat( "pm_crouchviewheight" );
			if ( ImGui::SliderFloatWidget( "##pm_crouchviewheight", &value, 16.0f, 64.0f, "Height of player's view while crouched" ) ) {
				cvarSystem->SetCVarFloat( "pm_crouchviewheight", value );
			}
		});

		ImGui::LabeledWidget( "Crouch Rate", [&]() {
			float value = cvarSystem->GetCVarFloat( "pm_crouchrate" );
			if ( ImGui::SliderFloatWidget( "##pm_crouchrate", &value, 0.1f, 2.0f, "Time it takes for player's view to change from standing to crouching" ) ) {
				cvarSystem->SetCVarFloat( "pm_crouchrate", value );
			}
		});

		ImGui::TextDisabled( "Dead" );

		ImGui::LabeledWidget( "Dead Height", [&]() {
			float value = cvarSystem->GetCVarFloat( "pm_deadheight" );
			if ( ImGui::SliderFloatWidget( "##pm_deadheight", &value, 4.0f, 32.0f, "Height of player's bounding box while dead" ) ) {
				cvarSystem->SetCVarFloat( "pm_deadheight", value );
			}
		});

		ImGui::LabeledWidget( "Dead View Height", [&]() {
			float value = cvarSystem->GetCVarFloat( "pm_deadviewheight" );
			if ( ImGui::SliderFloatWidget( "##pm_deadviewheight", &value, 4.0f, 32.0f, "Height of player's view while dead" ) ) {
				cvarSystem->SetCVarFloat( "pm_deadviewheight", value );
			}
		});

		ImGui::LabeledWidget( "Use Cylinder", [&]() {
			bool value = cvarSystem->GetCVarBool( "pm_usecylinder" );
			if ( ImGui::CheckboxWidget( "##pm_usecylinder", &value, "Use a cylinder approximation instead of a bounding box for player collision detection" ) ) {
				cvarSystem->SetCVarBool( "pm_usecylinder", value );
			}
		});
	});
}

void hcPlayerEditor::DrawStaminaSection( void ) {
	if ( !ImGui::CollapsingHeader( "Stamina", showStaminaSection ? ImGuiTreeNodeFlags_DefaultOpen : 0 ) ) {
		return;
	}
	showStaminaSection = true;

	ImGui::PropertyTable( "stamina_props", propertyTableWidth, [&]() {
		ImGui::LabeledWidget( "Stamina", [&]() {
			float value = cvarSystem->GetCVarFloat( "pm_stamina" );
			if ( ImGui::SliderFloatWidget( "##pm_stamina", &value, 0.0f, 100.0f, "Length of time player can run" ) ) {
				cvarSystem->SetCVarFloat( "pm_stamina", value );
			}
		});

		ImGui::LabeledWidget( "Threshold", [&]() {
			float value = cvarSystem->GetCVarFloat( "pm_staminathreshold" );
			if ( ImGui::SliderFloatWidget( "##pm_staminathreshold", &value, 0.0f, 100.0f, "When stamina drops below this value, player gradually slows to a walk" ) ) {
				cvarSystem->SetCVarFloat( "pm_staminathreshold", value );
			}
		});

		ImGui::LabeledWidget( "Regen Rate", [&]() {
			float value = cvarSystem->GetCVarFloat( "pm_staminarate" );
			if ( ImGui::SliderFloatWidget( "##pm_staminarate", &value, 0.0f, 5.0f, "Rate that player regains stamina" ) ) {
				cvarSystem->SetCVarFloat( "pm_staminarate", value );
			}
		});
	});
}

void hcPlayerEditor::DrawBobSection( void ) {
	if ( !ImGui::CollapsingHeader( "View Bob", showBobSection ? ImGuiTreeNodeFlags_DefaultOpen : 0 ) ) {
		return;
	}
	showBobSection = true;

	ImGui::PropertyTable( "bob_props", propertyTableWidth, [&]() {
		ImGui::TextDisabled( "Bob Intensity" );

		ImGui::LabeledWidget( "Walk Bob", [&]() {
			float value = cvarSystem->GetCVarFloat( "pm_walkbob" );
			if ( ImGui::SliderFloatWidget( "##pm_walkbob", &value, 0.0f, 2.0f, "Bob intensity when walking" ) ) {
				cvarSystem->SetCVarFloat( "pm_walkbob", value );
			}
		});

		ImGui::LabeledWidget( "Run Bob", [&]() {
			float value = cvarSystem->GetCVarFloat( "pm_runbob" );
			if ( ImGui::SliderFloatWidget( "##pm_runbob", &value, 0.0f, 2.0f, "Bob intensity when running" ) ) {
				cvarSystem->SetCVarFloat( "pm_runbob", value );
			}
		});

		ImGui::LabeledWidget( "Crouch Bob", [&]() {
			float value = cvarSystem->GetCVarFloat( "pm_crouchbob" );
			if ( ImGui::SliderFloatWidget( "##pm_crouchbob", &value, 0.0f, 2.0f, "Bob intensity when crouched" ) ) {
				cvarSystem->SetCVarFloat( "pm_crouchbob", value );
			}
		});

		ImGui::TextDisabled( "Bob Parameters" );

		ImGui::LabeledWidget( "Bob Up", [&]() {
			float value = cvarSystem->GetCVarFloat( "pm_bobup" );
			if ( ImGui::SliderFloatWidget( "##pm_bobup", &value, 0.0f, 0.05f, "Vertical bob amplitude" ) ) {
				cvarSystem->SetCVarFloat( "pm_bobup", value );
			}
		});

		ImGui::LabeledWidget( "Bob Pitch", [&]() {
			float value = cvarSystem->GetCVarFloat( "pm_bobpitch" );
			if ( ImGui::SliderFloatWidget( "##pm_bobpitch", &value, 0.0f, 0.02f, "View pitch bob intensity" ) ) {
				cvarSystem->SetCVarFloat( "pm_bobpitch", value );
			}
		});

		ImGui::LabeledWidget( "Bob Roll", [&]() {
			float value = cvarSystem->GetCVarFloat( "pm_bobroll" );
			if ( ImGui::SliderFloatWidget( "##pm_bobroll", &value, 0.0f, 0.02f, "View roll bob intensity" ) ) {
				cvarSystem->SetCVarFloat( "pm_bobroll", value );
			}
		});

		ImGui::TextDisabled( "Run Parameters" );

		ImGui::LabeledWidget( "Run Pitch", [&]() {
			float value = cvarSystem->GetCVarFloat( "pm_runpitch" );
			if ( ImGui::SliderFloatWidget( "##pm_runpitch", &value, 0.0f, 0.02f, "View pitch change when running" ) ) {
				cvarSystem->SetCVarFloat( "pm_runpitch", value );
			}
		});

		ImGui::LabeledWidget( "Run Roll", [&]() {
			float value = cvarSystem->GetCVarFloat( "pm_runroll" );
			if ( ImGui::SliderFloatWidget( "##pm_runroll", &value, 0.0f, 0.02f, "View roll change when running" ) ) {
				cvarSystem->SetCVarFloat( "pm_runroll", value );
			}
		});
	});
}

void hcPlayerEditor::DrawThirdPersonSection( void ) {
	if ( !ImGui::CollapsingHeader( "Third Person", showThirdPersonSection ? ImGuiTreeNodeFlags_DefaultOpen : 0 ) ) {
		return;
	}
	showThirdPersonSection = true;

	ImGui::PropertyTable( "thirdperson_props", propertyTableWidth, [&]() {
		ImGui::LabeledWidget( "Enable", [&]() {
			bool value = cvarSystem->GetCVarBool( "pm_thirdPerson" );
			if ( ImGui::CheckboxWidget( "##pm_thirdPerson", &value, "Enables third person view" ) ) {
				cvarSystem->SetCVarBool( "pm_thirdPerson", value );
			}
		});

		ImGui::LabeledWidget( "On Death", [&]() {
			bool value = cvarSystem->GetCVarBool( "pm_thirdPersonDeath" );
			if ( ImGui::CheckboxWidget( "##pm_thirdPersonDeath", &value, "Enables third person view when player dies" ) ) {
				cvarSystem->SetCVarBool( "pm_thirdPersonDeath", value );
			}
		});

		ImGui::LabeledWidget( "Clip to World", [&]() {
			bool value = cvarSystem->GetCVarBool( "pm_thirdPersonClip" );
			if ( ImGui::CheckboxWidget( "##pm_thirdPersonClip", &value, "Clip third person view into world space" ) ) {
				cvarSystem->SetCVarBool( "pm_thirdPersonClip", value );
			}
		});

		ImGui::LabeledWidget( "Range", [&]() {
			float value = cvarSystem->GetCVarFloat( "pm_thirdPersonRange" );
			if ( ImGui::SliderFloatWidget( "##pm_thirdPersonRange", &value, 0.0f, 300.0f, "Camera distance from player in 3rd person" ) ) {
				cvarSystem->SetCVarFloat( "pm_thirdPersonRange", value );
			}
		});

		ImGui::LabeledWidget( "Height", [&]() {
			float value = cvarSystem->GetCVarFloat( "pm_thirdPersonHeight" );
			if ( ImGui::SliderFloatWidget( "##pm_thirdPersonHeight", &value, -50.0f, 100.0f, "Height of camera from normal view height in 3rd person" ) ) {
				cvarSystem->SetCVarFloat( "pm_thirdPersonHeight", value );
			}
		});

		ImGui::LabeledWidget( "Angle", [&]() {
			float value = cvarSystem->GetCVarFloat( "pm_thirdPersonAngle" );
			if ( ImGui::SliderFloatWidget( "##pm_thirdPersonAngle", &value, 0.0f, 360.0f, "Direction of camera from player (0 = behind, 180 = in front)" ) ) {
				cvarSystem->SetCVarFloat( "pm_thirdPersonAngle", value );
			}
		});

		ImGui::LabeledWidget( "Model View", [&]() {
			int value = cvarSystem->GetCVarInteger( "pm_modelView" );
			if ( ImGui::SliderIntWidget( "##pm_modelView", &value, 0, 2, "Camera from player model POV (0 = off, 1 = always, 2 = when dead)" ) ) {
				cvarSystem->SetCVarInteger( "pm_modelView", value );
			}
		});
	});
}

void hcPlayerEditor::DrawWeaponSection( void ) {
	if ( !ImGui::CollapsingHeader( "Weapon View Model", showWeaponSection ? ImGuiTreeNodeFlags_DefaultOpen : 0 ) ) {
		return;
	}
	showWeaponSection = true;

	ImGui::PropertyTable( "weapon_props", propertyTableWidth, [&]() {
		ImGui::LabeledWidget( "Gun X", [&]() {
			float value = cvarSystem->GetCVarFloat( "g_gunX" );
			if ( ImGui::SliderFloatWidget( "##g_gunX", &value, -20.0f, 20.0f, "Weapon view model X offset (left/right)" ) ) {
				cvarSystem->SetCVarFloat( "g_gunX", value );
			}
		});

		ImGui::LabeledWidget( "Gun Y", [&]() {
			float value = cvarSystem->GetCVarFloat( "g_gunY" );
			if ( ImGui::SliderFloatWidget( "##g_gunY", &value, -20.0f, 20.0f, "Weapon view model Y offset (forward/back)" ) ) {
				cvarSystem->SetCVarFloat( "g_gunY", value );
			}
		});

		ImGui::LabeledWidget( "Gun Z", [&]() {
			float value = cvarSystem->GetCVarFloat( "g_gunZ" );
			if ( ImGui::SliderFloatWidget( "##g_gunZ", &value, -20.0f, 20.0f, "Weapon view model Z offset (up/down)" ) ) {
				cvarSystem->SetCVarFloat( "g_gunZ", value );
			}
		});

		ImGui::LabeledWidget( "Reset All", [&]() {
			if ( ImGui::Button( "Reset##weapon_reset" ) ) {
				cvarSystem->SetCVarFloat( "g_gunX", 0.0f );
				cvarSystem->SetCVarFloat( "g_gunY", 0.0f );
				cvarSystem->SetCVarFloat( "g_gunZ", 0.0f );
			}
		});
	});
}

void hcPlayerEditor::Draw( void ) {
	if ( !visible ) {
		return;
	}

	int windowFlags = D3::ImGuiHooks::GetOpenWindowsMask();
	if ( !( windowFlags & D3::ImGuiHooks::D3_ImGuiWin_PlayerEditor ) ) {
		visible = false;
		return;
	}

	ImGui::SetNextWindowSize( ImVec2( 400, 600 ), ImGuiCond_FirstUseEver );

	bool open = true;
	if ( ImGui::Begin( "Player Editor", &open, ImGuiWindowFlags_None ) ) {
		DrawMovementSection();
		DrawBoundsSection();
		DrawStaminaSection();
		DrawBobSection();
		DrawThirdPersonSection();
		DrawWeaponSection();
	}
	ImGui::End();

	if ( !open ) {
		D3::ImGuiHooks::CloseWindow( D3::ImGuiHooks::D3_ImGuiWin_PlayerEditor );
	}
}

/*
================
Com_OpenCloseImGuiPlayerEditor

Open/Close handler
================
*/
void Com_OpenCloseImGuiPlayerEditor( bool open ) {
	if ( !playerEditor ) {
		playerEditor = new hcPlayerEditor();
	}

	if ( open ) {
		playerEditor->Init( nullptr );
	} else {
		playerEditor->Shutdown();
	}
}

/*
================
Com_ImGuiPlayerEditor_f

Console command handler
================
*/
void Com_ImGuiPlayerEditor_f( const idCmdArgs& args ) {
	bool menuOpen = (D3::ImGuiHooks::GetOpenWindowsMask() & D3::ImGuiHooks::D3_ImGuiWin_PlayerEditor) != 0;
	if ( !menuOpen ) {
		D3::ImGuiHooks::OpenWindow( D3::ImGuiHooks::D3_ImGuiWin_PlayerEditor );
	} else {
		if ( ImGui::IsWindowFocused( ImGuiFocusedFlags_AnyWindow ) ) {
			D3::ImGuiHooks::CloseWindow( D3::ImGuiHooks::D3_ImGuiWin_PlayerEditor );
		} else {
			ImGui::SetNextWindowFocus();
		}
	}
}

#else // IMGUI_DISABLE - stub implementation

#include "framework/Common.h"
#include "PlayerEditor.h"

hcPlayerEditor* playerEditor = nullptr;

hcPlayerEditor::hcPlayerEditor( void ) {}
hcPlayerEditor::~hcPlayerEditor( void ) {}
void hcPlayerEditor::Init( const idDict* spawnArgs ) {}
void hcPlayerEditor::Shutdown( void ) {}
void hcPlayerEditor::Draw( void ) {}
bool hcPlayerEditor::IsVisible( void ) const { return false; }
void hcPlayerEditor::SetVisible( bool visible ) {}
void hcPlayerEditor::DrawMovementSection( void ) {}
void hcPlayerEditor::DrawBoundsSection( void ) {}
void hcPlayerEditor::DrawStaminaSection( void ) {}
void hcPlayerEditor::DrawBobSection( void ) {}
void hcPlayerEditor::DrawThirdPersonSection( void ) {}
void hcPlayerEditor::DrawWeaponSection( void ) {}

void Com_OpenCloseImGuiPlayerEditor( bool open ) {}

void Com_ImGuiPlayerEditor_f( const idCmdArgs& args ) {
	common->Warning( "This editor requires imgui to be enabled" );
}

#endif // IMGUI_DISABLE
