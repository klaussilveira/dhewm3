#ifndef IMGUI_DISABLE

#include "sys/platform.h"

#include "framework/CmdSystem.h"
#include "framework/Common.h"
#include "framework/Game.h"
#include "Editor.h"
#include "SceneInspector.h"

#include "sys/sys_imgui.h"

hcSceneInspector*	sceneInspector = nullptr;

hcSceneInspector::hcSceneInspector( void ) {
	visible = false;
	needsRefresh = true;
	lastEntityCount = 0;
	filterBuffer[0] = '\0';
	selectedIndex = -1;
}

hcSceneInspector::~hcSceneInspector( void ) {
	Shutdown();
}

void hcSceneInspector::Init( const idDict* spawnArgs ) {
	visible = false;
	needsRefresh = true;
	lastEntityCount = 0;
	filterBuffer[0] = '\0';
	selectedIndex = -1;
	entityList.Clear();
}

void hcSceneInspector::Shutdown( void ) {
	entityList.Clear();
}

bool hcSceneInspector::IsVisible( void ) const {
	return visible;
}

void hcSceneInspector::SetVisible( bool show ) {
	visible = show;
	if ( visible ) {
		needsRefresh = true;
	}
}

void hcSceneInspector::RefreshEntityList( void ) {
	entityList.Clear();

	if ( !gameEdit || !gameEdit->PlayerIsValid() ) {
		return;
	}

	int numEntities = gameEdit->GetNumSpawnedEntities();
	entityList.SetGranularity( 256 );

	for ( int i = 0; i < numEntities; i++ ) {
		EntityInfo info;
		if ( gameEdit->GetSpawnedEntityInfo( i, info.name, info.classname, info.origin ) ) {
			entityList.Append( info );
		}
	}

	lastEntityCount = numEntities;
	needsRefresh = false;
}

void hcSceneInspector::TeleportToEntity( const EntityInfo& info ) {
	if ( info.name.Length() == 0 ) {
		return;
	}

	cmdSystem->BufferCommandText( CMD_EXEC_NOW, va( "teleport \"%s\"", info.name.c_str() ) );
	common->Printf( "Teleported to entity: %s\n", info.name.c_str() );
}

/*
================
hcSceneInspector::DrawEntityList
================
*/
void hcSceneInspector::DrawEntityList( void ) {
	ImGui::Text( "Filter:" );

	ImGui::SameLine();

	ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x - 80 );
	if ( ImGui::InputText( "##EntityFilter", filterBuffer, sizeof(filterBuffer) ) ) {
		// Filter changed, reset selection
		selectedIndex = -1;
	}

	ImGui::AddTooltip( "Type to filter entities by name or classname" );

	ImGui::SameLine();
	if ( ImGui::Button( "Refresh" ) ) {
		needsRefresh = true;
	}
	ImGui::AddTooltip( "Refresh the entity list" );

	ImGui::Separator();

	ImGui::BeginChild( "EntityListChild", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2 - 4), ImGuiChildFlags_Borders );

	idStr filterLower = filterBuffer;
	filterLower.ToLower();

	int displayCount = 0;
	int displayIndex = 0;

	for ( int i = 0; i < entityList.Num(); i++ ) {
		const EntityInfo& info = entityList[i];

		// Apply filter
		if ( filterBuffer[0] != '\0' ) {
			idStr nameLower = info.name;
			idStr classLower = info.classname;
			nameLower.ToLower();
			classLower.ToLower();

			if ( nameLower.Find( filterLower.c_str() ) == -1 &&
				 classLower.Find( filterLower.c_str() ) == -1 ) {
				continue;
			}
		}

		displayCount++;

		// Create display label
		idStr label;
		if ( info.name.Length() > 0 ) {
			label = va( "%s (%s)", info.name.c_str(), info.classname.c_str() );
		} else {
			label = va( "[unnamed] (%s)", info.classname.c_str() );
		}

		bool isSelected = (selectedIndex == i);
		if ( ImGui::Selectable( label.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick ) ) {
			selectedIndex = i;

			if ( ImGui::IsMouseDoubleClicked( 0 ) ) {
				TeleportToEntity( info );
			}
		}

		// Tooltip with position info
		if ( ImGui::IsItemHovered() ) {
			ImGui::BeginTooltip();
			ImGui::Text( "Name: %s", info.name.Length() > 0 ? info.name.c_str() : "[unnamed]" );
			ImGui::Text( "Class: %s", info.classname.c_str() );
			ImGui::Text( "Position: (%.1f, %.1f, %.1f)", info.origin.x, info.origin.y, info.origin.z );
			ImGui::Separator();
			ImGui::TextDisabled( "Double-click to teleport" );
			ImGui::EndTooltip();
		}

		displayIndex++;
	}

	if ( displayCount == 0 ) {
		if ( entityList.Num() == 0 ) {
			ImGui::TextDisabled( "No entities in scene" );
		} else {
			ImGui::TextDisabled( "No entities match the filter" );
		}
	}

	ImGui::EndChild();

	// Footer with entity count
	ImGui::Text( "Entities: %d / %d", displayCount, entityList.Num() );

	// Action buttons row
	bool hasSelection = (selectedIndex >= 0 && selectedIndex < entityList.Num());
	bool hasNamedSelection = hasSelection && entityList[selectedIndex].name.Length() > 0;

	if ( ImGui::Button( "Spawn..." ) ) {
		if ( editorMenuBar ) {
			editorMenuBar->OpenSpawnDialog();
		}
	}
	ImGui::AddTooltip( "Open the spawn entity dialog" );

	ImGui::SameLine();

	if ( !hasNamedSelection ) {
		ImGui::BeginDisabled();
	}
	if ( ImGui::Button( "Trigger" ) ) {
		cmdSystem->BufferCommandText( CMD_EXEC_NOW, va( "trigger \"%s\"", entityList[selectedIndex].name.c_str() ) );
		common->Printf( "Triggered entity: %s\n", entityList[selectedIndex].name.c_str() );
	}
	ImGui::AddTooltip( "Trigger the selected entity" );
	if ( !hasNamedSelection ) {
		ImGui::EndDisabled();
	}

	ImGui::SameLine();

	if ( !hasNamedSelection ) {
		ImGui::BeginDisabled();
	}
	if ( ImGui::Button( "Delete" ) ) {
		cmdSystem->BufferCommandText( CMD_EXEC_NOW, va( "remove \"%s\"", entityList[selectedIndex].name.c_str() ) );
		common->Printf( "Deleted entity: %s\n", entityList[selectedIndex].name.c_str() );
		needsRefresh = true;
		selectedIndex = -1;
	}
	ImGui::AddTooltip( "Delete the selected entity from the game" );
	if ( !hasNamedSelection ) {
		ImGui::EndDisabled();
	}

	ImGui::SameLine();

	if ( !hasNamedSelection ) {
		ImGui::BeginDisabled();
	}

	if ( ImGui::Button( "Teleport" ) ) {
		TeleportToEntity( entityList[selectedIndex] );
	}
	ImGui::AddTooltip( "Teleport to the selected entity" );

	if ( !hasNamedSelection ) {
		ImGui::EndDisabled();
	}
}

void hcSceneInspector::Draw( void ) {
	int windowFlags = D3::ImGuiHooks::GetOpenWindowsMask();
	if ( !(windowFlags & D3::ImGuiHooks::D3_ImGuiWin_SceneInspector) ) {
		visible = false;
		return;
	}

	visible = true;

	if ( gameEdit && gameEdit->PlayerIsValid() ) {
		int currentCount = gameEdit->GetNumSpawnedEntities();
		if ( currentCount != lastEntityCount ) {
			needsRefresh = true;
		}
	}

	if ( needsRefresh ) {
		RefreshEntityList();
	}

	// Create window
	ImGui::SetNextWindowSize( ImVec2(400, 500), ImGuiCond_FirstUseEver );
	bool open = true;

	if ( ImGui::Begin( "Scene Inspector", &open, ImGuiWindowFlags_None ) ) {
		if ( !gameEdit || !gameEdit->PlayerIsValid() ) {
			ImGui::TextDisabled( "No active game session" );
		} else {
			DrawEntityList();
		}
	}
	ImGui::End();

	if ( !open ) {
		D3::ImGuiHooks::CloseWindow( D3::ImGuiHooks::D3_ImGuiWin_SceneInspector );
	}
}

#else // IMGUI_DISABLE - stub implementations

#include "SceneInspector.h"

hcSceneInspector*	sceneInspector = nullptr;

hcSceneInspector::hcSceneInspector( void ) {}
hcSceneInspector::~hcSceneInspector( void ) {}
void hcSceneInspector::Init( const idDict* spawnArgs ) {}
void hcSceneInspector::Shutdown( void ) {}
void hcSceneInspector::Draw( void ) {}
bool hcSceneInspector::IsVisible( void ) const { return false; }
void hcSceneInspector::SetVisible( bool visible ) {}

#endif // IMGUI_DISABLE
