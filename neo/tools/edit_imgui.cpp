#include "edit_public.h"
#include "framework/Common.h"
#include "sys/platform.h"
#include "sys/sys_imgui.h"

void RadiantInit(void) {
  common->Printf("The level editor is still not implemented in imgui\n");
}
void RadiantShutdown(void) {}
void RadiantRun(void) {}
void RadiantPrint(const char *text) {}
void RadiantSync(const char *mapName, const idVec3 &viewOrg,
                 const idAngles &viewAngles) {}

void LightEditorInit(const idDict *spawnArgs) {
  D3::ImGuiHooks::OpenWindow(D3::ImGuiHooks::D3_ImGuiWin_LightEditor);
}
void LightEditorShutdown(void) {
  D3::ImGuiHooks::CloseWindow(D3::ImGuiHooks::D3_ImGuiWin_LightEditor);
}
void LightEditorRun(void) {}

void SoundEditorInit(const idDict *spawnArgs) {
  common->Printf("The sound editor is still not implemented in imgui\n");
}
void SoundEditorShutdown(void) {}
void SoundEditorRun(void) {}

void AFEditorInit(const idDict *spawnArgs) {
  D3::ImGuiHooks::OpenWindow( D3::ImGuiHooks::D3_ImGuiWin_AFEditor );
}
void AFEditorShutdown(void) {
  D3::ImGuiHooks::CloseWindow( D3::ImGuiHooks::D3_ImGuiWin_AFEditor );
}
void AFEditorRun(void) {}

void ParticleEditorInit(const idDict *spawnArgs) {
  D3::ImGuiHooks::OpenWindow(D3::ImGuiHooks::D3_ImGuiWin_ParticleEditor);
}
void ParticleEditorShutdown(void) {
  D3::ImGuiHooks::CloseWindow(D3::ImGuiHooks::D3_ImGuiWin_ParticleEditor);
}
void ParticleEditorRun(void) {}

void PlayerEditorInit(const idDict *spawnArgs) {
  D3::ImGuiHooks::OpenWindow(D3::ImGuiHooks::D3_ImGuiWin_PlayerEditor);
}
void PlayerEditorShutdown(void) {
  D3::ImGuiHooks::CloseWindow(D3::ImGuiHooks::D3_ImGuiWin_PlayerEditor);
}

void EntityEditorInit(const idDict *spawnArgs) {
  D3::ImGuiHooks::OpenWindow(D3::ImGuiHooks::D3_ImGuiWin_EntityEditor);
}
void EntityEditorShutdown(void) {
  D3::ImGuiHooks::CloseWindow(D3::ImGuiHooks::D3_ImGuiWin_EntityEditor);
}

void ScriptEditorInit(const idDict *spawnArgs) {
  common->Printf("The Script Editor is still not implemented in imgui\n");
}
void ScriptEditorShutdown(void) {}
void ScriptEditorRun(void) {}

void DeclBrowserInit(const idDict *spawnArgs) {
  common->Printf("The Declaration Browser is still not implemented in imgui\n");
}
void DeclBrowserShutdown(void) {}
void DeclBrowserRun(void) {}
void DeclBrowserReloadDeclarations(void) {}

void GUIEditorInit(void) {
  common->Printf("The GUI Editor is still not implemented in imgui\n");
}
void GUIEditorShutdown(void) {}
void GUIEditorRun(void) {}
bool GUIEditorHandleMessage(void *msg) { return false; }

void DebuggerClientLaunch(void) {}
void DebuggerClientInit(const char *cmdline) {
  common->Printf(
      "The Script Debugger Client is still not implemented in imgui\n");
}

void PDAEditorInit(const idDict *spawnArgs) {
  common->Printf("The PDA editor is still not implemented in imgui\n");
}

void MaterialEditorInit() {
  common->Printf("The Material editor is still not implemented in imgui\n");
}
void MaterialEditorPrintConsole(const char *text) {}
