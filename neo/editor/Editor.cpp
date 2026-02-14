#ifndef IMGUI_DISABLE

#include "sys/platform.h"
#include "sys/sys_imgui.h"

#include "framework/CmdSystem.h"
#include "framework/Common.h"
#include "framework/CVarSystem.h"
#include "framework/DeclManager.h"
#include "framework/Game.h"
#include "Editor.h"
#include "EntityEditor.h"
#include "LightEditor.h"
#include "ParticleEditor.h"
#include "PlayerEditor.h"

hcEditorMenuBar*	editorMenuBar = nullptr;
static bool			editorModeActive = false;

hcEditorMenuBar::hcEditorMenuBar( void ) {
	translucent = false;
	originalAlpha = 1.0f;
	showSpawnDialog = false;
	spawnFilter[0] = '\0';
	selectedEntityDefIndex = -1;
}

void hcEditorMenuBar::SetTranslucent( bool enable ) {
	if ( translucent == enable ) {
		return;
	}

	translucent = enable;

	ImGuiStyle& style = ImGui::GetStyle();
	if ( enable ) {
		originalAlpha = style.Alpha;
		style.Alpha = 0.85f;
	} else {
		style.Alpha = originalAlpha;
	}
}

void hcEditorMenuBar::DrawCvarToggle( const char* label, const char* cvarName, const char* tooltip ) {
	bool value = cvarSystem->GetCVarBool( cvarName );
	if ( ImGui::MenuItem( label, nullptr, value ) ) {
		cvarSystem->SetCVarBool( cvarName, !value );
	}
	if ( tooltip ) {
		ImGui::AddTooltip( tooltip );
	}
}

void hcEditorMenuBar::DrawCvarIntCombo( const char* label, const char* cvarName, const char** options, int numOptions, const char* tooltip ) {
	int value = cvarSystem->GetCVarInteger( cvarName );
	if ( value < 0 ) value = 0;
	if ( value >= numOptions ) value = numOptions - 1;

	if ( ImGui::BeginMenu( label ) ) {
		for ( int i = 0; i < numOptions; i++ ) {
			if ( ImGui::MenuItem( options[i], nullptr, value == i ) ) {
				cvarSystem->SetCVarInteger( cvarName, i );
			}
		}

		ImGui::EndMenu();
	}

	if ( tooltip ) {
		ImGui::AddTooltip( tooltip );
	}
}

void hcEditorMenuBar::DrawMapMenu( void ) {
	if ( ImGui::BeginMenu( "Map" ) ) {
		if ( ImGui::MenuItem( "Save Changes", nullptr, false, gameEdit != nullptr ) ) {
			if ( gameEdit != nullptr ) {
				gameEdit->MapSave();
				common->Printf( "Map saved.\n" );
			}
		}
		ImGui::AddTooltip( "Save all pending changes to the .map file" );

		ImGui::Separator();

		bool canSpawn = ( gameEdit != nullptr && gameEdit->PlayerIsValid() );
		if ( ImGui::MenuItem( "Spawn Entity...", nullptr, false, canSpawn ) ) {
			OpenSpawnDialog();
		}
		ImGui::AddTooltip( "Spawn a new entity at the player's location" );

		ImGui::EndMenu();
	}
}

void hcEditorMenuBar::DrawWindowMenu( void ) {
	if ( ImGui::BeginMenu( "Window" ) ) {
		int mask = D3::ImGuiHooks::GetOpenWindowsMask();

		bool lightEditorOpen = (mask & D3::ImGuiHooks::D3_ImGuiWin_LightEditor) != 0;
		if ( ImGui::MenuItem( "Light Editor", "editLights", lightEditorOpen ) ) {
			if ( lightEditorOpen ) {
				D3::ImGuiHooks::CloseWindow( D3::ImGuiHooks::D3_ImGuiWin_LightEditor );
			} else {
				D3::ImGuiHooks::OpenWindow( D3::ImGuiHooks::D3_ImGuiWin_LightEditor );
			}
		}

		bool particleEditorOpen = (mask & D3::ImGuiHooks::D3_ImGuiWin_ParticleEditor) != 0;
		if ( ImGui::MenuItem( "Particle Editor", "editParticles", particleEditorOpen ) ) {
			if ( particleEditorOpen ) {
				D3::ImGuiHooks::CloseWindow( D3::ImGuiHooks::D3_ImGuiWin_ParticleEditor );
			} else {
				D3::ImGuiHooks::OpenWindow( D3::ImGuiHooks::D3_ImGuiWin_ParticleEditor );
			}
		}

		bool playerEditorOpen = (mask & D3::ImGuiHooks::D3_ImGuiWin_PlayerEditor) != 0;
		if ( ImGui::MenuItem( "Player Editor", "editPlayer", playerEditorOpen ) ) {
			if ( playerEditorOpen ) {
				D3::ImGuiHooks::CloseWindow( D3::ImGuiHooks::D3_ImGuiWin_PlayerEditor );
			} else {
				D3::ImGuiHooks::OpenWindow( D3::ImGuiHooks::D3_ImGuiWin_PlayerEditor );
			}
		}

		bool entityEditorOpen = (mask & D3::ImGuiHooks::D3_ImGuiWin_EntityEditor) != 0;
		if ( ImGui::MenuItem( "Entity Editor", "editEntities", entityEditorOpen ) ) {
			if ( entityEditorOpen ) {
				D3::ImGuiHooks::CloseWindow( D3::ImGuiHooks::D3_ImGuiWin_EntityEditor );
			} else {
				D3::ImGuiHooks::OpenWindow( D3::ImGuiHooks::D3_ImGuiWin_EntityEditor );
			}
		}

		ImGui::Separator();

		if ( ImGui::MenuItem( "Translucent Windows", nullptr, translucent ) ) {
			SetTranslucent( !translucent );
		}
		ImGui::AddTooltip( "Make editor windows semi-transparent" );

		ImGui::Separator();

		if ( ImGui::MenuItem( "Hide Editors", "toggleEditors" ) ) {
			Editor_SetModeActive( false );
		}
		ImGui::AddTooltip( "Hide all editor windows temporarily" );

		if ( ImGui::MenuItem( "Close All Editors" ) ) {
			D3::ImGuiHooks::CloseWindow( D3::ImGuiHooks::D3_ImGuiWin_LightEditor );
			D3::ImGuiHooks::CloseWindow( D3::ImGuiHooks::D3_ImGuiWin_ParticleEditor );
			D3::ImGuiHooks::CloseWindow( D3::ImGuiHooks::D3_ImGuiWin_PlayerEditor );
			D3::ImGuiHooks::CloseWindow( D3::ImGuiHooks::D3_ImGuiWin_EntityEditor );
		}

		ImGui::EndMenu();
	}
}

void hcEditorMenuBar::DrawEditModeMenu( void ) {
	if ( ImGui::BeginMenu( "Edit Mode" ) ) {
		static const char* editModes[] = {
			"0 - Off",
			"1 - Lights",
			"2 - Sounds",
			"3 - Articulated Figures",
			"4 - Particles",
			"5 - Monsters",
			"6 - Static Entities (Name)",
			"7 - Static Entities (Model)"
		};

		int editMode = cvarSystem->GetCVarInteger( "g_editEntityMode" );
		for ( int i = 0; i < 8; i++ ) {
			if ( ImGui::MenuItem( editModes[i], nullptr, editMode == i ) ) {
				cvarSystem->SetCVarInteger( "g_editEntityMode", i );
			}
		}

		ImGui::EndMenu();
	}
}

void hcEditorMenuBar::DrawDebugMenu( void ) {
	if ( ImGui::BeginMenu( "Debug" ) ) {
		if ( ImGui::BeginMenu( "Visibility" ) ) {
			DrawCvarToggle( "Show PVS", "g_showPVS", "Show Potentially Visible Set information" );
			DrawCvarToggle( "Show Portals", "r_showPortals", "Show portal rendering" );
			static const char* showTrisOptions[] = { "Off", "Visible Only", "Front Facing", "All" };
			DrawCvarIntCombo( "Show Tris", "r_showTris", showTrisOptions, 4, "Show triangle outlines" );
			ImGui::EndMenu();
		}

		if ( ImGui::BeginMenu( "Entities" ) ) {
			DrawCvarToggle( "Show Targets", "g_showTargets", "Show entity target connections" );
			DrawCvarToggle( "Show Triggers", "g_showTriggers", "Show trigger volumes" );
			DrawCvarToggle( "Show Entity Info", "g_showEntityInfo", "Show entity debug information" );
			ImGui::EndMenu();
		}

		if ( ImGui::BeginMenu( "Collision" ) ) {
			DrawCvarToggle( "Show Collision World", "g_showCollisionWorld", "Show collision world geometry" );
			DrawCvarToggle( "Show Collision Models", "g_showCollisionModels", "Show collision models" );
			DrawCvarToggle( "Show Collision Traces", "g_showCollisionTraces", "Show collision trace lines" );
			ImGui::EndMenu();
		}

		if ( ImGui::BeginMenu( "Physics" ) ) {
			DrawCvarToggle( "Show Active Entities", "g_showActiveEntities", "Show entities with active physics" );
			DrawCvarToggle( "Vehicle Debug", "g_vehicleDebug", "Show vehicle debug information" );
			ImGui::EndMenu();
		}

		if ( ImGui::BeginMenu( "Animation" ) ) {
			DrawCvarToggle( "IK Debug", "ik_debug", "Show inverse kinematics debug information" );
			DrawCvarToggle( "AF Debug", "af_showActive", "Show active articulated figure joints" );
			ImGui::EndMenu();
		}

		if ( ImGui::BeginMenu( "AI" ) ) {
			DrawCvarToggle( "Show AAS", "aas_showAreas", "Show AAS areas" );
			DrawCvarToggle( "Show Paths", "ai_showPaths", "Show AI pathfinding" );
			DrawCvarToggle( "Show Obstacles", "ai_showObstacleAvoidance", "Show AI obstacle avoidance" );
			ImGui::EndMenu();
		}

		ImGui::Separator();

		ImGui::TextDisabled( "Quick Toggles:" );
		DrawCvarToggle( "Show FPS", "com_showFPS", "Show frames per second" );
		DrawCvarToggle( "Show Memory", "com_showMemoryUsage", "Show memory usage" );

		ImGui::EndMenu();
	}
}

void hcEditorMenuBar::DrawPlayerMenu( void ) {
	if ( ImGui::BeginMenu( "Player" ) ) {
		if ( ImGui::MenuItem( "God Mode", "god" ) ) {
			cmdSystem->BufferCommandText( CMD_EXEC_NOW, "god" );
		}
		ImGui::AddTooltip( "Toggle invincibility" );

		if ( ImGui::MenuItem( "Noclip", "noclip" ) ) {
			cmdSystem->BufferCommandText( CMD_EXEC_NOW, "noclip" );
		}
		ImGui::AddTooltip( "Toggle collision detection" );

		if ( ImGui::MenuItem( "Notarget", "notarget" ) ) {
			cmdSystem->BufferCommandText( CMD_EXEC_NOW, "notarget" );
		}
		ImGui::AddTooltip( "Toggle being targeted by enemies" );

		ImGui::EndMenu();
	}
}

void hcEditorMenuBar::DrawReloadMenu( void ) {
	if ( ImGui::BeginMenu( "Reload" ) ) {
		if ( ImGui::MenuItem( "Reload Decls", "reloadDecls" ) ) {
			cmdSystem->BufferCommandText( CMD_EXEC_NOW, "reloadDecls" );
		}
		ImGui::AddTooltip( "Reload all declarations (materials, sounds, particles, etc.)" );

		if ( ImGui::MenuItem( "Reload Images", "reloadImages" ) ) {
			cmdSystem->BufferCommandText( CMD_EXEC_NOW, "reloadImages" );
		}
		ImGui::AddTooltip( "Reload all images and textures" );

		if ( ImGui::MenuItem( "Reload Models", "reloadModels" ) ) {
			cmdSystem->BufferCommandText( CMD_EXEC_NOW, "reloadModels" );
		}
		ImGui::AddTooltip( "Reload all models" );

		if ( ImGui::MenuItem( "Reload Sounds", "reloadSounds" ) ) {
			cmdSystem->BufferCommandText( CMD_EXEC_NOW, "reloadSounds" );
		}
		ImGui::AddTooltip( "Reload all sounds" );

		ImGui::Separator();

		if ( ImGui::MenuItem( "Reload Script", "reloadScript" ) ) {
			cmdSystem->BufferCommandText( CMD_EXEC_NOW, "reloadScript" );
		}
		ImGui::AddTooltip( "Reload game scripts" );

		if ( ImGui::MenuItem( "Reload Anims", "reloadanims" ) ) {
			cmdSystem->BufferCommandText( CMD_EXEC_NOW, "reloadanims" );
		}
		ImGui::AddTooltip( "Reload all animations" );

		if ( ImGui::MenuItem( "Reload GUIs", "reloadGuis" ) ) {
			cmdSystem->BufferCommandText( CMD_EXEC_NOW, "reloadGuis" );
		}
		ImGui::AddTooltip( "Reload all GUI definitions" );

		ImGui::Separator();

		if ( ImGui::MenuItem( "Reload ARB Shaders", "reloadARBprograms" ) ) {
			cmdSystem->BufferCommandText( CMD_EXEC_NOW, "reloadARBprograms" );
		}
		ImGui::AddTooltip( "Reload ARB shader files" );

		if ( ImGui::MenuItem( "Reload Surface", "reloadSurface" ) ) {
			cmdSystem->BufferCommandText( CMD_EXEC_NOW, "reloadSurface" );
		}
		ImGui::AddTooltip( "Reload decl and images for selected surface" );

		ImGui::Separator();

		if ( ImGui::MenuItem( "Reload Language", "reloadLanguage" ) ) {
			cmdSystem->BufferCommandText( CMD_EXEC_NOW, "reloadLanguage" );
		}
		ImGui::AddTooltip( "Reload language dictionary" );

		if ( ImGui::MenuItem( "Reload Engine", "reloadEngine" ) ) {
			cmdSystem->BufferCommandText( CMD_EXEC_NOW, "reloadEngine" );
		}
		ImGui::AddTooltip( "Full engine reload including file system" );

		ImGui::EndMenu();
	}
}

void hcEditorMenuBar::EnumEntityDefs( void ) {
	entityDefNames.Clear();

	int numDecls = declManager->GetNumDecls( DECL_ENTITYDEF );
	for ( int i = 0; i < numDecls; i++ ) {
		const idDecl* decl = declManager->DeclByIndex( DECL_ENTITYDEF, i, false );
		if ( decl ) {
			entityDefNames.Append( decl->GetName() );
		}
	}

	entityDefNames.Sort();
}

void hcEditorMenuBar::OpenSpawnDialog( void ) {
	showSpawnDialog = true;
	spawnFilter[0] = '\0';
	selectedEntityDefIndex = -1;

	if ( entityDefNames.Num() == 0 ) {
		EnumEntityDefs();
	}
}

void hcEditorMenuBar::DrawSpawnDialog( void ) {
	if ( !showSpawnDialog ) {
		return;
	}

	ImGui::OpenPopup( "Spawn Entity###SpawnEntityPopup" );

	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos( center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f) );
	ImGui::SetNextWindowSize( ImVec2(500, 400), ImGuiCond_Appearing );

	if ( ImGui::BeginPopupModal( "Spawn Entity###SpawnEntityPopup", &showSpawnDialog, ImGuiWindowFlags_None ) ) {
		ImGui::Text( "Select an entity type to spawn at the player location" );
		ImGui::Separator();

		// Filter input
		ImGui::Text( "Filter:" );
		ImGui::SameLine();
		ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
		ImGui::InputText( "##SpawnFilter", spawnFilter, sizeof(spawnFilter) );
		ImGui::AddTooltip( "Type to filter entity types" );

		ImGui::Separator();

		ImGui::BeginChild( "EntityList", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2), ImGuiChildFlags_Borders );

		idStr filterLower = spawnFilter;
		filterLower.ToLower();

		int displayCount = 0;
		for ( int i = 0; i < entityDefNames.Num(); i++ ) {
			if ( spawnFilter[0] != '\0' ) {
				idStr nameLower = entityDefNames[i];
				nameLower.ToLower();
				if ( nameLower.Find( filterLower.c_str() ) == -1 ) {
					continue;
				}
			}

			displayCount++;
			bool isSelected = (selectedEntityDefIndex == i);
			if ( ImGui::Selectable( entityDefNames[i].c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick ) ) {
				selectedEntityDefIndex = i;

				// Spawn entity wehn double clicking
				if ( ImGui::IsMouseDoubleClicked( 0 ) ) {
					cmdSystem->BufferCommandText( CMD_EXEC_NOW, va( "spawn %s", entityDefNames[i].c_str() ) );
					common->Printf( "Spawned entity: %s\n", entityDefNames[i].c_str() );
					showSpawnDialog = false;
					ImGui::CloseCurrentPopup();
				}
			}
		}

		if ( displayCount == 0 ) {
			ImGui::TextDisabled( "No entities match the filter" );
		}

		ImGui::EndChild();

		ImGui::Text( "Entities: %d / %d", displayCount, entityDefNames.Num() );
		ImGui::SameLine( ImGui::GetContentRegionAvail().x - 130 );

		bool canSpawn = (selectedEntityDefIndex >= 0 && selectedEntityDefIndex < entityDefNames.Num());
		if ( !canSpawn ) {
			ImGui::BeginDisabled();
		}

		if ( ImGui::Button( "Cancel" ) ) {
			showSpawnDialog = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();

		if ( ImGui::Button( "Spawn" ) ) {
			cmdSystem->BufferCommandText( CMD_EXEC_NOW, va( "spawn %s", entityDefNames[selectedEntityDefIndex].c_str() ) );
			common->Printf( "Spawned entity: %s\n", entityDefNames[selectedEntityDefIndex].c_str() );
			showSpawnDialog = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::AddTooltip( "Spawn the selected entity at the player location" );

		if ( !canSpawn ) {
			ImGui::EndDisabled();
		}

		ImGui::EndPopup();
	}
}

void hcEditorMenuBar::Draw( void ) {
	if ( ImGui::BeginMainMenuBar() ) {
		DrawMapMenu();
		DrawWindowMenu();
		DrawEditModeMenu();
		DrawPlayerMenu();
		DrawReloadMenu();
		DrawDebugMenu();

		// Show player position
		idStr statusText;
		if ( gameEdit && gameEdit->PlayerIsValid() ) {
			idVec3 eyePos;
			idAngles viewAngles;
			gameEdit->PlayerGetEyePosition( eyePos );
			gameEdit->PlayerGetViewAngles( viewAngles );
			statusText = va( "(%.0f %.0f %.0f) %.0f", eyePos.x, eyePos.y, eyePos.z, viewAngles.yaw );
		}

		int editMode = cvarSystem->GetCVarInteger( "g_editEntityMode" );
		idStr modeText;
		if ( editMode > 0 && editMode <= 7 ) {
			static const char* modeNames[] = { "", "Lights", "Sounds", "AF", "Particles", "Monsters", "Static", "Static" };
			modeText = va( "Editing %s", modeNames[editMode] );
		}

		// Calculate total status width
		float statusWidth = 0.0f;
		if ( statusText.Length() > 0 ) {
			statusWidth += ImGui::CalcTextSize( statusText.c_str() ).x + 20.0f;
		}
		if ( modeText.Length() > 0 ) {
			statusWidth += ImGui::CalcTextSize( modeText.c_str() ).x + 20.0f;
		}

		float windowWidth = ImGui::GetWindowWidth();
		ImGui::SameLine( windowWidth - statusWidth );

		if ( statusText.Length() > 0 ) {
			ImGui::TextDisabled( "%s", statusText.c_str() );
			ImGui::AddTooltip( "Player position (X Y Z) and yaw angle" );

			if ( modeText.Length() > 0 ) {
				ImGui::SameLine();
			}
		}

		if ( modeText.Length() > 0 ) {
			ImGui::TextDisabled( "%s", modeText.c_str() );
		}

		ImGui::EndMainMenuBar();
	}

	DrawSpawnDialog();
}

void Editor_Init( void ) {
	if ( editorMenuBar == nullptr ) {
		editorMenuBar = new hcEditorMenuBar();
	}

	if ( lightEditor == nullptr ) {
		lightEditor = new hcLightEditor();
	}

	if ( particleEditor == nullptr ) {
		particleEditor = new hcParticleEditor();
	}

	if ( playerEditor == nullptr ) {
		playerEditor = new hcPlayerEditor();
	}

	if ( entityEditor == nullptr ) {
		entityEditor = new hcEntityEditor();
	}
}

void Editor_Shutdown( void ) {
	if ( lightEditor ) {
		delete lightEditor;
		lightEditor = nullptr;
	}

	if ( particleEditor ) {
		delete particleEditor;
		particleEditor = nullptr;
	}

	if ( playerEditor ) {
		delete playerEditor;
		playerEditor = nullptr;
	}

	if ( entityEditor ) {
		delete entityEditor;
		entityEditor = nullptr;
	}

	if ( editorMenuBar ) {
		delete editorMenuBar;
		editorMenuBar = nullptr;
	}
}

void Editor_Draw( void ) {
	Editor_Init();

	if ( !editorModeActive ) {
		return;
	}

	if ( editorMenuBar ) {
		editorMenuBar->Draw();
	}

	if ( lightEditor ) {
		lightEditor->Draw();
	}

	if ( particleEditor ) {
		particleEditor->Draw();
	}

	if ( playerEditor ) {
		playerEditor->Draw();
	}

	if ( entityEditor ) {
		entityEditor->Draw();
	}
}

void Editor_ToggleMode( void ) {
	editorModeActive = !editorModeActive;

	if ( editorModeActive ) {
		D3::ImGuiHooks::OpenWindow( D3::ImGuiHooks::D3_ImGuiWin_EditorMode );
		common->Printf( "Editor mode ON\n" );
		common->ActivateTool( true );
	} else {
		D3::ImGuiHooks::CloseWindow( D3::ImGuiHooks::D3_ImGuiWin_EditorMode );
		common->Printf( "Editor mode OFF\n" );
		common->ActivateTool( false );
	}
}

bool Editor_IsModeActive( void ) {
	return editorModeActive;
}

void Editor_SetModeActive( bool active ) {
	if ( editorModeActive != active ) {
		editorModeActive = active;
		if ( active ) {
			D3::ImGuiHooks::OpenWindow( D3::ImGuiHooks::D3_ImGuiWin_EditorMode );
		} else {
			D3::ImGuiHooks::CloseWindow( D3::ImGuiHooks::D3_ImGuiWin_EditorMode );
		}

		common->ActivateTool( active );
	}
}

#else // IMGUI_DISABLE - stub implementations

#include "Editor.h"

hcEditorMenuBar*	editorMenuBar = nullptr;

hcEditorMenuBar::hcEditorMenuBar( void ) {}
void hcEditorMenuBar::Draw( void ) {}
void hcEditorMenuBar::SetTranslucent( bool enable ) {}

void Editor_Init( void ) {}
void Editor_Shutdown( void ) {}
void Editor_Draw( void ) {}
void Editor_ToggleMode( void ) {}
bool Editor_IsModeActive( void ) { return false; }
void Editor_SetModeActive( bool active ) {}

#endif // IMGUI_DISABLE
