#ifndef __AF_EDITOR_H__
#define __AF_EDITOR_H__

#include "Editor.h"
#include "framework/DeclAF.h"

/**
 * Humanoid Template for AF Body Generation
 */
struct AFBodySlot {
    const char* name;           // Body name: "pelvis", "l_upperarm"
    const char* displayName;    // UI label: "Pelvis", "Left Upper Arm"
    const char* parentSlot;     // Parent body name, NULL for root
    const char* endJointSlot;   // Slot name for bone endpoint, NULL for box bodies

    int constraintType;         // DECLAF_CONSTRAINT_* or -1 for none
    bool isHinge;               // true=hinge, false=universal
    float limitAngle1;          // Cone angle or hinge min
    float limitAngle2;          // Pyramid angle2 or hinge max
    float limitAngle3;          // Pyramid roll angle (60=right, 120=left)
    float boneWidth;            // Collision capsule radius
    float density;              // Body density for mass calculation
    float constraintFriction;   // Joint friction

    // Constraint orientations
    float shaft1[3];
    float shaft2[3];
    float limitAxis[3];         // Cone/pyramid limit axis direction
};

#define NUM_HUMANOID_SLOTS 13
extern const AFBodySlot humanoidTemplate[NUM_HUMANOID_SLOTS];

class hcAFEditor : public hcEditor {
public:
                            hcAFEditor( void );
    virtual                 ~hcAFEditor( void );

    virtual void            Init( const idDict* spawnArgs = nullptr ) override;
    virtual void            Shutdown( void ) override;
    virtual void            Draw( void ) override;
    virtual bool            IsVisible( void ) const override;
    virtual void            SetVisible( bool visible ) override;
    virtual const char*     GetName( void ) const override { return "AF Editor"; }

private:
    bool                    initialized;
    bool                    visible;
    bool                    hasTestEntity;
    float                   propertyTableWidth;

    // UI state
    enum TabType {
        TAB_VIEW = 0,
        TAB_PROPERTIES,
        TAB_BODIES,
        TAB_CONSTRAINTS
    };

    int                     activeTab;
    bool                    fileModified;
    bool                    showNewAFDialog;
    bool                    showNewBodyDialog;
    bool                    showNewConstraintDialog;
    bool                    showRenameDialog;
    bool                    showModelBrowser;
    bool                    showSkinBrowser;
    bool                    showDeleteConfirm;
    bool                    showJointMappingDialog;

    // Dialog buffers
    char                    newNameBuffer[64];
    char                    renameBuffer[64];
    char                    modelFilterBuffer[128];
    char                    skinFilterBuffer[128];

    // AF selection
    int                     currentAFIndex;
    idList<idStr>           afNames;
    int                     currentBodyIndex;
    int                     currentConstraintIndex;

    // Joint mapping for generation
    idStr                   slotJointMapping[NUM_HUMANOID_SLOTS];

    // Rename context
    enum RenameTarget {
        RENAME_BODY,
        RENAME_CONSTRAINT
    };

    int                     renameTarget;

    // Delete context
    enum DeleteTarget {
        DELETE_AF,
        DELETE_BODY,
        DELETE_CONSTRAINT
    };

    int                     deleteTarget;

    // Model/skin browser
    idList<idStr>           modelNames;
    idList<idStr>           skinNames;
    idList<int>             filteredModelIndices;
    idList<int>             filteredSkinIndices;
    char                    lastModelFilter[128];
    char                    lastSkinFilter[128];
    int                     modelBrowserScrollToIdx;
    int                     skinBrowserScrollToIdx;

    // Joint list
    idList<idStr>           jointNames;

    // View settings
    bool                    viewShowBodies;
    bool                    viewShowBodyNames;
    bool                    viewShowMass;
    bool                    viewShowTotalMass;
    bool                    viewShowInertia;
    bool                    viewShowVelocity;
    bool                    viewShowConstraints;
    bool                    viewShowConstraintNames;
    bool                    viewShowPrimaryOnly;
    bool                    viewShowLimits;
    bool                    viewShowConstrainedBodies;
    bool                    viewShowTrees;
    bool                    viewShowSkeleton;
    bool                    viewSkeletonOnly;
    bool                    viewNoFriction;
    bool                    viewNoLimits;
    bool                    viewNoGravity;
    bool                    viewNoSelfCollision;
    bool                    viewDragEntity;
    bool                    viewDragShowSelection;
    bool                    viewShowTimings;
    bool                    viewDebugLineDepthTest;
    bool                    viewDebugLineUseArrows;

    void                    EnumAFs( void );
    idDeclAF*               GetCurrentAF( void );
    idDeclAF_Body*          GetCurrentBody( void );
    idDeclAF_Constraint*    GetCurrentConstraint( void );
    bool                    SelectAFByName( const char* name );

    void                    EnumJoints( void );
    int                     FindJointIndex( const char* jointName );
    void                    EnumModels( void );
    void                    EnumSkins( void );
    void                    RebuildFilteredModelList( void );
    void                    RebuildFilteredSkinList( void );

    void                    LoadViewSettings( void );
    void                    ApplyViewSettings( void );

    void                    SpawnTestEntity( void );
    void                    KillTestEntity( void );
    void                    UpdateTestEntity( void );
    void                    ResetTestEntityPose( void );
    void                    ActivateTestEntityPhysics( void );

    void                    SaveCurrentAF( void );
    void                    ReloadCurrentAF( void );
    void                    RevertChanges( void );

    void                    OpenNewAFDialog( void );
    void                    OpenNewBodyDialog( void );
    void                    OpenNewConstraintDialog( void );
    void                    OpenRenameDialog( RenameTarget target );
    void                    OpenDeleteConfirmDialog( DeleteTarget target );
    void                    OpenModelBrowser( void );
    void                    OpenSkinBrowser( void );

    void                    DrawToolbar( void );
    void                    DrawViewTab( void );
    void                    DrawPropertiesTab( void );
    void                    DrawBodiesTab( void );
    void                    DrawConstraintsTab( void );

    void                    DrawBodyList( void );
    void                    DrawBodyJointSection( idDeclAF_Body* body );
    void                    DrawBodyModelSection( idDeclAF_Body* body );
    void                    DrawBodyOriginSection( idDeclAF_Body* body );
    void                    DrawBodyPhysicsSection( idDeclAF_Body* body );

    void                    DrawConstraintList( void );
    void                    DrawConstraintGeneralSection( idDeclAF_Constraint* constraint );
    void                    DrawConstraintTypeProperties( idDeclAF_Constraint* constraint );
    void                    DrawFixedConstraint( idDeclAF_Constraint* constraint );
    void                    DrawBallAndSocketConstraint( idDeclAF_Constraint* constraint );
    void                    DrawUniversalConstraint( idDeclAF_Constraint* constraint );
    void                    DrawHingeConstraint( idDeclAF_Constraint* constraint );
    void                    DrawSliderConstraint( idDeclAF_Constraint* constraint );
    void                    DrawSpringConstraint( idDeclAF_Constraint* constraint );

    bool                    DrawAFVectorWidget( const char* label, idAFVector& vec, const char* tooltip = nullptr );
    bool                    DrawJointCombo( const char* label, idStr& jointName, const char* tooltip = nullptr );
    bool                    DrawBodyCombo( const char* label, idStr& bodyName, const char* tooltip = nullptr );
    bool                    DrawContentsWidget( const char* label, int& contents, const char* tooltip = nullptr );

    void                    DrawNewAFDialog( void );
    void                    DrawNewBodyDialog( void );
    void                    DrawNewConstraintDialog( void );
    void                    DrawRenameDialog( void );
    void                    DrawDeleteConfirmDialog( void );
    void                    DrawModelBrowser( void );
    void                    DrawSkinBrowser( void );
    void                    DrawJointMappingDialog( void );

    void                    OpenJointMappingDialog( void );
    void                    AutoMatchJoints( void );
    void                    GenerateFromTemplate( void );
    int                     FindSlotIndex( const char* slotName );
};

extern hcAFEditor*          afEditor;

#endif /* !__AF_EDITOR_H__ */
