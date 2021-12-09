#pragma once

#include <array>
#include <chrono>

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

	u8 ReadRegister(const u16 addr);
	void WriteRegister(const u16 addr, const u8 data);

	void StreamState(SerializationStream& stream) override;

private:
	static constexpr u8 dmc_rate_table_ntsc[16] = {
		214, 190, 170, 160, 143, 127, 113, 107, 95, 80, 71, 64, 53, 42, 36, 27
	};

	static constexpr u8 dmc_rate_table_pal[16] = {
		199, 177, 158, 149, 138, 118, 105, 99, 88, 74, 66, 59, 49, 39, 33, 25
	};

	static constexpr u8 length_table[32] = {
		10, 254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14,
		12,  16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
	};

	static constexpr u8 pulse_duty_table[4][8] = {
		{ 0, 1, 0, 0, 0, 0, 0, 0 },
		{ 0, 1, 1, 0, 0, 0, 0, 0 },
		{ 0, 1, 1, 1, 1, 0, 0, 0 },
		{ 1, 0, 0, 1, 1, 1, 1, 1 }
	};

	static constexpr u8 triangle_duty_table[32] = {
		15, 14, 13, 12, 11, 10, 9, 8, 7, 6,  5,  4,  3,  2,  1,  0,
		 0,  1,  2,  3,  4,  5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
	};

	static constexpr u16 noise_period_table_ntsc[16] = {
		4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
	};

	static constexpr u16 noise_period_table_pal[16] = {
		4, 8, 14, 30, 60, 88, 118, 148, 188, 236, 354, 472, 708, 944, 1890, 3778
	};

	static constexpr unsigned frame_counter_step_cycle_table_ntsc[8] = {
		7457, 14913, 22371, 29828, 29829, 29830, 37281, 37282
	};

	static constexpr unsigned frame_counter_step_cycle_table_pal[8] = {
		8313, 16627, 24939, 33252, 33253, 33254, 41565, 41566
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
	static constexpr unsigned cpu_cycles_per_sec_pal = 1662607;
	static constexpr unsigned num_audio_channels = 2;
	static constexpr unsigned sample_buffer_size_per_channel = 512;
	static constexpr unsigned sample_buffer_size = sample_buffer_size_per_channel * num_audio_channels;;
	static constexpr unsigned sample_rate = 44100;
	static constexpr u64 nanoseconds_per_audio_enqueue =
		double(sample_buffer_size_per_channel) / double(sample_rate) * 1'000'000'000;

	struct Standard
	{
		const u8* dmc_rate_table;
		const u16* noise_period_table;
		const unsigned* frame_counter_step_cycle_table;
		unsigned cpu_cycles_per_sec;
	} standard;

	static constexpr Standard NTSC  = { dmc_rate_table_ntsc, noise_period_table_ntsc, frame_counter_step_cycle_table_ntsc, cpu_cycles_per_sec_ntsc };
	static constexpr Standard Dendy = { dmc_rate_table_ntsc, noise_period_table_ntsc, frame_counter_step_cycle_table_ntsc, cpu_cycles_per_sec_ntsc }; /* TODO: not sure about this */
	static constexpr Standard PAL   = { dmc_rate_table_pal , noise_period_table_pal , frame_counter_step_cycle_table_pal , cpu_cycles_per_sec_pal  };

	struct Envelope
	{
		bool const_vol  = false;
		bool start_flag = false;
		unsigned decay_level_cnt : 4 = 0;
		unsigned divider         : 4 = 0;
		unsigned divider_period  : 4 = 0; // Doubles as the channels volume when in constant volume mode
	};

	struct LengthCounter
	{
		bool halt                         = false;
		bool write_to_halt_next_cpu_cycle = false; /* Write to halt flag is delayed by one clock */
		bool bit_to_write_to_halt         = false;
		u8 value = 0; /* The maximum value that can be loaded is 254 */

		void UpdateHaltFlag()
		{
			if (write_to_halt_next_cpu_cycle)
			{
				halt = bit_to_write_to_halt;
				write_to_halt_next_cpu_cycle = false;
			}
		}
	};

	struct LinearCounter
	{
		/* Note: this counter has a control flag, but it is the same thing has the length counter halt flag. */
		bool reload = false;
		unsigned reload_value : 7 = 0;
		unsigned value        : 7 = 0;
	};

	struct Sweep
	{
		bool enabled = false;
		bool muting  = false;
		bool negate  = false;
		bool reload  = false;
		unsigned divider         : 3 = 0;
		unsigned divider_period  : 3 = 0;
		unsigned shift_count     : 3 = 0;
		unsigned target_timer_period = 0;
	};

	struct PulseCh
	{
		explicit PulseCh(int id) : id(id) {};
		const int id; /* 1 or 2 */

		bool enabled = false;
		u8 output = 0;
		u8 volume = 0;

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
			if (length_counter.value == 0 || sweep.muting)
				volume = 0;
			else if (envelope.const_vol)
				volume = envelope.divider_period; // Doubles as the channel's volume when in constant volume mode
			else
				volume = envelope.decay_level_cnt;
		}
	} pulse_ch_1{ 1 }, pulse_ch_2{ 2 };

	struct TriangleCh
	{
		bool enabled = false;
		u8 output = 0;

		unsigned duty_pos     :  5 = 0;
		unsigned timer        : 11 = 0;
		unsigned timer_period : 11 = 0;
		LengthCounter length_counter;
		LinearCounter linear_counter;

		void ClockLength();
		void ClockLinear();
		void Step();
		/* Note: the triangle channel does not have volume control; the waveform is either cycling or suspended. */
	} triangle_ch;

	struct NoiseCh
	{
		bool enabled = false;
		bool output = 0;
		u8 volume = 0;

		bool mode = 0;
		unsigned LFSR : 15 = 1;
		u16 timer = 0;
		u16 timer_period = 0;
		Envelope envelope;
		LengthCounter length_counter;

		void ClockEnvelope();
		void ClockLength();
		void Step();

		void UpdateVolume()
		{
			if (length_counter.value == 0 || (LFSR & 1)) // The volume is 0 if bit 0 of the LFSR is set
				volume = 0;
			else if (envelope.const_vol)
				volume = envelope.divider_period; // Doubles as the channel's volume when in constant volume mode
			else
				volume = envelope.decay_level_cnt;
		}
	} noise_ch;

	struct DMC
	{
		DMC(APU* apu) : apu(apu) {};
		APU* apu; /* This unit needs access to some members outside of the struct. */

		bool enabled                = false;
		bool interrupt              = false;
		bool IRQ_enable             = false;
		bool loop                   = false;
		bool sample_buffer_is_empty = true;
		bool silence_flag           = true; /* Note: this flag being set doesn't actually make the output 0. */
		unsigned output_level : 7 = 0;
		u8 apu_cycles_until_step  = 0;
		u8 bits_remaining         = 8;
		u8 sample_buffer          = 0;
		u8 shift_register         = 0;
		u16 bytes_remaining       = 0;
		u16 current_sample_addr   = 0;
		u16 period                = 0;
		u16 sample_addr_start     = 0;
		u16 sample_length         = 0;

		void ReadSampleByte();
		void RestartSample();
		void Step();
	} dmc{ this };

	struct FrameCounter
	{
		FrameCounter(APU* apu) : apu(apu) {}
		APU* apu; /* This unit needs access to some members outside of the struct. */

		bool interrupt          = 0;
		bool interrupt_inhibit  = 0;
		bool mode               = 0;
		bool pending_4017_write = 0;
		u8 data_written_to_4017;
		unsigned cpu_cycle_count = 0;
		unsigned cpu_cycles_until_apply_4017_write;

		void Step();

		void ClockEnvelopeUnits()
		{
			apu->pulse_ch_1.ClockEnvelope();
			apu->pulse_ch_2.ClockEnvelope();
			apu->noise_ch.ClockEnvelope();
		}
		void ClockLengthUnits()
		{
			apu->pulse_ch_1.ClockLength();
			apu->pulse_ch_2.ClockLength();
			apu->triangle_ch.ClockLength();
			apu->noise_ch.ClockLength();
		}
		void ClockLinearUnits()
		{
			apu->triangle_ch.ClockLinear();
		}
		void ClockSweepUnits()
		{
			apu->pulse_ch_1.ClockSweep();
			apu->pulse_ch_2.ClockSweep();
		}
	} frame_counter{ this };

	bool on_apu_cycle = true;

	unsigned cpu_cycle_sample_counter = 0;
	unsigned microsecond_counter = 0;
	unsigned sample_buffer_index = 0;

	SDL_AudioDeviceID audio_device_id;

	std::array<f32, sample_buffer_size> sample_buffer{};

	std::chrono::steady_clock::time_point last_audio_enqueue_time_point = std::chrono::steady_clock::now();

	void SetFrameCounterIRQLow()
	{
		frame_counter.interrupt = 1;
		nes->cpu->SetIRQLow(IRQSource::APU_FRAME);
	}

	void SetFrameCounterIRQHigh()
	{
		frame_counter.interrupt = 0;
		nes->cpu->SetIRQHigh(IRQSource::APU_FRAME);
	}

	void SetDMCIRQLow()
	{
		dmc.interrupt = 1;
		nes->cpu->SetIRQLow(IRQSource::APU_DMC);
	}

	void SetDMCIRQHigh()
	{
		dmc.interrupt = 0;
		nes->cpu->SetIRQHigh(IRQSource::APU_DMC);
	}

	void SampleAndMix();
};