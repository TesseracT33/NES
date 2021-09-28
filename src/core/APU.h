#pragma once

#include <array>

#include "SDL.h"

#include "Bus.h"
#include "Component.h"

#include "mappers/BaseMapper.h"

class APU final : public Component
{
public:
	BaseMapper* mapper;

	void Initialize();
	void Power();
	void Reset();
	void Update();

	u8 ReadRegister(u16 addr);
	void WriteRegister(u16 addr, u8 data);

private:
	static constexpr u8 noise_length[32] = {
		10, 254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14,
		12,  16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
	};

	static constexpr u8 pulse_duty_table[4][8] = {
		{0, 1, 0, 0, 0, 0, 0, 0},
		{0, 1, 1, 0, 0, 0, 0, 0},
		{0, 1, 1, 1, 1, 0, 0, 0},
		{1, 0, 0, 1, 1, 1, 1, 1}
	};

	static constexpr u8 triangle_duty_table[32] = {
		15, 14, 13, 12, 11, 10, 9, 8, 7, 6,  5,  4,  3,  2,  1,  0,
		 0,  1,  2,  3,  4,  5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
	};

	static constexpr u16 dmc_rate_ntsc[16] = {
		428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106, 84, 72, 54
	};

	static constexpr u16 dmc_rate_pal[16] = {
		398, 354, 316, 298, 276, 236, 210, 198, 176, 148, 132, 118, 98, 78, 66, 50
	};

	static constexpr u16 noise_period_ntsc[16] = {
		4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068 
	};

	static constexpr u16 noise_period_pal[16] = {
		4, 8, 14, 30, 60, 88, 118, 148, 188, 236, 354, 472, 708, 944, 1890, 3778 
	};

	static constexpr size_t pulse_table_len = 31;
	static constexpr size_t tnd_table_len = 203;

	static constexpr std::array<f32, pulse_table_len> pulse_table = [] {
		// https://wiki.nesdev.org/w/index.php?title=APU_Mixer#Lookup_Table
		std::array<f32, pulse_table_len> table{};
		table[0] = 0.f;
		for (size_t i = 1; i < pulse_table_len; i++)
		{
			f32 pulse_sum = i;
			table[i] = 95.52f / (8128.f / pulse_sum + 100.f);
		}
		return table;
	}();

	static constexpr std::array<f32, tnd_table_len> tnd_table = [] {
		std::array<f32, tnd_table_len> table{};
		table[0] = 0.f;
		for (size_t i = 1; i < tnd_table_len; i++)
		{
			f32 tnd_sum = i;
			table[i] = 163.67f / (24329.f / tnd_sum + 100.f);
		}
		return table;
	}();

	static constexpr unsigned sample_buffer_size = 1024;
	static constexpr unsigned sample_rate = 44100;
	static constexpr unsigned cpu_cycles_per_sample = 1789773 / sample_rate;

	/* Note: the initial values of the below structs are set from within CPU::Power() / CPU::Reset(). */

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
		unsigned timer_target_period : 11;
	};

	struct Channel
	{
		bool enabled = false;
		u8 output;
	};

	struct PulseCh : Channel
	{
		PulseCh(int _id) : id(_id) {};
		const int id;

		bool const_vol;
		bool len_cnt_halt;
		bool muted; // Whether the channel is muted by the sweep unit.
		unsigned duty : 2;
		unsigned duty_pos : 3;
		unsigned volume : 4;
		unsigned len_cnt : 5;
		unsigned timer : 11;
		unsigned timer_period : 11;
		Envelope envelope;
		Sweep sweep;

		void ClockEnvelope();
		void ClockLength();
		void ClockSweep();
		void ComputeTargetTimerPeriod();
		void Step();
	} pulse_ch_1{1}, pulse_ch_2{2};

	struct TriangleCh : Channel
	{
		bool linear_cnt_control; // doubles as length counter halt flag
		bool linear_cnt_reload_flag;
		bool volume; // TODO: temp
		unsigned duty_pos : 5;
		unsigned len_cnt : 5;
		unsigned linear_cnt : 7;
		unsigned linear_cnt_reload : 7;
		unsigned timer : 11;
		unsigned timer_period : 11;

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
		unsigned volume : 4;
		unsigned len_cnt : 5;
		unsigned LFSR : 15;
		u16 timer;
		u16 timer_period;
		Envelope envelope;

		void ClockEnvelope();
		void ClockLength();
		void Step();
	} noise_ch;

	struct DMC : Channel
	{
		bool interrupt_flag;
		bool IRQ_enable;
		bool loop;
		bool sample_buffer_is_empty;
		bool silence_flag;
		unsigned output_level : 7;
		u8 bits_remaining;
		u8 sample_buffer;
		u8 shift_register;
		u16 bytes_remaining;
		u16 cpu_cycles_until_step;
		u16 current_sample_addr;
		u16 rate;
		u16 sample_addr;
		u16 sample_len;

		void Step();
	} dmc;

	struct FrameCounter
	{
		bool interrupt = 0;
		bool interrupt_inhibit = 0;
		bool mode = 0;
		bool pending_4017_write = false;
		u8 data_written_to_4017;
		unsigned cpu_cycle_count = 0;
		unsigned cpu_cycles_until_apply_4017_write;
	} frame_counter;

	bool on_apu_cycle = 1;

	unsigned cpu_cycles_until_sample = cpu_cycles_per_sample;
	unsigned sample_buffer_index = 0;

	f32 sample_buffer[sample_buffer_size];

	SDL_AudioSpec audio_spec;

	__forceinline void ClockEnvelopeUnits()
	{
		pulse_ch_1.ClockEnvelope();
		pulse_ch_2.ClockEnvelope();
		noise_ch.ClockEnvelope();
	}

	__forceinline void ClockLengthUnits()
	{
		pulse_ch_1.ClockLength();
		pulse_ch_2.ClockLength();
		triangle_ch.ClockLength();
		noise_ch.ClockLength();
	}

	__forceinline void ClockLinearUnits()
	{
		triangle_ch.ClockLinear();
	}

	__forceinline void ClockSweepUnits()
	{
		pulse_ch_1.ClockSweep();
		pulse_ch_2.ClockSweep();
	}

	void Mix();
	void ReadSample();
	void StepFrameCounter();

	void State(Serialization::BaseFunctor& functor);
};

