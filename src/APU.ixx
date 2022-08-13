export module APU;

import NumericalTypes;
import SerializationStream;

import <array>;

namespace APU
{
	export
	{
		void ApplyNewSampleRate();
		void Initialize();
		u8 PeekRegister(u16 addr);
		void PowerOn();
		u8 ReadRegister(u16 addr);
		void Reset();
		void StreamState(SerializationStream& stream);
		void Update();
		void WriteRegister(u16 addr, u8 data);
	}

	struct Envelope
	{
		bool const_vol = false;
		bool start_flag = false;
		uint decay_level_cnt : 4 = 0;
		uint divider : 4 = 0;
		uint divider_period : 4 = 0; // Doubles as the channels volume when in constant volume mode
	};

	struct LengthCounter
	{
		void UpdateHaltFlag();

		bool halt = false;
		bool write_to_halt_next_cpu_cycle = false; /* Write to halt flag is delayed by one clock */
		bool bit_to_write_to_halt = false;
		u8 value = 0; /* The maximum value that can be loaded is 254 */
	};

	struct LinearCounter
	{
		/* Note: this counter has a control flag, but it is the same thing has the length counter halt flag. */
		bool reload = false;
		uint reload_value : 7 = 0;
		uint value : 7 = 0;
	};

	struct Sweep
	{
		bool enabled = false;
		bool muting = false;
		bool negate = false;
		bool reload = false;
		uint divider : 3 = 0;
		uint divider_period : 3 = 0;
		uint shift_count : 3 = 0;
		uint target_timer_period = 0;
	};

	template<uint id>
	struct PulseChannel
	{
		void ClockEnvelope();
		void ClockLength();
		void ClockSweep();
		void ComputeTargetTimerPeriod();
		u8 GetOutput();
		void Step();
		void UpdateSweepMuting();
		void UpdateVolume();

		bool enabled = false;
		u8 output = 0;
		u8 volume = 0;
		uint duty : 2 = 0;
		uint duty_pos : 3 = 0;
		uint timer : 11 = 0;
		uint timer_period : 11 = 0;
		Envelope envelope;
		LengthCounter length_counter;
		Sweep sweep;
	};

	PulseChannel<1> pulse_ch_1;
	PulseChannel<2> pulse_ch_2;

	struct TriangleChannel
	{
		void ClockLength();
		void ClockLinear();
		u8 GetOutput();
		void Step();

		bool enabled = false;
		u8 output = 0;
		uint duty_pos : 5 = 0;
		uint timer : 11 = 0;
		uint timer_period : 11 = 0;
		LengthCounter length_counter;
		LinearCounter linear_counter;
		/* Note: the triangle channel does not have volume control; the waveform is either cycling or suspended. */
	} triangle_ch;

	struct NoiseChannel
	{
		void ClockEnvelope();
		void ClockLength();
		u8 GetOutput();
		void Step();
		void UpdateVolume();

		bool enabled = false;
		bool output = 0;
		u8 volume = 0;
		bool mode = 0;
		u16 lfsr : 15 = 1;
		u16 timer = 0;
		u16 timer_period = 0;
		Envelope envelope;
		LengthCounter length_counter;
	} noise_ch;

	struct DMC
	{
		u8 GetOutput();
		void ReadSampleByte();
		void RestartSample();
		void Step();

		bool enabled = false;
		bool interrupt = false;
		bool irq_enable = false;
		bool loop = false;
		bool sample_buffer_is_empty = true;
		bool silence_flag = true; /* Note: this flag being set doesn't actually make the output 0. */
		uint output_level : 7 = 0;
		u8 apu_cycles_until_step = 0;
		u8 bits_remaining = 8;
		u8 period = 0;
		u8 sample_buffer = 0;
		u8 shift_register = 0;
		u16 bytes_remaining = 0;
		u16 current_sample_addr = 0;
		u16 sample_addr_start = 0;
		u16 sample_length = 0;
	} dmc;

	struct FrameCounter
	{
		void Step();

		bool interrupt = 0;
		bool interrupt_inhibit = 0;
		bool mode = 0;
		bool pending_4017_write = 0;
		u8 data_written_to_4017;
		uint cpu_cycle_count = 0;
		uint cpu_cycles_until_apply_4017_write;
	} frame_counter;

	void ClockEnvelopeUnits();
	void ClockLengthUnits();
	void ClockLinearUnits();
	void ClockSweepUnits();
	void SampleAndMix();
	void SetFrameCounterIrqLow();
	void SetFrameCounterIrqHigh();
	void SetDmcIrqLow();
	void SetDmcIrqHigh();

	bool on_apu_cycle = true;

	uint cpu_cycle_sample_counter;
	uint sample_rate;
}