#pragma once
#include "Application/IEditorWindow.h"
#include "Graphics/Textures/Texture3D.h"

/**
 * Handles displaying debug information
 */
class DebugWindow final : public IEditorWindow {
public:
	MAKE_PTRS(DebugWindow);
	DebugWindow();
	virtual ~DebugWindow();

	// Inherited from IEditorWindow

	virtual void RenderMenuBar() override;

	void SetWarmCC(Texture3D::Sptr lut);
	void SetCoolCC(Texture3D::Sptr lut);
	void SetCustomCC(Texture3D::Sptr lut);
protected:

	Texture3D::Sptr warm;
	Texture3D::Sptr cool;
	Texture3D::Sptr custom;
};