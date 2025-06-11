/*
 * Copyright (C) 2024 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include <reshade.hpp>
#include <cstdlib>

struct format_upgrade_mapping
{
	reshade::api::format old_format;
	reshade::api::format new_format;
	const char *config_name;
};

static const format_upgrade_mapping format_upgrades[] = {
	{ reshade::api::format::r8g8b8a8_typeless, reshade::api::format::r16g16b16a16_float, "upgrade_res_r8g8b8a8_typeless" },
	{ reshade::api::format::r8g8b8a8_unorm, reshade::api::format::r16g16b16a16_float, "upgrade_res_r8g8b8a8_unorm" },
	{ reshade::api::format::r11g11b10_float, reshade::api::format::r16g16b16a16_float, "upgrade_res_r11g11b10_float" },
	{ reshade::api::format::r10g10b10a2_unorm, reshade::api::format::r16g16b16a16_float, "upgrade_res_r10g10b10a2_unorm" }
};

static bool on_create_swapchain(reshade::api::device_api, reshade::api::swapchain_desc &desc, void *)
{
	bool modified = false;

	if (bool force_vsync;
		reshade::get_config_value(nullptr, "APP", "ForceVsync", force_vsync))
	{
		if (force_vsync)
		{
			desc.sync_interval = 1;
			modified = true;
		}
	}
	else
	{
		reshade::set_config_value(nullptr, "APP", "ForceVsync", "0");
	}

	if (bool force_windowed;
		reshade::get_config_value(nullptr, "APP", "ForceWindowed", force_windowed))
	{
		if (force_windowed)
		{
			desc.fullscreen_state = false;
			modified = true;
		}
	}
	else
	{
		reshade::set_config_value(nullptr, "APP", "ForceWindowed", "0");
	}

	if (bool force_fullscreen;
		reshade::get_config_value(nullptr, "APP", "ForceFullscreen", force_fullscreen))
	{
		if (force_fullscreen)
		{
			desc.fullscreen_state = true;
			modified = true;
		}
	}
	else
	{
		reshade::set_config_value(nullptr, "APP", "ForceFullscreen", "0");
	}

	if (bool force_10bit_format;
		reshade::get_config_value(nullptr, "APP", "Force10BitFormat", force_10bit_format))
	{
		if (force_10bit_format)
		{
			desc.back_buffer.texture.format = reshade::api::format::r10g10b10a2_unorm;
			modified = true;
		}
	}
	else
	{
		reshade::set_config_value(nullptr, "APP", "Force10BitFormat", "0");
	}
	
	if (bool force_12bit_format;
		reshade::get_config_value(nullptr, "APP", "Force12BitFormat", force_12bit_format))
	{
		if (force_12bit_format)
		{
			desc.back_buffer.texture.format = reshade::api::format::r11g11b10_float;
			modified = true;
		}
	} 
	else
	{
		reshade::set_config_value(nullptr, "APP", "Force12BitFormat", "0");
	}

	if (bool force_16bit_format;
		reshade::get_config_value(nullptr, "APP", "Force16BitFormat", force_16bit_format))
	{
		if (force_16bit_format)
		{
			desc.back_buffer.texture.format = reshade::api::format::r16g16b16a16_float;
			modified = true;
		}
	}
	else
	{
		reshade::set_config_value(nullptr, "APP", "Force16BitFormat", "0");
	}

	if (bool force_default_refresh_rate;
		reshade::get_config_value(nullptr, "APP", "ForceDefaultRefreshRate", force_default_refresh_rate))
	{
		if (force_default_refresh_rate)
		{
			desc.fullscreen_refresh_rate = 0.0f;
			modified = true;
		}
	}
	else
	{
		reshade::set_config_value(nullptr, "APP", "ForceDefaultRefreshRate", "0");
	}


	char force_resolution_string[32], *force_resolution_p = force_resolution_string;
	size_t force_resolution_string_size = sizeof(force_resolution_string);
	if (reshade::get_config_value(nullptr, "APP", "ForceResolution", force_resolution_string, &force_resolution_string_size))
	{
		const unsigned long width = std::strtoul(force_resolution_p, &force_resolution_p, 10);
		const char width_terminator = *force_resolution_p++;
		const unsigned long height = std::strtoul(force_resolution_p, &force_resolution_p, 10);
		const char height_terminator = *force_resolution_p++;

		if (width != 0 && width_terminator == '\0' &&
			height != 0 && height_terminator == '\0' &&
			static_cast<size_t>(force_resolution_p - force_resolution_string) == force_resolution_string_size)
		{
			desc.back_buffer.texture.width = static_cast<uint32_t>(width);
			desc.back_buffer.texture.height = static_cast<uint32_t>(height);
			modified = true;
		}
	}
	else
	{
		reshade::set_config_value(nullptr, "APP", "ForceResolution", "0,0");
	}
	for (const auto &upgrade : format_upgrades)
	{
		bool should_upgrade;
		if (!reshade::get_config_value(nullptr, "APP", upgrade.config_name, should_upgrade)) {
			reshade::set_config_value(nullptr, "APP", upgrade.config_name, "0");
		}
	}


	return modified;
}

static bool on_set_fullscreen_state(reshade::api::swapchain *, bool fullscreen, void *)
{
	if (bool force_windowed;
		reshade::get_config_value(nullptr, "APP", "ForceWindowed", force_windowed))
	{
		if (force_windowed && fullscreen)
			return true; // Prevent entering fullscreen mode
	}
	if (bool force_fullscreen;
		reshade::get_config_value(nullptr, "APP", "ForceFullscreen", force_fullscreen))
	{
		if (force_fullscreen && !fullscreen)
			return true; // Prevent leaving fullscreen mode
	}

	return false;
}

static bool is_4k_aspect_ratio(uint32_t width, uint32_t height)
{
	// 4K resolution is 3840x2160, which has an aspect ratio of 16:9
	// We'll check if the texture has the same aspect ratio
	const float target_aspect = 3840.0f / 2160.0f;
	const float texture_aspect = static_cast<float>(width) / static_cast<float>(height);
	
	// Allow for small floating point differences
	const float aspect_tolerance = 0.01f;
	return std::abs(texture_aspect - target_aspect) < aspect_tolerance;
}

static bool on_create_resource(reshade::api::device *device, reshade::api::resource_desc &desc, reshade::api::subresource_data *initial_data, reshade::api::resource_usage initial_state)
{
	bool modified = false;

	// Only process textures with 4K aspect ratio
	if (desc.type == reshade::api::resource_type::texture_2d && 
		is_4k_aspect_ratio(desc.texture.width, desc.texture.height))
	{
		for (const auto &upgrade : format_upgrades)
		{
			if (desc.texture.format == upgrade.old_format)
			{
				if (bool should_upgrade = false; 
					reshade::get_config_value(nullptr, "APP", upgrade.config_name, should_upgrade))
				{
					if (should_upgrade)
					{
						desc.texture.format = upgrade.new_format;
						modified = true;
						break;
					}
				}
			}
		}
	}

	return modified;
}

static bool on_create_resource_view(reshade::api::device *device, reshade::api::resource resource, reshade::api::resource_usage usage_type, reshade::api::resource_view_desc &desc)
{
	bool modified = false;

	// Get the resource description to check dimensions
	reshade::api::resource_desc resource_desc = device->get_resource_desc(resource);
	if (//resource_desc &&
		resource_desc.type == reshade::api::resource_type::texture_2d &&
		is_4k_aspect_ratio(resource_desc.texture.width, resource_desc.texture.height))
	{
		for (const auto &upgrade : format_upgrades)
		{
			if (desc.format == upgrade.old_format)
			{
				if (bool should_upgrade = false;
					reshade::get_config_value(nullptr, "APP", upgrade.config_name, should_upgrade))
				{
					if (should_upgrade)
					{
						desc.format = upgrade.new_format;
						modified = true;
						break;
					}
				}
			}
		}
	}

	return modified;
}

extern "C" __declspec(dllexport) const char *NAME = "Swap chain override";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Adds options to force the application into windowed or fullscreen mode, or force a specific resolution or the default refresh rate.\n\n"
	"These are controlled via ReShade.ini:\n"
	"[APP]\n"
	"ForceVsync=<0/1>\n"
	"ForceWindowed=<0/1>\n"
	"ForceFullscreen=<0/1>\n"
	"Force10BitFormat=<0/1> - Forces 10-bit color format for swap chain\n"
	"Force12BitFormat=<0/1> - Forces 12-bit color format for swap chain\n"
	"Force16BitFormat=<0/1> - Forces 16-bit color format for swap chain\n"
	"ForceDefaultRefreshRate=<0/1>\n"
	"ForceResolution=<width>,<height>\n\n"
	"Resource format upgrades (applies to all resources, not just swap chain):\n"
	"upgrade_res_r8g8b8a8_typeless=<0/1> - Upgrades R8G8B8A8 typeless to R16G16B16A16 float\n"
	"upgrade_res_r8g8b8a8_unorm=<0/1> - Upgrades R8G8B8A8 unorm to R16G16B16A16 float\n"
	"upgrade_res_r11g11b10_float=<0/1> - Upgrades R11G11B10 float to R16G16B16A16 float\n"
	"upgrade_res_r10g10b10a2_unorm=<0/1> - Upgrades R10G10B10A2 unorm to R16G16B16A16 float";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;
		reshade::register_event<reshade::addon_event::create_swapchain>(on_create_swapchain);
		reshade::register_event<reshade::addon_event::set_fullscreen_state>(on_set_fullscreen_state);
		reshade::register_event<reshade::addon_event::create_resource>(on_create_resource);
		reshade::register_event<reshade::addon_event::create_resource_view>(on_create_resource_view);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}
