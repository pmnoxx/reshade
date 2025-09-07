/*
 * Copyright (C) 2024 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include <imgui.h>
#include <reshade.hpp>
#include <Windows.h>
#include <Xinput.h>
#include <string>
#include <vector>
#include <cstring>
#include <thread>
#include <chrono>

static bool s_enable_logging = true;
static bool s_show_overlay = true;
static std::vector<std::string> s_log_messages;
static const size_t MAX_LOG_MESSAGES = 50;

// Vibration control
static bool s_enable_vibration = true;
static float s_vibration_intensity = 1.0f;
static bool s_test_vibration = false;

// Deadzone override control
static bool s_enable_deadzone_override = false;
static int s_left_thumb_deadzone = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
static int s_right_thumb_deadzone = XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;
static int s_trigger_threshold = XINPUT_GAMEPAD_TRIGGER_THRESHOLD;

// Gamepad state tracking
struct GamepadState
{
	bool connected = false;
	XINPUT_STATE state = {};
	XINPUT_STATE last_state = {};
	bool buttons_changed = false;
	bool triggers_changed = false;
	bool thumbsticks_changed = false;
	DWORD last_check_time = 0; // For periodic connection checks
};

static GamepadState s_gamepad_states[4]; // Support up to 4 controllers
static const DWORD CONNECTION_CHECK_INTERVAL = 1000; // Check every 1 second

static void log_message(const std::string& message)
{
	if (!s_enable_logging)
		return;

	s_log_messages.push_back(message);
	if (s_log_messages.size() > MAX_LOG_MESSAGES)
		s_log_messages.erase(s_log_messages.begin());
}

static void check_controller_connections()
{
	DWORD current_time = GetTickCount();

	for (int i = 0; i < 4; i++)
	{
		GamepadState& gamepad = s_gamepad_states[i];

		// Only check periodically to avoid excessive calls
		if (current_time - gamepad.last_check_time < CONNECTION_CHECK_INTERVAL)
			continue;

		gamepad.last_check_time = current_time;

		XINPUT_STATE test_state;
		DWORD result = XInputGetState(i, &test_state);
		bool is_connected = (result == ERROR_SUCCESS);

		if (is_connected != gamepad.connected)
		{
			gamepad.connected = is_connected;
		/*	if (is_connected)
			{
				log_message("Controller " + std::to_string(i) + " CONNECTED");
			}
			else
			{
				log_message("Controller " + std::to_string(i) + " DISCONNECTED");
			}*/
		}
	}
}

static void apply_deadzone_overrides(XINPUT_STATE* state)
{
	if (!s_enable_deadzone_override || state == nullptr)
		return;

	// Apply left thumbstick deadzone
	if (abs(state->Gamepad.sThumbLX) < s_left_thumb_deadzone)
		state->Gamepad.sThumbLX = 0;
	if (abs(state->Gamepad.sThumbLY) < s_left_thumb_deadzone)
		state->Gamepad.sThumbLY = 0;

	// Apply right thumbstick deadzone
	if (abs(state->Gamepad.sThumbRX) < s_right_thumb_deadzone)
		state->Gamepad.sThumbRX = 0;
	if (abs(state->Gamepad.sThumbRY) < s_right_thumb_deadzone)
		state->Gamepad.sThumbRY = 0;

	// Apply trigger threshold
	if (state->Gamepad.bLeftTrigger < s_trigger_threshold)
		state->Gamepad.bLeftTrigger = 0;
	if (state->Gamepad.bRightTrigger < s_trigger_threshold)
		state->Gamepad.bRightTrigger = 0;
}

static std::string get_button_name(WORD button)
{
	switch (button)
	{
	case XINPUT_GAMEPAD_DPAD_UP: return "DPAD_UP";
	case XINPUT_GAMEPAD_DPAD_DOWN: return "DPAD_DOWN";
	case XINPUT_GAMEPAD_DPAD_LEFT: return "DPAD_LEFT";
	case XINPUT_GAMEPAD_DPAD_RIGHT: return "DPAD_RIGHT";
	case XINPUT_GAMEPAD_START: return "START";
	case XINPUT_GAMEPAD_BACK: return "BACK";
	case XINPUT_GAMEPAD_LEFT_THUMB: return "LEFT_THUMB";
	case XINPUT_GAMEPAD_RIGHT_THUMB: return "RIGHT_THUMB";
	case XINPUT_GAMEPAD_LEFT_SHOULDER: return "LEFT_SHOULDER";
	case XINPUT_GAMEPAD_RIGHT_SHOULDER: return "RIGHT_SHOULDER";
	case XINPUT_GAMEPAD_A: return "A";
	case XINPUT_GAMEPAD_B: return "B";
	case XINPUT_GAMEPAD_X: return "X";
	case XINPUT_GAMEPAD_Y: return "Y";
	default: return "UNKNOWN";
	}
}

static void check_button_changes(const XINPUT_STATE& current, const XINPUT_STATE& last, int controller_index)
{
	WORD current_buttons = current.Gamepad.wButtons;
	WORD last_buttons = last.Gamepad.wButtons;
	WORD changed_buttons = current_buttons ^ last_buttons;

	if (changed_buttons == 0)
		return;

	s_gamepad_states[controller_index].buttons_changed = true;

	// Check for button presses and releases
	for (int i = 0; i < 16; i++)
	{
		WORD button_mask = 1 << i;
		if (changed_buttons & button_mask)
		{
			bool is_pressed = (current_buttons & button_mask) != 0;
			std::string button_name = get_button_name(button_mask);

			if (is_pressed)
			{
				log_message("Controller " + std::to_string(controller_index) + ": " + button_name + " PRESSED");
			}
			else
			{
				log_message("Controller " + std::to_string(controller_index) + ": " + button_name + " RELEASED");
			}
		}
	}
}

static void check_trigger_changes(const XINPUT_STATE& current, const XINPUT_STATE& last, int controller_index)
{
	bool left_changed = current.Gamepad.bLeftTrigger != last.Gamepad.bLeftTrigger;
	bool right_changed = current.Gamepad.bRightTrigger != last.Gamepad.bRightTrigger;

	if (left_changed || right_changed)
	{
		s_gamepad_states[controller_index].triggers_changed = true;

		if (left_changed)
		{
			log_message("Controller " + std::to_string(controller_index) +
				": LEFT_TRIGGER = " + std::to_string(current.Gamepad.bLeftTrigger));
		}
		if (right_changed)
		{
			log_message("Controller " + std::to_string(controller_index) +
				": RIGHT_TRIGGER = " + std::to_string(current.Gamepad.bRightTrigger));
		}
	}
}

static void check_thumbstick_changes(const XINPUT_STATE& current, const XINPUT_STATE& last, int controller_index)
{
	bool left_x_changed = current.Gamepad.sThumbLX != last.Gamepad.sThumbLX;
	bool left_y_changed = current.Gamepad.sThumbLY != last.Gamepad.sThumbLY;
	bool right_x_changed = current.Gamepad.sThumbRX != last.Gamepad.sThumbRX;
	bool right_y_changed = current.Gamepad.sThumbRY != last.Gamepad.sThumbRY;

	if (left_x_changed || left_y_changed || right_x_changed || right_y_changed)
	{
		s_gamepad_states[controller_index].thumbsticks_changed = true;

		if (left_x_changed || left_y_changed)
		{
			log_message("Controller " + std::to_string(controller_index) +
				": LEFT_THUMB = (" + std::to_string(current.Gamepad.sThumbLX) +
				", " + std::to_string(current.Gamepad.sThumbLY) + ")");
		}
		if (right_x_changed || right_y_changed)
		{
			log_message("Controller " + std::to_string(controller_index) +
				": RIGHT_THUMB = (" + std::to_string(current.Gamepad.sThumbRX) +
				", " + std::to_string(current.Gamepad.sThumbRY) + ")");
		}
	}
}

static bool on_xinput_get_state(uint32_t dwUserIndex, void* pState)
{
	if (dwUserIndex >= 4 || pState == nullptr)
		return false;

	// Check all controller connections periodically
	check_controller_connections();

	XINPUT_STATE* state = static_cast<XINPUT_STATE*>(pState);
	GamepadState& gamepad = s_gamepad_states[dwUserIndex];

	// Store the last state for comparison
	gamepad.last_state = gamepad.state;
	gamepad.state = *state;

	// Apply deadzone overrides if enabled
	apply_deadzone_overrides(&gamepad.state);
	apply_deadzone_overrides(state); // Also apply to the original state

	// If we're getting state data, the controller is connected
	gamepad.connected = true;

	// Reset change flags
	gamepad.buttons_changed = false;
	gamepad.triggers_changed = false;
	gamepad.thumbsticks_changed = false;

	// Check for changes if we have a previous state
	if (state->dwPacketNumber != gamepad.last_state.dwPacketNumber)
	{
		check_button_changes(gamepad.state, gamepad.last_state, dwUserIndex);
		check_trigger_changes(gamepad.state, gamepad.last_state, dwUserIndex);
		check_thumbstick_changes(gamepad.state, gamepad.last_state, dwUserIndex);
	}

	return false; // Don't modify the state, just observe
}

static bool on_xinput_set_state(uint32_t dwUserIndex, void* pVibration)
{
	if (dwUserIndex >= 4 || pVibration == nullptr)
		return false;

	XINPUT_VIBRATION* vibration = static_cast<XINPUT_VIBRATION*>(pVibration);

	if (s_enable_logging)
	{
		log_message("Controller " + std::to_string(dwUserIndex) +
			": VIBRATION SET - Left=" + std::to_string(vibration->wLeftMotorSpeed) +
			" Right=" + std::to_string(vibration->wRightMotorSpeed));
	}

	// Apply vibration intensity if enabled
	if (s_enable_vibration && s_vibration_intensity != 1.0f)
	{
		vibration->wLeftMotorSpeed = static_cast<WORD>(vibration->wLeftMotorSpeed * s_vibration_intensity);
		vibration->wRightMotorSpeed = static_cast<WORD>(vibration->wRightMotorSpeed * s_vibration_intensity);
	}

	// If vibration is disabled, prevent it from being sent
	return !s_enable_vibration;
}

static bool on_xinput_get_state_ex(uint32_t dwUserIndex, void* pState)
{
	if (dwUserIndex >= 4 || pState == nullptr)
		return false;

	if (s_enable_logging)
	{
		// XInputGetStateEx provides access to the Guide button (Xbox logo button)
		// which is not available in standard XInputGetState
		log_message("Controller " + std::to_string(dwUserIndex) + ": XInputGetStateEx called (Guide button access)");

		// Note: The actual structure would be XINPUT_STATE_EX which includes
		// the Guide button state in addition to all standard XInput data
		// This is accessed via ordinal 100 in the XInput DLL
	}

	// Don't prevent the extended state from being retrieved
	return false;
}

static void draw_overlay(reshade::api::effect_runtime*)
{
	if (!s_show_overlay)
		return;

	// Check controller connections periodically
	check_controller_connections();

	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("XInput Events Monitor", &s_show_overlay))
	{
		ImGui::Checkbox("Enable Logging", &s_enable_logging);
		ImGui::SameLine();
		if (ImGui::Button("Clear Log"))
		{
			s_log_messages.clear();
		}

		ImGui::Separator();

		// Vibration controls
		ImGui::Text("Vibration Controls:");
		ImGui::Checkbox("Enable Vibration", &s_enable_vibration);
		ImGui::SameLine();
		if (ImGui::Button("Test Vibration"))
		{
			s_test_vibration = true;
		}

		ImGui::SliderFloat("Vibration Intensity", &s_vibration_intensity, 0.0f, 2.0f, "%.2f");
		ImGui::SetItemTooltip("Multiplier for vibration intensity (0.0 = no vibration, 1.0 = normal, 2.0 = double)");

		ImGui::Separator();

		// Deadzone override controls
		ImGui::Text("Deadzone Override:");
		ImGui::Checkbox("Enable Deadzone Override", &s_enable_deadzone_override);
		ImGui::SetItemTooltip("Override default deadzone values for all controllers");

		if (s_enable_deadzone_override)
		{
			ImGui::Indent();

			ImGui::SliderInt("Left Thumbstick Deadzone", &s_left_thumb_deadzone, 0, 32767, "%d");
			ImGui::SetItemTooltip("Deadzone for left thumbstick (0 = no deadzone, 32767 = full range)");

			ImGui::SliderInt("Right Thumbstick Deadzone", &s_right_thumb_deadzone, 0, 32767, "%d");
			ImGui::SetItemTooltip("Deadzone for right thumbstick (0 = no deadzone, 32767 = full range)");

			ImGui::SliderInt("Trigger Threshold", &s_trigger_threshold, 0, 255, "%d");
			ImGui::SetItemTooltip("Threshold for trigger activation (0 = no threshold, 255 = full press)");

			ImGui::SameLine();
			if (ImGui::Button("Reset to Defaults"))
			{
				s_left_thumb_deadzone = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
				s_right_thumb_deadzone = XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;
				s_trigger_threshold = XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
			}

			ImGui::Unindent();
		}

		ImGui::Separator();

		// XInputGetStateEx information
		ImGui::Text("XInputGetStateEx Information:");
		ImGui::Indent();
		ImGui::Text("• Provides access to Guide button (Xbox logo button)");
		ImGui::Text("• Not available in standard XInputGetState");
		ImGui::Text("• Accessed via ordinal 100 in XInput DLL");
		ImGui::Text("• Undocumented Microsoft function");
		ImGui::Unindent();

		ImGui::Separator();

		// Display controller status
		ImGui::Text("Controller Status:");
		for (int i = 0; i < 4; i++)
		{
			const GamepadState& gamepad = s_gamepad_states[i];
			ImGui::Text("Controller %d: %s", i, gamepad.connected ? "Connected" : "Disconnected");

			if (gamepad.connected)
			{
				ImGui::SameLine();
				ImGui::Text("(Packet: %u)", gamepad.state.dwPacketNumber);
			}
		}

		ImGui::Separator();

		// Display current button states for connected controllers
		ImGui::Text("Current Button States:");
		for (int i = 0; i < 4; i++)
		{
			const GamepadState& gamepad = s_gamepad_states[i];
			if (!gamepad.connected)
				continue;

			ImGui::Text("Controller %d:", i);
			ImGui::Indent();

			WORD buttons = gamepad.state.Gamepad.wButtons;
			if (buttons != 0)
			{
				ImGui::Text("Buttons: ");
				ImGui::SameLine();

				bool first = true;
				for (int j = 0; j < 16; j++)
				{
					WORD button_mask = 1 << j;
					if (buttons & button_mask)
					{
						if (!first)
							ImGui::SameLine();
						ImGui::Text("%s", get_button_name(button_mask).c_str());
						first = false;
					}
				}
			}
			else
			{
				ImGui::Text("No buttons pressed");
			}

			// Display trigger values
			ImGui::Text("Triggers: L=%.0f R=%.0f",
				gamepad.state.Gamepad.bLeftTrigger / 255.0f * 100.0f,
				gamepad.state.Gamepad.bRightTrigger / 255.0f * 100.0f);

			// Display thumbstick values
			ImGui::Text("Thumbsticks: L(%.1f,%.1f) R(%.1f,%.1f)",
				gamepad.state.Gamepad.sThumbLX / 32767.0f,
				gamepad.state.Gamepad.sThumbLY / 32767.0f,
				gamepad.state.Gamepad.sThumbRX / 32767.0f,
				gamepad.state.Gamepad.sThumbRY / 32767.0f);

			// Display deadzone information from last GetState
			ImGui::Text("Deadzone Status:");
			ImGui::Indent();

			// Left thumbstick deadzone
			bool left_x_dead = (abs(gamepad.state.Gamepad.sThumbLX) < s_left_thumb_deadzone);
			bool left_y_dead = (abs(gamepad.state.Gamepad.sThumbLY) < s_left_thumb_deadzone);
			ImGui::Text("Left: X=%s Y=%s (Deadzone: %d)",
				left_x_dead ? "DEAD" : "ACTIVE",
				left_y_dead ? "DEAD" : "ACTIVE",
				s_left_thumb_deadzone);

			// Right thumbstick deadzone
			bool right_x_dead = (abs(gamepad.state.Gamepad.sThumbRX) < s_right_thumb_deadzone);
			bool right_y_dead = (abs(gamepad.state.Gamepad.sThumbRY) < s_right_thumb_deadzone);
			ImGui::Text("Right: X=%s Y=%s (Deadzone: %d)",
				right_x_dead ? "DEAD" : "ACTIVE",
				right_y_dead ? "DEAD" : "ACTIVE",
				s_right_thumb_deadzone);

			// Trigger deadzone
			bool left_trigger_dead = (gamepad.state.Gamepad.bLeftTrigger < s_trigger_threshold);
			bool right_trigger_dead = (gamepad.state.Gamepad.bRightTrigger < s_trigger_threshold);
			ImGui::Text("Triggers: L=%s R=%s (Threshold: %d)",
				left_trigger_dead ? "DEAD" : "ACTIVE",
				right_trigger_dead ? "DEAD" : "ACTIVE",
				s_trigger_threshold);

			ImGui::Unindent();

			ImGui::Unindent();
		}

		ImGui::Separator();

		// Display log messages
		ImGui::Text("Event Log:");
		ImGui::BeginChild("Log", ImVec2(0, 0), true);
		for (const auto& message : s_log_messages)
		{
			ImGui::Text("%s", message.c_str());
		}
		ImGui::EndChild();
	}
	ImGui::End();

	// Handle test vibration
	if (s_test_vibration)
	{
		s_test_vibration = false;
		// Test vibration on all connected controllers
		for (int i = 0; i < 4; i++)
		{
			if (s_gamepad_states[i].connected)
			{
				XINPUT_VIBRATION vibration = {};
				vibration.wLeftMotorSpeed = static_cast<WORD>(32767 * s_vibration_intensity);
				vibration.wRightMotorSpeed = static_cast<WORD>(32767 * s_vibration_intensity);
				XInputSetState(i, &vibration);

				// Stop vibration after a short delay
				std::thread([i]() {
					std::this_thread::sleep_for(std::chrono::milliseconds(500));
					XINPUT_VIBRATION stop_vibration = {};
					XInputSetState(i, &stop_vibration);
				}).detach();
			}
		}
	}
}

extern "C" __declspec(dllexport) const char* NAME = "XInput Events Monitor";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "Example add-on that monitors and logs XInput gamepad events, including vibration control and XInputGetStateEx calls (Guide button access). Displays controller states, input changes, deadzone information, and allows vibration testing and intensity adjustment.";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;
		reshade::register_event<reshade::addon_event::xinput_get_state>(on_xinput_get_state);
		reshade::register_event<reshade::addon_event::xinput_set_state>(on_xinput_set_state);
		reshade::register_event<reshade::addon_event::xinput_get_state_ex>(on_xinput_get_state_ex);
		reshade::register_overlay(nullptr, draw_overlay);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}
