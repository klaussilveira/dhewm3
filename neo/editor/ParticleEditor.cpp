#ifndef IMGUI_DISABLE

#include "sys/platform.h"

#define IMGUI_DEFINE_MATH_OPERATORS

#include "framework/Common.h"
#include "framework/DeclManager.h"
#include "framework/DeclParticle.h"
#include "framework/FileSystem.h"
#include "framework/Game.h"
#include "ParticleEditor.h"

#include "sys/sys_imgui.h"
#include "../libs/imgui/imgui_internal.h"
#include "../libs/imgui/ImSequencer.h"
#include "../libs/imgui/ImGuizmo.h"

#include "renderer/tr_local.h"

hcParticleEditor* particleEditor = nullptr;

static const int TIMELINE_FRAMERATE = 30;

static const char* distributionNames[] = {
	"Rectangle",
	"Cylinder",
	"Sphere"
};

static const char* directionNames[] = {
	"Cone",
	"Outward"
};

static const char* orientationNames[] = {
	"View",
	"Aimed",
	"X Axis",
	"Y Axis",
	"Z Axis"
};

static const char* customPathNames[] = {
	"Standard",
	"Helix",
	"Flies",
	"Orbit",
	"Drip"
};

hcParticleSequence::hcParticleSequence( void ) : particle( nullptr ), editingStage( -1 ) {
	memset( cachedStart, 0, sizeof( cachedStart ) );
	memset( cachedEnd, 0, sizeof( cachedEnd ) );
}

int hcParticleSequence::GetFrameMin( void ) const {
	return 0;
}

int hcParticleSequence::GetFrameMax( void ) const {
	if ( !particle || particle->stages.Num() == 0 ) {
		return TIMELINE_FRAMERATE * 5;
	}

	float maxTime = 0.0f;
	for ( int i = 0; i < particle->stages.Num(); i++ ) {
		idParticleStage* stage = particle->stages[i];
		float stageEnd = stage->timeOffset + stage->particleLife + stage->deadTime;

		if ( stage->cycles > 0 ) {
			stageEnd = stage->timeOffset + (stage->particleLife + stage->deadTime) * stage->cycles;
		}

		if ( stageEnd > maxTime ) {
			maxTime = stageEnd;
		}
	}

	maxTime = Max( maxTime + 1.0f, 5.0f );

	return (int)(maxTime * TIMELINE_FRAMERATE);
}

int hcParticleSequence::GetItemCount( void ) const {
	return particle ? particle->stages.Num() : 0;
}

const char* hcParticleSequence::GetItemLabel( int index ) const {
	static char label[24];

	if ( particle && index >= 0 && index < particle->stages.Num() ) {
		idParticleStage* stage = particle->stages[index];
		if ( stage->hidden ) {
			idStr::snPrintf( label, sizeof(label), "[Hidden] Stage %d", index );
		} else {
			idStr::snPrintf( label, sizeof(label), "Stage %d", index );
		}

		return label;
	}

	return "";
}

void hcParticleSequence::Get( int index, int** start, int** end, int* type, unsigned int* color ) {
	if ( !particle || index < 0 || index >= particle->stages.Num() || index >= 64 ) {
		return;
	}

	idParticleStage* stage = particle->stages[index];

	if ( editingStage != index ) {
		cachedStart[index] = (int) (stage->timeOffset * TIMELINE_FRAMERATE);
		float endTime = stage->timeOffset + stage->particleLife;
		cachedEnd[index] = (int) (endTime * TIMELINE_FRAMERATE);
	}

	if ( start ) {
		*start = &cachedStart[index];
	}

	if ( end ) {
		*end = &cachedEnd[index];
	}

	if ( type ) {
		*type = 0;
	}

	if ( color ) {
		static unsigned int stageColors[] = {
			0xFF7B6FAF, 0xFF4BA886, 0xFFCF5868, 0xFF4A9FD4,
			0xFF57B35A, 0xFFCC5CA0, 0xFF5882C7, 0xFF49BBAF,
		};

		int colorIdx = index % (sizeof(stageColors) / sizeof(stageColors[0]));

		if ( stage->hidden ) {
			*color = 0x18404040;
		} else {
			*color = stageColors[colorIdx];
		}
	}
}

void hcParticleSequence::BeginEdit( int index ) {
	editingStage = index;
}

void hcParticleSequence::EndEdit( void ) {
	if ( editingStage >= 0 && particle && editingStage < particle->stages.Num() ) {
		UpdateStageTiming( editingStage, cachedStart[editingStage], cachedEnd[editingStage] );
	}

	editingStage = -1;
}

void hcParticleSequence::DoubleClick( int index ) {
	if ( particle && index >= 0 && index < particle->stages.Num() ) {
		particle->stages[index]->hidden = !particle->stages[index]->hidden;
	}
}

void hcParticleSequence::UpdateStageTiming( int index, int startFrame, int endFrame ) {
	if ( !particle || index < 0 || index >= particle->stages.Num() ) {
		return;
	}

	idParticleStage* stage = particle->stages[index];

	float newTimeOffset = (float)startFrame / TIMELINE_FRAMERATE;
	float newEndTime = (float)endFrame / TIMELINE_FRAMERATE;
	float newParticleLife = newEndTime - newTimeOffset;

	newTimeOffset = Max( 0.0f, newTimeOffset );
	newParticleLife = Max( 0.1f, newParticleLife );

	stage->timeOffset = newTimeOffset;
	stage->particleLife = newParticleLife;
}

hcParticleEditor::hcParticleEditor( void ) {
	initialized = false;
	visible = false;
	currentParticleIndex = -1;
	currentStageIndex = 0;
	visualizationMode = VIS_TESTMODEL;
	timelineCurrentFrame = 0;
	timelineExpanded = true;
	timelineSelectedEntry = -1;
	timelineFirstFrame = 0;
	showNewParticleDialog = false;
	showSaveAsDialog = false;
	newParticleName[0] = '\0';
	newParticleFile[0] = '\0';
	copyCurrentParticle = false;
	saveAsName[0] = '\0';
	saveAsFile[0] = '\0';
	showDropDialog = false;
	dropEntityName[0] = '\0';
	showMaterialBrowser = false;
	materialFilter[0] = '\0';
	materialBrowserScrollToIdx = -1;
	materialBrowserTab = 0;
	materialThumbnailSize = 64.0f;
	lastMaterialFilter[0] = '\0';
	mapModified = false;
	showGizmo = true;
	gizmoOperation = ImGuizmo::TRANSLATE;
	gizmoMode = ImGuizmo::WORLD;
	lastSelectedEntity = nullptr;
	propertyTableWidth = 180.0f;
}

hcParticleEditor::~hcParticleEditor( void ) {
	Shutdown();
}

void hcParticleEditor::Init( const idDict* spawnArgs ) {
	initialized = false;
	visible = true;
	common->ActivateTool( true );
}

void hcParticleEditor::Shutdown( void ) {
	visible = false;
	cvarSystem->SetCVarInteger( "g_testParticle", 0 );
	common->ActivateTool( false );
}

bool hcParticleEditor::IsVisible( void ) const {
	return visible;
}

void hcParticleEditor::SetVisible( bool vis ) {
	if ( vis && !visible ) {
		Init( nullptr );
	} else if ( !vis && visible ) {
		Shutdown();
	}

	visible = vis;
}

void hcParticleEditor::EnumParticles( void ) {
	particleNames.Clear();
	particleDeclIndices.Clear();

	int numDecls = declManager->GetNumDecls( DECL_PARTICLE );
	for ( int i = 0; i < numDecls; i++ ) {
		const idDecl* decl = declManager->DeclByIndex( DECL_PARTICLE, i, false );
		if ( decl ) {
			particleNames.Append( decl->GetName() );
			particleDeclIndices.Append( i );
		}
	}
}

void hcParticleEditor::EnumMaterials( void ) {
	materialNames.Clear();

	int numDecls = declManager->GetNumDecls( DECL_MATERIAL );
	for ( int i = 0; i < numDecls; i++ ) {
		const idDecl* decl = declManager->DeclByIndex( DECL_MATERIAL, i, false );
		if ( decl ) {
			materialNames.Append( decl->GetName() );
		}
	}

	materialNames.Sort();
}

idDeclParticle* hcParticleEditor::GetCurrentParticle( void ) {
	if ( currentParticleIndex < 0 || currentParticleIndex >= particleDeclIndices.Num() ) {
		return nullptr;
	}

	int declIndex = particleDeclIndices[currentParticleIndex];

	return static_cast<idDeclParticle*>(
		const_cast<idDecl*>( declManager->DeclByIndex( DECL_PARTICLE, declIndex ) )
	);
}

idParticleStage* hcParticleEditor::GetCurrentStage( void ) {
	idDeclParticle* particle = GetCurrentParticle();
	if ( particle == nullptr || currentStageIndex < 0 || currentStageIndex >= particle->stages.Num() ) {
		return nullptr;
	}

	return particle->stages[currentStageIndex];
}

bool hcParticleEditor::SelectParticleByName( const char* name ) {
	if ( name == nullptr || name[0] == '\0' ) {
		return false;
	}

	idStr particleName = name;
	particleName.StripFileExtension();

	for ( int i = 0; i < particleNames.Num(); i++ ) {
		if ( particleNames[i].Icmp( particleName.c_str() ) == 0 ) {
			if ( currentParticleIndex != i ) {
				currentParticleIndex = i;
				currentStageIndex = 0;
				particleSequence.SetParticle( GetCurrentParticle() );
			}

			return true;
		}
	}

	return false;
}

void hcParticleEditor::CheckSelectedParticleEntity( void ) {
	if ( cvarSystem->GetCVarInteger( "g_editEntityMode" ) != 4 ) {
		lastSelectedEntity = nullptr;

		return;
	}

	if ( gameEdit == nullptr || !gameEdit->PlayerIsValid() ) {
		lastSelectedEntity = nullptr;

		return;
	}

	idEntity* selected = nullptr;
	int numSelected = gameEdit->GetSelectedEntities( &selected, 1 );
	if ( numSelected > 0 && selected != lastSelectedEntity ) {
		lastSelectedEntity = selected;

		const idDict* spawnArgs = gameEdit->EntityGetSpawnArgs( selected );
		if ( spawnArgs ) {
			const char* model = spawnArgs->GetString( "model", "" );
			if ( model[0] != '\0' && SelectParticleByName( model ) ) {
				visualizationMode = VIS_SELECTED;
				SetParticleView();
			}
		}
	} else if ( numSelected == 0 ) {
		lastSelectedEntity = nullptr;
	}
}

void hcParticleEditor::SetParticleView( void ) {
	idDeclParticle* particle = GetCurrentParticle();
	if ( particle == nullptr ) {
		return;
	}

	cmdSystem->BufferCommandText( CMD_EXEC_NOW, "testmodel" );

	idStr str = particle->GetName();
	str.SetFileExtension( ".prt" );

	switch ( visualizationMode ) {
		case VIS_TESTMODEL:
			cmdSystem->BufferCommandText( CMD_EXEC_NOW, va( "testmodel %s\n", str.c_str() ) );
			break;
		case VIS_IMPACT:
			cvarSystem->SetCVarInteger( "g_testParticle", 1 );
			cvarSystem->SetCVarString( "g_testParticleName", str.c_str() );
			break;
		case VIS_MUZZLE:
			cvarSystem->SetCVarInteger( "g_testParticle", 2 );
			cvarSystem->SetCVarString( "g_testParticleName", str.c_str() );
			break;
		case VIS_FLIGHT:
		case VIS_SELECTED:
			cvarSystem->SetCVarInteger( "g_testParticle", 3 );
			cvarSystem->SetCVarString( "g_testParticleName", str.c_str() );
			break;
	}
}

void hcParticleEditor::MoveSelectedEntities( float x, float y, float z ) {
	if ( gameEdit == nullptr || !gameEdit->PlayerIsValid() ) {
		return;
	}

	idVec3 offset( x, y, z );

	idEntity* ent = nullptr;
	int numSelected = gameEdit->GetSelectedEntities( &ent, 1 );
	if ( numSelected <= 0 ) {
		return;
	}

	const idDict* spawnArgs = gameEdit->EntityGetSpawnArgs( ent );
	if ( spawnArgs == nullptr ) {
		return;
	}

	const char* name = spawnArgs->GetString( "name" );
	gameEdit->EntityTranslate( ent, offset );
	gameEdit->EntityUpdateVisuals( ent );
	gameEdit->MapEntityTranslate( name, offset );
	mapModified = true;
}

void hcParticleEditor::OpenNewParticleDialog( void ) {
	showNewParticleDialog = true;
	newParticleName[0] = '\0';
	idStr::Copynz( newParticleFile, "particles/my_particle.prt", sizeof(newParticleFile) );
	copyCurrentParticle = false;
}

void hcParticleEditor::OpenSaveAsDialog( void ) {
	idDeclParticle* particle = GetCurrentParticle();
	if ( particle == nullptr ) {
		return;
	}

	showSaveAsDialog = true;
	idStr::Copynz( saveAsName, particle->GetName(), sizeof(saveAsName) );
	idStr::Copynz( saveAsFile, particle->GetFileName(), sizeof(saveAsFile) );
}

void hcParticleEditor::OpenDropDialog( void ) {
	if ( gameEdit == nullptr || !gameEdit->PlayerIsValid() ) {
		return;
	}

	idDeclParticle* particle = GetCurrentParticle();
	if ( particle == nullptr ) {
		return;
	}

	showDropDialog = true;
	const char* uniqueName = gameEdit->GetUniqueEntityName( "func_emitter" );
	idStr::Copynz( dropEntityName, uniqueName, sizeof(dropEntityName) );
}

void hcParticleEditor::OpenMaterialBrowser( void ) {
	showMaterialBrowser = true;
	materialFilter[0] = '\0';

	if ( materialNames.Num() == 0 ) {
		EnumMaterials();
	}

	// Reset filter cache to force rebuild
	filteredMaterialIndices.Clear();
	lastMaterialFilter[0] = '\0';

	materialBrowserScrollToIdx = -1;
	idParticleStage* stage = GetCurrentStage();
	if ( stage && stage->material ) {
		const char* currentMat = stage->material->GetName();
		for ( int i = 0; i < materialNames.Num(); i++ ) {
			if ( materialNames[i].Icmp( currentMat ) == 0 ) {
				materialBrowserScrollToIdx = i;
				break;
			}
		}
	}
}

void hcParticleEditor::DrawParticleGizmo( void ) {
	bool inParticleMode = ( cvarSystem->GetCVarInteger( "g_editEntityMode" ) == 4 );
	if ( !inParticleMode || !showGizmo ) {
		return;
	}

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

	idVec3 entityOrigin;
	idMat3 entityAxis;
	gameEdit->EntityGetOrigin( ent, entityOrigin );
	gameEdit->EntityGetAxis( ent, entityAxis );

	idMat4 gizmoMatrix( entityAxis, entityOrigin );
	idMat4 manipMatrix = gizmoMatrix.Transpose();

	ImGuizmo::SetOrthographic( false );
	ImGuizmo::SetDrawlist( ImGui::GetBackgroundDrawList() );
	ImGuizmo::SetRect( 0, 0, (float)glConfig.vidWidth, (float)glConfig.vidHeight );

	if ( ImGuizmo::Manipulate( cameraView, cameraProjection,
			(ImGuizmo::OPERATION)gizmoOperation, (ImGuizmo::MODE)gizmoMode,
			manipMatrix.ToFloatPtr(), nullptr, nullptr ) ) {
		idMat4 resultMatrix = manipMatrix.Transpose();

		idVec3 newOrigin;
		newOrigin.x = resultMatrix[0][3];
		newOrigin.y = resultMatrix[1][3];
		newOrigin.z = resultMatrix[2][3];

		idMat3 newAxis;
		newAxis[0][0] = resultMatrix[0][0];
		newAxis[0][1] = resultMatrix[1][0];
		newAxis[0][2] = resultMatrix[2][0];
		newAxis[1][0] = resultMatrix[0][1];
		newAxis[1][1] = resultMatrix[1][1];
		newAxis[1][2] = resultMatrix[2][1];
		newAxis[2][0] = resultMatrix[0][2];
		newAxis[2][1] = resultMatrix[1][2];
		newAxis[2][2] = resultMatrix[2][2];

		const idDict* spawnArgs = gameEdit->EntityGetSpawnArgs( ent );
		if ( spawnArgs ) {
			const char* name = spawnArgs->GetString( "name" );

			if ( gizmoOperation == ImGuizmo::TRANSLATE ) {
				gameEdit->EntitySetOrigin( ent, newOrigin );
				gameEdit->MapSetEntityKeyVal( name, "origin", newOrigin.ToString() );
			} else if ( gizmoOperation == ImGuizmo::ROTATE ) {
				gameEdit->EntitySetAxis( ent, newAxis );
				gameEdit->MapSetEntityKeyVal( name, "rotation", newAxis.ToString() );
			}

			gameEdit->EntityUpdateVisuals( ent );
			mapModified = true;
		}
	}
}

void hcParticleEditor::DrawEntityControls( void ) {
	bool inParticleMode = ( cvarSystem->GetCVarInteger( "g_editEntityMode" ) == 4 );
	if ( !inParticleMode ) {
		return;
	}

	ImGui::Text( "Entity Information" );
	ImGui::Separator();

	idEntity* ent = nullptr;
	int numSelected = 0;
	if ( gameEdit && gameEdit->PlayerIsValid() ) {
		numSelected = gameEdit->GetSelectedEntities( &ent, 1 );
	}

	if ( numSelected == 0 ) {
		ImGui::TextDisabled( "No entity selected" );
		return;
	}

	const idDict* spawnArgs = gameEdit->EntityGetSpawnArgs( ent );
	if ( spawnArgs ) {
		ImGui::Text( "Selected: %s", spawnArgs->GetString( "name", "unknown" ) );
	}

	ImGui::Text( "Move:" );
	ImGui::SameLine();

	float nudgeAmount = 8.0f;

	if ( ImGui::Button( "X+" ) ) { MoveSelectedEntities( nudgeAmount, 0, 0 ); }
	ImGui::SameLine();
	if ( ImGui::Button( "X-" ) ) { MoveSelectedEntities( -nudgeAmount, 0, 0 ); }
	ImGui::SameLine();
	ImGui::Spacing();
	ImGui::SameLine();
	if ( ImGui::Button( "Y+" ) ) { MoveSelectedEntities( 0, nudgeAmount, 0 ); }
	ImGui::SameLine();
	if ( ImGui::Button( "Y-" ) ) { MoveSelectedEntities( 0, -nudgeAmount, 0 ); }
	ImGui::SameLine();
	ImGui::Spacing();
	ImGui::SameLine();
	if ( ImGui::Button( "Z+" ) ) { MoveSelectedEntities( 0, 0, nudgeAmount ); }
	ImGui::SameLine();
	if ( ImGui::Button( "Z-" ) ) { MoveSelectedEntities( 0, 0, -nudgeAmount ); }

	idVec3 origin;
	gameEdit->EntityGetOrigin( ent, origin );
	ImGui::Text( "Origin: %.1f, %.1f, %.1f", origin.x, origin.y, origin.z );

	if ( mapModified ) {
		ImGui::SameLine();
		ImGui::TextColored( ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "(modified)" );
	}

	ImGui::Separator();

	ImGui::Checkbox( "Show Gizmo", &showGizmo );
	ImGui::SameLine();
	if ( ImGui::RadioButton( "Translate", gizmoOperation == ImGuizmo::TRANSLATE ) ) {
		gizmoOperation = ImGuizmo::TRANSLATE;
	}
	ImGui::SameLine();
	if ( ImGui::RadioButton( "Rotate", gizmoOperation == ImGuizmo::ROTATE ) ) {
		gizmoOperation = ImGuizmo::ROTATE;
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

void hcParticleEditor::DrawParticleSelector( void ) {
	ImGui::Text( "Particle:" );
	ImGui::SameLine();

	const char* currentName = (currentParticleIndex >= 0 && currentParticleIndex < particleNames.Num())
		? particleNames[currentParticleIndex].c_str()
		: "<none>";

	ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x - 100 );
	if ( ImGui::BeginCombo( "##ParticleCombo", currentName ) ) {
		for ( int i = 0; i < particleNames.Num(); i++ ) {
			bool isSelected = (currentParticleIndex == i);
			if ( ImGui::Selectable( particleNames[i].c_str(), isSelected ) ) {
				currentParticleIndex = i;
				currentStageIndex = 0;
				SetParticleView();
			}
			if ( isSelected ) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	ImGui::SameLine();
	if ( ImGui::Button( "Refresh" ) ) {
		EnumParticles();
	}

	ImGui::AddTooltip( "Refresh particle list from declaration manager" );
}

void hcParticleEditor::DrawVisualizationControls( void ) {
	ImGui::Text( "Preview Mode:" );
	ImGui::SameLine();

	if ( ImGui::RadioButton( "Test Model", visualizationMode == VIS_TESTMODEL ) ) {
		visualizationMode = VIS_TESTMODEL;
		SetParticleView();
	}

	ImGui::SameLine();

	if ( ImGui::RadioButton( "Impact", visualizationMode == VIS_IMPACT ) ) {
		visualizationMode = VIS_IMPACT;
		SetParticleView();
	}

	ImGui::SameLine();

	if ( ImGui::RadioButton( "Muzzle", visualizationMode == VIS_MUZZLE ) ) {
		visualizationMode = VIS_MUZZLE;
		SetParticleView();
	}

	ImGui::SameLine();

	if ( ImGui::RadioButton( "Flight", visualizationMode == VIS_FLIGHT ) ) {
		visualizationMode = VIS_FLIGHT;
		SetParticleView();
	}

	ImGui::SameLine();

	if ( ImGui::RadioButton( "Selected", visualizationMode == VIS_SELECTED ) ) {
		visualizationMode = VIS_SELECTED;
		SetParticleView();
	}
}

void hcParticleEditor::DrawTimeline( void ) {
	idDeclParticle* particle = GetCurrentParticle();
	if ( particle == nullptr ) {
		return;
	}

	particleSequence.SetParticle( particle );

	ImGui::Separator();

	ImGui::Text( "Timeline (%.1f fps)", (float)TIMELINE_FRAMERATE );
	ImGui::SameLine();
	ImGui::TextDisabled( "Frame: %d (%.2fs)", timelineCurrentFrame, (float)timelineCurrentFrame / TIMELINE_FRAMERATE );
	ImGui::SameLine();
	ImGui::TextDisabled( "| Drag bars to adjust timing, double-click to toggle" );

	int sequencerOptions = ImSequencer::SEQUENCER_EDIT_STARTEND |
						   ImSequencer::SEQUENCER_CHANGE_FRAME;

	int prevSelectedEntry = timelineSelectedEntry;

	ImGui::PushStyleColor( ImGuiCol_ChildBg, ImGui::GetStyleColorVec4( ImGuiCol_FrameBg ) );
	ImGui::BeginChild( "ResizableChild", ImVec2( -FLT_MIN, ImGui::GetTextLineHeightWithSpacing() * 8 ), ImGuiChildFlags_ResizeY );

	ImSequencer::Sequencer(
		&particleSequence,
		&timelineCurrentFrame,
		&timelineExpanded,
		&timelineSelectedEntry,
		&timelineFirstFrame,
		sequencerOptions
	);

	ImGui::PopStyleColor();
	ImGui::EndChild();

	if ( timelineSelectedEntry != prevSelectedEntry && timelineSelectedEntry >= 0 ) {
		currentStageIndex = timelineSelectedEntry;
	}

	if ( currentStageIndex >= 0 && currentStageIndex != timelineSelectedEntry ) {
		timelineSelectedEntry = currentStageIndex;
	}
}

void hcParticleEditor::DrawStageList( void ) {
	idDeclParticle* particle = GetCurrentParticle();
	if ( particle == nullptr ) {
		ImGui::TextDisabled( "No particle selected" );
		return;
	}

	ImGui::Text( "Stages:" );

	ImGui::BeginChild( "StageList", ImVec2( 0, 120 ), ImGuiChildFlags_Borders );
	for ( int i = 0; i < particle->stages.Num(); i++ ) {
		idParticleStage* stage = particle->stages[i];
		char label[64];
		if ( stage->hidden ) {
			idStr::snPrintf( label, sizeof(label), "Stage %d (Hidden)", i );
		} else {
			idStr::snPrintf( label, sizeof(label), "Stage %d", i );
		}

		if ( ImGui::Selectable( label, currentStageIndex == i ) ) {
			currentStageIndex = i;
		}
	}
	ImGui::EndChild();

	if ( ImGui::Button( "Add" ) ) {
		idParticleStage* newStage = new idParticleStage;
		newStage->Default();
		particle->stages.Append( newStage );
		currentStageIndex = particle->stages.Num() - 1;
	}
	ImGui::AddTooltip( "Add a new particle stage" );

	ImGui::SameLine();
	if ( ImGui::Button( "Copy" ) ) {
		idParticleStage* curStage = GetCurrentStage();
		if ( curStage ) {
			idParticleStage* newStage = new idParticleStage;
			*newStage = *curStage;
			particle->stages.Append( newStage );
			currentStageIndex = particle->stages.Num() - 1;
		}
	}
	ImGui::AddTooltip( "Duplicate the current stage" );

	ImGui::SameLine();
	bool canRemove = particle->stages.Num() > 1 && currentStageIndex >= 0;
	if ( !canRemove ) {
		ImGui::BeginDisabled();
	}
	if ( ImGui::Button( "Remove" ) ) {
		if ( currentStageIndex >= 0 && currentStageIndex < particle->stages.Num() ) {
			delete particle->stages[currentStageIndex];
			particle->stages.RemoveIndex( currentStageIndex );
			if ( currentStageIndex >= particle->stages.Num() ) {
				currentStageIndex = particle->stages.Num() - 1;
			}
		}
	}
	if ( !canRemove ) {
		ImGui::EndDisabled();
	}
	ImGui::AddTooltip( "Remove the current stage" );

	ImGui::SameLine();
	idParticleStage* stage = GetCurrentStage();
	if ( stage ) {
		if ( stage->hidden ) {
			if ( ImGui::Button( "Show" ) ) {
				stage->hidden = false;
			}
		} else {
			if ( ImGui::Button( "Hide" ) ) {
				stage->hidden = true;
			}
		}
	}
}

void hcParticleEditor::DrawStageProperties( void ) {
	idParticleStage* stage = GetCurrentStage();
	if ( stage == nullptr ) {
		ImGui::TextDisabled( "No stage selected" );
		return;
	}

	idDeclParticle* particle = GetCurrentParticle();

	// Material section
	if ( ImGui::CollapsingHeader( "Material", ImGuiTreeNodeFlags_DefaultOpen ) ) {
		ImGui::PropertyTable( "MaterialProperties", propertyTableWidth, [&]() {
			static char materialName[256] = "";
			if ( stage->material ) {
				idStr::Copynz( materialName, stage->material->GetName(), sizeof( materialName ) );
			}

			ImGui::LabeledWidget( "Material", [&]() {
				if ( ImGui::InputText( "##material", materialName, sizeof( materialName ), ImGuiInputTextFlags_EnterReturnsTrue ) ) {
					stage->material = declManager->FindMaterial( materialName );
				}
				ImGui::AddTooltip( "Material for this particle stage" );
			});

			ImGui::UnlabeledWidget( [&]() {
				if ( ImGui::Button( "Browse..." ) ) {
					OpenMaterialBrowser();
				}
			});

			ImGui::LabeledWidget( "Color", [&]() {
				ImGui::ColorWidget( "##color", stage->color, "Particle color" );
			});

			ImGui::LabeledWidget( "Fade Color", [&]() {
				ImGui::ColorWidget( "##fadeColor", stage->fadeColor, "Color to fade to (0,0,0,0 for additive)" );
			});

			ImGui::LabeledWidget( "Entity Color", [&]() {
				ImGui::Checkbox( "##entityColor", &stage->entityColor );
				ImGui::AddTooltip( "Use entity color instead of particle color" );
			});

			ImGui::LabeledWidget( "Fade In", [&]() {
				ImGui::SliderFloatWidget( "##fadeInFraction", &stage->fadeInFraction, 0.0f, 1.0f, "Fraction of life to fade in" );
			});

			ImGui::LabeledWidget( "Fade Out", [&]() {
				ImGui::SliderFloatWidget( "##fadeOutFraction", &stage->fadeOutFraction, 0.0f, 1.0f, "Fraction of life to fade out" );
			});

			ImGui::LabeledWidget( "Fade Index", [&]() {
				ImGui::SliderFloatWidget( "##fadeIndexFraction", &stage->fadeIndexFraction, 0.0f, 1.0f, "Fade based on particle index" );
			});

			ImGui::LabeledWidget( "Animation Frames", [&]() {
				ImGui::InputIntWidget( "##animationFrames", &stage->animationFrames, "Number of animation frames in the material" );
			});

			ImGui::LabeledWidget( "Animation Rate", [&]() {
				ImGui::InputFloatWidget( "##animationRate", &stage->animationRate, "Animation frames per second" );
			});
		});
	}

	// Timing section
	if ( ImGui::CollapsingHeader( "Timing" ) ) {
		ImGui::PropertyTable( "TimingProperties", propertyTableWidth, [&]() {
			ImGui::LabeledWidget( "Count", [&]() {
				ImGui::SliderIntWidget( "##count", &stage->totalParticles, 1, 4096, "Total number of particles" );
			});

			ImGui::LabeledWidget( "Time", [&]() {
				ImGui::SliderFloatWidget( "##time", &stage->particleLife, 0.0f, 10.0f, "Lifetime of each particle in seconds" );
			});

			ImGui::LabeledWidget( "Cycles", [&]() {
				ImGui::SliderFloatWidget( "##cycles", &stage->cycles, 0.0f, 10.0f, "Number of cycles (0 = infinite)" );
			});

			ImGui::LabeledWidget( "Time Offset", [&]() {
				ImGui::InputFloatWidget( "##timeOffset", &stage->timeOffset, "Time offset before first particle spawns" );
			});

			ImGui::LabeledWidget( "Dead Time", [&]() {
				ImGui::InputFloatWidget( "##deadTime", &stage->deadTime, "Time after particle life before respawning" );
			});

			ImGui::LabeledWidget( "Bunching", [&]() {
				ImGui::SliderFloatWidget( "##bunching", &stage->spawnBunching, 0.0f, 1.0f, "0 = all spawn at once, 1 = evenly spaced" );
			});
		});
	}

	// Distribution section
	if ( ImGui::CollapsingHeader( "Distribution" ) ) {
		ImGui::PropertyTable( "DistributionProperties", propertyTableWidth, [&]() {
			ImGui::LabeledWidget( "Type", [&]() {
				int distType = (int)stage->distributionType;
				if ( ImGui::Combo( "##dist", &distType, distributionNames, IM_ARRAYSIZE(distributionNames) ) ) {
					stage->distributionType = (prtDistribution_t)distType;
				}
				ImGui::AddTooltip( "How particles are distributed at spawn" );
			});

			ImGui::LabeledWidget( "Size X", [&]() {
				ImGui::InputFloatWidget( "##distSizeX", &stage->distributionParms[0], "Distribution size X" );
			});

			ImGui::LabeledWidget( "Size Y", [&]() {
				ImGui::InputFloatWidget( "##distSizeY", &stage->distributionParms[1], "Distribution size Y" );
			});

			ImGui::LabeledWidget( "Size Z", [&]() {
				ImGui::InputFloatWidget( "##distSizeZ", &stage->distributionParms[2], "Distribution size Z" );
			});

			if ( stage->distributionType == PDIST_CYLINDER || stage->distributionType == PDIST_SPHERE ) {
				ImGui::LabeledWidget( "Ring Offset", [&]() {
					ImGui::SliderFloatWidget( "##ringOffset", &stage->distributionParms[3], 0.0f, 1.0f,
						"Ring fraction - inner radius of ring (0 = full volume, 0.9 = outer 10% only)" );
				});
			}

			ImGui::LabeledWidget( "Random Distribution", [&]() {
				ImGui::Checkbox( "##randomDist", &stage->randomDistribution );
				ImGui::AddTooltip( "Randomly orient particles on emission" );
			});

			ImGui::LabeledWidget( "Offset", [&]() {
				ImGui::Vec3Widget( "##offset", stage->offset, "Offset from origin for all particles" );
			});
		});
	}

	// Direction section
	if ( ImGui::CollapsingHeader( "Direction" ) ) {
		ImGui::PropertyTable( "DirectionProperties", propertyTableWidth, [&]() {
			ImGui::LabeledWidget( "Type", [&]() {
				int dirType = (int)stage->directionType;
				if ( ImGui::Combo( "##dir", &dirType, directionNames, IM_ARRAYSIZE(directionNames) ) ) {
					stage->directionType = (prtDirection_t)dirType;
				}
				ImGui::AddTooltip( "Direction type for particle emission" );
			});

			if ( stage->directionType == PDIR_CONE ) {
				ImGui::LabeledWidget( "Cone Angle", [&]() {
					ImGui::SliderFloatWidget( "##coneAngle", &stage->directionParms[0], 0.0f, 180.0f, "Solid cone angle in degrees" );
				});
			} else {
				ImGui::LabeledWidget( "Upward Bias", [&]() {
					ImGui::SliderFloatWidget( "##upwardBias", &stage->directionParms[0], -4.0f, 4.0f, "Upward bias for outward direction" );
				});
			}
		});
	}

	// Orientation section
	if ( ImGui::CollapsingHeader( "Orientation" ) ) {
		ImGui::PropertyTable( "OrientationProperties", propertyTableWidth, [&]() {
			ImGui::LabeledWidget( "Type", [&]() {
				int orientType = (int)stage->orientation;
				if ( ImGui::Combo( "##orient", &orientType, orientationNames, IM_ARRAYSIZE(orientationNames) ) ) {
					stage->orientation = (prtOrientation_t)orientType;
				}
				ImGui::AddTooltip( "How particles are oriented" );
			});

			if ( stage->orientation == POR_AIMED ) {
				ImGui::LabeledWidget( "Trails", [&]() {
					ImGui::InputFloatWidget( "##trails", &stage->orientationParms[0], "Number of trail segments" );
				});

				ImGui::LabeledWidget( "Trail Time", [&]() {
					ImGui::InputFloatWidget( "##trailTime", &stage->orientationParms[1], "Time between trail segments" );
				});
			}

			ImGui::LabeledWidget( "Initial Angle", [&]() {
				ImGui::InputFloatWidget( "##initialAngle", &stage->initialAngle, "Initial rotation angle (0 = random)" );
			});
		});
	}

	// Speed section
	if ( ImGui::CollapsingHeader( "Speed & Movement" ) ) {
		ImGui::PropertyTable( "SpeedProperties", propertyTableWidth, [&]() {
			ImGui::LabeledWidget( "Speed From", [&]() {
				float speedFrom = stage->speed.from;
				if ( ImGui::SliderFloatWidget( "##speedFrom", &speedFrom, -300.0f, 300.0f, "Minimum initial speed" ) ) {
					stage->speed.from = speedFrom;
				}
			});

			ImGui::LabeledWidget( "Speed To", [&]() {
				float speedTo = stage->speed.to;
				if ( ImGui::SliderFloatWidget( "##speedTo", &speedTo, -300.0f, 300.0f, "Maximum initial speed" ) ) {
					stage->speed.to = speedTo;
				}
			});

			ImGui::LabeledWidget( "Gravity", [&]() {
				ImGui::SliderFloatWidget( "##gravity", &stage->gravity, -300.0f, 300.0f, "Gravity (negative = float up)" );
			});

			ImGui::LabeledWidget( "World Gravity", [&]() {
				ImGui::Checkbox( "##worldGravity", &stage->worldGravity );
				ImGui::AddTooltip( "Apply gravity in world space instead of local" );
			});
		});
	}

	// Rotation section
	if ( ImGui::CollapsingHeader( "Rotation" ) ) {
		ImGui::PropertyTable( "RotationProperties", propertyTableWidth, [&]() {
			ImGui::LabeledWidget( "Rotation From", [&]() {
				float rotFrom = stage->rotationSpeed.from;
				if ( ImGui::SliderFloatWidget( "##rotFrom", &rotFrom, 0.0f, 100.0f, "Minimum rotation speed" ) ) {
					stage->rotationSpeed.from = rotFrom;
				}
			});

			ImGui::LabeledWidget( "Rotation To", [&]() {
				float rotTo = stage->rotationSpeed.to;
				if ( ImGui::SliderFloatWidget( "##rotTo", &rotTo, 0.0f, 100.0f, "Maximum rotation speed" ) ) {
					stage->rotationSpeed.to = rotTo;
				}
			});
		});
	}

	// Size section
	if ( ImGui::CollapsingHeader( "Size" ) ) {
		ImGui::PropertyTable( "SizeProperties", propertyTableWidth, [&]() {
			ImGui::LabeledWidget( "Size From", [&]() {
				float sizeFrom = stage->size.from;
				if ( ImGui::SliderFloatWidget( "##sizeFrom", &sizeFrom, 0.0f, 128.0f, "Minimum particle size" ) ) {
					stage->size.from = sizeFrom;
				}
			});

			ImGui::LabeledWidget( "Size To", [&]() {
				float sizeTo = stage->size.to;
				if ( ImGui::SliderFloatWidget( "##sizeTo", &sizeTo, 0.0f, 128.0f, "Maximum particle size" ) ) {
					stage->size.to = sizeTo;
				}
			});

			ImGui::LabeledWidget( "Aspect From", [&]() {
				float aspectFrom = stage->aspect.from;
				if ( ImGui::SliderFloatWidget( "##aspectFrom", &aspectFrom, 0.0f, 8.0f, "Minimum aspect ratio" ) ) {
					stage->aspect.from = aspectFrom;
				}
			});

			ImGui::LabeledWidget( "Aspect To", [&]() {
				float aspectTo = stage->aspect.to;
				if ( ImGui::SliderFloatWidget( "##aspectTo", &aspectTo, 0.0f, 8.0f, "Maximum aspect ratio" ) ) {
					stage->aspect.to = aspectTo;
				}
			});
		});
	}

	// Custom Path section
	if ( ImGui::CollapsingHeader( "Custom Path" ) ) {
		ImGui::PropertyTable( "CustomPathProperties", propertyTableWidth, [&]() {
			ImGui::LabeledWidget( "Path Type", [&]() {
				int pathType = (int)stage->customPathType;
				if ( ImGui::Combo( "##pathType", &pathType, customPathNames, IM_ARRAYSIZE(customPathNames) ) ) {
					stage->customPathType = (prtCustomPth_t)pathType;
				}
				ImGui::AddTooltip( "Custom path type for particle movement" );
			});

			if ( stage->customPathType != PPATH_STANDARD ) {
				const char* pathDesc = nullptr;
				switch ( stage->customPathType ) {
					case PPATH_HELIX:
						pathDesc = "Helix: Particles spiral around an axis";
						break;
					case PPATH_FLIES:
						pathDesc = "Flies: Particles move in random fly-like patterns";
						break;
					case PPATH_ORBIT:
						pathDesc = "Orbit: Particles orbit around the origin";
						break;
					case PPATH_DRIP:
						pathDesc = "Drip: Particles fall straight down";
						break;
					default:
						break;
				}
				if ( pathDesc ) {
					ImGui::UnlabeledWidget( [&]() {
						ImGui::TextWrapped( "%s", pathDesc );
					});
				}

				switch ( stage->customPathType ) {
					case PPATH_HELIX:
						ImGui::LabeledWidget( "Helix Size X", [&]() {
							ImGui::InputFloatWidget( "##helixSizeX", &stage->customPathParms[0], "Helix radius X" );
						});
						ImGui::LabeledWidget( "Helix Size Y", [&]() {
							ImGui::InputFloatWidget( "##helixSizeY", &stage->customPathParms[1], "Helix radius Y" );
						});
						ImGui::LabeledWidget( "Helix Size Z", [&]() {
							ImGui::InputFloatWidget( "##helixSizeZ", &stage->customPathParms[2], "Helix height range" );
						});
						ImGui::LabeledWidget( "Radial Speed", [&]() {
							ImGui::InputFloatWidget( "##helixRadialSpeed", &stage->customPathParms[3], "Speed of rotation around axis" );
						});
						ImGui::LabeledWidget( "Axial Speed", [&]() {
							ImGui::InputFloatWidget( "##helixAxialSpeed", &stage->customPathParms[4], "Speed along axis" );
						});
						break;
					case PPATH_FLIES:
						ImGui::LabeledWidget( "Radial Speed", [&]() {
							ImGui::InputFloatWidget( "##fliesRadialSpeed", &stage->customPathParms[0], "Speed of radial movement" );
						});
						ImGui::LabeledWidget( "Axial Speed", [&]() {
							ImGui::InputFloatWidget( "##fliesAxialSpeed", &stage->customPathParms[1], "Speed of axial movement" );
						});
						ImGui::LabeledWidget( "Size", [&]() {
							ImGui::InputFloatWidget( "##fliesSize", &stage->customPathParms[2], "Size of fly movement pattern" );
						});
						break;
					case PPATH_ORBIT:
						ImGui::LabeledWidget( "Radius", [&]() {
							ImGui::InputFloatWidget( "##orbitRadius", &stage->customPathParms[0], "Orbit radius" );
						});
						ImGui::LabeledWidget( "Speed", [&]() {
							ImGui::InputFloatWidget( "##orbitSpeed", &stage->customPathParms[1], "Orbit speed" );
						});
						break;
					case PPATH_DRIP:
						ImGui::LabeledWidget( "Speed", [&]() {
							ImGui::InputFloatWidget( "##dripSpeed", &stage->customPathParms[0], "Drip fall speed" );
						});
						break;
					default:
						break;
				}
			}
		});
	}

	// Advanced section
	if ( ImGui::CollapsingHeader( "Advanced" ) ) {
		ImGui::PropertyTable( "AdvancedProperties", propertyTableWidth, [&]() {
			if ( particle ) {
				ImGui::LabeledWidget( "Depth Hack", [&]() {
					ImGui::InputFloatWidget( "##depthHack", &particle->depthHack, "Depth hack value" );
				});
			}

			ImGui::LabeledWidget( "Bounds Expansion", [&]() {
				ImGui::InputFloatWidget( "##boundsExpansion", &stage->boundsExpansion, "Expand calculated bounds" );
			});
		});
	}
}

void hcParticleEditor::DrawSaveControls( void ) {
	idDeclParticle* particle = GetCurrentParticle();

	if ( ImGui::Button( "New..." ) ) {
		OpenNewParticleDialog();
	}
	ImGui::AddTooltip( "Create a new particle" );

	ImGui::SameLine();

	if ( particle == nullptr ) {
		ImGui::BeginDisabled();
	}

	if ( ImGui::Button( "Save" ) ) {
		if ( particle ) {
			if ( idStr::FindText( particle->GetFileName(), "implicit", false ) >= 0 ) {
				OpenSaveAsDialog();
			} else {
				particle->Save();
				common->Printf( "Saved particle: %s\n", particle->GetName() );
			}
		}
	}
	ImGui::AddTooltip( "Save the current particle" );

	ImGui::SameLine();
	if ( ImGui::Button( "Save As..." ) ) {
		OpenSaveAsDialog();
	}
	ImGui::AddTooltip( "Save the current particle to a different file" );

	if ( particle == nullptr ) {
		ImGui::EndDisabled();
	}

	ImGui::SameLine();
	bool inParticleMode = ( cvarSystem->GetCVarInteger( "g_editEntityMode" ) == 4 );
	bool saveToMapDisabled = ( !inParticleMode || !mapModified );
	if ( saveToMapDisabled ) {
		ImGui::BeginDisabled();
	}

	if ( ImGui::Button( "Save to Map" ) ) {
		cmdSystem->BufferCommandText( CMD_EXEC_NOW, "saveParticles" );
		mapModified = false;
		common->Printf( "Saved particles to map file\n" );
	}

	ImGui::AddTooltip( "Save particle entity changes to the .map file" );
	if ( saveToMapDisabled ) {
		ImGui::EndDisabled();
	}

	ImGui::SameLine();

	bool canDrop = ( particle != nullptr && gameEdit != nullptr && gameEdit->PlayerIsValid() );
	if ( !canDrop ) {
		ImGui::BeginDisabled();
	}

	if ( ImGui::Button( "Drop to Map" ) ) {
		OpenDropDialog();
	}

	ImGui::AddTooltip( "Create a new func_emitter entity with the current particle" );
	if ( !canDrop ) {
		ImGui::EndDisabled();
	}

	if ( particle ) {
		ImGui::SameLine();
		ImGui::TextDisabled( "File: %s", particle->GetFileName() );
	}
}

void hcParticleEditor::DrawNewParticleDialog( void ) {
	if ( !showNewParticleDialog ) {
		return;
	}

	ImGui::OpenPopup( "New Particle###NewParticlePopup" );

	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos( center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f) );

	if ( ImGui::BeginPopupModal( "New Particle###NewParticlePopup", &showNewParticleDialog, ImGuiWindowFlags_AlwaysAutoResize ) ) {
		ImGui::Text( "Create a new particle declaration" );
		ImGui::Separator();

		ImGui::Text( "Particle Name:" );
		ImGui::SetNextItemWidth( 300 );
		ImGui::InputText( "##NewName", newParticleName, sizeof(newParticleName) );
		ImGui::AddTooltip( "Name for the new particle (e.g., 'my_explosion')" );

		ImGui::Text( "File:" );
		ImGui::SetNextItemWidth( 300 );
		ImGui::InputText( "##NewFile", newParticleFile, sizeof(newParticleFile) );
		ImGui::AddTooltip( "File path relative to base folder" );

		idDeclParticle* currentParticle = GetCurrentParticle();
		if ( currentParticle ) {
			ImGui::Checkbox( "Copy current particle", &copyCurrentParticle );
			ImGui::AddTooltip( "Copy stages from the currently selected particle" );
		}

		ImGui::Separator();

		bool canCreate = (newParticleName[0] != '\0') && (newParticleFile[0] != '\0');
		bool alreadyExists = (declManager->FindType( DECL_PARTICLE, newParticleName, false ) != nullptr);

		if ( alreadyExists ) {
			ImGui::TextColored( ImVec4(1,0.3f,0.3f,1), "A particle with this name already exists!" );
			canCreate = false;
		}

		if ( !canCreate ) {
			ImGui::BeginDisabled();
		}

		if ( ImGui::Button( "Create", ImVec2(120, 0) ) ) {
			idStr fileName = newParticleFile;
			fileName.SetFileExtension( ".prt" );

			idDeclParticle* newDecl = static_cast<idDeclParticle*>(
				declManager->CreateNewDecl( DECL_PARTICLE, newParticleName, fileName.c_str() )
			);

			if ( newDecl ) {
				if ( copyCurrentParticle && currentParticle ) {
					newDecl->depthHack = currentParticle->depthHack;
					newDecl->stages.DeleteContents( true );
					for ( int i = 0; i < currentParticle->stages.Num(); i++ ) {
						idParticleStage* newStage = new idParticleStage;
						*newStage = *currentParticle->stages[i];
						newDecl->stages.Append( newStage );
					}
				}

				newDecl->Save( fileName.c_str() );

				EnumParticles();
				for ( int i = 0; i < particleNames.Num(); i++ ) {
					if ( particleNames[i].Icmp( newParticleName ) == 0 ) {
						currentParticleIndex = i;
						currentStageIndex = 0;
						SetParticleView();
						break;
					}
				}

				common->Printf( "Created new particle: %s in %s\n", newParticleName, fileName.c_str() );
				showNewParticleDialog = false;
				ImGui::CloseCurrentPopup();
			} else {
				common->Warning( "Failed to create particle declaration" );
			}
		}

		if ( !canCreate ) {
			ImGui::EndDisabled();
		}

		ImGui::SameLine();
		if ( ImGui::Button( "Cancel" ) ) {
			showNewParticleDialog = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void hcParticleEditor::DrawSaveAsDialog( void ) {
	if ( !showSaveAsDialog ) {
		return;
	}

	idDeclParticle* currentParticle = GetCurrentParticle();
	if ( currentParticle == nullptr ) {
		showSaveAsDialog = false;
		return;
	}

	ImGui::OpenPopup( "Save As###SaveAsPopup" );

	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos( center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f) );

	if ( ImGui::BeginPopupModal( "Save As###SaveAsPopup", &showSaveAsDialog, ImGuiWindowFlags_AlwaysAutoResize ) ) {
		ImGui::Text( "Save particle with a new name" );
		ImGui::Separator();

		ImGui::Text( "New Particle Name:" );
		ImGui::SetNextItemWidth( 300 );
		ImGui::InputText( "##SaveAsName", saveAsName, sizeof(saveAsName) );
		ImGui::AddTooltip( "New name for the particle" );

		ImGui::Text( "File:" );
		ImGui::SetNextItemWidth( 300 );
		ImGui::InputText( "##SaveAsFile", saveAsFile, sizeof(saveAsFile) );
		ImGui::AddTooltip( "File path to save to" );

		ImGui::Separator();

		bool canSave = (saveAsName[0] != '\0') && (saveAsFile[0] != '\0');
		bool sameNameDifferentFile = false;

		const idDecl* existing = declManager->FindType( DECL_PARTICLE, saveAsName, false );
		if ( existing && existing != currentParticle ) {
			ImGui::TextColored( ImVec4(1,0.3f,0.3f,1), "A particle with this name already exists!" );
			canSave = false;
		} else if ( existing == currentParticle && idStr::Icmp( saveAsFile, currentParticle->GetFileName() ) != 0 ) {
			sameNameDifferentFile = true;
		}

		if ( !canSave ) {
			ImGui::BeginDisabled();
		}

		if ( ImGui::Button( "Save", ImVec2(120, 0) ) ) {
			idStr fileName = saveAsFile;
			fileName.SetFileExtension( ".prt" );

			if ( sameNameDifferentFile || idStr::Icmp( saveAsName, currentParticle->GetName() ) == 0 ) {
				currentParticle->Save( fileName.c_str() );
				common->Printf( "Saved particle %s to %s\n", currentParticle->GetName(), fileName.c_str() );
			} else {
				idDeclParticle* newDecl = static_cast<idDeclParticle*>(
					declManager->CreateNewDecl( DECL_PARTICLE, saveAsName, fileName.c_str() )
				);

				if ( newDecl ) {
					newDecl->depthHack = currentParticle->depthHack;
					newDecl->stages.DeleteContents( true );
					for ( int i = 0; i < currentParticle->stages.Num(); i++ ) {
						idParticleStage* newStage = new idParticleStage;
						*newStage = *currentParticle->stages[i];
						newDecl->stages.Append( newStage );
					}

					newDecl->Save( fileName.c_str() );

					EnumParticles();
					for ( int i = 0; i < particleNames.Num(); i++ ) {
						if ( particleNames[i].Icmp( saveAsName ) == 0 ) {
							currentParticleIndex = i;
							currentStageIndex = 0;
							SetParticleView();
							break;
						}
					}

					common->Printf( "Saved particle as: %s in %s\n", saveAsName, fileName.c_str() );
				}
			}

			showSaveAsDialog = false;
			ImGui::CloseCurrentPopup();
		}

		if ( !canSave ) {
			ImGui::EndDisabled();
		}

		ImGui::SameLine();
		if ( ImGui::Button( "Cancel" ) ) {
			showSaveAsDialog = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void hcParticleEditor::DrawDropDialog( void ) {
	if ( !showDropDialog ) {
		return;
	}

	idDeclParticle* particle = GetCurrentParticle();
	if ( particle == nullptr ) {
		showDropDialog = false;
		return;
	}

	if ( gameEdit == nullptr || !gameEdit->PlayerIsValid() ) {
		showDropDialog = false;
		return;
	}

	ImGui::OpenPopup( "Drop Emitter to Map###DropEmitterPopup" );

	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos( center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f) );

	if ( ImGui::BeginPopupModal( "Drop Emitter to Map###DropEmitterPopup", &showDropDialog, ImGuiWindowFlags_AlwaysAutoResize ) ) {
		ImGui::Text( "Create a func_emitter entity with the current particle" );
		ImGui::Separator();

		ImGui::Text( "Particle: %s", particle->GetName() );

		ImGui::Text( "Entity Name:" );
		ImGui::SetNextItemWidth( 300 );
		ImGui::InputText( "##DropEntityName", dropEntityName, sizeof(dropEntityName) );
		ImGui::AddTooltip( "Name for the new emitter entity" );

		ImGui::Separator();

		bool canDrop = (dropEntityName[0] != '\0');
		bool nameExists = (gameEdit->FindEntity( dropEntityName ) != nullptr);

		if ( nameExists ) {
			ImGui::TextColored( ImVec4(1,0.3f,0.3f,1), "An entity with this name already exists!" );
			canDrop = false;
		}

		if ( ImGui::Button( "Cancel" ) ) {
			showDropDialog = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();

		if ( !canDrop ) {
			ImGui::BeginDisabled();
		}

		if ( ImGui::Button( "Drop", ImVec2(120, 0) ) ) {
			idVec3 eyePos;
			idAngles viewAngles;
			gameEdit->PlayerGetEyePosition( eyePos );
			gameEdit->PlayerGetViewAngles( viewAngles );

			idVec3 spawnPos = eyePos + idAngles( 0, viewAngles.yaw, 0 ).ToForward() * 80.0f + idVec3( 0, 0, 1 );

			idDict args;
			args.Set( "classname", "func_emitter" );
			args.Set( "name", dropEntityName );
			args.Set( "origin", spawnPos.ToString() );
			args.Set( "angle", va( "%f", viewAngles.yaw + 180.0f ) );

			idStr modelName = particle->GetName();
			modelName.SetFileExtension( ".prt" );
			args.Set( "model", modelName.c_str() );

			idEntity* ent = nullptr;
			gameEdit->SpawnEntityDef( args, &ent );

			if ( ent ) {
				gameEdit->EntityUpdateChangeableSpawnArgs( ent, nullptr );
				gameEdit->ClearEntitySelection();
				gameEdit->AddSelectedEntity( ent );
			}

			gameEdit->MapAddEntity( &args );

			common->Printf( "Dropped emitter '%s' with particle '%s'\n", dropEntityName, particle->GetName() );

			showDropDialog = false;
			ImGui::CloseCurrentPopup();
		}

		if ( !canDrop ) {
			ImGui::EndDisabled();
		}

		ImGui::EndPopup();
	}
}

void hcParticleEditor::RebuildFilteredMaterialList( void ) {
	bool filterChanged = (idStr::Cmp( lastMaterialFilter, materialFilter ) != 0) || (filteredMaterialIndices.Num() == 0 && materialNames.Num() > 0);
	if ( filterChanged ) {
		filteredMaterialIndices.Clear();
		idStr filterLower = materialFilter;
		filterLower.ToLower();

		for ( int i = 0; i < materialNames.Num(); i++ ) {
			if ( materialFilter[0] != '\0' ) {
				idStr nameLower = materialNames[i];
				nameLower.ToLower();
				if ( nameLower.Find( filterLower.c_str() ) == -1 ) {
					continue;
				}
			}
			filteredMaterialIndices.Append( i );
		}

		idStr::Copynz( lastMaterialFilter, materialFilter, sizeof(lastMaterialFilter) );
	}
}

void hcParticleEditor::DrawMaterialBrowserListTab( idParticleStage* stage ) {
	RebuildFilteredMaterialList();

	ImGui::TextDisabled( "Showing %d of %d materials", filteredMaterialIndices.Num(), materialNames.Num() );

	ImGui::BeginChild( "MaterialList", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2), ImGuiChildFlags_Borders );

	int totalItems = filteredMaterialIndices.Num();
	if ( totalItems == 0 ) {
		ImGui::TextDisabled( "No materials match the filter" );
		ImGui::EndChild();
		return;
	}

	// Handle scroll-to-selection
	if ( materialBrowserScrollToIdx >= 0 ) {
		for ( int fi = 0; fi < filteredMaterialIndices.Num(); fi++ ) {
			if ( filteredMaterialIndices[fi] == materialBrowserScrollToIdx ) {
				float scrollY = fi * ImGui::GetTextLineHeightWithSpacing();
				ImGui::SetScrollY( scrollY );
				break;
			}
		}
		materialBrowserScrollToIdx = -1;
	}

	// Use clipper for virtualized rendering
	ImGuiListClipper clipper;
	clipper.Begin( totalItems );

	while ( clipper.Step() ) {
		for ( int fi = clipper.DisplayStart; fi < clipper.DisplayEnd; fi++ ) {
			int i = filteredMaterialIndices[fi];
			bool isSelected = (stage->material && materialNames[i].Icmp( stage->material->GetName() ) == 0);

			if ( ImGui::Selectable( materialNames[i].c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick ) ) {
				stage->material = declManager->FindMaterial( materialNames[i].c_str() );
				if ( ImGui::IsMouseDoubleClicked( 0 ) ) {
					showMaterialBrowser = false;
					ImGui::CloseCurrentPopup();
				}
			}
		}
	}

	clipper.End();

	ImGui::EndChild();
}

void hcParticleEditor::DrawMaterialBrowserVisualTab( idParticleStage* stage ) {
	RebuildFilteredMaterialList();

	// Size slider and count display
	ImGui::Text( "Size:" );
	ImGui::SameLine();
	ImGui::SetNextItemWidth( 100 );
	ImGui::SliderFloat( "##ThumbnailSize", &materialThumbnailSize, 32.0f, 128.0f, "%.0f" );

	// Show count
	ImGui::SameLine();
	ImGui::TextDisabled( "Showing %d of %d materials", filteredMaterialIndices.Num(), materialNames.Num() );

	ImGui::BeginChild( "MaterialGrid", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2), ImGuiChildFlags_Borders );

	int totalItems = filteredMaterialIndices.Num();
	if ( totalItems == 0 ) {
		ImGui::TextDisabled( "No materials match the filter" );
		ImGui::EndChild();
		return;
	}

	// Calculate grid layout
	float windowWidth = ImGui::GetContentRegionAvail().x;
	float itemWidth = materialThumbnailSize + ImGui::GetStyle().ItemSpacing.x;
	int columns = Max( 1, (int)(windowWidth / itemWidth) );
	int totalRows = (totalItems + columns - 1) / columns;
	float rowHeight = materialThumbnailSize + ImGui::GetStyle().ItemSpacing.y;

	// Handle scroll-to-selection
	if ( materialBrowserScrollToIdx >= 0 ) {
		for ( int fi = 0; fi < filteredMaterialIndices.Num(); fi++ ) {
			if ( filteredMaterialIndices[fi] == materialBrowserScrollToIdx ) {
				int targetRow = fi / columns;
				float scrollY = targetRow * rowHeight;
				ImGui::SetScrollY( scrollY );
				break;
			}
		}
		materialBrowserScrollToIdx = -1;
	}

	// Use clipper for virtualized rendering, only render what is visible
	ImGuiListClipper clipper;
	clipper.Begin( totalRows, rowHeight );

	while ( clipper.Step() ) {
		for ( int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++ ) {
			int startIdx = row * columns;
			int endIdx = Min( startIdx + columns, totalItems );

			for ( int col = 0; col < (endIdx - startIdx); col++ ) {
				int filteredIdx = startIdx + col;
				int i = filteredMaterialIndices[filteredIdx];

				bool isSelected = (stage->material && materialNames[i].Icmp( stage->material->GetName() ) == 0);

				// Get material and its editor image
				const idMaterial* mat = declManager->FindMaterial( materialNames[i].c_str(), false );
				idImage* image = nullptr;
				if ( mat ) {
					image = mat->GetEditorImage();
				}

				ImGui::PushID( i );

				if ( col > 0 ) {
					ImGui::SameLine();
				}

				ImGui::BeginGroup();

				// Draw image or placeholder
				ImVec2 imageSize( materialThumbnailSize, materialThumbnailSize );
				bool clicked = false;

				if ( image && image->texnum != idImage::TEXTURE_NOT_LOADED ) {
					// Use OpenGL texture ID
					ImTextureID texId = (ImTextureID)(intptr_t)image->texnum;

					if ( isSelected ) {
						ImGui::PushStyleColor( ImGuiCol_Button, ImGui::GetStyleColorVec4( ImGuiCol_ButtonActive ) );
					}

					clicked = ImGui::ImageButton( "##matimg", texId, imageSize, ImVec2(0,0), ImVec2(1,1) );

					if ( isSelected ) {
						ImGui::PopStyleColor();
					}
				} else {
					// Placeholder for materials without editor image
					if ( isSelected ) {
						ImGui::PushStyleColor( ImGuiCol_Button, ImGui::GetStyleColorVec4( ImGuiCol_ButtonActive ) );
					}

					clicked = ImGui::Button( "##placeholder", imageSize );

					if ( isSelected ) {
						ImGui::PopStyleColor();
					}

					// Draw centered placeholder text that scales with thumbnail size
					ImVec2 pos = ImGui::GetItemRectMin();
					ImVec2 size = ImGui::GetItemRectSize();
					ImDrawList* drawList = ImGui::GetWindowDrawList();

					const char* placeholderText = (materialThumbnailSize >= 64.0f) ? "No Preview" : "N/A";
					ImVec2 textSize = ImGui::CalcTextSize( placeholderText );
					ImVec2 textPos(
						pos.x + (size.x - textSize.x) * 0.5f,
						pos.y + (size.y - textSize.y) * 0.5f
					);
					drawList->AddText( textPos, IM_COL32(128,128,128,255), placeholderText );
				}

				if ( clicked ) {
					stage->material = declManager->FindMaterial( materialNames[i].c_str() );
					if ( ImGui::IsMouseDoubleClicked( 0 ) ) {
						showMaterialBrowser = false;
						ImGui::CloseCurrentPopup();
					}
				}

				// Tooltip with material name
				if ( ImGui::IsItemHovered() ) {
					ImGui::SetTooltip( "%s", materialNames[i].c_str() );
				}

				// Selection highlight
				if ( isSelected ) {
					ImVec2 min = ImGui::GetItemRectMin();
					ImVec2 max = ImGui::GetItemRectMax();
					ImGui::GetWindowDrawList()->AddRect( min, max, IM_COL32(255, 200, 0, 255), 0.0f, 0, 2.0f );
				}

				ImGui::EndGroup();

				ImGui::PopID();
			}
		}
	}

	clipper.End();

	ImGui::EndChild();
}

void hcParticleEditor::DrawMaterialBrowser( void ) {
	if ( !showMaterialBrowser ) {
		return;
	}

	idParticleStage* stage = GetCurrentStage();
	if ( stage == nullptr ) {
		showMaterialBrowser = false;
		return;
	}

	ImGui::OpenPopup( "Material Browser###MaterialBrowserPopup" );

	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos( center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f) );
	ImGui::SetNextWindowSize( ImVec2(600, 500), ImGuiCond_Appearing );

	if ( ImGui::BeginPopupModal( "Material Browser###MaterialBrowserPopup", &showMaterialBrowser, ImGuiWindowFlags_None ) ) {
		// Filter input
		ImGui::Text( "Filter:" );
		ImGui::SameLine();
		ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
		if ( ImGui::InputText( "##MaterialFilter", materialFilter, sizeof(materialFilter) ) ) {
			materialBrowserScrollToIdx = -1;
		}
		ImGui::AddTooltip( "Type to filter materials (e.g., 'particle', 'smoke', 'fire')" );

		ImGui::Separator();

		// Tab bar
		if ( ImGui::BeginTabBar( "MaterialBrowserTabs" ) ) {
			if ( ImGui::BeginTabItem( "List" ) ) {
				materialBrowserTab = 0;
				DrawMaterialBrowserListTab( stage );
				ImGui::EndTabItem();
			}

			if ( ImGui::BeginTabItem( "Gallery" ) ) {
				materialBrowserTab = 1;
				DrawMaterialBrowserVisualTab( stage );
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		// Footer
		ImGui::Text( "Current: %s", stage->material ? stage->material->GetName() : "<none>" );

		float mbw = ImGui::CalcButtonWidth( "Cancel" ) + ImGui::CalcButtonWidth( "OK" ) + ImGui::GetStyle().ItemSpacing.x;
		ImGui::SameLineRight( mbw );
		if ( ImGui::Button( "Cancel" ) ) {
			showMaterialBrowser = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();

		if ( ImGui::Button( "OK" ) ) {
			showMaterialBrowser = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void hcParticleEditor::Draw( void ) {
	if ( !visible ) {
		return;
	}

	ImGuizmo::BeginFrame();

	if ( !initialized ) {
		EnumParticles();
		cvarSystem->SetCVarBool( "r_useCachedDynamicModels", false );
		initialized = true;
	}

	CheckSelectedParticleEntity();

	const float fontSize = ImGui::GetFontSize();
	ImVec2 minSize( fontSize * 40, fontSize * 30 );
	ImVec2 maxSize( ImGui::GetMainViewport()->WorkSize );
	ImGui::SetNextWindowSizeConstraints( minSize, maxSize );

	ImGuiWindowFlags winFlags = ImGuiWindowFlags_MenuBar;

	if ( !ImGui::Begin( "Particle Editor###ParticleEditorWindow", &visible, winFlags ) ) {
		ImGui::End();
		return;
	}

	// Menu bar
	if ( ImGui::BeginMenuBar() ) {
		if ( ImGui::BeginMenu( "File" ) ) {
			if ( ImGui::MenuItem( "New Particle..." ) ) {
				OpenNewParticleDialog();
			}

			if ( ImGui::MenuItem( "Save", nullptr, false, GetCurrentParticle() != nullptr ) ) {
				idDeclParticle* particle = GetCurrentParticle();
				if ( particle ) {
					if ( idStr::FindText( particle->GetFileName(), "implicit", false ) >= 0 ) {
						OpenSaveAsDialog();
					} else {
						particle->Save();
						common->Printf( "Saved particle: %s\n", particle->GetName() );
					}
				}
			}

			if ( ImGui::MenuItem( "Save As...", nullptr, false, GetCurrentParticle() != nullptr ) ) {
				OpenSaveAsDialog();
			}

			bool inParticleMode = ( cvarSystem->GetCVarInteger( "g_editEntityMode" ) == 4 );
			if ( ImGui::MenuItem( "Save to Map", nullptr, false, inParticleMode && mapModified ) ) {
				cmdSystem->BufferCommandText( CMD_EXEC_NOW, "saveParticles" );
				mapModified = false;
				common->Printf( "Saved particle entities to map file\n" );
			}

			bool canDrop = ( GetCurrentParticle() != nullptr && gameEdit != nullptr && gameEdit->PlayerIsValid() );
			if ( ImGui::MenuItem( "Drop to Map...", nullptr, false, canDrop ) ) {
				OpenDropDialog();
			}

			ImGui::Separator();

			if ( ImGui::MenuItem( "Close" ) ) {
				visible = false;
			}

			ImGui::EndMenu();
		}
		if ( ImGui::BeginMenu( "View" ) ) {
			if ( ImGui::MenuItem( "Refresh Particles" ) ) {
				EnumParticles();
			}

			ImGui::Separator();

			if ( ImGui::MenuItem( "Test Model", nullptr, visualizationMode == VIS_TESTMODEL ) ) {
				visualizationMode = VIS_TESTMODEL;
				SetParticleView();
			}

			if ( ImGui::MenuItem( "Impact", nullptr, visualizationMode == VIS_IMPACT ) ) {
				visualizationMode = VIS_IMPACT;
				SetParticleView();
			}

			if ( ImGui::MenuItem( "Muzzle", nullptr, visualizationMode == VIS_MUZZLE ) ) {
				visualizationMode = VIS_MUZZLE;
				SetParticleView();
			}

			if ( ImGui::MenuItem( "Flight", nullptr, visualizationMode == VIS_FLIGHT ) ) {
				visualizationMode = VIS_FLIGHT;
				SetParticleView();
			}

			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	// Top bar
	DrawEntityControls();
	DrawParticleSelector();
	DrawVisualizationControls();

	// Timeline
	DrawTimeline();

	ImGui::Separator();

	// Main content area with two columns
	if ( ImGui::BeginTable( "mainLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_PadOuterX ) ) {
		ImGui::TableSetupColumn( "stages", ImGuiTableColumnFlags_WidthFixed, 150.0f );
		ImGui::TableSetupColumn( "properties", ImGuiTableColumnFlags_WidthStretch );

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		DrawStageList();

		ImGui::TableNextColumn();
		DrawStageProperties();

		ImGui::EndTable();
	}

	// Bottom bar
	DrawSaveControls();

	ImGui::End();

	// Draw gizmos
	DrawParticleGizmo();

	// Draw modal dialogs
	DrawNewParticleDialog();
	DrawSaveAsDialog();
	DrawDropDialog();
	DrawMaterialBrowser();

	if ( !visible ) {
		D3::ImGuiHooks::CloseWindow( D3::ImGuiHooks::D3_ImGuiWin_ParticleEditor );
	}
}

void Com_DrawImGuiParticleEditor( void ) {
	if ( particleEditor ) {
		particleEditor->Draw();
	}
}

/*
================
Com_OpenCloseImGuiParticleEditor

Open/Close handler
================
*/
void Com_OpenCloseImGuiParticleEditor( bool open ) {
	if ( !particleEditor ) {
		particleEditor = new hcParticleEditor();
	}

	if ( open ) {
		particleEditor->Init( nullptr );
	} else {
		particleEditor->Shutdown();
	}
}

/*
================
Com_ImGuiParticleEditor_f

Console command handler
================
*/
void Com_ImGuiParticleEditor_f( const idCmdArgs& args ) {
	bool menuOpen = (D3::ImGuiHooks::GetOpenWindowsMask() & D3::ImGuiHooks::D3_ImGuiWin_ParticleEditor) != 0;
	if ( !menuOpen ) {
		D3::ImGuiHooks::OpenWindow( D3::ImGuiHooks::D3_ImGuiWin_ParticleEditor );
	} else {
		if ( ImGui::IsWindowFocused( ImGuiFocusedFlags_AnyWindow ) ) {
			D3::ImGuiHooks::CloseWindow( D3::ImGuiHooks::D3_ImGuiWin_ParticleEditor );
		} else {
			ImGui::SetNextWindowFocus();
		}
	}
}

#else // IMGUI_DISABLE - stub implementations

#include "framework/Common.h"
#include "ParticleEditor.h"

hcParticleEditor* particleEditor = nullptr;

hcParticleEditor::hcParticleEditor( void ) {}
hcParticleEditor::~hcParticleEditor( void ) {}
void hcParticleEditor::Init( const idDict* spawnArgs ) {}
void hcParticleEditor::Shutdown( void ) {}
void hcParticleEditor::Draw( void ) {}
bool hcParticleEditor::IsVisible( void ) const { return false; }
void hcParticleEditor::SetVisible( bool visible ) {}

void Com_DrawImGuiParticleEditor( void ) {}
void Com_OpenCloseImGuiParticleEditor( bool open ) {}

void Com_ImGuiParticleEditor_f( const idCmdArgs& args ) {
	common->Warning( "This editor requires imgui to be enabled" );
}

#endif // IMGUI_DISABLE
