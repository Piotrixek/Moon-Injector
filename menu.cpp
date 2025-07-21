#include "menu.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "icons.h"
#include "ultralight_controller.h"
#include <memory>
#include <Windows.h>

// this links the g_ultralight_controller from main.cpp
extern std::unique_ptr<UltralightController> g_ultralight_controller;

int ImGuiKeyToVK(ImGuiKey key)
{
	switch (key)
	{
	case ImGuiKey_Tab: return VK_TAB;
	case ImGuiKey_LeftArrow: return VK_LEFT;
	case ImGuiKey_RightArrow: return VK_RIGHT;
	case ImGuiKey_UpArrow: return VK_UP;
	case ImGuiKey_DownArrow: return VK_DOWN;
	case ImGuiKey_PageUp: return VK_PRIOR;
	case ImGuiKey_PageDown: return VK_NEXT;
	case ImGuiKey_Home: return VK_HOME;
	case ImGuiKey_End: return VK_END;
	case ImGuiKey_Insert: return VK_INSERT;
	case ImGuiKey_Delete: return VK_DELETE;
	case ImGuiKey_Backspace: return VK_BACK;
	case ImGuiKey_Space: return VK_SPACE;
	case ImGuiKey_Enter: return VK_RETURN;
	case ImGuiKey_Escape: return VK_ESCAPE;
	case ImGuiKey_A: return 'A'; case ImGuiKey_B: return 'B'; case ImGuiKey_C: return 'C';
	case ImGuiKey_D: return 'D'; case ImGuiKey_E: return 'E'; case ImGuiKey_F: return 'F';
	case ImGuiKey_G: return 'G'; case ImGuiKey_H: return 'H'; case ImGuiKey_I: return 'I';
	case ImGuiKey_J: return 'J'; case ImGuiKey_K: return 'K'; case ImGuiKey_L: return 'L';
	case ImGuiKey_M: return 'M'; case ImGuiKey_N: return 'N'; case ImGuiKey_O: return 'O';
	case ImGuiKey_P: return 'P'; case ImGuiKey_Q: return 'Q'; case ImGuiKey_R: return 'R';
	case ImGuiKey_S: return 'S'; case ImGuiKey_T: return 'T'; case ImGuiKey_U: return 'U';
	case ImGuiKey_V: return 'V'; case ImGuiKey_W: return 'W'; case ImGuiKey_X: return 'X';
	case ImGuiKey_Y: return 'Y'; case ImGuiKey_Z: return 'Z';
	case ImGuiKey_0: return '0'; case ImGuiKey_1: return '1'; case ImGuiKey_2: return '2';
	case ImGuiKey_3: return '3'; case ImGuiKey_4: return '4'; case ImGuiKey_5: return '5';
	case ImGuiKey_6: return '6'; case ImGuiKey_7: return '7'; case ImGuiKey_8: return '8';
	case ImGuiKey_9: return '9';
	case ImGuiKey_F1: return VK_F1; case ImGuiKey_F2: return VK_F2; case ImGuiKey_F3: return VK_F3;
	case ImGuiKey_F4: return VK_F4; case ImGuiKey_F5: return VK_F5; case ImGuiKey_F6: return VK_F6;
	case ImGuiKey_F7: return VK_F7; case ImGuiKey_F8: return VK_F8; case ImGuiKey_F9: return VK_F9;
	case ImGuiKey_F10: return VK_F10; case ImGuiKey_F11: return VK_F11; case ImGuiKey_F12: return VK_F12;
	default: return 0;
	}
}

int GetMods()
{
	auto& io = ImGui::GetIO();
	int mods = 0;
	if (io.KeyAlt) mods |= ultralight::KeyEvent::kMod_AltKey;
	if (io.KeyCtrl) mods |= ultralight::KeyEvent::kMod_CtrlKey;
	if (io.KeyShift) mods |= ultralight::KeyEvent::kMod_ShiftKey;
	if (io.KeySuper) mods |= ultralight::KeyEvent::kMod_MetaKey;
	return mods;
}

void ShowApp(bool* p_open)
{
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	ImGuiWindowFlags host_window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBackground;

	ImGui::Begin("DockspaceHost", NULL, host_window_flags);
	ImGui::PopStyleVar(3);

	ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
	ImGui::End();

	ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_Once);

	ImGui::PushStyleColor(ImGuiCol_ResizeGrip, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_ResizeGripHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

	// added the noresize and nobackground flags here
	ImGui::Begin("MoonInjector", p_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground);

	ImVec2 content_pos = ImGui::GetCursorScreenPos();
	ImVec2 content_size = ImGui::GetContentRegionAvail();
	if (content_size.x < 1) content_size.x = 1;
	if (content_size.y < 1) content_size.y = 1;

	if (g_ultralight_controller)
	{
		g_ultralight_controller->Resize((int)content_size.x, (int)content_size.y);

		g_ultralight_controller->Update();
		g_ultralight_controller->Render();

		ID3D11ShaderResourceView* texture = g_ultralight_controller->getTextureView();
		if (texture)
		{
			ImGui::GetWindowDrawList()->AddImage(
				(ImTextureID)reinterpret_cast<uintptr_t>(texture),
				content_pos,
				ImVec2(content_pos.x + content_size.x, content_pos.y + content_size.y)
			);
		}
	}

	if (g_ultralight_controller && g_ultralight_controller->IsLoaded() && ImGui::IsWindowFocused())
	{
		ultralight::View* view = g_ultralight_controller->GetView();
		if (view) {
			ImGuiIO& io = ImGui::GetIO();
			ImRect content_rect(content_pos, ImVec2(content_pos.x + content_size.x, content_pos.y + content_size.y));

			if (ImGui::IsMouseHoveringRect(content_rect.Min, content_rect.Max))
			{
				ultralight::MouseEvent evt;
				evt.x = (int)(io.MousePos.x - content_pos.x);
				evt.y = (int)(io.MousePos.y - content_pos.y);

				if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				{
					evt.type = ultralight::MouseEvent::kType_MouseDown;
					evt.button = ultralight::MouseEvent::kButton_Left;
					view->FireMouseEvent(evt);
				}
				else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
				{
					evt.type = ultralight::MouseEvent::kType_MouseUp;
					evt.button = ultralight::MouseEvent::kButton_Left;
					view->FireMouseEvent(evt);
				}
				else
				{
					evt.type = ultralight::MouseEvent::kType_MouseMoved;
					view->FireMouseEvent(evt);
				}

				if (io.MouseWheel != 0.0f || io.MouseWheelH != 0.0f)
				{
					ultralight::ScrollEvent scroll;
					scroll.type = ultralight::ScrollEvent::kType_ScrollByPixel;
					scroll.delta_y = (int)(io.MouseWheel * 100);
					scroll.delta_x = (int)(io.MouseWheelH * 100);
					view->FireScrollEvent(scroll);
				}
			}

			int mods = GetMods();
			for (int i = ImGuiKey_NamedKey_BEGIN; i < ImGuiKey_NamedKey_END; ++i)
			{
				ImGuiKey key = static_cast<ImGuiKey>(i);
				int vk = ImGuiKeyToVK(key);
				if (vk == 0) continue;

				ultralight::KeyEvent key_evt;
				key_evt.virtual_key_code = vk;
				key_evt.native_key_code = vk;
				key_evt.modifiers = mods;

				if (ImGui::IsKeyPressed(key, false))
				{
					key_evt.type = ultralight::KeyEvent::kType_RawKeyDown;
					view->FireKeyEvent(key_evt);
				}
				if (ImGui::IsKeyReleased(key))
				{
					key_evt.type = ultralight::KeyEvent::kType_KeyUp;
					view->FireKeyEvent(key_evt);
				}
			}

			if (!io.InputQueueCharacters.empty())
			{
				for (int i = 0; i < io.InputQueueCharacters.Size; i++)
				{
					ImWchar c = io.InputQueueCharacters[i];
					if (c == 0 || c >= 0xF700) continue;
					ultralight::KeyEvent char_evt;
					char_evt.type = ultralight::KeyEvent::kType_Char;
					char_evt.text = ultralight::String16((ultralight::Char16*)&c, 1);
					view->FireKeyEvent(char_evt);
				}
			}
		}
	}

	ImGui::End();
	ImGui::PopStyleColor(2);
}
