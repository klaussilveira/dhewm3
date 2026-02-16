#ifndef IMGUI_DISABLE

#include "sys/platform.h"

#define IMGUI_DEFINE_MATH_OPERATORS

#include "sys/sys_imgui.h"
#include "framework/Common.h"
#include "framework/Game.h"
#include "renderer/tr_local.h"
#include "EntityEditor.h"

#include "../libs/imgui/imgui_internal.h"
#include "../libs/imgui/ImGuizmo.h"

hcEntityEditor* entityEditor = nullptr;

hcEntityEditor::hcEntityEditor( void ) {
	visible = false;
	selectedEntity = nullptr;
	gizmoOperation = ImGuizmo::TRANSLATE;
	gizmoMode = ImGuizmo::WORLD;
	entityOrigin.Zero();
	entityAxis.Identity();
	entityScale = 1.0f;
	showSpawnArgsSection = true;
	showTransformSection = true;
	propertyTableWidth = 120.0f;
	spawnArgFilter[0] = '\0';
}

hcEntityEditor::~hcEntityEditor( void ) {
	Shutdown();
}

void hcEntityEditor::Init( const idDict* spawnArgs ) {
	selectedEntity = nullptr;
	visible = true;

	common->ActivateTool( true );

	// Get the currently selected entity
	if ( gameEdit && gameEdit->PlayerIsValid() ) {
		idEntity* lastSelected = nullptr;
		int numSelected = gameEdit->GetSelectedEntities( &lastSelected, 1 );
		if ( numSelected > 0 ) {
			selectedEntity = lastSelected;
			RefreshEntityData();
		}
	}
}

void hcEntityEditor::Shutdown( void ) {
	visible = false;
	selectedEntity = nullptr;
	common->ActivateTool( false );
}

bool hcEntityEditor::IsVisible( void ) const {
	return visible;
}

void hcEntityEditor::SetVisible( bool vis ) {
	if ( vis && !visible ) {
		Init( nullptr );
	} else if ( !vis && visible ) {
		Shutdown();
	}

	visible = vis;
}

void hcEntityEditor::CheckSelectedEntity( void ) {
	if ( !gameEdit || !gameEdit->PlayerIsValid() ) {
		selectedEntity = nullptr;

		return;
	}

	int editMode = cvarSystem->GetCVarInteger( "g_editEntityMode" );
	if ( editMode == 0 ) {
		return;
	}

	idEntity* lastSelected = nullptr;
	int numSelected = gameEdit->GetSelectedEntities( &lastSelected, 1 );

	if ( numSelected == 0 ) {
		selectedEntity = nullptr;
	} else if ( lastSelected != selectedEntity ) {
		selectedEntity = lastSelected;
		RefreshEntityData();
	}
}

void hcEntityEditor::RefreshEntityData( void ) {
	if ( selectedEntity == nullptr || gameEdit == nullptr ) {
		return;
	}

	// Fetch entity params from game
	gameEdit->EntityGetOrigin( selectedEntity, entityOrigin );
	gameEdit->EntityGetAxis( selectedEntity, entityAxis );

	// Get modelscale from spawn args
	const idDict* spawnArgs = gameEdit->EntityGetSpawnArgs( selectedEntity );
	if ( spawnArgs != nullptr ) {
		entityScale = spawnArgs->GetFloat( "modelscale", "1.0" );
		if ( entityScale <= 0.0f ) {
			entityScale = 1.0f;
		}
	} else {
		entityScale = 1.0f;
	}
}

void hcEntityEditor::ApplyEntityChanges( void ) {
	if ( selectedEntity == nullptr || gameEdit == nullptr ) {
		return;
	}

	const idDict* spawnArgs = gameEdit->EntityGetSpawnArgs( selectedEntity );
	if ( spawnArgs == nullptr ) {
		return;
	}

	const char* name = spawnArgs->GetString( "name" );
	if ( !name || !name[0] ) {
		return;
	}

	// Update entity in game
	gameEdit->EntitySetOrigin( selectedEntity, entityOrigin );
	gameEdit->EntitySetAxis( selectedEntity, entityAxis );

	// Update spawn args
	idDict newArgs;
	newArgs.SetVector( "origin", entityOrigin );

	idAngles angles = entityAxis.ToAngles();
	if ( angles.pitch == 0.0f && angles.yaw == 0.0f && angles.roll == 0.0f ) {
		newArgs.Set( "rotation", "" );
		newArgs.Set( "angle", "" );
	} else {
		newArgs.SetAngles( "rotation", angles );
	}

	// Handle modelscale
	if ( entityScale != 1.0f && entityScale > 0.0f ) {
		newArgs.SetFloat( "modelscale", entityScale );
	} else {
		newArgs.Set( "modelscale", "" ); // Delete if default
	}

	gameEdit->EntityChangeSpawnArgs( selectedEntity, &newArgs );

	// Update render entity scale for immediate visual feedback
	gameEdit->EntitySetModelScale( selectedEntity, entityScale );

	gameEdit->EntityUpdateVisuals( selectedEntity );

	// Update map file
	gameEdit->MapSetEntityKeyVal( name, "origin", entityOrigin.ToString() );
	if ( angles.pitch == 0.0f && angles.yaw == 0.0f && angles.roll == 0.0f ) {
		gameEdit->MapSetEntityKeyVal( name, "rotation", "" );
	} else {
		gameEdit->MapSetEntityKeyVal( name, "rotation", entityAxis.ToString() );
	}

	// Update modelscale in map file
	if ( entityScale != 1.0f && entityScale > 0.0f ) {
		gameEdit->MapSetEntityKeyVal( name, "modelscale", va( "%.3f", entityScale ) );
	} else {
		gameEdit->MapSetEntityKeyVal( name, "modelscale", "" );
	}
}

void hcEntityEditor::DrawEntityInfo( void ) {
	if ( selectedEntity == nullptr || gameEdit == nullptr ) {
		ImGui::TextDisabled( "No entity selected" );
		ImGui::TextWrapped( "Enable entity edit mode (g_editEntityMode 1) and click on an entity to select it." );

		return;
	}

	const idDict* spawnArgs = gameEdit->EntityGetSpawnArgs( selectedEntity );
	if ( spawnArgs == nullptr ) {
		ImGui::TextDisabled( "No spawn args available" );

		return;
	}

	const char* classname = spawnArgs->GetString( "classname", "unknown" );
	const char* name = spawnArgs->GetString( "name", "unnamed" );

	ImGui::Text( "Class: %s", classname );
	ImGui::Text( "Name: %s", name );
}

void hcEntityEditor::DrawGizmoControls( void ) {
	ImGui::Text( "Gizmo:" );
	ImGui::SameLine();

	if ( ImGui::RadioButton( "Translate", gizmoOperation == ImGuizmo::TRANSLATE ) ) {
		gizmoOperation = ImGuizmo::TRANSLATE;
	}

	ImGui::SameLine();
	if ( ImGui::RadioButton( "Rotate", gizmoOperation == ImGuizmo::ROTATE ) ) {
		gizmoOperation = ImGuizmo::ROTATE;
	}

	ImGui::SameLine();
	if ( ImGui::RadioButton( "Scale", gizmoOperation == ImGuizmo::SCALEU ) ) {
		gizmoOperation = ImGuizmo::SCALEU;
	}

	ImGui::SameLine();
	ImGui::Spacing();
	ImGui::SameLine();

	if ( ImGui::RadioButton( "Local", gizmoMode == ImGuizmo::LOCAL ) ) {
		gizmoMode = ImGuizmo::LOCAL;
	}

	ImGui::SameLine();

	if ( ImGui::RadioButton( "World", gizmoMode == ImGuizmo::WORLD ) ) {
		gizmoMode = ImGuizmo::WORLD;
	}
}

void hcEntityEditor::DrawGizmo( void ) {
	if ( gameEdit == nullptr || !gameEdit->PlayerIsValid() ) {
		return;
	}

	idEntity* ent = nullptr;
	int numSelected = gameEdit->GetSelectedEntities( &ent, 1 );
	if ( numSelected == 0 ) {
		return;
	}

	if ( tr.primaryView == nullptr ) {
		return;
	}

	float* cameraView = tr.primaryView->worldSpace.modelViewMatrix;
	float* cameraProjection = tr.primaryView->projectionMatrix;

	idVec3 entOrigin;
	idMat3 entAxis;
	gameEdit->EntityGetOrigin( ent, entOrigin );
	gameEdit->EntityGetAxis( ent, entAxis );

	// Build matrix with scale applied for ImGuizmo
	idMat3 scaledAxis = entAxis * entityScale;
	idMat4 gizmoMatrix( scaledAxis, entOrigin );
	idMat4 manipulationMatrix = gizmoMatrix.Transpose();

	// Set up ImGuizmo
	ImGuizmo::SetOrthographic( false );
	ImGuizmo::SetDrawlist( ImGui::GetBackgroundDrawList() );
	ImGuizmo::SetRect( 0, 0, (float)glConfig.vidWidth, (float)glConfig.vidHeight );

	// Draw and manipulate the gizmo
	if ( ImGuizmo::Manipulate( cameraView, cameraProjection,
			(ImGuizmo::OPERATION)gizmoOperation, (ImGuizmo::MODE)gizmoMode,
			manipulationMatrix.ToFloatPtr(), nullptr, nullptr ) ) {
		idMat4 resultMatrix = manipulationMatrix.Transpose();

		idVec3 newOrigin;
		newOrigin.x = resultMatrix[0][3];
		newOrigin.y = resultMatrix[1][3];
		newOrigin.z = resultMatrix[2][3];

		// Extract scaled axis from result matrix
		idVec3 col0( resultMatrix[0][0], resultMatrix[1][0], resultMatrix[2][0] );
		idVec3 col1( resultMatrix[0][1], resultMatrix[1][1], resultMatrix[2][1] );
		idVec3 col2( resultMatrix[0][2], resultMatrix[1][2], resultMatrix[2][2] );

		// Extract scale from column lengths (uniform scale = average)
		float scaleX = col0.Length();
		float scaleY = col1.Length();
		float scaleZ = col2.Length();
		float newScale = ( scaleX + scaleY + scaleZ ) / 3.0f;

		// Normalize to get unscaled axis
		idMat3 newAxis;
		if ( scaleX > 0.001f ) {
			newAxis[0] = col0 / scaleX;
		}
		if ( scaleY > 0.001f ) {
			newAxis[1] = col1 / scaleY;
		}
		if ( scaleZ > 0.001f ) {
			newAxis[2] = col2 / scaleZ;
		}

		const idDict* spawnArgs = gameEdit->EntityGetSpawnArgs( ent );
		if ( spawnArgs ) {
			const char* name = spawnArgs->GetString( "name" );

			if ( gizmoOperation == ImGuizmo::TRANSLATE ) {
				gameEdit->EntitySetOrigin( ent, newOrigin );
				gameEdit->MapSetEntityKeyVal( name, "origin", newOrigin.ToString() );
				entityOrigin = newOrigin;
			} else if ( gizmoOperation == ImGuizmo::ROTATE ) {
				gameEdit->EntitySetAxis( ent, newAxis );
				gameEdit->MapSetEntityKeyVal( name, "rotation", newAxis.ToString() );
				entityAxis = newAxis;
			} else if ( gizmoOperation == ImGuizmo::SCALEU ) {
				// Clamp scale to reasonable range
				if ( newScale < 0.01f ) {
					newScale = 0.01f;
				} else if ( newScale > 100.0f ) {
					newScale = 100.0f;
				}

				entityScale = newScale;

				// Update render entity scale (for immediate visual feedback)
				gameEdit->EntitySetModelScale( ent, entityScale );

				// Update spawn args and map file
				idDict newArgs;
				newArgs.SetFloat( "modelscale", entityScale );
				gameEdit->EntityChangeSpawnArgs( ent, &newArgs );
				gameEdit->MapSetEntityKeyVal( name, "modelscale", va( "%.3f", entityScale ) );
			}

			gameEdit->EntityUpdateVisuals( ent );
		}
	}
}

void hcEntityEditor::DrawTransformSection( void ) {
	if ( !ImGui::CollapsingHeader( "Transform", showTransformSection ? ImGuiTreeNodeFlags_DefaultOpen : 0 ) ) {
		return;
	}
	showTransformSection = true;

	if ( selectedEntity == nullptr ) {
		ImGui::TextDisabled( "No entity selected" );

		return;
	}

	bool changed = false;
	const float dragSpeed = 1.0f;

	ImGui::PropertyTable( "transform_props", propertyTableWidth, [&]() {
		ImGui::LabeledWidget( "Origin X", [&]() {
			if ( ImGui::DragFloat( "##origin_x", &entityOrigin.x, dragSpeed, 0.0f, 0.0f, "%.1f" ) ) {
				changed = true;
			}
		});

		ImGui::LabeledWidget( "Origin Y", [&]() {
			if ( ImGui::DragFloat( "##origin_y", &entityOrigin.y, dragSpeed, 0.0f, 0.0f, "%.1f" ) ) {
				changed = true;
			}
		});

		ImGui::LabeledWidget( "Origin Z", [&]() {
			if ( ImGui::DragFloat( "##origin_z", &entityOrigin.z, dragSpeed, 0.0f, 0.0f, "%.1f" ) ) {
				changed = true;
			}
		});

		idAngles angles = entityAxis.ToAngles();

		ImGui::LabeledWidget( "Pitch", [&]() {
			if ( ImGui::DragFloat( "##pitch", &angles.pitch, dragSpeed, -180.0f, 180.0f, "%.1f" ) ) {
				entityAxis = angles.ToMat3();
				changed = true;
			}
		});

		ImGui::LabeledWidget( "Yaw", [&]() {
			if ( ImGui::DragFloat( "##yaw", &angles.yaw, dragSpeed, -180.0f, 180.0f, "%.1f" ) ) {
				entityAxis = angles.ToMat3();
				changed = true;
			}
		});

		ImGui::LabeledWidget( "Roll", [&]() {
			if ( ImGui::DragFloat( "##roll", &angles.roll, dragSpeed, -180.0f, 180.0f, "%.1f" ) ) {
				entityAxis = angles.ToMat3();
				changed = true;
			}
		});

		ImGui::LabeledWidget( "Scale", [&]() {
			if ( ImGui::DragFloat( "##scale", &entityScale, 0.01f, 0.01f, 100.0f, "%.3f" ) ) {
				changed = true;
			}
		});
	});

	if ( changed ) {
		ApplyEntityChanges();
	}
}

void hcEntityEditor::DrawSpawnArgsSection( void ) {
	if ( !ImGui::CollapsingHeader( "Spawn Args", showSpawnArgsSection ? ImGuiTreeNodeFlags_DefaultOpen : 0 ) ) {
		return;
	}
	showSpawnArgsSection = true;

	if ( selectedEntity == nullptr || gameEdit == nullptr ) {
		ImGui::TextDisabled( "No entity selected" );

		return;
	}

	const idDict* spawnArgs = gameEdit->EntityGetSpawnArgs( selectedEntity );
	if ( spawnArgs == nullptr ) {
		ImGui::TextDisabled( "No spawn args available" );

		return;
	}

	// Filter input
	ImGui::SetNextItemWidth( -FLT_MIN );
	ImGui::InputTextWithHint( "##filter", "Filter...", spawnArgFilter, sizeof( spawnArgFilter ) );

	ImGui::Separator();

	ImGui::BeginChild( "SpawnArgsList", ImVec2( 0, 200 ), true );
	if ( ImGui::BeginTable( "SpawnArgsTable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV ) ) {
		ImGui::TableSetupColumn( "Key", ImGuiTableColumnFlags_WidthFixed, 120.0f );
		ImGui::TableSetupColumn( "Value", ImGuiTableColumnFlags_WidthStretch );
		ImGui::TableHeadersRow();

		int numKeyVals = spawnArgs->GetNumKeyVals();
		for ( int i = 0; i < numKeyVals; i++ ) {
			const idKeyValue* kv = spawnArgs->GetKeyVal( i );
			if ( kv == nullptr ) {
				continue;
			}

			const char* key = kv->GetKey().c_str();
			const char* value = kv->GetValue().c_str();
			if ( spawnArgFilter[0] != '\0' ) {
				if ( idStr::FindText( key, spawnArgFilter, false ) == -1 &&
					 idStr::FindText( value, spawnArgFilter, false ) == -1 ) {
					continue;
				}
			}

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted( key );
			ImGui::TableNextColumn();
			ImGui::TextUnformatted( value );
		}

		ImGui::EndTable();
	}

	ImGui::EndChild();
}

void hcEntityEditor::DrawActionsSection( void ) {
	ImGui::Separator();

	bool hasSelection = ( selectedEntity != nullptr );

	if ( !hasSelection ) {
		ImGui::BeginDisabled();
	}

	if ( ImGui::Button( "Delete Entity", ImVec2( -1, 0 ) ) ) {
		if ( gameEdit != nullptr && selectedEntity != nullptr ) {
			const idDict* spawnArgs = gameEdit->EntityGetSpawnArgs( selectedEntity );
			if ( spawnArgs != nullptr ) {
				const char* name = spawnArgs->GetString( "name" );
				if ( name && name[0] ) {
					gameEdit->MapRemoveEntity( name );
					gameEdit->EntityDelete( selectedEntity );
					gameEdit->ClearEntitySelection();
					common->Printf( "Deleted entity '%s'\n", name );
				}
			}
			selectedEntity = nullptr;
		}
	}

	if ( ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled ) ) {
		ImGui::SetTooltip( "Delete the selected entity from the map" );
	}

	if ( !hasSelection ) {
		ImGui::EndDisabled();
	}
}

void hcEntityEditor::Draw( void ) {
	if ( !visible ) {
		return;
	}

	ImGuizmo::BeginFrame();

	int windowFlags = D3::ImGuiHooks::GetOpenWindowsMask();
	if ( !( windowFlags & D3::ImGuiHooks::D3_ImGuiWin_EntityEditor ) ) {
		visible = false;
		return;
	}

	CheckSelectedEntity();
	ImGui::SetNextWindowSize( ImVec2( 350, 500 ), ImGuiCond_FirstUseEver );

	bool open = true;
	if ( ImGui::Begin( "Entity Editor", &open, ImGuiWindowFlags_None ) ) {
		DrawEntityInfo();

		ImGui::Separator();

		DrawGizmoControls();
		DrawTransformSection();
		DrawSpawnArgsSection();
		DrawActionsSection();
	}

	ImGui::End();

	DrawGizmo();

	if ( !open ) {
		D3::ImGuiHooks::CloseWindow( D3::ImGuiHooks::D3_ImGuiWin_EntityEditor );
	}
}

/*
================
Com_OpenCloseImGuiEntityEditor

Open/Close handler
================
*/
void Com_OpenCloseImGuiEntityEditor( bool open ) {
	if ( !entityEditor ) {
		entityEditor = new hcEntityEditor();
	}

	if ( open ) {
		entityEditor->Init( nullptr );
	} else {
		entityEditor->Shutdown();
	}
}

/*
================
Com_ImGuiEntityEditor_f

Console command handler
================
*/
void Com_ImGuiEntityEditor_f( const idCmdArgs& args ) {
	bool menuOpen = (D3::ImGuiHooks::GetOpenWindowsMask() & D3::ImGuiHooks::D3_ImGuiWin_EntityEditor) != 0;
	if ( !menuOpen ) {
		D3::ImGuiHooks::OpenWindow( D3::ImGuiHooks::D3_ImGuiWin_EntityEditor );
	} else {
		if ( ImGui::IsWindowFocused( ImGuiFocusedFlags_AnyWindow ) ) {
			D3::ImGuiHooks::CloseWindow( D3::ImGuiHooks::D3_ImGuiWin_EntityEditor );
		} else {
			ImGui::SetNextWindowFocus();
		}
	}
}

#else // IMGUI_DISABLE - stub implementation

#include "framework/Common.h"
#include "EntityEditor.h"

hcEntityEditor* entityEditor = nullptr;

hcEntityEditor::hcEntityEditor( void ) {}
hcEntityEditor::~hcEntityEditor( void ) {}
void hcEntityEditor::Init( const idDict* spawnArgs ) {}
void hcEntityEditor::Shutdown( void ) {}
void hcEntityEditor::Draw( void ) {}
bool hcEntityEditor::IsVisible( void ) const { return false; }
void hcEntityEditor::SetVisible( bool visible ) {}
void hcEntityEditor::CheckSelectedEntity( void ) {}
void hcEntityEditor::RefreshEntityData( void ) {}
void hcEntityEditor::ApplyEntityChanges( void ) {}
void hcEntityEditor::DrawEntityInfo( void ) {}
void hcEntityEditor::DrawGizmoControls( void ) {}
void hcEntityEditor::DrawGizmo( void ) {}
void hcEntityEditor::DrawTransformSection( void ) {}
void hcEntityEditor::DrawSpawnArgsSection( void ) {}
void hcEntityEditor::DrawActionsSection( void ) {}

void Com_OpenCloseImGuiEntityEditor( bool open ) {}

void Com_ImGuiEntityEditor_f( const idCmdArgs& args ) {
	common->Warning( "This editor requires imgui to be enabled" );
}

#endif // IMGUI_DISABLE
