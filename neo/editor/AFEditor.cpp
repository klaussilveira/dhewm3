#ifndef IMGUI_DISABLE

#include "sys/platform.h"

#define IMGUI_DEFINE_MATH_OPERATORS

#include "framework/Common.h"
#include "framework/CmdSystem.h"
#include "framework/CVarSystem.h"
#include "framework/Game.h"
#include "framework/DeclManager.h"
#include "framework/FileSystem.h"
#include "renderer/Model.h"
#include "AFEditor.h"

#include "sys/sys_imgui.h"
#include "../libs/imgui/imgui_internal.h"

#include "idlib/geometry/TraceModel.h"

hcAFEditor* afEditor = nullptr;

static const char* constraintTypeNames[] = {
    "Fixed",
    "Ball and Socket",
    "Universal",
    "Hinge",
    "Slider",
    "Spring"
};

static const char* modelTypeNames[] = {
    "Box",
    "Octahedron",
    "Dodecahedron",
    "Cylinder",
    "Cone",
    "Bone"
};

static const char* jointModNames[] = {
    "Orientation",
    "Position",
    "Both"
};

static const char* limitTypeNames[] = {
    "None",
    "Cone",
    "Pyramid"
};

// Humanoid template for automatic body generation (13 bodies)
// Simplified: pelvis -> chest -> head, arms with hands, legs with cone lower legs (no feet)
//
// Key rules for stable ragdolls:
// - Root body (pelvis) controls "origin" joint but is positioned at mapped hip joint
// - Torso bodies use boxes positioned at bonecenter to avoid overlap
// - Arms use bone models between joints
// - Lower legs use cone models with rotation angles (like original Doom 3 AFs)
// - Self-collision disabled to prevent contact points fighting with constraints
const AFBodySlot humanoidTemplate[NUM_HUMANOID_SLOTS] = {
    // name           displayName           parent        endJoint       constType                          hinge  ang1  ang2 ang3  width dens  fric   shaft1            shaft2            limitAxis
    // Torso - 3 boxes: pelvis, chest, head
    { "pelvis",       "Pelvis",             NULL,         "chest",       -1,                                false,   0,    0,   0,  6.0f, 0.02f, 0.8f, {0,0,0},          {0,0,0},          {0,0,0} },
    { "chest",        "Chest",              "pelvis",     "head",        DECLAF_CONSTRAINT_UNIVERSALJOINT,  false,  60,    0,   0,  7.0f, 0.01f, 0.8f, {0,0,1},          {0,0,-1},         {0,0,1} },
    { "head",         "Head",               "chest",      NULL,          DECLAF_CONSTRAINT_UNIVERSALJOINT,  false,  60,    0,   0,  4.0f, 0.02f, 0.8f, {0,0,1},          {0,0,-1},         {0,0,1} },
    // Left arm - bones with universal joints
    { "l_upperarm",   "Left Upper Arm",     "chest",      "l_forearm",   DECLAF_CONSTRAINT_UNIVERSALJOINT,  false,  90,    0,   0,  4.0f, 0.15f, 0.8f, {0,1,0},          {0,-1,0},         {0,0.89f,-0.45f} },
    { "l_forearm",    "Left Forearm",       "l_upperarm", "l_hand",      DECLAF_CONSTRAINT_UNIVERSALJOINT,  false,  80,    0,   0,  3.0f, 0.10f, 0.8f, {0,1,0},          {0,-1,0},         {0.47f,0.81f,-0.34f} },
    { "l_hand",       "Left Hand",          "l_forearm",  NULL,          DECLAF_CONSTRAINT_UNIVERSALJOINT,  false,  45,    0,   0,  2.0f, 0.05f, 0.8f, {0,1,0},          {0,-1,0},         {0,1,0} },
    // Right arm - bones with universal joints (mirrored)
    { "r_upperarm",   "Right Upper Arm",    "chest",      "r_forearm",   DECLAF_CONSTRAINT_UNIVERSALJOINT,  false,  90,    0,   0,  4.0f, 0.15f, 0.8f, {0,-1,0},         {0,1,0},          {0,-0.89f,-0.45f} },
    { "r_forearm",    "Right Forearm",      "r_upperarm", "r_hand",      DECLAF_CONSTRAINT_UNIVERSALJOINT,  false,  80,    0,   0,  3.0f, 0.10f, 0.8f, {0,-1,0},         {0,1,0},          {0.47f,-0.81f,-0.34f} },
    { "r_hand",       "Right Hand",         "r_forearm",  NULL,          DECLAF_CONSTRAINT_UNIVERSALJOINT,  false,  45,    0,   0,  2.0f, 0.05f, 0.8f, {0,-1,0},         {0,1,0},          {0,-1,0} },
    // Left leg - upper leg bone, lower leg cone with rotation
    { "l_upperleg",   "Left Upper Leg",     "pelvis",     "l_lowerleg",  DECLAF_CONSTRAINT_UNIVERSALJOINT,  false, 100,   50, 120,  5.0f, 0.08f, 2.0f, {0,0,-1},         {0,0,1},          {0.41f,0.29f,-0.87f} },
    { "l_lowerleg",   "Left Lower Leg",     "l_upperleg", NULL,          DECLAF_CONSTRAINT_HINGE,           true,  140,  100,  94,  4.0f, 0.04f, 0.8f, {0,-1,0},         {0,0,0},          {0,0,0} },
    // Right leg - upper leg bone, lower leg cone with rotation
    { "r_upperleg",   "Right Upper Leg",    "pelvis",     "r_lowerleg",  DECLAF_CONSTRAINT_UNIVERSALJOINT,  false, 100,   50,  60,  5.0f, 0.08f, 2.0f, {0,0,-1},         {0,0,1},          {0.41f,-0.29f,-0.87f} },
    { "r_lowerleg",   "Right Lower Leg",    "r_upperleg", NULL,          DECLAF_CONSTRAINT_HINGE,           true,  140,  100,  94,  4.0f, 0.04f, 0.8f, {0,-1,0},         {0,0,0},          {0,0,0} },
};

// Auto-match patterns for joint names
struct JointMatchPattern {
    const char* slotName;
    const char* patterns[8];
};

static const JointMatchPattern autoMatchPatterns[] = {
    // Torso (3 slots: pelvis, chest, head)
    { "pelvis",     { "pelvis", "hips", "hip", NULL } },
    { "chest",      { "waist", "chest", "spine", "ribcage", "torso", NULL } },
    { "head",       { "head", "skull", "neck", "loneck", NULL } },
    // Arms (6 slots)
    { "l_upperarm", { "luparm", "l_upperarm", "leftupper", "l_arm", "lshldr", "l_shoulder", NULL } },
    { "l_forearm",  { "lloarm", "l_forearm", "leftlower", "l_elbow", "lforearm", NULL } },
    { "l_hand",     { "lhand", "l_hand", "lefthand", "lwrist", "lfing", NULL } },
    { "r_upperarm", { "ruparm", "r_upperarm", "rightupper", "r_arm", "rshldr", "r_shoulder", NULL } },
    { "r_forearm",  { "rloarm", "r_forearm", "rightlower", "r_elbow", "rforearm", NULL } },
    { "r_hand",     { "rhand", "r_hand", "righthand", "rwrist", "rfing", NULL } },
    // Legs (4 slots - lower legs are cones, no feet)
    { "l_upperleg", { "lupleg", "l_thigh", "leftthigh", "l_upperleg", "lhip", NULL } },
    { "l_lowerleg", { "lloleg", "l_calf", "leftcalf", "l_lowerleg", "l_knee", "lknee", NULL } },
    { "r_upperleg", { "rupleg", "r_thigh", "rightthigh", "r_upperleg", "rhip", NULL } },
    { "r_lowerleg", { "rloleg", "r_calf", "rightcalf", "r_lowerleg", "r_knee", "rknee", NULL } },
    { NULL,         { NULL } }  // Terminator
};

static bool ValidateNameCharacter( char c ) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') ||
           (c == '_');
}

static int ImGuiNameInputCallback( ImGuiInputTextCallbackData* data ) {
    if ( data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter ) {
        if ( !ValidateNameCharacter( (char)data->EventChar ) ) {
            return 1;
        }
    }

    return 0;
}

hcAFEditor::hcAFEditor( void ) {
    initialized = false;
    visible = false;
    currentAFIndex = -1;
    currentBodyIndex = -1;
    currentConstraintIndex = -1;
    hasTestEntity = false;
    activeTab = TAB_VIEW;
    fileModified = false;

    showNewAFDialog = false;
    showNewBodyDialog = false;
    showNewConstraintDialog = false;
    showRenameDialog = false;
    showModelBrowser = false;
    showSkinBrowser = false;
    showDeleteConfirm = false;
    showJointMappingDialog = false;

    newNameBuffer[0] = '\0';
    renameBuffer[0] = '\0';
    modelFilterBuffer[0] = '\0';
    skinFilterBuffer[0] = '\0';
    lastModelFilter[0] = '\0';
    lastSkinFilter[0] = '\0';

    renameTarget = RENAME_BODY;
    deleteTarget = DELETE_AF;

    modelBrowserScrollToIdx = -1;
    skinBrowserScrollToIdx = -1;

    viewShowBodies = true;
    viewShowBodyNames = false;
    viewShowMass = false;
    viewShowTotalMass = false;
    viewShowInertia = false;
    viewShowVelocity = false;
    viewShowConstraints = true;
    viewShowConstraintNames = false;
    viewShowPrimaryOnly = false;
    viewShowLimits = false;
    viewShowConstrainedBodies = false;
    viewShowTrees = false;
    viewShowSkeleton = true;
    viewSkeletonOnly = false;
    viewNoFriction = false;
    viewNoLimits = false;
    viewNoGravity = false;
    viewNoSelfCollision = false;
    viewDragEntity = true;
    viewDragShowSelection = true;
    viewShowTimings = false;
    viewDebugLineDepthTest = false;
    viewDebugLineUseArrows = false;

    propertyTableWidth = 180.0f;
}

hcAFEditor::~hcAFEditor( void ) {
    Shutdown();
}

void hcAFEditor::Init( const idDict* spawnArgs ) {
    initialized = false;
    visible = true;
    hasTestEntity = false;

    common->ActivateTool( true );

    EnumAFs();
    LoadViewSettings();

    // If an AF name was passed, select it
    if ( spawnArgs ) {
        const char* afName = spawnArgs->GetString( "articulatedFigure", "" );
        if ( afName[0] != '\0' ) {
            SelectAFByName( afName );
        }
    }
}

void hcAFEditor::Shutdown( void ) {
    visible = false;
    KillTestEntity();

    // Clear debug cvars
    cvarSystem->SetCVarString( "af_highlightBody", "" );
    cvarSystem->SetCVarString( "af_highlightConstraint", "" );

    common->ActivateTool( false );
}

bool hcAFEditor::IsVisible( void ) const {
    return visible;
}

void hcAFEditor::SetVisible( bool vis ) {
    if ( vis && !visible ) {
        Init( nullptr );
    } else if ( !vis && visible ) {
        Shutdown();
    }

    visible = vis;
}

void hcAFEditor::EnumAFs( void ) {
    afNames.Clear();

    int numDecls = declManager->GetNumDecls( DECL_AF );
    for ( int i = 0; i < numDecls; i++ ) {
        const idDecl* decl = declManager->DeclByIndex( DECL_AF, i, false );
        if ( decl ) {
            afNames.Append( decl->GetName() );
        }
    }

    afNames.Sort();
}

idDeclAF* hcAFEditor::GetCurrentAF( void ) {
    if ( currentAFIndex < 0 || currentAFIndex >= afNames.Num() ) {
        return nullptr;
    }

    return static_cast<idDeclAF*>(
        const_cast<idDecl*>( declManager->FindType( DECL_AF, afNames[currentAFIndex].c_str() ) )
    );
}

idDeclAF_Body* hcAFEditor::GetCurrentBody( void ) {
    idDeclAF* af = GetCurrentAF();
    if ( !af || currentBodyIndex < 0 || currentBodyIndex >= af->bodies.Num() ) {
        return nullptr;
    }

    return af->bodies[currentBodyIndex];
}

idDeclAF_Constraint* hcAFEditor::GetCurrentConstraint( void ) {
    idDeclAF* af = GetCurrentAF();
    if ( !af || currentConstraintIndex < 0 || currentConstraintIndex >= af->constraints.Num() ) {
        return nullptr;
    }

    return af->constraints[currentConstraintIndex];
}

bool hcAFEditor::SelectAFByName( const char* name ) {
    for ( int i = 0; i < afNames.Num(); i++ ) {
        if ( afNames[i].Icmp( name ) == 0 ) {
            currentAFIndex = i;
            currentBodyIndex = 0;
            currentConstraintIndex = 0;
            EnumJoints();

            return true;
        }
    }

    return false;
}

void hcAFEditor::EnumJoints( void ) {
    jointNames.Clear();

    idDeclAF* af = GetCurrentAF();
    if ( !af || af->model.IsEmpty() ) {
        return;
    }

    // Get joint names from the model via gameEdit
    if ( gameEdit ) {
        const idRenderModel* model = gameEdit->ANIM_GetModelFromName( af->model.c_str() );
        if ( model ) {
            int numJoints = model->NumJoints();
            for ( int i = 0; i < numJoints; i++ ) {
                const char* jointName = model->GetJointName( (jointHandle_t)i );
                if ( jointName ) {
                    jointNames.Append( jointName );
                }
            }
        }
    }
}

int hcAFEditor::FindJointIndex( const char* jointName ) {
    for ( int i = 0; i < jointNames.Num(); i++ ) {
        if ( jointNames[i].Icmp( jointName ) == 0 ) {
            return i;
        }
    }

    return -1;
}

void hcAFEditor::EnumModels( void ) {
    if ( modelNames.Num() > 0 ) {
        return;
    }

    int numDecls = declManager->GetNumDecls( DECL_MODELDEF );
    for ( int i = 0; i < numDecls; i++ ) {
        const idDecl* decl = declManager->DeclByIndex( DECL_MODELDEF, i, false );
        if ( decl ) {
            modelNames.Append( decl->GetName() );
        }
    }

    modelNames.Sort();
}

void hcAFEditor::EnumSkins( void ) {
    if ( skinNames.Num() > 0 ) {
        return;
    }

    int numDecls = declManager->GetNumDecls( DECL_SKIN );
    for ( int i = 0; i < numDecls; i++ ) {
        const idDecl* decl = declManager->DeclByIndex( DECL_SKIN, i, false );
        if ( decl ) {
            skinNames.Append( decl->GetName() );
        }
    }

    skinNames.Sort();
}

void hcAFEditor::RebuildFilteredModelList( void ) {
    bool filterChanged = (idStr::Cmp( lastModelFilter, modelFilterBuffer ) != 0) || (filteredModelIndices.Num() == 0 && modelNames.Num() > 0);
    if ( filterChanged ) {
        filteredModelIndices.Clear();
        idStr filterLower = modelFilterBuffer;
        filterLower.ToLower();

        for ( int i = 0; i < modelNames.Num(); i++ ) {
            if ( modelFilterBuffer[0] != '\0' ) {
                idStr nameLower = modelNames[i];
                nameLower.ToLower();
                if ( nameLower.Find( filterLower.c_str() ) == -1 ) {
                    continue;
                }
            }

            filteredModelIndices.Append( i );
        }

        idStr::Copynz( lastModelFilter, modelFilterBuffer, sizeof(lastModelFilter) );
    }
}

void hcAFEditor::RebuildFilteredSkinList( void ) {
    bool filterChanged = (idStr::Cmp( lastSkinFilter, skinFilterBuffer ) != 0) || (filteredSkinIndices.Num() == 0 && skinNames.Num() > 0);
    if ( filterChanged ) {
        filteredSkinIndices.Clear();
        idStr filterLower = skinFilterBuffer;
        filterLower.ToLower();

        for ( int i = 0; i < skinNames.Num(); i++ ) {
            if ( skinFilterBuffer[0] != '\0' ) {
                idStr nameLower = skinNames[i];
                nameLower.ToLower();
                if ( nameLower.Find( filterLower.c_str() ) == -1 ) {
                    continue;
                }
            }

            filteredSkinIndices.Append( i );
        }

        idStr::Copynz( lastSkinFilter, skinFilterBuffer, sizeof(lastSkinFilter) );
    }
}

void hcAFEditor::LoadViewSettings( void ) {
    viewShowBodies = cvarSystem->GetCVarBool( "af_showBodies" );
    viewShowBodyNames = cvarSystem->GetCVarBool( "af_showBodyNames" );
    viewShowMass = cvarSystem->GetCVarBool( "af_showMass" );
    viewShowTotalMass = cvarSystem->GetCVarBool( "af_showTotalMass" );
    viewShowInertia = cvarSystem->GetCVarBool( "af_showInertia" );
    viewShowVelocity = cvarSystem->GetCVarBool( "af_showVelocity" );
    viewShowConstraints = cvarSystem->GetCVarBool( "af_showConstraints" );
    viewShowConstraintNames = cvarSystem->GetCVarBool( "af_showConstraintNames" );
    viewShowPrimaryOnly = cvarSystem->GetCVarBool( "af_showPrimaryOnly" );
    viewShowLimits = cvarSystem->GetCVarBool( "af_showLimits" );
    viewShowConstrainedBodies = cvarSystem->GetCVarBool( "af_showConstrainedBodies" );
    viewShowTrees = cvarSystem->GetCVarBool( "af_showTrees" );
    viewShowTimings = cvarSystem->GetCVarBool( "af_showTimings" );
    viewDebugLineDepthTest = cvarSystem->GetCVarBool( "r_debugLineDepthTest" );
    viewDebugLineUseArrows = (cvarSystem->GetCVarInteger( "r_debugArrowStep" ) != 0);
    viewNoFriction = cvarSystem->GetCVarBool( "af_skipFriction" );
    viewNoLimits = cvarSystem->GetCVarBool( "af_skipLimits" );
    viewNoSelfCollision = cvarSystem->GetCVarBool( "af_skipSelfCollision" );
    viewDragEntity = cvarSystem->GetCVarBool( "g_dragEntity" );
    viewDragShowSelection = cvarSystem->GetCVarBool( "g_dragShowSelection" );

    int showSkel = cvarSystem->GetCVarInteger( "r_showSkel" );
    viewShowSkeleton = (showSkel > 0);
    viewSkeletonOnly = (showSkel == 2);

    float gravity = cvarSystem->GetCVarFloat( "g_gravity" );
    viewNoGravity = (gravity == 0.0f);
}

void hcAFEditor::ApplyViewSettings( void ) {
    cvarSystem->SetCVarBool( "af_showBodies", viewShowBodies );
    cvarSystem->SetCVarBool( "af_showBodyNames", viewShowBodyNames );
    cvarSystem->SetCVarBool( "af_showMass", viewShowMass );
    cvarSystem->SetCVarBool( "af_showTotalMass", viewShowTotalMass );
    cvarSystem->SetCVarBool( "af_showInertia", viewShowInertia );
    cvarSystem->SetCVarBool( "af_showVelocity", viewShowVelocity );
    cvarSystem->SetCVarBool( "af_showConstraints", viewShowConstraints );
    cvarSystem->SetCVarBool( "af_showConstraintNames", viewShowConstraintNames );
    cvarSystem->SetCVarBool( "af_showPrimaryOnly", viewShowPrimaryOnly );
    cvarSystem->SetCVarBool( "af_showLimits", viewShowLimits );
    cvarSystem->SetCVarBool( "af_showConstrainedBodies", viewShowConstrainedBodies );
    cvarSystem->SetCVarBool( "af_showTrees", viewShowTrees );
    cvarSystem->SetCVarBool( "af_showTimings", viewShowTimings );
    cvarSystem->SetCVarBool( "r_debugLineDepthTest", viewDebugLineDepthTest );
    cvarSystem->SetCVarInteger( "r_debugArrowStep", viewDebugLineUseArrows ? 120 : 0 );
    cvarSystem->SetCVarBool( "af_skipFriction", viewNoFriction );
    cvarSystem->SetCVarBool( "af_skipLimits", viewNoLimits );
    cvarSystem->SetCVarBool( "af_skipSelfCollision", viewNoSelfCollision );
    cvarSystem->SetCVarBool( "g_dragEntity", viewDragEntity );
    cvarSystem->SetCVarBool( "g_dragShowSelection", viewDragShowSelection );

    if ( viewShowSkeleton ) {
        cvarSystem->SetCVarInteger( "r_showSkel", viewSkeletonOnly ? 2 : 1 );
    } else {
        cvarSystem->SetCVarInteger( "r_showSkel", 0 );
    }

    if ( viewNoGravity ) {
        cvarSystem->SetCVarFloat( "g_gravity", 0.0f );
    } else {
        cvarSystem->SetCVarFloat( "g_gravity", 1066.0f );
    }
}

void hcAFEditor::SpawnTestEntity( void ) {
    idDeclAF* af = GetCurrentAF();
    if ( !af ) {
        return;
    }

    KillTestEntity();

    if ( gameEdit ) {
        if ( gameEdit->AF_SpawnEntity( af->GetName() ) ) {
            hasTestEntity = true;
        }
    }
}

void hcAFEditor::KillTestEntity( void ) {
    if ( hasTestEntity && gameEdit ) {
        cmdSystem->BufferCommandText( CMD_EXEC_NOW, "deleteSelected" );
        hasTestEntity = false;
    }
}

void hcAFEditor::UpdateTestEntity( void ) {
    idDeclAF* af = GetCurrentAF();
    if ( !af ) {
        return;
    }

    if ( gameEdit ) {
        gameEdit->AF_UpdateEntities( af->GetName() );
    }
}

void hcAFEditor::ResetTestEntityPose( void ) {
    idDeclAF* af = GetCurrentAF();
    if ( af && gameEdit ) {
        gameEdit->AF_UpdateEntities( af->GetName() );
    }
}

void hcAFEditor::ActivateTestEntityPhysics( void ) {
    idDeclAF* af = GetCurrentAF();
    if ( af && gameEdit ) {
        gameEdit->AF_ActivatePhysics( af->GetName() );
    }
}

void hcAFEditor::SaveCurrentAF( void ) {
    idDeclAF* af = GetCurrentAF();
    if ( af ) {
        if ( af->Save() ) {
            fileModified = false;
            common->Printf( "Saved AF: %s\n", af->GetName() );
        } else {
            common->Warning( "Failed to save AF: %s. File may be read-only.", af->GetName() );
        }
    }
}

void hcAFEditor::ReloadCurrentAF( void ) {
    idDeclAF* af = GetCurrentAF();
    if ( af ) {
        declManager->ReloadFile( af->GetFileName(), true );
        fileModified = false;
        UpdateTestEntity();
    }
}

void hcAFEditor::RevertChanges( void ) {
    if ( gameEdit ) {
        gameEdit->AF_UndoChanges();
    }

    fileModified = false;
    UpdateTestEntity();
}

void hcAFEditor::OpenNewAFDialog( void ) {
    showNewAFDialog = true;
    newNameBuffer[0] = '\0';
}

void hcAFEditor::OpenNewBodyDialog( void ) {
    showNewBodyDialog = true;
    idStr::Copynz( newNameBuffer, "body", sizeof(newNameBuffer) );
}

void hcAFEditor::OpenNewConstraintDialog( void ) {
    showNewConstraintDialog = true;
    idStr::Copynz( newNameBuffer, "constraint", sizeof(newNameBuffer) );
}

void hcAFEditor::OpenRenameDialog( RenameTarget target ) {
    renameTarget = target;
    showRenameDialog = true;

    if ( target == RENAME_BODY ) {
        idDeclAF_Body* body = GetCurrentBody();
        if ( body ) {
            idStr::Copynz( renameBuffer, body->name.c_str(), sizeof(renameBuffer) );
        }
    } else {
        idDeclAF_Constraint* constraint = GetCurrentConstraint();
        if ( constraint ) {
            idStr::Copynz( renameBuffer, constraint->name.c_str(), sizeof(renameBuffer) );
        }
    }
}

void hcAFEditor::OpenDeleteConfirmDialog( DeleteTarget target ) {
    deleteTarget = target;
    showDeleteConfirm = true;
}

void hcAFEditor::OpenModelBrowser( void ) {
    showModelBrowser = true;
    modelFilterBuffer[0] = '\0';
    EnumModels();
    filteredModelIndices.Clear();
    lastModelFilter[0] = '\0';
    modelBrowserScrollToIdx = -1;

    // Find current model in the list of models
    idDeclAF* af = GetCurrentAF();
    if ( af && af->model.Length() > 0 ) {
        for ( int i = 0; i < modelNames.Num(); i++ ) {
            if ( modelNames[i].Icmp( af->model ) == 0 ) {
                modelBrowserScrollToIdx = i;
                break;
            }
        }
    }
}

void hcAFEditor::OpenSkinBrowser( void ) {
    showSkinBrowser = true;
    skinFilterBuffer[0] = '\0';
    EnumSkins();
    filteredSkinIndices.Clear();
    lastSkinFilter[0] = '\0';
    skinBrowserScrollToIdx = -1;

    // Find current skin in the list of skins
    idDeclAF* af = GetCurrentAF();
    if ( af && af->skin.Length() > 0 ) {
        for ( int i = 0; i < skinNames.Num(); i++ ) {
            if ( skinNames[i].Icmp( af->skin ) == 0 ) {
                skinBrowserScrollToIdx = i;
                break;
            }
        }
    }
}

void hcAFEditor::DrawToolbar( void ) {
    idDeclAF* af = GetCurrentAF();

    // AF selector combo
    ImGui::SetNextItemWidth( 200.0f );
    const char* currentName = currentAFIndex >= 0 ? afNames[currentAFIndex].c_str() : "<Select AF>";
    if ( ImGui::BeginCombo( "##AFSelector", currentName ) ) {
        for ( int i = 0; i < afNames.Num(); i++ ) {
            bool isSelected = (currentAFIndex == i);
            if ( ImGui::Selectable( afNames[i].c_str(), isSelected ) ) {
                currentAFIndex = i;
                currentBodyIndex = 0;
                currentConstraintIndex = 0;
                fileModified = false;
                EnumJoints();
                // Clear highlighting when switching AFs
                cvarSystem->SetCVarString( "af_highlightBody", "" );
                cvarSystem->SetCVarString( "af_highlightConstraint", "" );
            }
            if ( isSelected ) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::AddTooltip( "Select an Articulated Figure to edit" );

    ImGui::SameLine();
    if ( ImGui::Button( "New" ) ) {
        OpenNewAFDialog();
    }
    ImGui::AddTooltip( "Create a new Articulated Figure" );

    ImGui::SameLine();
    if ( ImGui::Button( "Delete" ) ) {
        if ( af ) {
            OpenDeleteConfirmDialog( DELETE_AF );
        }
    }
    ImGui::AddTooltip( "Delete the current Articulated Figure" );

    ImGui::SameLine();

    // Disable Save/Revert when not modified
    if ( !fileModified ) {
        ImGui::BeginDisabled();
    }
    if ( ImGui::Button( "Save" ) ) {
        SaveCurrentAF();
    }
    ImGui::AddTooltip( "Save changes to the .af file" );

    ImGui::SameLine();
    if ( ImGui::Button( "Revert" ) ) {
        RevertChanges();
    }
    ImGui::AddTooltip( "Revert all unsaved changes" );
    if ( !fileModified ) {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if ( fileModified ) {
        ImGui::TextColored( ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "(Modified)" );
    }

    ImGui::NewLine();

    if ( !af ) {
        ImGui::BeginDisabled();
    }

    if ( ImGui::Button( "Spawn" ) ) {
        SpawnTestEntity();
    }
    ImGui::AddTooltip( "Spawn a test entity with this AF" );

    ImGui::SameLine();
    if ( ImGui::Button( "T-Pose" ) ) {
        ResetTestEntityPose();
    }
    ImGui::AddTooltip( "Reset spawned entities to T-pose" );

    ImGui::SameLine();
    if ( ImGui::Button( "Kill" ) ) {
        KillTestEntity();
    }
    ImGui::AddTooltip( "Remove the test entity" );

    ImGui::SameLine();
    if ( ImGui::Button( "Ragdoll" ) ) {
        ActivateTestEntityPhysics();
    }
    ImGui::AddTooltip( "Activate ragdoll physics on test entity" );

    ImGui::SameLine();
    if ( ImGui::Button( "Generate..." ) ) {
        OpenJointMappingDialog();
    }
    ImGui::AddTooltip( "Generate bodies and constraints from skeleton" );

    if ( !af ) {
        ImGui::EndDisabled();
    }

    ImGui::Separator();
}

void hcAFEditor::DrawViewTab( void ) {
    bool changed = false;

    if ( ImGui::CollapsingHeader( "Display Options", ImGuiTreeNodeFlags_DefaultOpen ) ) {
        ImGui::PropertyTable( "ViewDisplayProps", propertyTableWidth, [&]() {
            ImGui::LabeledWidget( "Show Bodies", [&]() {
                if ( ImGui::Checkbox( "##showBodies", &viewShowBodies ) ) changed = true;
            });
            ImGui::LabeledWidget( "Body Names", [&]() {
                if ( ImGui::Checkbox( "##showBodyNames", &viewShowBodyNames ) ) changed = true;
            });
            ImGui::LabeledWidget( "Show Mass", [&]() {
                if ( ImGui::Checkbox( "##showMass", &viewShowMass ) ) changed = true;
            });
            ImGui::LabeledWidget( "Total Mass", [&]() {
                if ( ImGui::Checkbox( "##showTotalMass", &viewShowTotalMass ) ) changed = true;
            });
            ImGui::LabeledWidget( "Show Inertia", [&]() {
                if ( ImGui::Checkbox( "##showInertia", &viewShowInertia ) ) changed = true;
            });
            ImGui::LabeledWidget( "Show Velocity", [&]() {
                if ( ImGui::Checkbox( "##showVelocity", &viewShowVelocity ) ) changed = true;
            });
        });
    }

    if ( ImGui::CollapsingHeader( "Constraint Display", ImGuiTreeNodeFlags_DefaultOpen ) ) {
        ImGui::PropertyTable( "ViewConstraintProps", propertyTableWidth, [&]() {
            ImGui::LabeledWidget( "Show Constraints", [&]() {
                if ( ImGui::Checkbox( "##showConstraints", &viewShowConstraints ) ) changed = true;
            });
            ImGui::LabeledWidget( "Constraint Names", [&]() {
                if ( ImGui::Checkbox( "##showConstraintNames", &viewShowConstraintNames ) ) changed = true;
            });
            ImGui::LabeledWidget( "Primary Only", [&]() {
                if ( ImGui::Checkbox( "##showPrimaryOnly", &viewShowPrimaryOnly ) ) changed = true;
            });
            ImGui::LabeledWidget( "Show Limits", [&]() {
                if ( ImGui::Checkbox( "##showLimits", &viewShowLimits ) ) changed = true;
            });
            ImGui::LabeledWidget( "Constrained Bodies", [&]() {
                if ( ImGui::Checkbox( "##showConstrainedBodies", &viewShowConstrainedBodies ) ) changed = true;
            });
            ImGui::AddTooltip( "Show bodies constrained by current constraint (body1 = cyan, body2 = blue)" );
            ImGui::LabeledWidget( "Show Trees", [&]() {
                if ( ImGui::Checkbox( "##showTrees", &viewShowTrees ) ) changed = true;
            });
        });
    }

    if ( ImGui::CollapsingHeader( "Skeleton Display" ) ) {
        ImGui::PropertyTable( "ViewSkeletonProps", propertyTableWidth, [&]() {
            ImGui::LabeledWidget( "Show Skeleton", [&]() {
                if ( ImGui::Checkbox( "##showSkeleton", &viewShowSkeleton ) ) changed = true;
            });
            ImGui::LabeledWidget( "Skeleton Only", [&]() {
                if ( ImGui::Checkbox( "##skeletonOnly", &viewSkeletonOnly ) ) changed = true;
            });
        });
    }

    if ( ImGui::CollapsingHeader( "Physics Override" ) ) {
        ImGui::PropertyTable( "ViewPhysicsProps", propertyTableWidth, [&]() {
            ImGui::LabeledWidget( "No Friction", [&]() {
                if ( ImGui::Checkbox( "##noFriction", &viewNoFriction ) ) changed = true;
            });
            ImGui::LabeledWidget( "No Limits", [&]() {
                if ( ImGui::Checkbox( "##noLimits", &viewNoLimits ) ) changed = true;
            });
            ImGui::LabeledWidget( "No Gravity", [&]() {
                if ( ImGui::Checkbox( "##noGravity", &viewNoGravity ) ) changed = true;
            });
            ImGui::LabeledWidget( "No Self Collision", [&]() {
                if ( ImGui::Checkbox( "##noSelfCollision", &viewNoSelfCollision ) ) changed = true;
            });
        });
    }

    if ( ImGui::CollapsingHeader( "Interaction" ) ) {
        ImGui::PropertyTable( "ViewInteractionProps", propertyTableWidth, [&]() {
            ImGui::LabeledWidget( "Drag Entity", [&]() {
                if ( ImGui::Checkbox( "##dragEntity", &viewDragEntity ) ) changed = true;
            });
            ImGui::AddTooltip( "Enable clicking and dragging AF entities in the world" );
            ImGui::LabeledWidget( "Show Selection", [&]() {
                if ( ImGui::Checkbox( "##dragShowSelection", &viewDragShowSelection ) ) changed = true;
            });
            ImGui::AddTooltip( "Show selection box around dragged entity" );
        });
    }

    if ( ImGui::CollapsingHeader( "Debug" ) ) {
        ImGui::PropertyTable( "ViewDebugProps", propertyTableWidth, [&]() {
            ImGui::LabeledWidget( "Show Timings", [&]() {
                if ( ImGui::Checkbox( "##showTimings", &viewShowTimings ) ) changed = true;
            });
            ImGui::AddTooltip( "Show physics performance timings" );
            ImGui::LabeledWidget( "Line Depth Test", [&]() {
                if ( ImGui::Checkbox( "##lineDepthTest", &viewDebugLineDepthTest ) ) changed = true;
            });
            ImGui::AddTooltip( "Apply z-buffer testing to debug lines" );
            ImGui::LabeledWidget( "Use Arrows", [&]() {
                if ( ImGui::Checkbox( "##useArrows", &viewDebugLineUseArrows ) ) changed = true;
            });
            ImGui::AddTooltip( "Draw arrows instead of lines for direction vectors" );
        });
    }

    if ( changed ) {
        ApplyViewSettings();
    }
}

void hcAFEditor::DrawPropertiesTab( void ) {
    idDeclAF* af = GetCurrentAF();
    if ( !af ) {
        ImGui::TextDisabled( "No AF selected" );
        return;
    }

    bool changed = false;

    if ( ImGui::CollapsingHeader( "Model", ImGuiTreeNodeFlags_DefaultOpen ) ) {
        ImGui::PropertyTable( "ModelProps", propertyTableWidth, [&]() {
            ImGui::LabeledWidget( "Model", [&]() {
                ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x - 70 );
                ImGui::InputText( "##model", (char*)af->model.c_str(), af->model.Size(), ImGuiInputTextFlags_ReadOnly );
                ImGui::SameLine();
                if ( ImGui::Button( "Browse##model" ) ) {
                    OpenModelBrowser();
                }
            });
            ImGui::LabeledWidget( "Skin", [&]() {
                ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x - 70 );
                ImGui::InputText( "##skin", (char*)af->skin.c_str(), af->skin.Size(), ImGuiInputTextFlags_ReadOnly );
                ImGui::SameLine();
                if ( ImGui::Button( "Browse##skin" ) ) {
                    OpenSkinBrowser();
                }
            });
        });
    }

    if ( ImGui::CollapsingHeader( "Collision", ImGuiTreeNodeFlags_DefaultOpen ) ) {
        ImGui::PropertyTable( "CollisionProps", propertyTableWidth, [&]() {
            ImGui::LabeledWidget( "Self Collision", [&]() {
                if ( ImGui::Checkbox( "##afSelfCollision", &af->selfCollision ) ) {
                    changed = true;
                }
                ImGui::SameLine();
                if ( ImGui::Button( "Apply##selfCollision" ) ) {
                    for ( int i = 0; i < af->bodies.Num(); i++ ) {
                        af->bodies[i]->selfCollision = af->selfCollision;
                    }
                    changed = true;
                }
                ImGui::AddTooltip( "Apply this value to all bodies" );
            });
            ImGui::LabeledWidget( "Contents", [&]() {
                if ( DrawContentsWidget( "afContents", af->contents, "Default contents for all bodies" ) ) {
                    changed = true;
                }
                ImGui::SameLine();
                if ( ImGui::Button( "Apply##contents" ) ) {
                    for ( int i = 0; i < af->bodies.Num(); i++ ) {
                        af->bodies[i]->contents = af->contents;
                    }
                    changed = true;
                }
                ImGui::AddTooltip( "Apply this value to all bodies" );
            });
            ImGui::LabeledWidget( "Clip Mask", [&]() {
                if ( DrawContentsWidget( "afClipMask", af->clipMask, "Collision clip mask" ) ) {
                    changed = true;
                }
                ImGui::SameLine();
                if ( ImGui::Button( "Apply##clipMask" ) ) {
                    for ( int i = 0; i < af->bodies.Num(); i++ ) {
                        af->bodies[i]->clipMask = af->clipMask;
                    }
                    changed = true;
                }
                ImGui::AddTooltip( "Apply this value to all bodies" );
            });
        });
    }

    if ( ImGui::CollapsingHeader( "Default Friction" ) ) {
        ImGui::PropertyTable( "FrictionProps", propertyTableWidth, [&]() {
            ImGui::LabeledWidget( "Linear", [&]() {
                if ( ImGui::SliderFloat( "##afLinearFriction", &af->defaultLinearFriction, 0.0f, 1.0f ) ) {
                    changed = true;
                }
                ImGui::SameLine();
                if ( ImGui::Button( "Apply##linear" ) ) {
                    for ( int i = 0; i < af->bodies.Num(); i++ ) {
                        af->bodies[i]->linearFriction = af->defaultLinearFriction;
                    }
                    changed = true;
                }
                ImGui::AddTooltip( "Apply this value to all bodies" );
            });
            ImGui::LabeledWidget( "Angular", [&]() {
                if ( ImGui::SliderFloat( "##afAngularFriction", &af->defaultAngularFriction, 0.0f, 1.0f ) ) {
                    changed = true;
                }
                ImGui::SameLine();
                if ( ImGui::Button( "Apply##angular" ) ) {
                    for ( int i = 0; i < af->bodies.Num(); i++ ) {
                        af->bodies[i]->angularFriction = af->defaultAngularFriction;
                    }
                    changed = true;
                }
                ImGui::AddTooltip( "Apply this value to all bodies" );
            });
            ImGui::LabeledWidget( "Contact", [&]() {
                if ( ImGui::SliderFloat( "##afContactFriction", &af->defaultContactFriction, 0.0f, 1.0f ) ) {
                    changed = true;
                }
                ImGui::SameLine();
                if ( ImGui::Button( "Apply##contact" ) ) {
                    for ( int i = 0; i < af->bodies.Num(); i++ ) {
                        af->bodies[i]->contactFriction = af->defaultContactFriction;
                    }
                    changed = true;
                }
                ImGui::AddTooltip( "Apply this value to all bodies" );
            });
            ImGui::LabeledWidget( "Constraint", [&]() {
                if ( ImGui::SliderFloat( "##constraintFriction", &af->defaultConstraintFriction, 0.0f, 1.0f ) ) {
                    changed = true;
                }
                ImGui::SameLine();
                if ( ImGui::Button( "Apply##constraint" ) ) {
                    for ( int i = 0; i < af->constraints.Num(); i++ ) {
                        af->constraints[i]->friction = af->defaultConstraintFriction;
                    }
                    changed = true;
                }
                ImGui::AddTooltip( "Apply this value to all constraints" );
            });
        });
    }

    if ( ImGui::CollapsingHeader( "Mass" ) ) {
        ImGui::PropertyTable( "MassProps", propertyTableWidth, [&]() {
            ImGui::LabeledWidget( "Total Mass", [&]() {
                if ( ImGui::DragFloat( "##totalMass", &af->totalMass, 1.0f, -1.0f, 10000.0f, "%.1f" ) ) {
                    changed = true;
                }
            });
            ImGui::AddTooltip( "Total mass of the AF (-1 for auto-calculated from body densities)" );
        });
    }

    if ( ImGui::CollapsingHeader( "Suspend Settings" ) ) {
        ImGui::PropertyTable( "SuspendProps", propertyTableWidth, [&]() {
            ImGui::LabeledWidget( "Linear Vel", [&]() {
                if ( ImGui::DragFloat( "##suspendLinVel", &af->suspendVelocity.x, 1.0f, 0.0f, 1000.0f, "%.1f" ) ) {
                    changed = true;
                }
            });
            ImGui::AddTooltip( "Max linear velocity for suspension" );
            ImGui::LabeledWidget( "Angular Vel", [&]() {
                if ( ImGui::DragFloat( "##suspendAngVel", &af->suspendVelocity.y, 1.0f, 0.0f, 1000.0f, "%.1f" ) ) {
                    changed = true;
                }
            });
            ImGui::AddTooltip( "Max angular velocity for suspension" );
            ImGui::LabeledWidget( "Linear Acc", [&]() {
                if ( ImGui::DragFloat( "##suspendLinAcc", &af->suspendAcceleration.x, 1.0f, 0.0f, 10000.0f, "%.1f" ) ) {
                    changed = true;
                }
            });
            ImGui::AddTooltip( "Max linear acceleration for suspension" );
            ImGui::LabeledWidget( "Angular Acc", [&]() {
                if ( ImGui::DragFloat( "##suspendAngAcc", &af->suspendAcceleration.y, 1.0f, 0.0f, 10000.0f, "%.1f" ) ) {
                    changed = true;
                }
            });
            ImGui::AddTooltip( "Max angular acceleration for suspension" );
            ImGui::LabeledWidget( "No Move Time", [&]() {
                if ( ImGui::DragFloat( "##noMoveTime", &af->noMoveTime, 0.01f, 0.0f, 10.0f, "%.2f sec" ) ) {
                    changed = true;
                }
            });
            ImGui::AddTooltip( "Time with no movement before suspending simulation" );
            ImGui::LabeledWidget( "Linear Tolerance", [&]() {
                if ( ImGui::DragFloat( "##noMoveTranslation", &af->noMoveTranslation, 0.1f, 0.0f, 100.0f, "%.1f" ) ) {
                    changed = true;
                }
            });
            ImGui::AddTooltip( "Maximum translation considered no movement" );
            ImGui::LabeledWidget( "Angular Tolerance", [&]() {
                if ( ImGui::DragFloat( "##noMoveRotation", &af->noMoveRotation, 0.1f, 0.0f, 100.0f, "%.1f" ) ) {
                    changed = true;
                }
            });
            ImGui::AddTooltip( "Maximum rotation considered no movement" );
            ImGui::LabeledWidget( "Min Move Time", [&]() {
                if ( ImGui::DragFloat( "##minMoveTime", &af->minMoveTime, 0.01f, 0.0f, 10.0f, "%.2f sec" ) ) {
                    changed = true;
                }
            });
            ImGui::AddTooltip( "Minimum time AF must be simulated" );
            ImGui::LabeledWidget( "Max Move Time", [&]() {
                if ( ImGui::DragFloat( "##maxMoveTime", &af->maxMoveTime, 0.01f, 0.0f, 60.0f, "%.2f sec" ) ) {
                    changed = true;
                }
            });
            ImGui::AddTooltip( "Maximum time AF will be simulated before going to rest" );
        });
    }

    if ( changed ) {
        af->modified = true;
        fileModified = true;
        UpdateTestEntity();
    }
}

void hcAFEditor::DrawBodyList( void ) {
    idDeclAF* af = GetCurrentAF();
    if ( !af ) {
        return;
    }

    ImGui::Text( "Bodies" );
    float bw = ImGui::CalcButtonWidth( "New##body" ) + ImGui::CalcButtonWidth( "Rename##body" ) + ImGui::CalcButtonWidth( "Delete##body" ) + ImGui::GetStyle().ItemSpacing.x * 2;
    ImGui::SameLineRight( bw );

    // Disable New button if we have at least as many bodies as joints
    // (can't have more bodies than joints)
    bool canAddBody = (jointNames.Num() > af->bodies.Num());
    if ( !canAddBody ) {
        ImGui::BeginDisabled();
    }
    if ( ImGui::Button( "New##body" ) ) {
        OpenNewBodyDialog();
    }
    if ( !canAddBody ) {
        ImGui::EndDisabled();
        ImGui::AddTooltip( "Cannot add more bodies than joints in the model" );
    }
    ImGui::SameLine();

    // Disable Rename/Delete if no body selected
    bool hasSelection = (currentBodyIndex >= 0 && currentBodyIndex < af->bodies.Num());
    if ( !hasSelection ) {
        ImGui::BeginDisabled();
    }
    if ( ImGui::Button( "Rename##body" ) ) {
        OpenRenameDialog( RENAME_BODY );
    }
    ImGui::SameLine();
    if ( ImGui::Button( "Delete##body" ) ) {
        OpenDeleteConfirmDialog( DELETE_BODY );
    }
    if ( !hasSelection ) {
        ImGui::EndDisabled();
    }

    ImGui::BeginChild( "BodyList", ImVec2(0, 120), ImGuiChildFlags_Borders );
    for ( int i = 0; i < af->bodies.Num(); i++ ) {
        bool isSelected = (currentBodyIndex == i);
        if ( ImGui::Selectable( af->bodies[i]->name.c_str(), isSelected ) ) {
            currentBodyIndex = i;
            // Highlight the selected body in the game view
            cvarSystem->SetCVarString( "af_highlightBody", af->bodies[i]->name.c_str() );
        }
    }
    ImGui::EndChild();
}

void hcAFEditor::DrawBodyJointSection( idDeclAF_Body* body ) {
    bool changed = false;
    idDeclAF* af = GetCurrentAF();

    // Check if this body can change its joint - there must always be at least one body
    // that modifies the "origin" joint
    bool canChangeJoint = true;
    if ( body->jointName.Icmp( "origin" ) == 0 && af ) {
        // This body modifies origin - check if any OTHER body also modifies origin
        bool otherOriginExists = false;
        for ( int i = 0; i < af->bodies.Num(); i++ ) {
            if ( af->bodies[i] != body && af->bodies[i]->jointName.Icmp( "origin" ) == 0 ) {
                otherOriginExists = true;
                break;
            }
        }
        // If no other body modifies origin, this one can't change
        canChangeJoint = otherOriginExists;
    }

    if ( ImGui::CollapsingHeader( "Joint", ImGuiTreeNodeFlags_DefaultOpen ) ) {
        ImGui::PropertyTable( "BodyJointProps", propertyTableWidth, [&]() {
            ImGui::LabeledWidget( "Joint", [&]() {
                if ( !canChangeJoint ) {
                    ImGui::BeginDisabled();
                }
                if ( DrawJointCombo( "bodyJoint", body->jointName, "The skeleton joint this body modifies" ) ) {
                    changed = true;
                }
                if ( !canChangeJoint ) {
                    ImGui::EndDisabled();
                    ImGui::AddTooltip( "Cannot change: at least one body must modify the origin joint" );
                }
            });

            ImGui::LabeledWidget( "Modify", [&]() {
                int jointMod = (int)body->jointMod;
                if ( ImGui::Combo( "##jointMod", &jointMod, jointModNames, 3 ) ) {
                    body->jointMod = (declAFJointMod_t)jointMod;
                    changed = true;
                }
            });
            ImGui::AddTooltip( "How this body affects the joint: orientation, position, or both" );

            ImGui::LabeledWidget( "Contained Joints", [&]() {
                char buffer[256];
                idStr::Copynz( buffer, body->containedJoints.c_str(), sizeof(buffer) );
                if ( ImGui::InputText( "##containedJoints", buffer, sizeof(buffer) ) ) {
                    body->containedJoints = buffer;
                    changed = true;
                }
            });
            ImGui::AddTooltip( "Wildcard pattern for joints contained by this body (e.g., *origin, Lupperarm)" );
        });
    }

    if ( changed ) {
        idDeclAF* af = GetCurrentAF();
        if ( af ) {
            af->modified = true;
            fileModified = true;
            UpdateTestEntity();
        }
    }
}

void hcAFEditor::DrawBodyModelSection( idDeclAF_Body* body ) {
    bool changed = false;

    if ( ImGui::CollapsingHeader( "Collision Model", ImGuiTreeNodeFlags_DefaultOpen ) ) {
        ImGui::PropertyTable( "BodyModelProps", propertyTableWidth, [&]() {
            ImGui::LabeledWidget( "Type", [&]() {
                int modelType = body->modelType - TRM_BOX;
                if ( modelType < 0 ) modelType = 0;
                if ( modelType > 5 ) modelType = 5;
                int oldModelType = modelType;
                if ( ImGui::Combo( "##modelType", &modelType, modelTypeNames, 6 ) ) {
                    body->modelType = modelType + TRM_BOX;
                    // When changing model type, ensure valid minimum dimensions
                    // to avoid invalid mass calculation
                    if ( oldModelType != modelType ) {
                        idVec3 vec1 = body->v1.ToVec3();
                        idVec3 vec2 = body->v2.ToVec3();
                        idVec3 size = vec2 - vec1;
                        // Ensure minimum size of 1.0 in each dimension
                        bool needsFixup = false;
                        if ( size.x < 1.0f ) { size.x = 1.0f; needsFixup = true; }
                        if ( size.y < 1.0f ) { size.y = 1.0f; needsFixup = true; }
                        if ( size.z < 1.0f ) { size.z = 1.0f; needsFixup = true; }
                        if ( needsFixup && body->v1.type == idAFVector::VEC_COORDS ) {
                            body->v2.ToVec3() = body->v1.ToVec3() + size;
                        }
                        // For bone type, ensure width is valid and origin is bone center
                        if ( body->modelType == TRM_BONE ) {
                            if ( body->width < 1.0f ) {
                                body->width = 1.0f;
                            }
                            // Bone type always uses bone center for origin
                            body->origin.type = idAFVector::VEC_BONECENTER;
                            // Zero out angles since bone orientation is determined by joints
                            body->angles.Zero();
                        }
                    }
                    changed = true;
                }
            });

            // Type-specific properties
            switch ( body->modelType ) {
                case TRM_BOX:
                case TRM_OCTAHEDRON:
                case TRM_DODECAHEDRON:
                    if ( DrawAFVectorWidget( "Min", body->v1, "Minimum corner of bounding box" ) ) changed = true;
                    if ( DrawAFVectorWidget( "Max", body->v2, "Maximum corner of bounding box" ) ) changed = true;
                    break;

                case TRM_CYLINDER:
                case TRM_CONE:
                    if ( DrawAFVectorWidget( "Start", body->v1, "Start point of cylinder/cone axis" ) ) changed = true;
                    if ( DrawAFVectorWidget( "End", body->v2, "End point of cylinder/cone axis" ) ) changed = true;
                    ImGui::LabeledWidget( "Sides", [&]() {
                        if ( ImGui::SliderInt( "##numSides", &body->numSides, 3, 10 ) ) {
                            // Clamp to valid range (3-10 per MFC editor)
                            if ( body->numSides < 3 ) body->numSides = 3;
                            if ( body->numSides > 10 ) body->numSides = 10;
                            changed = true;
                        }
                    });
                    ImGui::AddTooltip( "Number of sides for the cylinder/cone (3-10)" );
                    break;

                case TRM_BONE:
                    if ( DrawAFVectorWidget( "Start", body->v1, "Start joint of bone" ) ) changed = true;
                    if ( DrawAFVectorWidget( "End", body->v2, "End joint of bone" ) ) changed = true;
                    ImGui::LabeledWidget( "Width", [&]() {
                        if ( ImGui::DragFloat( "##boneWidth", &body->width, 0.1f, 1.0f, 100.0f, "%.1f" ) ) {
                            // Clamp to minimum of 1.0 per MFC editor
                            if ( body->width < 1.0f ) body->width = 1.0f;
                            changed = true;
                        }
                    });
                    ImGui::AddTooltip( "Width of the bone collision model (minimum 1.0)" );
                    break;
            }
        });
    }

    if ( changed ) {
        idDeclAF* af = GetCurrentAF();
        if ( af ) {
            af->modified = true;
            fileModified = true;
            UpdateTestEntity();
        }
    }
}

void hcAFEditor::DrawBodyOriginSection( idDeclAF_Body* body ) {
    bool changed = false;

    // Bone type uses automatic origin from bone joints - disable manual controls
    bool isBoneType = (body->modelType == TRM_BONE);

    if ( ImGui::CollapsingHeader( "Origin & Orientation" ) ) {
        if ( isBoneType ) {
            ImGui::BeginDisabled();
            ImGui::TextDisabled( "Origin and angles are determined by bone joints" );
        }
        ImGui::PropertyTable( "BodyOriginProps", propertyTableWidth, [&]() {
            if ( DrawAFVectorWidget( "Origin", body->origin, "Position of the collision model relative to the joint" ) ) {
                changed = true;
            }

            ImGui::LabeledWidget( "Angles", [&]() {
                float angles[3] = { body->angles.pitch, body->angles.yaw, body->angles.roll };
                if ( ImGui::DragFloat3( "##bodyAngles", angles, 1.0f, -180.0f, 180.0f, "%.1f°" ) ) {
                    body->angles.pitch = angles[0];
                    body->angles.yaw = angles[1];
                    body->angles.roll = angles[2];
                    changed = true;
                }
            });
            ImGui::AddTooltip( "Rotation of collision model (pitch, yaw, roll)" );
        });
        if ( isBoneType ) {
            ImGui::EndDisabled();
        }
    }

    if ( changed ) {
        idDeclAF* af = GetCurrentAF();
        if ( af ) {
            af->modified = true;
            fileModified = true;
            UpdateTestEntity();
        }
    }
}

void hcAFEditor::DrawBodyPhysicsSection( idDeclAF_Body* body ) {
    bool changed = false;

    if ( ImGui::CollapsingHeader( "Physics Properties" ) ) {
        ImGui::PropertyTable( "BodyPhysicsProps", propertyTableWidth, [&]() {
            ImGui::LabeledWidget( "Density", [&]() {
                if ( ImGui::DragFloat( "##bodyDensity", &body->density, 0.001f, 0.000001f, 10.0f, "%.6f" ) ) {
                    // Clamp to minimum of 1e-6 per MFC editor
                    if ( body->density < 1e-6f ) body->density = 1e-6f;
                    changed = true;
                }
            });
            ImGui::AddTooltip( "Mass density of the body (mass = density × volume, minimum 1e-6)" );

            // Inertia Scale - displayed as "none" for identity or 9 float values
            ImGui::LabeledWidget( "Inertia Scale", [&]() {
                static char inertiaBuffer[256];
                if ( body->inertiaScale == mat3_identity ) {
                    idStr::Copynz( inertiaBuffer, "none", sizeof(inertiaBuffer) );
                } else {
                    idStr::snPrintf( inertiaBuffer, sizeof(inertiaBuffer), "%.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f",
                        body->inertiaScale[0][0], body->inertiaScale[0][1], body->inertiaScale[0][2],
                        body->inertiaScale[1][0], body->inertiaScale[1][1], body->inertiaScale[1][2],
                        body->inertiaScale[2][0], body->inertiaScale[2][1], body->inertiaScale[2][2] );
                }
                if ( ImGui::InputText( "##inertiaScale", inertiaBuffer, sizeof(inertiaBuffer), ImGuiInputTextFlags_EnterReturnsTrue ) ) {
                    if ( idStr::Icmp( inertiaBuffer, "none" ) == 0 ) {
                        body->inertiaScale.Identity();
                    } else {
                        float vals[9];
                        if ( sscanf( inertiaBuffer, "%f %f %f %f %f %f %f %f %f",
                            &vals[0], &vals[1], &vals[2], &vals[3], &vals[4], &vals[5], &vals[6], &vals[7], &vals[8] ) == 9 ) {
                            body->inertiaScale[0][0] = vals[0]; body->inertiaScale[0][1] = vals[1]; body->inertiaScale[0][2] = vals[2];
                            body->inertiaScale[1][0] = vals[3]; body->inertiaScale[1][1] = vals[4]; body->inertiaScale[1][2] = vals[5];
                            body->inertiaScale[2][0] = vals[6]; body->inertiaScale[2][1] = vals[7]; body->inertiaScale[2][2] = vals[8];
                        }
                    }
                    changed = true;
                }
            });
            ImGui::AddTooltip( "Inertia tensor scale matrix (enter 'none' for identity, or 9 space-separated values)" );

            ImGui::LabeledWidget( "Self Collision", [&]() {
                if ( ImGui::Checkbox( "##bodySelfCollision", &body->selfCollision ) ) {
                    changed = true;
                }
            });

            ImGui::LabeledWidget( "Contents", [&]() {
                if ( DrawContentsWidget( "bodyContents", body->contents, "Body contents flags" ) ) {
                    changed = true;
                }
            });
            ImGui::LabeledWidget( "Clip Mask", [&]() {
                if ( DrawContentsWidget( "bodyClipMask", body->clipMask, "Collision clip mask" ) ) {
                    changed = true;
                }
            });
        });
    }

    if ( ImGui::CollapsingHeader( "Friction" ) ) {
        ImGui::PropertyTable( "BodyFrictionProps", propertyTableWidth, [&]() {
            ImGui::LabeledWidget( "Linear", [&]() {
                if ( ImGui::SliderFloat( "##bodyLinearFriction", &body->linearFriction, -1.0f, 1.0f ) ) {
                    changed = true;
                }
            });
            ImGui::AddTooltip( "-1 uses default friction" );
            ImGui::LabeledWidget( "Angular", [&]() {
                if ( ImGui::SliderFloat( "##bodyAngularFriction", &body->angularFriction, -1.0f, 1.0f ) ) {
                    changed = true;
                }
            });
            ImGui::LabeledWidget( "Contact", [&]() {
                if ( ImGui::SliderFloat( "##bodyContactFriction", &body->contactFriction, -1.0f, 1.0f ) ) {
                    changed = true;
                }
            });
            if ( DrawAFVectorWidget( "Friction Dir", body->frictionDirection, "Direction of anisotropic friction" ) ) {
                changed = true;
            }
            if ( DrawAFVectorWidget( "Motor Dir", body->contactMotorDirection, "Direction of contact motor force" ) ) {
                changed = true;
            }
        });
    }

    if ( changed ) {
        idDeclAF* af = GetCurrentAF();
        if ( af ) {
            af->modified = true;
            fileModified = true;
            UpdateTestEntity();
        }
    }
}

void hcAFEditor::DrawBodiesTab( void ) {
    idDeclAF* af = GetCurrentAF();
    if ( !af ) {
        ImGui::TextDisabled( "No AF selected" );
        return;
    }

    DrawBodyList();

    ImGui::Separator();

    idDeclAF_Body* body = GetCurrentBody();
    if ( !body ) {
        ImGui::TextDisabled( "No body selected" );
        return;
    }

    ImGui::Text( "Editing: %s", body->name.c_str() );

    DrawBodyJointSection( body );
    DrawBodyModelSection( body );
    DrawBodyOriginSection( body );
    DrawBodyPhysicsSection( body );
}

void hcAFEditor::DrawConstraintList( void ) {
    idDeclAF* af = GetCurrentAF();
    if ( !af ) {
        return;
    }

    ImGui::Text( "Constraints" );
    float cw = ImGui::CalcButtonWidth( "New##constraint" ) + ImGui::CalcButtonWidth( "Rename##constraint" ) + ImGui::CalcButtonWidth( "Delete##constraint" ) + ImGui::GetStyle().ItemSpacing.x * 2;
    ImGui::SameLineRight( cw );

    // Disable New button if there are no bodies (need at least one body for a constraint)
    bool canAddConstraint = (af->bodies.Num() >= 1);
    if ( !canAddConstraint ) {
        ImGui::BeginDisabled();
    }
    if ( ImGui::Button( "New##constraint" ) ) {
        OpenNewConstraintDialog();
    }
    if ( !canAddConstraint ) {
        ImGui::EndDisabled();
        ImGui::AddTooltip( "Cannot add constraints without at least one body" );
    }
    ImGui::SameLine();

    // Disable Rename/Delete if no constraint selected
    bool hasSelection = (currentConstraintIndex >= 0 && currentConstraintIndex < af->constraints.Num());
    if ( !hasSelection ) {
        ImGui::BeginDisabled();
    }
    if ( ImGui::Button( "Rename##constraint" ) ) {
        OpenRenameDialog( RENAME_CONSTRAINT );
    }
    ImGui::SameLine();
    if ( ImGui::Button( "Delete##constraint" ) ) {
        OpenDeleteConfirmDialog( DELETE_CONSTRAINT );
    }
    if ( !hasSelection ) {
        ImGui::EndDisabled();
    }

    ImGui::BeginChild( "ConstraintList", ImVec2(0, 120), ImGuiChildFlags_Borders );
    for ( int i = 0; i < af->constraints.Num(); i++ ) {
        bool isSelected = (currentConstraintIndex == i);
        if ( ImGui::Selectable( af->constraints[i]->name.c_str(), isSelected ) ) {
            currentConstraintIndex = i;
            // Highlight the selected constraint in the game view
            cvarSystem->SetCVarString( "af_highlightConstraint", af->constraints[i]->name.c_str() );
        }
    }
    ImGui::EndChild();
}

void hcAFEditor::DrawConstraintGeneralSection( idDeclAF_Constraint* constraint ) {
    bool changed = false;
    idDeclAF* af = GetCurrentAF();

    if ( ImGui::CollapsingHeader( "General", ImGuiTreeNodeFlags_DefaultOpen ) ) {
        ImGui::PropertyTable( "ConstraintGeneralProps", propertyTableWidth, [&]() {
            ImGui::LabeledWidget( "Type", [&]() {
                int typeIndex = (int)constraint->type - 1;
                if ( typeIndex < 0 ) typeIndex = 0;
                if ( typeIndex > 5 ) typeIndex = 5;
                if ( ImGui::Combo( "##constraintType", &typeIndex, constraintTypeNames, 6 ) ) {
                    constraint->type = (declAFConstraintType_t)(typeIndex + 1);
                    changed = true;
                }
            });

            ImGui::LabeledWidget( "Body 1", [&]() {
                if ( DrawBodyCombo( "constraintBody1", constraint->body1, "First body connected by this constraint" ) ) {
                    changed = true;
                }
            });
            ImGui::LabeledWidget( "Body 2", [&]() {
                if ( DrawBodyCombo( "constraintBody2", constraint->body2, "Second body connected by this constraint (can be 'world')" ) ) {
                    changed = true;
                }
            });

            ImGui::LabeledWidget( "Friction", [&]() {
                if ( ImGui::SliderFloat( "##friction", &constraint->friction, 0.0f, 1.0f ) ) {
                    changed = true;
                }
            });
        });
    }

    if ( changed && af ) {
        af->modified = true;
        fileModified = true;
        UpdateTestEntity();
    }
}

void hcAFEditor::DrawFixedConstraint( idDeclAF_Constraint* constraint ) {
    // Fixed constraints have no additional properties
    ImGui::TextDisabled( "Fixed constraints rigidly connect two bodies with no degrees of freedom." );
}

void hcAFEditor::DrawBallAndSocketConstraint( idDeclAF_Constraint* constraint ) {
    bool changed = false;
    idDeclAF* af = GetCurrentAF();

    if ( ImGui::CollapsingHeader( "Ball and Socket Properties", ImGuiTreeNodeFlags_DefaultOpen ) ) {
        ImGui::PropertyTable( "BallSocketProps", propertyTableWidth, [&]() {
            if ( DrawAFVectorWidget( "Anchor", constraint->anchor, "Pivot point of the ball joint" ) ) {
                changed = true;
            }
        });
    }

    if ( ImGui::CollapsingHeader( "Limit###BallSocketLimit" ) ) {
        ImGui::PropertyTable( "BallSocketLimitProps", propertyTableWidth, [&]() {
            ImGui::LabeledWidget( "Limit Type", [&]() {
                int limitType = constraint->limit + 1;
                if ( ImGui::Combo( "##limitType", &limitType, limitTypeNames, 3 ) ) {
                    constraint->limit = (decltype(constraint->limit))(limitType - 1);
                    changed = true;
                }
            });

            if ( constraint->limit == idDeclAF_Constraint::LIMIT_CONE ) {
                if ( DrawAFVectorWidget( "Limit Axis", constraint->limitAxis, "Axis of the cone limit" ) ) {
                    changed = true;
                }
                ImGui::LabeledWidget( "Cone Angle", [&]() {
                    if ( ImGui::SliderFloat( "##coneAngle", &constraint->limitAngles[0], 0.0f, 180.0f ) ) {
                        changed = true;
                    }
                });
                if ( DrawAFVectorWidget( "Shaft", constraint->shaft[0], "Shaft that must stay within the cone" ) ) {
                    changed = true;
                }
            } else if ( constraint->limit == idDeclAF_Constraint::LIMIT_PYRAMID ) {
                if ( DrawAFVectorWidget( "Limit Axis", constraint->limitAxis, "Axis of the pyramid limit" ) ) {
                    changed = true;
                }
                ImGui::LabeledWidget( "Angle 1", [&]() {
                    if ( ImGui::SliderFloat( "##pyramidAngle1", &constraint->limitAngles[0], 0.0f, 180.0f ) ) {
                        changed = true;
                    }
                });
                ImGui::LabeledWidget( "Angle 2", [&]() {
                    if ( ImGui::SliderFloat( "##pyramidAngle2", &constraint->limitAngles[1], 0.0f, 180.0f ) ) {
                        changed = true;
                    }
                });
                ImGui::LabeledWidget( "Roll", [&]() {
                    if ( ImGui::SliderFloat( "##pyramidRoll", &constraint->limitAngles[2], -180.0f, 180.0f ) ) {
                        changed = true;
                    }
                });
                if ( DrawAFVectorWidget( "Shaft", constraint->shaft[0], "Shaft that must stay within the pyramid" ) ) {
                    changed = true;
                }
            }
        });
    }

    if ( changed && af ) {
        af->modified = true;
        fileModified = true;
        UpdateTestEntity();
    }
}

void hcAFEditor::DrawUniversalConstraint( idDeclAF_Constraint* constraint ) {
    bool changed = false;
    idDeclAF* af = GetCurrentAF();

    if ( ImGui::CollapsingHeader( "Universal Joint Properties", ImGuiTreeNodeFlags_DefaultOpen ) ) {
        ImGui::PropertyTable( "UniversalProps", propertyTableWidth, [&]() {
            if ( DrawAFVectorWidget( "Anchor", constraint->anchor, "Pivot point of the universal joint" ) ) {
                changed = true;
            }
            if ( DrawAFVectorWidget( "Shaft 1", constraint->shaft[0], "First shaft (attached to body1)" ) ) {
                changed = true;
            }
            if ( DrawAFVectorWidget( "Shaft 2", constraint->shaft[1], "Second shaft (attached to body2)" ) ) {
                changed = true;
            }
        });
    }

    if ( ImGui::CollapsingHeader( "Limit###UniversalLimit" ) ) {
        ImGui::PropertyTable( "UniversalLimitProps", propertyTableWidth, [&]() {
            ImGui::LabeledWidget( "Limit Type", [&]() {
                int limitType = constraint->limit + 1;
                if ( ImGui::Combo( "##limitType", &limitType, limitTypeNames, 3 ) ) {
                    constraint->limit = (decltype(constraint->limit))(limitType - 1);
                    changed = true;
                }
            });

            if ( constraint->limit == idDeclAF_Constraint::LIMIT_CONE ) {
                if ( DrawAFVectorWidget( "Limit Axis", constraint->limitAxis, "Axis of the cone limit" ) ) {
                    changed = true;
                }
                ImGui::LabeledWidget( "Cone Angle", [&]() {
                    if ( ImGui::SliderFloat( "##coneAngle", &constraint->limitAngles[0], 0.0f, 180.0f ) ) {
                        changed = true;
                    }
                });
            } else if ( constraint->limit == idDeclAF_Constraint::LIMIT_PYRAMID ) {
                if ( DrawAFVectorWidget( "Limit Axis", constraint->limitAxis, "Axis of the pyramid limit" ) ) {
                    changed = true;
                }
                ImGui::LabeledWidget( "Angle 1", [&]() {
                    if ( ImGui::SliderFloat( "##pyramidAngle1", &constraint->limitAngles[0], 0.0f, 180.0f ) ) {
                        changed = true;
                    }
                });
                ImGui::LabeledWidget( "Angle 2", [&]() {
                    if ( ImGui::SliderFloat( "##pyramidAngle2", &constraint->limitAngles[1], 0.0f, 180.0f ) ) {
                        changed = true;
                    }
                });
                ImGui::LabeledWidget( "Roll", [&]() {
                    if ( ImGui::SliderFloat( "##pyramidRoll", &constraint->limitAngles[2], -180.0f, 180.0f ) ) {
                        changed = true;
                    }
                });
            }
        });
    }

    if ( changed && af ) {
        af->modified = true;
        fileModified = true;
        UpdateTestEntity();
    }
}

void hcAFEditor::DrawHingeConstraint( idDeclAF_Constraint* constraint ) {
    bool changed = false;
    idDeclAF* af = GetCurrentAF();

    if ( ImGui::CollapsingHeader( "Hinge Properties", ImGuiTreeNodeFlags_DefaultOpen ) ) {
        ImGui::PropertyTable( "HingeProps", propertyTableWidth, [&]() {
            if ( DrawAFVectorWidget( "Anchor", constraint->anchor, "Pivot point of the hinge" ) ) {
                changed = true;
            }
            if ( DrawAFVectorWidget( "Axis", constraint->axis, "Hinge rotation axis" ) ) {
                changed = true;
            }
        });
    }

    if ( ImGui::CollapsingHeader( "Limit###HingeLimit" ) ) {
        ImGui::PropertyTable( "HingeLimitProps", propertyTableWidth, [&]() {
            bool hasLimit = (constraint->limit == idDeclAF_Constraint::LIMIT_CONE);
            if ( ImGui::Checkbox( "Enable Limit##hinge", &hasLimit ) ) {
                constraint->limit = hasLimit ? idDeclAF_Constraint::LIMIT_CONE : idDeclAF_Constraint::LIMIT_NONE;
                changed = true;
            }

            if ( hasLimit ) {
                ImGui::LabeledWidget( "Center Angle", [&]() {
                    if ( ImGui::SliderFloat( "##centerAngle", &constraint->limitAngles[0], -180.0f, 180.0f ) ) {
                        changed = true;
                    }
                });
                ImGui::AddTooltip( "Center of the V-shaped limit" );
                ImGui::LabeledWidget( "Range", [&]() {
                    if ( ImGui::SliderFloat( "##range", &constraint->limitAngles[1], 0.0f, 180.0f ) ) {
                        changed = true;
                    }
                });
                ImGui::AddTooltip( "Half-width of the V-shaped limit" );
                ImGui::LabeledWidget( "Shaft Angle", [&]() {
                    if ( ImGui::SliderFloat( "##shaftAngle", &constraint->limitAngles[2], -180.0f, 180.0f ) ) {
                        changed = true;
                    }
                });
                ImGui::AddTooltip( "Orientation of the shaft relative to body1" );
            }
        });
    }

    if ( changed && af ) {
        af->modified = true;
        fileModified = true;
        UpdateTestEntity();
    }
}

void hcAFEditor::DrawSliderConstraint( idDeclAF_Constraint* constraint ) {
    bool changed = false;
    idDeclAF* af = GetCurrentAF();

    if ( ImGui::CollapsingHeader( "Slider Properties", ImGuiTreeNodeFlags_DefaultOpen ) ) {
        ImGui::PropertyTable( "SliderProps", propertyTableWidth, [&]() {
            if ( DrawAFVectorWidget( "Axis", constraint->axis, "Slider translation axis" ) ) {
                changed = true;
            }
        });
    }

    if ( changed && af ) {
        af->modified = true;
        fileModified = true;
        UpdateTestEntity();
    }
}

void hcAFEditor::DrawSpringConstraint( idDeclAF_Constraint* constraint ) {
    bool changed = false;
    idDeclAF* af = GetCurrentAF();

    if ( ImGui::CollapsingHeader( "Spring Properties", ImGuiTreeNodeFlags_DefaultOpen ) ) {
        ImGui::PropertyTable( "SpringProps", propertyTableWidth, [&]() {
            if ( DrawAFVectorWidget( "Anchor 1", constraint->anchor, "Attachment point on body1" ) ) {
                changed = true;
            }
            if ( DrawAFVectorWidget( "Anchor 2", constraint->anchor2, "Attachment point on body2" ) ) {
                changed = true;
            }

            ImGui::LabeledWidget( "Stretch", [&]() {
                if ( ImGui::DragFloat( "##springStretch", &constraint->stretch, 0.01f, 0.0f, 100.0f, "%.2f" ) ) {
                    changed = true;
                }
            });
            ImGui::AddTooltip( "Spring constant when stretched (higher = stiffer)" );

            ImGui::LabeledWidget( "Compress", [&]() {
                if ( ImGui::DragFloat( "##springCompress", &constraint->compress, 0.01f, 0.0f, 100.0f, "%.2f" ) ) {
                    changed = true;
                }
            });
            ImGui::AddTooltip( "Spring constant when compressed (higher = stiffer)" );

            ImGui::LabeledWidget( "Damping", [&]() {
                if ( ImGui::DragFloat( "##springDamping", &constraint->damping, 0.001f, 0.0f, 1.0f, "%.3f" ) ) {
                    changed = true;
                }
            });
            ImGui::AddTooltip( "Velocity damping (0 = no damping, 1 = critical damping)" );

            ImGui::LabeledWidget( "Rest Length", [&]() {
                if ( ImGui::DragFloat( "##springRestLen", &constraint->restLength, 0.5f, 0.0f, 1000.0f, "%.1f" ) ) {
                    changed = true;
                }
            });
            ImGui::AddTooltip( "Natural length of the spring when no force applied" );

            ImGui::LabeledWidget( "Min Length", [&]() {
                if ( ImGui::DragFloat( "##springMinLen", &constraint->minLength, 0.5f, 0.0f, 1000.0f, "%.1f" ) ) {
                    changed = true;
                }
            });
            ImGui::AddTooltip( "Minimum allowed spring length" );

            ImGui::LabeledWidget( "Max Length", [&]() {
                if ( ImGui::DragFloat( "##springMaxLen", &constraint->maxLength, 0.5f, 0.0f, 1000.0f, "%.1f" ) ) {
                    changed = true;
                }
            });
            ImGui::AddTooltip( "Maximum allowed spring length" );
        });
    }

    if ( changed && af ) {
        af->modified = true;
        fileModified = true;
        UpdateTestEntity();
    }
}

void hcAFEditor::DrawConstraintTypeProperties( idDeclAF_Constraint* constraint ) {
    switch ( constraint->type ) {
        case DECLAF_CONSTRAINT_FIXED:
            DrawFixedConstraint( constraint );
            break;
        case DECLAF_CONSTRAINT_BALLANDSOCKETJOINT:
            DrawBallAndSocketConstraint( constraint );
            break;
        case DECLAF_CONSTRAINT_UNIVERSALJOINT:
            DrawUniversalConstraint( constraint );
            break;
        case DECLAF_CONSTRAINT_HINGE:
            DrawHingeConstraint( constraint );
            break;
        case DECLAF_CONSTRAINT_SLIDER:
            DrawSliderConstraint( constraint );
            break;
        case DECLAF_CONSTRAINT_SPRING:
            DrawSpringConstraint( constraint );
            break;
        default:
            ImGui::TextDisabled( "Unknown constraint type" );
            break;
    }
}

void hcAFEditor::DrawConstraintsTab( void ) {
    idDeclAF* af = GetCurrentAF();
    if ( !af ) {
        ImGui::TextDisabled( "No AF selected" );
        return;
    }

    DrawConstraintList();

    ImGui::Separator();

    idDeclAF_Constraint* constraint = GetCurrentConstraint();
    if ( !constraint ) {
        ImGui::TextDisabled( "No constraint selected" );
        return;
    }

    ImGui::Text( "Editing: %s", constraint->name.c_str() );

    DrawConstraintGeneralSection( constraint );
    DrawConstraintTypeProperties( constraint );
}

/*
================
hcAFEditor::DrawAFVectorWidget

Draws a vector input widget that supports different vector types:
- VEC_COORDS: XYZ coordinates
- VEC_JOINT: Single joint reference
- VEC_BONECENTER: Center between two joints
- VEC_BONEDIR: Direction from joint1 to joint2
================
*/
bool hcAFEditor::DrawAFVectorWidget( const char* label, idAFVector& vec, const char* tooltip ) {
    bool changed = false;

    ImGui::LabeledWidget( label, [&]() {
        ImGui::PushID( label );

        // Type selector
        const char* typeNames[] = { "Coords", "Joint", "Bone Center", "Bone Dir" };
        int typeIndex = vec.type;
        ImGui::SetNextItemWidth( 100 );
        if ( ImGui::Combo( "##vecType", &typeIndex, typeNames, 4 ) ) {
            vec.type = (decltype(vec.type))typeIndex;
            changed = true;
        }

        ImGui::SameLine();

        switch ( vec.type ) {
            case idAFVector::VEC_COORDS: {
                float v[3] = { vec.ToVec3().x, vec.ToVec3().y, vec.ToVec3().z };
                ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
                if ( ImGui::DragFloat3( "##vecCoords", v, 0.5f, -1000.0f, 1000.0f, "%.1f" ) ) {
                    vec.ToVec3().x = v[0];
                    vec.ToVec3().y = v[1];
                    vec.ToVec3().z = v[2];
                    changed = true;
                }
                break;
            }
            case idAFVector::VEC_JOINT: {
                ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
                if ( ImGui::BeginCombo( "##vecJoint1", vec.joint1.c_str() ) ) {
                    for ( int i = 0; i < jointNames.Num(); i++ ) {
                        bool isSelected = (vec.joint1.Icmp( jointNames[i] ) == 0);
                        if ( ImGui::Selectable( jointNames[i].c_str(), isSelected ) ) {
                            vec.joint1 = jointNames[i];
                            changed = true;
                        }
                        if ( isSelected ) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
                break;
            }
            case idAFVector::VEC_BONECENTER:
            case idAFVector::VEC_BONEDIR: {
                float halfWidth = (ImGui::GetContentRegionAvail().x - 5) * 0.5f;
                ImGui::SetNextItemWidth( halfWidth );
                if ( ImGui::BeginCombo( "##vecBoneJ1", vec.joint1.c_str() ) ) {
                    for ( int i = 0; i < jointNames.Num(); i++ ) {
                        bool isSelected = (vec.joint1.Icmp( jointNames[i] ) == 0);
                        if ( ImGui::Selectable( jointNames[i].c_str(), isSelected ) ) {
                            vec.joint1 = jointNames[i];
                            changed = true;
                        }
                        if ( isSelected ) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::SameLine();
                ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
                if ( ImGui::BeginCombo( "##vecBoneJ2", vec.joint2.c_str() ) ) {
                    for ( int i = 0; i < jointNames.Num(); i++ ) {
                        bool isSelected = (vec.joint2.Icmp( jointNames[i] ) == 0);
                        if ( ImGui::Selectable( jointNames[i].c_str(), isSelected ) ) {
                            vec.joint2 = jointNames[i];
                            changed = true;
                        }
                        if ( isSelected ) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
                break;
            }
        }

        ImGui::PopID();
    });

    if ( tooltip ) {
        ImGui::AddTooltip( tooltip );
    }

    return changed;
}

/*
================
hcAFEditor::DrawJointCombo

Draws a joint selection combo box. The label parameter is used to create a unique ID.
This function is meant to be called from within a LabeledWidget context.

When used for "Modified Joint" selection, joints already claimed by other bodies
should be filtered out (pass filterUsedByOtherBodies=true).
================
*/
bool hcAFEditor::DrawJointCombo( const char* label, idStr& jointName, const char* tooltip ) {
    bool changed = false;
    idDeclAF* af = GetCurrentAF();
    idDeclAF_Body* currentBody = GetCurrentBody();

    ImGui::PushID( label );
    ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
    if ( ImGui::BeginCombo( "##joint", jointName.c_str() ) ) {
        for ( int i = 0; i < jointNames.Num(); i++ ) {
            // Check if this joint is already used by another body
            bool usedByOther = false;
            if ( af && currentBody ) {
                for ( int j = 0; j < af->bodies.Num(); j++ ) {
                    if ( af->bodies[j] != currentBody &&
                         af->bodies[j]->jointName.Icmp( jointNames[i] ) == 0 ) {
                        usedByOther = true;
                        break;
                    }
                }
            }

            if ( usedByOther ) {
                // Skip joints used by other bodies - they can't be selected
                continue;
            }

            bool isSelected = (jointName.Icmp( jointNames[i] ) == 0);
            if ( ImGui::Selectable( jointNames[i].c_str(), isSelected ) ) {
                jointName = jointNames[i];
                changed = true;
            }
            if ( isSelected ) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopID();

    if ( tooltip ) {
        ImGui::AddTooltip( tooltip );
    }

    return changed;
}

/*
================
hcAFEditor::DrawBodyCombo

Draws a body selection combo box. The label parameter is used to create a unique ID.
This function is meant to be called from within a LabeledWidget context.
================
*/
bool hcAFEditor::DrawBodyCombo( const char* label, idStr& bodyName, const char* tooltip ) {
    bool changed = false;
    idDeclAF* af = GetCurrentAF();

    ImGui::PushID( label );
    ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
    if ( ImGui::BeginCombo( "##body", bodyName.c_str() ) ) {
        // Add "world" option
        bool isWorld = (bodyName.Icmp( "world" ) == 0);
        if ( ImGui::Selectable( "world", isWorld ) ) {
            bodyName = "world";
            changed = true;
        }

        if ( af ) {
            for ( int i = 0; i < af->bodies.Num(); i++ ) {
                bool isSelected = (bodyName.Icmp( af->bodies[i]->name ) == 0);
                if ( ImGui::Selectable( af->bodies[i]->name.c_str(), isSelected ) ) {
                    bodyName = af->bodies[i]->name;
                    changed = true;
                }
                if ( isSelected ) {
                    ImGui::SetItemDefaultFocus();
                }
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopID();

    if ( tooltip ) {
        ImGui::AddTooltip( tooltip );
    }

    return changed;
}

/*
================
hcAFEditor::DrawContentsWidget

Draws checkboxes for contents flags. The label parameter is used to create unique IDs.
This function is meant to be called from within a LabeledWidget context.
================
*/
bool hcAFEditor::DrawContentsWidget( const char* label, int& contents, const char* tooltip ) {
    bool changed = false;

    ImGui::PushID( label );

    bool solid = (contents & CONTENTS_SOLID) != 0;
    bool body = (contents & CONTENTS_BODY) != 0;
    bool corpse = (contents & CONTENTS_CORPSE) != 0;
    bool playerClip = (contents & CONTENTS_PLAYERCLIP) != 0;
    bool monsterClip = (contents & CONTENTS_MONSTERCLIP) != 0;

    if ( ImGui::Checkbox( "Solid", &solid ) ) {
        if ( solid ) contents |= CONTENTS_SOLID; else contents &= ~CONTENTS_SOLID;
        changed = true;
    }
    ImGui::SameLine();
    if ( ImGui::Checkbox( "Body", &body ) ) {
        if ( body ) contents |= CONTENTS_BODY; else contents &= ~CONTENTS_BODY;
        changed = true;
    }
    ImGui::SameLine();
    if ( ImGui::Checkbox( "Corpse", &corpse ) ) {
        if ( corpse ) contents |= CONTENTS_CORPSE; else contents &= ~CONTENTS_CORPSE;
        changed = true;
    }
    ImGui::SameLine();
    if ( ImGui::Checkbox( "PClip", &playerClip ) ) {
        if ( playerClip ) contents |= CONTENTS_PLAYERCLIP; else contents &= ~CONTENTS_PLAYERCLIP;
        changed = true;
    }
    ImGui::SameLine();
    if ( ImGui::Checkbox( "MClip", &monsterClip ) ) {
        if ( monsterClip ) contents |= CONTENTS_MONSTERCLIP; else contents &= ~CONTENTS_MONSTERCLIP;
        changed = true;
    }

    ImGui::PopID();

    if ( tooltip ) {
        ImGui::AddTooltip( tooltip );
    }

    return changed;
}

void hcAFEditor::DrawNewAFDialog( void ) {
    if ( !showNewAFDialog ) {
        return;
    }

    ImGui::OpenPopup( "New Articulated Figure###NewAFDialog" );

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos( center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f) );
    ImGui::SetNextWindowSize( ImVec2(400, 150), ImGuiCond_Appearing );

    if ( ImGui::BeginPopupModal( "New Articulated Figure###NewAFDialog", &showNewAFDialog ) ) {
        ImGui::Text( "Enter a name for the new Articulated Figure:" );
        ImGui::TextDisabled( "(alphanumeric and underscore only)" );

        ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
        ImGui::InputText( "##newAFName", newNameBuffer, sizeof(newNameBuffer),
            ImGuiInputTextFlags_CallbackCharFilter, ImGuiNameInputCallback );

        bool validName = (newNameBuffer[0] != '\0');
        bool isDuplicate = false;

        // Check for existing AF with same name
        if ( validName ) {
            for ( int i = 0; i < afNames.Num(); i++ ) {
                if ( afNames[i].Icmp( newNameBuffer ) == 0 ) {
                    isDuplicate = true;
                    validName = false;
                    break;
                }
            }
        }

        // Show error message
        if ( isDuplicate ) {
            ImGui::TextColored( ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Name '%s' is already used.", newNameBuffer );
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if ( ImGui::Button( "Cancel" ) ) {
            showNewAFDialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if ( !validName ) {
            ImGui::BeginDisabled();
        }

        if ( ImGui::Button( "Create" ) ) {
            // Create new AF
            idDeclAF* newAF = static_cast<idDeclAF*>(
                declManager->CreateNewDecl( DECL_AF, newNameBuffer, "af/custom.af" )
            );
            if ( newAF ) {
                // Re-enumerate and select the new AF
                EnumAFs();
                SelectAFByName( newNameBuffer );
                common->Printf( "Created new AF: %s\n", newNameBuffer );
            }
            showNewAFDialog = false;
            ImGui::CloseCurrentPopup();
        }

        if ( !validName ) {
            ImGui::EndDisabled();
        }

        ImGui::EndPopup();
    }
}

void hcAFEditor::DrawNewBodyDialog( void ) {
    if ( !showNewBodyDialog ) {
        return;
    }

    ImGui::OpenPopup( "New Body###NewBodyDialog" );

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos( center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f) );
    ImGui::SetNextWindowSize( ImVec2(350, 130), ImGuiCond_Appearing );

    if ( ImGui::BeginPopupModal( "New Body###NewBodyDialog", &showNewBodyDialog ) ) {
        ImGui::Text( "Enter a name for the new body:" );
        ImGui::TextDisabled( "(alphanumeric and underscore only)" );

        ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
        ImGui::InputText( "##newBodyName", newNameBuffer, sizeof(newNameBuffer),
            ImGuiInputTextFlags_CallbackCharFilter, ImGuiNameInputCallback );

        idDeclAF* af = GetCurrentAF();
        bool validName = (newNameBuffer[0] != '\0');
        bool isDuplicate = false;
        bool isReserved = false;

        // Check for reserved names (used by constraints bound to world)
        if ( validName ) {
            if ( idStr::Icmp( newNameBuffer, "origin" ) == 0 || idStr::Icmp( newNameBuffer, "world" ) == 0 ) {
                isReserved = true;
                validName = false;
            }
        }

        // Check for duplicate
        if ( validName && af ) {
            for ( int i = 0; i < af->bodies.Num(); i++ ) {
                if ( af->bodies[i]->name.Icmp( newNameBuffer ) == 0 ) {
                    isDuplicate = true;
                    validName = false;
                    break;
                }
            }
        }

        // Show error message
        if ( isReserved ) {
            ImGui::TextColored( ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "'%s' is a reserved name.", newNameBuffer );
        } else if ( isDuplicate ) {
            ImGui::TextColored( ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Name '%s' is already used.", newNameBuffer );
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if ( ImGui::Button( "Cancel" ) ) {
            showNewBodyDialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if ( !validName ) {
            ImGui::BeginDisabled();
        }

        if ( ImGui::Button( "Create" ) ) {
            if ( af ) {
                af->NewBody( newNameBuffer );
                currentBodyIndex = af->bodies.Num() - 1;
                af->modified = true;
                fileModified = true;
                UpdateTestEntity();
            }
            showNewBodyDialog = false;
            ImGui::CloseCurrentPopup();
        }

        if ( !validName ) {
            ImGui::EndDisabled();
        }

        ImGui::EndPopup();
    }
}

void hcAFEditor::DrawNewConstraintDialog( void ) {
    if ( !showNewConstraintDialog ) {
        return;
    }

    ImGui::OpenPopup( "New Constraint###NewConstraintDialog" );

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos( center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f) );
    ImGui::SetNextWindowSize( ImVec2(350, 130), ImGuiCond_Appearing );

    if ( ImGui::BeginPopupModal( "New Constraint###NewConstraintDialog", &showNewConstraintDialog ) ) {
        ImGui::Text( "Enter a name for the new constraint:" );
        ImGui::TextDisabled( "(alphanumeric and underscore only)" );

        ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
        ImGui::InputText( "##newConstraintName", newNameBuffer, sizeof(newNameBuffer),
            ImGuiInputTextFlags_CallbackCharFilter, ImGuiNameInputCallback );

        idDeclAF* af = GetCurrentAF();
        bool validName = (newNameBuffer[0] != '\0');
        bool isDuplicate = false;

        // Check for duplicate
        if ( validName && af ) {
            for ( int i = 0; i < af->constraints.Num(); i++ ) {
                if ( af->constraints[i]->name.Icmp( newNameBuffer ) == 0 ) {
                    isDuplicate = true;
                    validName = false;
                    break;
                }
            }
        }

        // Show error message
        if ( isDuplicate ) {
            ImGui::TextColored( ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Name '%s' is already used.", newNameBuffer );
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if ( ImGui::Button( "Cancel" ) ) {
            showNewConstraintDialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if ( !validName ) {
            ImGui::BeginDisabled();
        }

        if ( ImGui::Button( "Create" ) ) {
            if ( af ) {
                af->NewConstraint( newNameBuffer );
                currentConstraintIndex = af->constraints.Num() - 1;
                af->modified = true;
                fileModified = true;
                UpdateTestEntity();
            }
            showNewConstraintDialog = false;
            ImGui::CloseCurrentPopup();
        }

        if ( !validName ) {
            ImGui::EndDisabled();
        }

        ImGui::EndPopup();
    }
}

void hcAFEditor::DrawRenameDialog( void ) {
    if ( !showRenameDialog ) {
        return;
    }

    const char* title = (renameTarget == RENAME_BODY) ? "Rename Body###RenameDialog" : "Rename Constraint###RenameDialog";

    ImGui::OpenPopup( title );

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos( center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f) );
    ImGui::SetNextWindowSize( ImVec2(350, 200), ImGuiCond_Appearing );

    if ( ImGui::BeginPopupModal( title, &showRenameDialog ) ) {
        ImGui::Text( "Enter the new name:" );
        ImGui::TextDisabled( "(alphanumeric and underscore only)" );

        ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
        ImGui::InputText( "##renameName", renameBuffer, sizeof(renameBuffer),
            ImGuiInputTextFlags_CallbackCharFilter, ImGuiNameInputCallback );

        idDeclAF* af = GetCurrentAF();
        bool validName = (renameBuffer[0] != '\0');
        bool isDuplicate = false;

        // Get current name for comparison (renaming to same name is allowed)
        const char* currentName = nullptr;
        if ( renameTarget == RENAME_BODY ) {
            idDeclAF_Body* body = GetCurrentBody();
            if ( body ) {
                currentName = body->name.c_str();
            }
        } else {
            idDeclAF_Constraint* constraint = GetCurrentConstraint();
            if ( constraint ) {
                currentName = constraint->name.c_str();
            }
        }

        // Check for duplicate (but allow same name)
        if ( validName && af && currentName ) {
            if ( renameTarget == RENAME_BODY ) {
                for ( int i = 0; i < af->bodies.Num(); i++ ) {
                    if ( af->bodies[i]->name.Icmp( renameBuffer ) == 0 &&
                         af->bodies[i]->name.Icmp( currentName ) != 0 ) {
                        isDuplicate = true;
                        validName = false;
                        break;
                    }
                }
            } else {
                for ( int i = 0; i < af->constraints.Num(); i++ ) {
                    if ( af->constraints[i]->name.Icmp( renameBuffer ) == 0 &&
                         af->constraints[i]->name.Icmp( currentName ) != 0 ) {
                        isDuplicate = true;
                        validName = false;
                        break;
                    }
                }
            }
        }

        // Show error message
        if ( isDuplicate ) {
            ImGui::TextColored( ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Name '%s' is already used.", renameBuffer );
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if ( ImGui::Button( "Cancel" ) ) {
            showRenameDialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if ( !validName ) {
            ImGui::BeginDisabled();
        }

        if ( ImGui::Button( "Rename" ) ) {
            if ( af ) {
                if ( renameTarget == RENAME_BODY ) {
                    idDeclAF_Body* body = GetCurrentBody();
                    if ( body ) {
                        af->RenameBody( body->name.c_str(), renameBuffer );
                    }
                } else {
                    idDeclAF_Constraint* constraint = GetCurrentConstraint();
                    if ( constraint ) {
                        af->RenameConstraint( constraint->name.c_str(), renameBuffer );
                    }
                }
                af->modified = true;
                fileModified = true;
                UpdateTestEntity();
            }
            showRenameDialog = false;
            ImGui::CloseCurrentPopup();
        }

        if ( !validName ) {
            ImGui::EndDisabled();
        }

        ImGui::EndPopup();
    }
}

void hcAFEditor::DrawDeleteConfirmDialog( void ) {
    if ( !showDeleteConfirm ) {
        return;
    }

    ImGui::OpenPopup( "Confirm Delete###DeleteConfirmDialog" );

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos( center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f) );
    ImGui::SetNextWindowSize( ImVec2(350, 200), ImGuiCond_Appearing );

    if ( ImGui::BeginPopupModal( "Confirm Delete###DeleteConfirmDialog", &showDeleteConfirm ) ) {
        idDeclAF* af = GetCurrentAF();
        const char* itemName = "";

        if ( deleteTarget == DELETE_AF && af ) {
            itemName = af->GetName();
            ImGui::Text( "Delete Articulated Figure '%s'?", itemName );
        } else if ( deleteTarget == DELETE_BODY ) {
            idDeclAF_Body* body = GetCurrentBody();
            if ( body ) {
                itemName = body->name.c_str();
                ImGui::Text( "Delete body '%s'?", itemName );
                ImGui::TextDisabled( "This will also delete related constraints." );
            }
        } else if ( deleteTarget == DELETE_CONSTRAINT ) {
            idDeclAF_Constraint* constraint = GetCurrentConstraint();
            if ( constraint ) {
                itemName = constraint->name.c_str();
                ImGui::Text( "Delete constraint '%s'?", itemName );
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if ( ImGui::Button( "Cancel" ) ) {
            showDeleteConfirm = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if ( ImGui::Button( "Delete" ) ) {
            if ( deleteTarget == DELETE_AF ) {
                common->Warning( "Cannot delete AF declarations from the editor" );
            } else if ( deleteTarget == DELETE_BODY && af ) {
                idDeclAF_Body* body = GetCurrentBody();
                if ( body ) {
                    af->DeleteBody( body->name.c_str() );
                    currentBodyIndex = Min( currentBodyIndex, af->bodies.Num() - 1 );
                    af->modified = true;
                    fileModified = true;
                    UpdateTestEntity();
                }
            } else if ( deleteTarget == DELETE_CONSTRAINT && af ) {
                idDeclAF_Constraint* constraint = GetCurrentConstraint();
                if ( constraint ) {
                    af->DeleteConstraint( constraint->name.c_str() );
                    currentConstraintIndex = Min( currentConstraintIndex, af->constraints.Num() - 1 );
                    af->modified = true;
                    fileModified = true;
                    UpdateTestEntity();
                }
            }
            showDeleteConfirm = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void hcAFEditor::DrawModelBrowser( void ) {
    if ( !showModelBrowser ) {
        return;
    }

    ImGui::OpenPopup( "Model Browser###ModelBrowserPopup" );

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos( center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f) );
    ImGui::SetNextWindowSize( ImVec2(500, 400), ImGuiCond_Appearing );

    if ( ImGui::BeginPopupModal( "Model Browser###ModelBrowserPopup", &showModelBrowser ) ) {
        idDeclAF* af = GetCurrentAF();

        // Filter input
        ImGui::Text( "Filter:" );
        ImGui::SameLine();
        ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
        if ( ImGui::InputText( "##ModelFilter", modelFilterBuffer, sizeof(modelFilterBuffer) ) ) {
            modelBrowserScrollToIdx = -1;
        }

        ImGui::Separator();

        RebuildFilteredModelList();

        ImGui::TextDisabled( "Showing %d of %d models", filteredModelIndices.Num(), modelNames.Num() );

        ImGui::BeginChild( "ModelList", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2), ImGuiChildFlags_Borders );

        // Handle scroll-to-selection
        if ( modelBrowserScrollToIdx >= 0 ) {
            for ( int fi = 0; fi < filteredModelIndices.Num(); fi++ ) {
                if ( filteredModelIndices[fi] == modelBrowserScrollToIdx ) {
                    float scrollY = fi * ImGui::GetTextLineHeightWithSpacing();
                    ImGui::SetScrollY( scrollY );
                    break;
                }
            }
            modelBrowserScrollToIdx = -1;
        }

        for ( int fi = 0; fi < filteredModelIndices.Num(); fi++ ) {
            int i = filteredModelIndices[fi];
            bool isSelected = af && (modelNames[i].Icmp( af->model ) == 0);

            if ( ImGui::Selectable( modelNames[i].c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick ) ) {
                if ( af ) {
                    af->model = modelNames[i];
                    af->modified = true;
                    fileModified = true;
                    EnumJoints();
                    UpdateTestEntity();
                }
                if ( ImGui::IsMouseDoubleClicked( 0 ) ) {
                    showModelBrowser = false;
                    ImGui::CloseCurrentPopup();
                }
            }
        }

        ImGui::EndChild();

        // Footer
        ImGui::Text( "Current: %s", af ? af->model.c_str() : "<none>" );

        float mbw = ImGui::CalcButtonWidth( "Cancel" ) + ImGui::CalcButtonWidth( "OK" ) + ImGui::GetStyle().ItemSpacing.x;
        ImGui::SameLineRight( mbw );
        if ( ImGui::Button( "Cancel" ) ) {
            showModelBrowser = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if ( ImGui::Button( "OK" ) ) {
            showModelBrowser = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void hcAFEditor::DrawSkinBrowser( void ) {
    if ( !showSkinBrowser ) {
        return;
    }

    ImGui::OpenPopup( "Skin Browser###SkinBrowserPopup" );

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos( center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f) );
    ImGui::SetNextWindowSize( ImVec2(500, 400), ImGuiCond_Appearing );

    if ( ImGui::BeginPopupModal( "Skin Browser###SkinBrowserPopup", &showSkinBrowser ) ) {
        idDeclAF* af = GetCurrentAF();

        // Filter input
        ImGui::Text( "Filter:" );
        ImGui::SameLine();
        ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
        if ( ImGui::InputText( "##SkinFilter", skinFilterBuffer, sizeof(skinFilterBuffer) ) ) {
            skinBrowserScrollToIdx = -1;
        }

        ImGui::Separator();

        RebuildFilteredSkinList();

        ImGui::TextDisabled( "Showing %d of %d skins", filteredSkinIndices.Num(), skinNames.Num() );

        ImGui::BeginChild( "SkinList", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2), ImGuiChildFlags_Borders );

        // Add "<None>" option
        bool noneSelected = af && af->skin.IsEmpty();
        if ( ImGui::Selectable( "<None>", noneSelected, ImGuiSelectableFlags_AllowDoubleClick ) ) {
            if ( af ) {
                af->skin = "";
                af->modified = true;
                fileModified = true;
                UpdateTestEntity();
            }
            if ( ImGui::IsMouseDoubleClicked( 0 ) ) {
                showSkinBrowser = false;
                ImGui::CloseCurrentPopup();
            }
        }

        // Handle scroll-to-selection
        if ( skinBrowserScrollToIdx >= 0 ) {
            for ( int fi = 0; fi < filteredSkinIndices.Num(); fi++ ) {
                if ( filteredSkinIndices[fi] == skinBrowserScrollToIdx ) {
                    float scrollY = (fi + 1) * ImGui::GetTextLineHeightWithSpacing();
                    ImGui::SetScrollY( scrollY );
                    break;
                }
            }
            skinBrowserScrollToIdx = -1;
        }

        for ( int fi = 0; fi < filteredSkinIndices.Num(); fi++ ) {
            int i = filteredSkinIndices[fi];
            bool isSelected = af && (skinNames[i].Icmp( af->skin ) == 0);

            if ( ImGui::Selectable( skinNames[i].c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick ) ) {
                if ( af ) {
                    af->skin = skinNames[i];
                    af->modified = true;
                    fileModified = true;
                    UpdateTestEntity();
                }
                if ( ImGui::IsMouseDoubleClicked( 0 ) ) {
                    showSkinBrowser = false;
                    ImGui::CloseCurrentPopup();
                }
            }
        }

        ImGui::EndChild();

        // Footer
        ImGui::Text( "Current: %s", af ? (af->skin.IsEmpty() ? "<None>" : af->skin.c_str()) : "<none>" );

        float sbw = ImGui::CalcButtonWidth( "Cancel" ) + ImGui::CalcButtonWidth( "OK" ) + ImGui::GetStyle().ItemSpacing.x;
        ImGui::SameLineRight( sbw );
        if ( ImGui::Button( "Cancel" ) ) {
            showSkinBrowser = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if ( ImGui::Button( "OK" ) ) {
            showSkinBrowser = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

int hcAFEditor::FindSlotIndex( const char* slotName ) {
    for ( int i = 0; i < NUM_HUMANOID_SLOTS; i++ ) {
        if ( idStr::Icmp( humanoidTemplate[i].name, slotName ) == 0 ) {
            return i;
        }
    }
    return -1;
}

void hcAFEditor::OpenJointMappingDialog( void ) {
    idDeclAF* af = GetCurrentAF();
    if ( !af ) {
        common->Warning( "No AF selected" );
        return;
    }

    if ( af->model.IsEmpty() ) {
        common->Warning( "AF has no model set. Please set a model first." );
        return;
    }

    // Clear previous mappings
    for ( int i = 0; i < NUM_HUMANOID_SLOTS; i++ ) {
        slotJointMapping[i].Clear();
    }

    // Make sure joints are enumerated
    EnumJoints();

    if ( jointNames.Num() == 0 ) {
        common->Warning( "Could not enumerate joints from model '%s'", af->model.c_str() );
        return;
    }

    // Try to auto-match
    AutoMatchJoints();

    showJointMappingDialog = true;
}

void hcAFEditor::AutoMatchJoints( void ) {
    // For each slot, try to find a matching joint
    for ( int patternIdx = 0; autoMatchPatterns[patternIdx].slotName != NULL; patternIdx++ ) {
        const JointMatchPattern& pattern = autoMatchPatterns[patternIdx];
        int slotIdx = FindSlotIndex( pattern.slotName );
        if ( slotIdx < 0 ) {
            continue;
        }

        // Try each pattern substring
        for ( int p = 0; pattern.patterns[p] != NULL; p++ ) {
            bool found = false;

            // Search through joints
            for ( int j = 0; j < jointNames.Num(); j++ ) {
                if ( idStr::FindText( jointNames[j].c_str(), pattern.patterns[p], false ) >= 0 ) {
                    slotJointMapping[slotIdx] = jointNames[j];
                    found = true;
                    break;
                }
            }

            if ( found ) {
                break;
            }
        }
    }
}

void hcAFEditor::DrawJointMappingDialog( void ) {
    if ( !showJointMappingDialog ) {
        return;
    }

    ImGui::OpenPopup( "Joint Mapping - Humanoid Template###JointMappingDialog" );

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos( center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f) );
    ImGui::SetNextWindowSize( ImVec2(500, 550), ImGuiCond_Appearing );

    if ( ImGui::BeginPopupModal( "Joint Mapping - Humanoid Template###JointMappingDialog", &showJointMappingDialog ) ) {
        // Auto-Match button
        if ( ImGui::Button( "Auto-Match" ) ) {
            AutoMatchJoints();
        }
        ImGui::AddTooltip( "Attempt to match joints by name patterns" );

        ImGui::SameLine();
        if ( ImGui::Button( "Clear All" ) ) {
            for ( int i = 0; i < NUM_HUMANOID_SLOTS; i++ ) {
                slotJointMapping[i].Clear();
            }
        }

        ImGui::Separator();

        // Count mapped slots
        int mappedCount = 0;
        for ( int i = 0; i < NUM_HUMANOID_SLOTS; i++ ) {
            if ( !slotJointMapping[i].IsEmpty() ) {
                mappedCount++;
            }
        }
        ImGui::Text( "Mapped: %d / %d", mappedCount, NUM_HUMANOID_SLOTS );

        ImGui::Separator();

        // Scrollable table of slots
        ImGui::BeginChild( "SlotList", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2), ImGuiChildFlags_Borders );

        if ( ImGui::BeginTable( "JointMappingTable", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV ) ) {
            ImGui::TableSetupColumn( "Body Part", ImGuiTableColumnFlags_WidthFixed, 150.0f );
            ImGui::TableSetupColumn( "Skeleton Joint", ImGuiTableColumnFlags_WidthStretch );
            ImGui::TableHeadersRow();

            for ( int i = 0; i < NUM_HUMANOID_SLOTS; i++ ) {
                const AFBodySlot& slot = humanoidTemplate[i];

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text( "%s", slot.displayName );

                ImGui::TableNextColumn();
                ImGui::PushID( i );
                ImGui::SetNextItemWidth( -FLT_MIN );

                // Build combo preview
                const char* preview = slotJointMapping[i].IsEmpty() ? "<None>" : slotJointMapping[i].c_str();

                if ( ImGui::BeginCombo( "##Joint", preview ) ) {
                    // None option
                    if ( ImGui::Selectable( "<None>", slotJointMapping[i].IsEmpty() ) ) {
                        slotJointMapping[i].Clear();
                    }

                    // Joint options
                    for ( int j = 0; j < jointNames.Num(); j++ ) {
                        bool isSelected = ( slotJointMapping[i].Icmp( jointNames[j] ) == 0 );
                        if ( ImGui::Selectable( jointNames[j].c_str(), isSelected ) ) {
                            slotJointMapping[i] = jointNames[j];
                        }
                        if ( isSelected ) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopID();
            }

            ImGui::EndTable();
        }

        ImGui::EndChild();

        if ( ImGui::Button( "Cancel" ) ) {
            showJointMappingDialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        // Footer buttons
        if ( ImGui::Button( "Generate" ) ) {
            GenerateFromTemplate();
            showJointMappingDialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::AddTooltip( "Generate bodies and constraints from mapping" );

        ImGui::EndPopup();
    }
}

void hcAFEditor::GenerateFromTemplate( void ) {
    idDeclAF* af = GetCurrentAF();
    if ( !af ) {
        return;
    }

    // Clear existing bodies and constraints
    af->bodies.DeleteContents( true );
    af->constraints.DeleteContents( true );

    // Reset selection indices
    currentBodyIndex = -1;
    currentConstraintIndex = -1;

    // Disable self-collision at AF level to prevent instability
    af->selfCollision = false;

    // Set total mass to scale all body masses - critical for stability
    // This ensures reasonable mass distribution regardless of body volumes
    af->totalMass = 150.0f;

    // Set default constraint friction higher for stability
    af->defaultConstraintFriction = 0.8f;

    // First pass: create bodies for each mapped slot
    for ( int i = 0; i < NUM_HUMANOID_SLOTS; i++ ) {
        if ( slotJointMapping[i].IsEmpty() ) {
            continue;
        }

        const AFBodySlot& slot = humanoidTemplate[i];

        idDeclAF_Body* body = new idDeclAF_Body();
        body->SetDefault( af );
        body->name = slot.name;

        // The pelvis (root) body MUST modify the "origin" joint
        // This is required by the AF system
        if ( slot.parentSlot == NULL ) {
            body->jointName = "origin";
        } else {
            body->jointName = slotJointMapping[i];
        }
        body->jointMod = DECLAF_JOINTMOD_AXIS;

        // Check if we have an endpoint joint for bone body
        int endSlotIdx = -1;
        if ( slot.endJointSlot ) {
            endSlotIdx = FindSlotIndex( slot.endJointSlot );
        }

        bool hasBoneEndpoint = ( endSlotIdx >= 0 && !slotJointMapping[endSlotIdx].IsEmpty() );

        // Determine if this body should use a box model instead of a bone model.
        // Boxes are required for:
        // - Root body (pelvis): must be at "origin" joint, not between skeleton joints
        // - Torso bodies (chest, head): use boxes at their joints for stability
        // - Lower legs: use cones with rotation angles (like original Doom 3 AFs)
        // - Any body with FIXED constraint: avoids twitching from misaligned positions
        bool isTorsoBody = ( idStr::Icmp( slot.name, "chest" ) == 0 ) ||
                           ( idStr::Icmp( slot.name, "head" ) == 0 );
        bool isLowerLeg = ( idStr::Icmp( slot.name, "l_lowerleg" ) == 0 ) ||
                          ( idStr::Icmp( slot.name, "r_lowerleg" ) == 0 );
        bool useBoxModel = ( slot.parentSlot == NULL ) || isTorsoBody || ( slot.constraintType == DECLAF_CONSTRAINT_FIXED );

        if ( isLowerLeg ) {
            // Cone model for lower legs - tapers toward ankle
            // Based on original: cone( ( -8.5, -4, -14.5 ), ( 8.5, 4, 14.5 ), 3 )
            // with angles ( 5, 180, 0 ) to orient downward
            body->modelType = TRM_CONE;
            body->v1.type = idAFVector::VEC_COORDS;
            body->v1.ToVec3().Set( -8.5f, -4.0f, -14.5f );
            body->v2.type = idAFVector::VEC_COORDS;
            body->v2.ToVec3().Set( 8.5f, 4.0f, 14.5f );
            body->numSides = 3;  // Tip radius
            body->origin.type = idAFVector::VEC_JOINT;
            body->origin.joint1 = slotJointMapping[i];
            body->angles.Set( 5.0f, 180.0f, 0.0f );  // Rotate to point downward
        } else if ( useBoxModel ) {
            // Box body for torso segments
            body->modelType = TRM_BOX;
            float w = slot.boneWidth;
            body->v1.type = idAFVector::VEC_COORDS;
            body->v1.ToVec3().Set( -w, -w, -w );
            body->v2.type = idAFVector::VEC_COORDS;
            body->v2.ToVec3().Set( w, w, w );

            // Position box at bonecenter between this joint and endpoint to avoid overlap
            // e.g., pelvis at bonecenter(Hips, Waist), chest at bonecenter(Waist, Neck)
            if ( hasBoneEndpoint ) {
                body->origin.type = idAFVector::VEC_BONECENTER;
                body->origin.joint1 = slotJointMapping[i];
                body->origin.joint2 = slotJointMapping[endSlotIdx];
            } else {
                // No endpoint (e.g., head) - just use joint position
                body->origin.type = idAFVector::VEC_JOINT;
                body->origin.joint1 = slotJointMapping[i];
            }
        } else if ( hasBoneEndpoint ) {
            // Bone (capsule) body between two joints
            body->modelType = TRM_BONE;
            body->v1.type = idAFVector::VEC_JOINT;
            body->v1.joint1 = slotJointMapping[i];
            body->v2.type = idAFVector::VEC_JOINT;
            body->v2.joint1 = slotJointMapping[endSlotIdx];
            body->width = slot.boneWidth;
            body->origin.type = idAFVector::VEC_BONECENTER;
            body->origin.joint1 = slotJointMapping[i];
            body->origin.joint2 = slotJointMapping[endSlotIdx];
        } else {
            // Box body for endpoints (hands)
            body->modelType = TRM_BOX;
            float w = slot.boneWidth;
            body->v1.type = idAFVector::VEC_COORDS;
            body->v1.ToVec3().Set( -w, -w, -w );
            body->v2.type = idAFVector::VEC_COORDS;
            body->v2.ToVec3().Set( w, w, w );
            body->origin.type = idAFVector::VEC_JOINT;
            body->origin.joint1 = slotJointMapping[i];
        }

        // Set body density from template (totalMass will scale these)
        body->density = slot.density;

        // Disable self-collision to prevent bodies fighting with constraints
        body->selfCollision = false;

        af->bodies.Append( body );
    }

    // Second pass: set containedJoints patterns
    // The root body claims all joints and excludes child body joints
    // Other bodies just claim their mapped joint
    for ( int i = 0; i < af->bodies.Num(); i++ ) {
        idDeclAF_Body* body = af->bodies[i];
        int slotIdx = FindSlotIndex( body->name.c_str() );
        if ( slotIdx < 0 ) {
            continue;
        }

        const AFBodySlot& slot = humanoidTemplate[slotIdx];

        if ( slot.parentSlot == NULL ) {
            // Root body: claim origin and exclude all child bodies' joints
            idStr pattern = "*origin";
            for ( int j = 0; j < NUM_HUMANOID_SLOTS; j++ ) {
                if ( j != slotIdx && !slotJointMapping[j].IsEmpty() ) {
                    const AFBodySlot& childSlot = humanoidTemplate[j];
                    // Only exclude direct children of root (spine, legs)
                    if ( childSlot.parentSlot && idStr::Icmp( childSlot.parentSlot, slot.name ) == 0 ) {
                        pattern += va( " -*%s", slotJointMapping[j].c_str() );
                    }
                }
            }
            body->containedJoints = pattern;
        } else {
            // Non-root body: claim this joint and exclude child bodies' joints
            idStr pattern = va( "*%s", slotJointMapping[slotIdx].c_str() );
            for ( int j = 0; j < NUM_HUMANOID_SLOTS; j++ ) {
                if ( j != slotIdx && !slotJointMapping[j].IsEmpty() ) {
                    const AFBodySlot& childSlot = humanoidTemplate[j];
                    // Only exclude direct children
                    if ( childSlot.parentSlot && idStr::Icmp( childSlot.parentSlot, slot.name ) == 0 ) {
                        pattern += va( " -*%s", slotJointMapping[j].c_str() );
                    }
                }
            }
            body->containedJoints = pattern;
        }
    }

    // Create constraints between connected bodies
    for ( int i = 0; i < NUM_HUMANOID_SLOTS; i++ ) {
        if ( slotJointMapping[i].IsEmpty() ) {
            continue;
        }

        const AFBodySlot& slot = humanoidTemplate[i];
        if ( !slot.parentSlot || slot.constraintType < 0 ) {
            continue;  // Root has no constraint
        }

        int parentIdx = FindSlotIndex( slot.parentSlot );
        if ( parentIdx < 0 || slotJointMapping[parentIdx].IsEmpty() ) {
            continue;  // Parent not mapped
        }

        idDeclAF_Constraint* c = new idDeclAF_Constraint();
        c->SetDefault( af );
        c->name = slot.name;
        c->type = (declAFConstraintType_t)slot.constraintType;
        c->body1 = slot.name;
        c->body2 = slot.parentSlot;

        // Anchor at child joint
        c->anchor.type = idAFVector::VEC_JOINT;
        c->anchor.joint1 = slotJointMapping[i];

        // Set constraint friction from template
        c->friction = slot.constraintFriction;

        // Fixed constraints don't need any additional properties
        if ( slot.constraintType == DECLAF_CONSTRAINT_FIXED ) {
            // Nothing else needed for fixed constraints
        } else if ( slot.isHinge ) {
            // Hinge constraint
            c->limit = idDeclAF_Constraint::LIMIT_CONE;
            c->limitAngles[0] = slot.limitAngle1;  // min angle
            c->limitAngles[1] = slot.limitAngle3;  // limit resistance (roll angle for hinges)
            c->limitAngles[2] = slot.limitAngle2;  // max angle

            // Set hinge axis from template
            c->axis.type = idAFVector::VEC_COORDS;
            c->axis.ToVec3().Set( slot.shaft1[0], slot.shaft1[1], slot.shaft1[2] );
        } else {
            // Universal constraint
            if ( slot.limitAngle2 > 0 ) {
                c->limit = idDeclAF_Constraint::LIMIT_PYRAMID;
                c->limitAngles[0] = slot.limitAngle1;
                c->limitAngles[1] = slot.limitAngle2;
                c->limitAngles[2] = slot.limitAngle3;  // roll angle
            } else if ( slot.limitAngle1 > 0 ) {
                c->limit = idDeclAF_Constraint::LIMIT_CONE;
                c->limitAngles[0] = slot.limitAngle1;
            }

            // Set shafts from template (only if non-zero)
            if ( slot.shaft1[0] != 0.0f || slot.shaft1[1] != 0.0f || slot.shaft1[2] != 0.0f ) {
                c->shaft[0].type = idAFVector::VEC_COORDS;
                c->shaft[0].ToVec3().Set( slot.shaft1[0], slot.shaft1[1], slot.shaft1[2] );
                c->shaft[1].type = idAFVector::VEC_COORDS;
                c->shaft[1].ToVec3().Set( slot.shaft2[0], slot.shaft2[1], slot.shaft2[2] );
            }
        }

        // Set limit axis from template (if non-zero)
        if ( slot.limitAxis[0] != 0.0f || slot.limitAxis[1] != 0.0f || slot.limitAxis[2] != 0.0f ) {
            c->limitAxis.type = idAFVector::VEC_COORDS;
            c->limitAxis.ToVec3().Set( slot.limitAxis[0], slot.limitAxis[1], slot.limitAxis[2] );
        }

        af->constraints.Append( c );
    }

    // Mark as modified
    af->modified = true;
    fileModified = true;

    // Update UI
    if ( af->bodies.Num() > 0 ) {
        currentBodyIndex = 0;
    }
    if ( af->constraints.Num() > 0 ) {
        currentConstraintIndex = 0;
    }

    // Update test entity if spawned
    UpdateTestEntity();

    common->Printf( "Generated %d bodies and %d constraints\n", af->bodies.Num(), af->constraints.Num() );
}

void hcAFEditor::Draw( void ) {
    if ( !visible ) {
        return;
    }

    if ( !initialized ) {
        initialized = true;
    }

    const float fontSize = ImGui::GetFontSize();
    ImVec2 minSize( fontSize * 35, fontSize * 30 );
    ImVec2 maxSize( ImGui::GetMainViewport()->WorkSize );
    ImGui::SetNextWindowSizeConstraints( minSize, maxSize );

    ImGuiWindowFlags winFlags = ImGuiWindowFlags_None;

    if ( !ImGui::Begin( "AF Editor###AFEditorWindow", &visible, winFlags ) ) {
        ImGui::End();
        return;
    }

    // Draw toolbar
    DrawToolbar();

    // Tab bar
    int prevTab = activeTab;
    if ( ImGui::BeginTabBar( "AFEditorTabs" ) ) {
        if ( ImGui::BeginTabItem( "View" ) ) {
            activeTab = TAB_VIEW;
            DrawViewTab();
            ImGui::EndTabItem();
        }

        if ( ImGui::BeginTabItem( "Properties" ) ) {
            activeTab = TAB_PROPERTIES;
            DrawPropertiesTab();
            ImGui::EndTabItem();
        }

        if ( ImGui::BeginTabItem( "Bodies" ) ) {
            activeTab = TAB_BODIES;
            DrawBodiesTab();
            ImGui::EndTabItem();
        }

        if ( ImGui::BeginTabItem( "Constraints" ) ) {
            activeTab = TAB_CONSTRAINTS;
            DrawConstraintsTab();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    // Handle highlighting CVars when switching tabs
    if ( prevTab != activeTab ) {
        idDeclAF* af = GetCurrentAF();

        // Clear highlights when leaving Bodies or Constraints tabs
        if ( prevTab == TAB_BODIES ) {
            cvarSystem->SetCVarString( "af_highlightBody", "" );
        }
        if ( prevTab == TAB_CONSTRAINTS ) {
            cvarSystem->SetCVarString( "af_highlightConstraint", "" );
        }

        // Set highlights when entering Bodies or Constraints tabs
        if ( activeTab == TAB_BODIES && af && currentBodyIndex >= 0 && currentBodyIndex < af->bodies.Num() ) {
            cvarSystem->SetCVarString( "af_highlightBody", af->bodies[currentBodyIndex]->name.c_str() );
        }
        if ( activeTab == TAB_CONSTRAINTS && af && currentConstraintIndex >= 0 && currentConstraintIndex < af->constraints.Num() ) {
            cvarSystem->SetCVarString( "af_highlightConstraint", af->constraints[currentConstraintIndex]->name.c_str() );
        }
    }

    ImGui::End();

    DrawNewAFDialog();
    DrawNewBodyDialog();
    DrawNewConstraintDialog();
    DrawRenameDialog();
    DrawDeleteConfirmDialog();
    DrawModelBrowser();
    DrawSkinBrowser();
    DrawJointMappingDialog();

    if ( !visible ) {
        D3::ImGuiHooks::CloseWindow( D3::ImGuiHooks::D3_ImGuiWin_AFEditor );
    }
}

/*
================
Com_OpenCloseImGuiAFEditor

Open/Close handler
================
*/
void Com_OpenCloseImGuiAFEditor( bool open ) {
    if ( !afEditor ) {
        afEditor = new hcAFEditor();
    }

    if ( open ) {
        afEditor->Init( nullptr );
        afEditor->SetVisible( true );
    } else {
        afEditor->SetVisible( false );
    }
}

/*
================
Com_ImGuiAFEditor_f

Console command handler
================
*/
void Com_ImGuiAFEditor_f( const idCmdArgs& args ) {
    bool menuOpen = (D3::ImGuiHooks::GetOpenWindowsMask() & D3::ImGuiHooks::D3_ImGuiWin_AFEditor) != 0;
    if ( !menuOpen ) {
        D3::ImGuiHooks::OpenWindow( D3::ImGuiHooks::D3_ImGuiWin_AFEditor );
    } else {
        if ( ImGui::IsWindowFocused( ImGuiFocusedFlags_AnyWindow ) ) {
            D3::ImGuiHooks::CloseWindow( D3::ImGuiHooks::D3_ImGuiWin_AFEditor );
        } else {
            ImGui::SetNextWindowFocus();
        }
    }
}

#else // IMGUI_DISABLE - stub implementations

#include "framework/Common.h"
#include "AFEditor.h"

hcAFEditor* afEditor = nullptr;

hcAFEditor::hcAFEditor( void ) {}
hcAFEditor::~hcAFEditor( void ) {}
void hcAFEditor::Init( const idDict* spawnArgs ) {}
void hcAFEditor::Shutdown( void ) {}
void hcAFEditor::Draw( void ) {}
bool hcAFEditor::IsVisible( void ) const { return false; }
void hcAFEditor::SetVisible( bool visible ) {}
void Com_OpenCloseImGuiAFEditor( bool open ) {}

void Com_ImGuiAFEditor_f( const idCmdArgs& args ) {
    common->Warning( "This editor requires imgui to be enabled" );
}

#endif // IMGUI_DISABLE
