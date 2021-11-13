#pragma once

#include <array>
#include <chrono>
#include <cmath>

#include "SDL.h"

#include "Bus.h"
#include "Component.h"
#include "CPU.h"
#include "IRQSources.h"
#include "System.h"

#include "mappers/BaseMapper.h"

class APU final : public Component
{
public:
	using Component::Component;

	void PowerOn(const System::VideoStandard standard);
	void Reset();
	void Update();

	u8 ReadRegister(u16 addr);
	void WriteRegister(u16 addr, u8 data);

private:
	static constexpr u8 length_table[32] = {
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

	static constexpr u16 dmc_rate_table_ntsc[16] = {
		428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106, 84, 72, 54
	};

	static constexpr u16 dmc_rate_table_pal[16] = {
		398, 354, 316, 298, 276, 236, 210, 198, 176, 148, 132, 118, 98, 78, 66, 50
	};

	static constexpr u16 noise_period_table_ntsc[16] = {
		4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068 
	};

	static constexpr u16 noise_period_table_pal[16] = {
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

	static constexpr unsigned cpu_cycles_per_sec_ntsc = 1789773;
	static constexpr unsigned cpu_cycles_per_sec_pal  = 1662607;
	static constexpr unsigned num_audio_channels = 2;
	static constexpr unsigned sample_buffer_size = 2048;
	static constexpr unsigned sample_buffer_size_per_channel = sample_buffer_size / num_audio_channels;
	static constexpr unsigned sample_rate = 44100;
	static constexpr unsigned microseconds_per_audio_enqueue =
		double(sample_buffer_size_per_channel) / double(sample_rate) * 1'000'000;

	struct Standard
	{
		const u16* dmc_rate_table;
		const u16* noise_period_table;
		unsigned cpu_cycles_per_sec;
	};

	static constexpr Standard NTSC  = { dmc_rate_table_ntsc, noise_period_table_ntsc, cpu_cycles_per_sec_ntsc };
	static constexpr Standard Dendy = { dmc_rate_table_ntsc, noise_period_table_ntsc, cpu_cycles_per_sec_ntsc }; /* TODO: not sure about this */
	static constexpr Standard PAL   = { dmc_rate_table_pal , noise_period_table_pal , cpu_cycles_per_sec_pal  };
	Standard standard = NTSC; /* The default */

	/* Note: many of the initial values of the below structs are set from within CPU::Power() / CPU::Reset(). */

	struct Envelope
	{
		bool const_vol;
		bool start_flag;
		unsigned decay_level_cnt : 4; 
		unsigned divider : 4;
		unsigned divider_period : 4; // Doubles as the channels volume when in constant volume mode
	};

	struct LengthCounter
	{
		bool halt;
		bool has_reached_zero = true;
		unsigned value : 5;
		
		void SetToZero()
		{
			value = 0;
			has_reached_zero = true;
		}
		void SetValue(unsigned value)
		{
			this->value = value;
			/* The length counter silences the channel when clocked while already zero,
			   so we should probably not set this to true if value == 0. */
			has_reached_zero = false;
		}
	};

	struct LinearCounter
	{
		/* Note: this counter has a control flag, but it is the same thing has the length counter halt flag. */
		bool has_reached_zero = true;
		bool reload;
		unsigned reload_value : 7;
		unsigned value : 7;

		void Reload()
		{
			value = reload_value;
			has_reached_zero = false;
		}
	};

	struct Sweep
	{
		bool enabled;
		bool muting;
		bool negate;
		bool reload;
		unsigned divider : 3;
		unsigned divider_period : 3;
		unsigned shift_count : 3;
		unsigned target_timer_period;
	};

	struct Channel
	{
		bool enabled = false;
		u8 output;
		u8 volume = 0;

		virtual void Step() = 0;
		virtual void UpdateVolume() = 0;
	};

	struct PulseCh : Channel
	{
		PulseCh(int id) : id(id) {};
		const int id; /* 1 or 2 */

		unsigned duty         :  2 = 0;
		unsigned duty_pos     :  3 = 0;
		unsigned timer        : 11 = 0;
		unsigned timer_period : 11 = 0;
		Envelope envelope;
		LengthCounter length_counter;
		Sweep sweep;

		void ClockEnvelope();
		void ClockLength();
		void ClockSweep();
		void ComputeTargetTimerPeriod();
		void Step();

		void UpdateSweepMuting()
		{
			sweep.muting = sweep.target_timer_period > 0x7FF || timer_period < 8;
			UpdateVolume();
		}

		void UpdateVolume()
		{
			if (length_counter.has_reached_zero || sweep.muting)
				volume = 0;
			else if (envelope.const_vol)
				volume = envelope.divider_period; // Doubles as the channel's volume when in constant volume mode
			else
				volume = envelope.decay_level_cnt;
		}
	} pulse_ch_1{1}, pulse_ch_2{2};

	struct TriangleCh : Channel
	{
		unsigned duty_pos : 5;
		unsigned timer : 11;
		unsigned timer_period : 11;
		LengthCounter length_counter;
		LinearCounter linear_counter;

		void ClockLength();
		void ClockLinear();
		void Step();

		void UpdateVolume()
		{
			if (length_counter.has_reached_zero || linear_counter.has_reached_zero)
				volume = 0;
			else
				volume = 1;
		}
	} triangle_ch;

	struct NoiseCh : Channel
	{
		bool loop_noise;
		unsigned LFSR : 15;
		u16 timer;
		u16 timer_period;
		Envelope envelope;
		LengthCounter length_counter;

		void ClockEnvelope();
		void ClockLength();
		void Step();

		void UpdateVolume()
		{
			if (length_counter.has_reached_zero || (LFSR & 1)) // The volume is 0 if bit 0 of the LFSR is set
				volume = 0;
			else if (envelope.const_vol)
				volume = envelope.divider_period; // Doubles as the channel's volume when in constant volume mode
			else
				volume = envelope.decay_level_cnt;
		}
	} noise_ch;

	struct DMC : Channel
	{
		DMC(APU* apu) { this->apu = apu; }
		APU* apu; /* This unit needs access to some members outside of the struct. */

		bool interrupt;
		bool IRQ_enable;
		bool loop;
		bool read_sample_on_next_apu_cycle = false;
		bool restart_sample_after_buffer_is_emptied = false;
		bool sample_buffer_is_empty = true;
		bool silence_flag;
		unsigned output_level : 7;
		u8 bits_remaining;
		u8 sample_buffer;
		u8 shift_register;
		u16 bytes_remaining;
		u16 cpu_cycles_until_step;
		u16 current_sample_addr;
		u16 period;
		u16 sample_addr;
		u16 sample_len;

		void ReadSample();
		void RestartSample()
		{
			current_sample_addr = sample_addr;
			bytes_remaining = sample_len;
		}
		void Step();

		void UpdateVolume() { /* TODO */ }
	} dmc{ this };

	struct FrameCounter
	{
		FrameCounter(APU* apu) { this->apu = apu; }
		APU* apu; /* This unit needs access to some members outside of the struct. */

		bool interrupt = 0;
		bool interrupt_inhibit = 0;
		bool mode = 0;
		bool pending_4017_write = false;
		u8 data_written_to_4017;
		unsigned cpu_cycle_count = 0;
		unsigned cpu_cycles_until_apply_4017_write;

		void Step();
	} frame_counter{ this };

	bool on_apu_cycle = true;

	unsigned cpu_cycle_sample_counter = 0;
	unsigned sample_buffer_index = 0;

	f32 sample_buffer[sample_buffer_size];

	SDL_AudioSpec audio_spec;

	std::chrono::steady_clock::time_point last_audio_enqueue_time_point = std::chrono::steady_clock::now();

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

	void MixAndSample();

	void StreamState(SerializationStream& stream) override;
};

