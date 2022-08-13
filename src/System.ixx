export module System;

import APU;

import NumericalTypes;
import SerializationStream;

import <array>;

export namespace System
{
	void StepAllComponentsButCpu();
	void StreamState(SerializationStream& stream);

	struct Standard
	{
		/* apu */
		std::array<u8, 16> dmc_rate_table;
		std::array<u16, 16> noise_period_table;
		std::array<uint, 8> frame_counter_step_cycle_table;
		/* cpu */
		uint cpu_cycles_per_sec;
		/* ppu */
		bool oam_can_be_written_to_during_forced_blanking;
		bool pre_render_line_is_one_dot_shorter_on_every_other_frame;
		float ppu_dots_per_cpu_cycle;
		int nmi_scanline;
		int num_scanlines;
		int num_scanlines_per_vblank;
		int num_visible_scanlines;
	};

	constexpr Standard standard_ntsc = {
		.dmc_rate_table = { 214, 190, 170, 160, 143, 127, 113, 107, 95, 80, 71, 64, 53, 42, 36, 27 },
		.noise_period_table = { 4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068 },
		.frame_counter_step_cycle_table = { 7457, 14913, 22371, 29828, 29829, 29830, 37281, 37282 },
		.cpu_cycles_per_sec = 1789773,
		.oam_can_be_written_to_during_forced_blanking = true,
		.pre_render_line_is_one_dot_shorter_on_every_other_frame = true,
		.ppu_dots_per_cpu_cycle = 3.0f,
		.nmi_scanline = 241,
		.num_scanlines = 262,
		.num_scanlines_per_vblank = 20,
		.num_visible_scanlines = 240
	};

	constexpr Standard standard_pal = {
		.dmc_rate_table = { 199, 177, 158, 149, 138, 118, 105, 99, 88, 74, 66, 59, 49, 39, 33, 25 },
		.noise_period_table = { 4, 8, 14, 30, 60, 88, 118, 148, 188, 236, 354, 472, 708, 944, 1890, 3778 },
		.frame_counter_step_cycle_table = { 8313, 16627, 24939, 33252, 33253, 33254, 41565, 41566 },
		.cpu_cycles_per_sec = 1662607,
		.oam_can_be_written_to_during_forced_blanking = false,
		.pre_render_line_is_one_dot_shorter_on_every_other_frame = false,
		.ppu_dots_per_cpu_cycle = 3.2f,
		.nmi_scanline = 240,
		.num_scanlines = 312,
		.num_scanlines_per_vblank = 70,
		.num_visible_scanlines = 239
	};

	constexpr Standard standard_dendy = { /* TODO: audio stuff is inaccurate */
		.dmc_rate_table = { 199, 177, 158, 149, 138, 118, 105, 99, 88, 74, 66, 59, 49, 39, 33, 25 },
		.noise_period_table = { 4, 8, 14, 30, 60, 88, 118, 148, 188, 236, 354, 472, 708, 944, 1890, 3778 },
		.frame_counter_step_cycle_table = { 8313, 16627, 24939, 33252, 33253, 33254, 41565, 41566 },
		.cpu_cycles_per_sec = 1662607,
		.oam_can_be_written_to_during_forced_blanking = true,
		.pre_render_line_is_one_dot_shorter_on_every_other_frame = false,
		.ppu_dots_per_cpu_cycle = 3.0f,
		.nmi_scanline = 290,
		.num_scanlines = 312,
		.num_scanlines_per_vblank = 20,
		.num_visible_scanlines = 239
	};

	Standard standard = standard_ntsc;
}