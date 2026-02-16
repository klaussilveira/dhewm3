#ifndef __EDITOR_H__
#define __EDITOR_H__

#include "idlib/math/Math.h"
#include "sys/sys_imgui.h"

class hcEditor {
public:
	virtual					~hcEditor( void ) {}
	virtual void			Init( const idDict* spawnArgs = nullptr ) = 0;
	virtual void			Shutdown( void ) = 0;
	virtual void			Draw( void ) = 0;
	virtual bool			IsVisible( void ) const = 0;
	virtual void			SetVisible( bool visible ) = 0;
	virtual const char*		GetName( void ) const = 0;
};

class hcEditorMenuBar {
public:
						hcEditorMenuBar( void );
	void				Draw( void );
	bool				IsTranslucent( void ) const { return translucent; }
	void				SetTranslucent( bool enable );
	void				OpenSpawnDialog( void );

private:
	bool				translucent;
	float				originalAlpha;
	bool				showSpawnDialog;
	char				spawnFilter[256];
	idList<idStr>		entityDefNames;
	int					selectedEntityDefIndex;

	void				DrawMapMenu( void );
	void				DrawWindowMenu( void );
	void				DrawDebugMenu( void );
	void				DrawEditModeMenu( void );
	void				DrawPlayerMenu( void );
	void				DrawReloadMenu( void );
	void				DrawSpawnDialog( void );
	void				EnumEntityDefs( void );

	void				DrawCvarToggle( const char* label, const char* cvarName, const char* tooltip = nullptr );
	void				DrawCvarIntCombo( const char* label, const char* cvarName, const char** options, int numOptions, const char* tooltip = nullptr );
};

extern hcEditorMenuBar*	editorMenuBar;

void					Editor_Init( void );
void					Editor_Shutdown( void );
void					Editor_Draw( void );
void					Editor_ToggleMode( void );
bool					Editor_IsModeActive( void );
void					Editor_SetModeActive( bool active );


// imgui helpers

namespace ImGui {
template<typename F>
void LabeledWidget( const char* label, F widget ) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::Text( "%s", label );
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-FLT_MIN);
    widget();
}

template<typename F>
void UnlabeledWidget( F widget ) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TableNextColumn();
    widget();
}

template<typename F>
void PropertyTable( const char* id, float labelWidth, F contents ) {
    if ( ImGui::BeginTable( id, 2, ImGuiTableFlags_Resizable ) ) {
        ImGui::TableSetupColumn( "label",  ImGuiTableColumnFlags_WidthFixed, labelWidth );
        ImGui::TableSetupColumn( "widget", ImGuiTableColumnFlags_WidthStretch );
        contents();
        ImGui::EndTable();
    }
}

inline void AddTooltip( const char* text )
{
    if ( ImGui::BeginItemTooltip() ) {
        ImGui::PushTextWrapPos( ImGui::GetFontSize() * 35.0f );
        ImGui::TextUnformatted( text );
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

inline bool SliderFloatWidget( const char* label, float* value, float minVal, float maxVal, const char* tooltip = nullptr )
{
    bool changed = ImGui::SliderFloat( label, value, minVal, maxVal, "%.3f" );
    if ( tooltip ) {
        ImGui::SetItemTooltip(tooltip);
    }

    return changed;
}

inline bool SliderIntWidget( const char* label, int* value, int minVal, int maxVal, const char* tooltip = nullptr )
{
    bool changed = ImGui::SliderInt( label, value, minVal, maxVal );
    if ( tooltip ) {
        AddTooltip( tooltip );
    }

    return changed;
}

inline bool CheckboxWidget( const char* label, bool* value, const char* tooltip = nullptr )
{
    bool changed = ImGui::Checkbox( label, value );
    if ( tooltip ) {
        AddTooltip( tooltip );
    }

    return changed;
}

inline bool InputFloatWidget( const char* label, float* value, const char* tooltip = nullptr )
{
    bool changed = ImGui::InputFloat( label, value, 0.1f, 1.0f, "%.3f" );
    if ( tooltip ) {
        AddTooltip( tooltip );
    }

    return changed;
}

inline bool InputIntWidget( const char* label, int* value, const char* tooltip = nullptr )
{
    bool changed = ImGui::InputInt( label, value );
    if ( tooltip ) {
        AddTooltip( tooltip );
    }

    return changed;
}

inline bool ColorWidget( const char* label, idVec4& color, const char* tooltip = nullptr )
{
    float col[4] = { color.x, color.y, color.z, color.w };
    bool changed = ImGui::ColorEdit4( label, col, ImGuiColorEditFlags_Float );
    if ( changed ) {
        color.x = col[0];
        color.y = col[1];
        color.z = col[2];
        color.w = col[3];
    }

    if ( tooltip ) {
        AddTooltip( tooltip );
    }

    return changed;
}

inline bool Vec3Widget( const char* label, idVec3& vec, const char* tooltip = nullptr )
{
    float v[3] = { vec.x, vec.y, vec.z };
    bool changed = ImGui::InputFloat3( label, v, "%.2f" );
    if ( changed ) {
        vec.x = v[0];
        vec.y = v[1];
        vec.z = v[2];
    }

    if ( tooltip ) {
        AddTooltip( tooltip );
    }

    return changed;
}
}

#endif /* !__EDITOR_H__ */
