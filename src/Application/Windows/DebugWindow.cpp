#include "DebugWindow.h"
#include "Application/Application.h"
#include "Application/ApplicationLayer.h"
#include "Application/Layers/RenderLayer.h"

DebugWindow::DebugWindow() :
	IEditorWindow()
{
	Name = "Debug";
	SplitDirection = ImGuiDir_::ImGuiDir_None;
	SplitDepth = 0.5f;
	Requirements = EditorWindowRequirements::Menubar;
}

DebugWindow::~DebugWindow() = default;

void DebugWindow::RenderMenuBar() 
{
	Application& app = Application::Get();
	RenderLayer::Sptr renderLayer = app.GetLayer<RenderLayer>(); 

	BulletDebugMode physicsDrawMode = app.CurrentScene()->GetPhysicsDebugDrawMode();
	if (BulletDebugDraw::DrawModeGui("Physics Debug Mode:", physicsDrawMode)) { 
		app.CurrentScene()->SetPhysicsDebugDrawMode(physicsDrawMode);
	}

	ImGui::Separator();

	RenderFlags flags = renderLayer->GetRenderFlags();
	bool changed = false;

	bool temp = *(flags & RenderFlags::NoLights);
	if (ImGui::Checkbox("No Lights", &temp)) {
		changed = true;
		flags = (flags & ~*RenderFlags::NoLights) | (temp ? RenderFlags::NoLights : RenderFlags::None);
		
	}

	temp = *(flags & RenderFlags::AmbientOnly);
	if (ImGui::Checkbox("Enable Ambient Only", &temp)) {
		changed = true;
		flags = (flags & ~*RenderFlags::AmbientOnly) | (temp ? RenderFlags::AmbientOnly : RenderFlags::None);
	}

	temp = *(flags & RenderFlags::SpecularOnly);
	if (ImGui::Checkbox("Enable Specular Only", &temp)) {
		changed = true;
		flags = (flags & ~*RenderFlags::SpecularOnly) | (temp ? RenderFlags::SpecularOnly : RenderFlags::None);
	}

	temp = *(flags & RenderFlags::AmbientSpecular);
	if (ImGui::Checkbox("Enable Ambient and Specular", &temp)) {
		changed = true;
		flags = (flags & ~*RenderFlags::AmbientSpecular) | (temp ? RenderFlags::AmbientSpecular : RenderFlags::None);
	}

	temp = *(flags & RenderFlags::AmbientSpecularShader);
	if (ImGui::Checkbox("Enable Ambient and Specular and Shader", &temp)) {
		changed = true;
		flags = (flags & ~*RenderFlags::AmbientSpecularShader) | (temp ? RenderFlags::AmbientSpecularShader : RenderFlags::None);
	}

	temp = *(flags & RenderFlags::DiffuseWarp);
	if (ImGui::Checkbox("Enable Diffuse Warp", &temp)) {
		changed = true;
		flags = (flags & ~*RenderFlags::DiffuseWarp) | (temp ? RenderFlags::DiffuseWarp : RenderFlags::None);
	}
	
	temp = *(flags & RenderFlags::SpecularWarp);
	if (ImGui::Checkbox("Enable Specular Warp", &temp)) {
		changed = true;
		flags = (flags & ~*RenderFlags::SpecularWarp) | (temp ? RenderFlags::SpecularWarp : RenderFlags::None);
	}

	temp = *(flags & RenderFlags::WarmColorCorrection);
	if (ImGui::Checkbox("Enable Warm Color Correction", &temp)) {
		changed = true;
			app.CurrentScene()->SetColorLUT(warm);
		flags = (flags & ~*RenderFlags::WarmColorCorrection) | (temp ? RenderFlags::WarmColorCorrection : RenderFlags::None);
	}


	temp = *(flags & RenderFlags::CoolColorCorrection);
	if (ImGui::Checkbox("Enable Cool Color Correction", &temp)) {
		changed = true;
		
			app.CurrentScene()->SetColorLUT(cool);
		
		flags = (flags & ~*RenderFlags::CoolColorCorrection) | (temp ? RenderFlags::CoolColorCorrection : RenderFlags::None);
	}


	temp = *(flags & RenderFlags::CustomColorCorrection);
	if (ImGui::Checkbox("Enable Custom Color Correction", &temp)) {
		changed = true;
		
			app.CurrentScene()->SetColorLUT(custom);
			//app.Quit();
		flags = (flags & ~*RenderFlags::CustomColorCorrection) | (temp ? RenderFlags::CustomColorCorrection : RenderFlags::None);
	}


	if (changed) {
		renderLayer->SetRenderFlags(flags);
	}
}

void DebugWindow::SetWarmCC(Texture3D::Sptr lut)
{
	warm = lut;
}

void DebugWindow::SetCoolCC(Texture3D::Sptr lut)
{
	cool = lut;

}

void DebugWindow::SetCustomCC(Texture3D::Sptr lut)
{
	custom = lut;
}
