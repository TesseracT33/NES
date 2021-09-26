#pragma once

#include <array>

#include "SDL.h"

#include "Bus.h"
#include "Component.h"

class APU final : public Component
{
public:
	void Initialize();
	void Power();
	void Reset();
	void Update();

	u8 ReadRegister(u16 addr);
	void WriteRegister(u16 addr, u8 data);

private:
	static constexpr u8 pulse_duty_table[4][8] =
	{
		{0, 1, 0, 0, 0, 0, 0, 0},
		{0, 1, 1, 0, 0, 0, 0, 0},
		{0, 1, 1, 1, 1, 0, 0, 0},
		{1, 0, 0, 1, 1, 1, 1, 1}
	};

	static constexpr u8 triangle_duty_table[32] =
	{
		15, 14, 13, 12, 11, 10, 9, 8, 7, 6,  5,  4,  3,  2,  1,  0,
		 0,  1,  2,  3,  4,  5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
	};

	static constexpr u16 noise_period_ntsc[16] = {
	4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068 };

	static constexpr u16 noise_period_pal[16] = {
		4, 8, 14, 30, 60, 88, 118, 148, 188, 236, 354, 472, 708, 944, 1890, 3778 };

	static constexpr size_t pulse_table_len = 31;
	static constexpr size_t tnd_table_len = 203;
	const std::array<f32, pulse_table_len> pulse_table = compute_pulse_table();
	const std::array<f32, tnd_table_len> tnd_table = compute_tnd_table();

	constexpr std::array<f32, pulse_table_len> compute_pulse_table()
	{
		// https://wiki.nesdev.org/w/index.php?title=APU_Mixer#Lookup_Table
		std::array<f32, pulse_table_len> table{};
		table[0] = 95.52f / 100.f;
		for (size_t i = 1; i < pulse_table_len; i++)
		{
			f32 pulse_sum = i;
			table[i] = 95.52f / (8128.f / pulse_sum + 100.f);
		}
		return table;
	}

	constexpr std::array<f32, tnd_table_len> compute_tnd_table()
	{
		std::array<f32, tnd_table_len> table{};
		table[0] = 163.67f / 100.f;
		for (size_t i = 1; i < tnd_table_len; i++)
		{
			f32 tnd_sum = i;
			table[i] = 163.67f / (24329.f / tnd_sum + 100.f);
		}
		return table;
	}

	static constexpr unsigned sample_buffer_size = 1024;
	static constexpr unsigned sample_rate = 44100;
	static constexpr unsigned cpu_cycles_per_sample = 1789773 / sample_rate;

	struct Envelope
	{
		bool start_flag;
		unsigned decay_level_cnt : 4;
		unsigned divider : 4;
		unsigned divider_period : 4;
	};

	struct Sweep
	{
		bool enabled;
		bool negate;
		bool reload;
		unsigned divider : 3;
		unsigned divider_period : 3;
		unsigned shift_count : 3;
	};

	struct Channel
	{
		bool enabled = false;
		u8 output;
	};

	struct PulseCh : Channel
	{
		bool const_vol;
		bool len_cnt_halt;
		unsigned duty : 2;
		unsigned duty_pos : 3;
		unsigned len_cnt : 5;
		unsigned len_cnt_reload : 5;
		unsigned timer : 11;
		unsigned timer_reload : 11;
		Envelope envelope;
		Sweep sweep;

		void ClockEnvelope();
		void ClockLength();
		void ClockSweep();
		void Step();
	} pulse_ch_1{}, pulse_ch_2{};

	struct TriangleCh : Channel
	{
		bool linear_cnt_control;
		bool linear_cnt_reload_flag;
		unsigned duty_pos : 5;
		unsigned len_cnt : 5;
		unsigned len_cnt_reload : 5;
		unsigned linear_cnt : 7;
		unsigned linear_cnt_reload : 7;
		unsigned timer : 11;
		unsigned timer_reload : 11;

		void ClockLength();
		void ClockLinear();
		void Step();
	} triangle_ch;

	struct NoiseCh : Channel
	{
		bool const_vol;
		bool len_cnt_halt;
		bool loop_noise;
		unsigned div_period : 4;
		unsigned len_cnt : 5;
		unsigned len_cnt_reload : 5;
		unsigned LFSR : 15;
		u16 timer;
		u16 timer_reload;
		Envelope envelope;

		void ClockEnvelope();
		void ClockLength();
		void Step();
	} noise_ch;

	struct DMC : Channel
	{
		bool active; // TODO temp var; bytes remaining more than 0
		bool IRQ_enable;
		bool loop;
		unsigned frequency : 4;
		unsigned load_cnt : 7;
		u8 sample_buffer;
		u16 sample_addr;
		u16 sample_len;
	} dmc;

	bool frame_cnt_mode;
	bool frame_cnt_interrupt_inhibit;
	bool frame_interrupt;
	bool on_apu_cycle = true;

	unsigned cpu_cycle_count = 0;
	unsigned cpu_cycles_until_sample = cpu_cycles_per_sample;
	unsigned sample_buffer_index = 0;

	f32 sample_buffer[sample_buffer_size];

	SDL_AudioSpec audio_spec;

	void Mix();
	void OutputSample();
	void StepFrameCounter();

	void State(Serialization::BaseFunctor& functor);
};

