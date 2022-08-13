module APU;

import Bus;
import Cartridge;
import CPU;
import System;

import Audio;

namespace APU
{
	void ApplyNewSampleRate()
	{
		sample_rate = Audio::GetSampleRate();
		cpu_cycle_sample_counter = 0;
	}


	void PowerOn()
	{
		for (u16 addr = 0x4000; addr <= 0x4013; addr++) {
			WriteRegister(addr, 0x00);
		}
		WriteRegister(0x4015, 0x00);
		WriteRegister(0x4017, 0x00);
		dmc.apu_cycles_until_step = dmc.period; /* To prevent underflow of 'apu_cycles_until_step' the first time DMC::Step() is called */
		Reset();
		ApplyNewSampleRate();
	}


	void Reset()
	{
		WriteRegister(0x4015, 0x00);
		cpu_cycle_sample_counter = 0;
	}


	void Update()
	{
		/* Update() is called every CPU cycle, and 2 CPU cycles = 1 APU cycle.
		   Some components update every APU cycle, others every CPU cycle.
		   The triangle channel's timer is clocked on every CPU cycle,
		   but the pulse, noise, and DMC timers are clocked only on every second CPU cycle.
		   Further, the frame counter should be stepped every cpu cycle, because it is counting cpu cycles. */
		if (on_apu_cycle) {
			dmc.Step();
			noise_ch.Step();
			pulse_ch_1.Step();
			pulse_ch_2.Step();
		}
		frame_counter.Step();
		triangle_ch.Step();
		/* If the length counter halt flag was set to be set/cleared on the last cpu cycle, set/clear it now. */
		pulse_ch_1.length_counter.UpdateHaltFlag();
		pulse_ch_2.length_counter.UpdateHaltFlag();
		triangle_ch.length_counter.UpdateHaltFlag();
		noise_ch.length_counter.UpdateHaltFlag();

		cpu_cycle_sample_counter += sample_rate;
		if (cpu_cycle_sample_counter >= System::standard.cpu_cycles_per_sec) {
			SampleAndMix();
			cpu_cycle_sample_counter -= System::standard.cpu_cycles_per_sec;
		}
		on_apu_cycle = !on_apu_cycle;
	}


	u8 ReadRegister(u16 addr)
	{
		// Only $4015 is readable, the rest are write only.
		if (addr == Bus::Addr::APU_STAT) {
			u8 ret = (pulse_ch_1.length_counter.value > 0)
				| (pulse_ch_2.length_counter.value    > 0) << 1
				| (triangle_ch.length_counter.value   > 0) << 2
				| (noise_ch.length_counter.value      > 0) << 3
				| (dmc.bytes_remaining                > 0) << 4
				| (frame_counter.interrupt               ) << 6
				| (dmc.interrupt                         ) << 7;

			SetFrameCounterIrqHigh();
			// TODO If an interrupt flag was set at the same moment of the read, it will read back as 1 but it will not be cleared.
			return ret;
		}
		else {
			return 0xFF;
		}
	}


	u8 PeekRegister(u16 addr)
	{
		if (addr == Bus::Addr::APU_STAT) {
			return (pulse_ch_1.length_counter.value > 0)
				| (pulse_ch_2.length_counter.value > 0) << 1
				| (triangle_ch.length_counter.value > 0) << 2
				| (noise_ch.length_counter.value > 0) << 3
				| (dmc.bytes_remaining > 0) << 4
				| (frame_counter.interrupt) << 6
				| (dmc.interrupt) << 7;
		}
		else {
			return 0xFF;
		}
	}


	void WriteRegister(const u16 addr, const u8 data)
	{
		static constexpr std::array<u8, 32> length_table = {
			10, 254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14,
			12,  16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
		};

		switch (addr) {
		case Bus::Addr::SQ1_VOL: // $4000
			pulse_ch_1.envelope.divider_period = data;
			pulse_ch_1.envelope.const_vol = data & 0x10;
			pulse_ch_1.length_counter.write_to_halt_next_cpu_cycle = true;
			pulse_ch_1.length_counter.bit_to_write_to_halt = data & 0x20;
			pulse_ch_1.duty = data >> 6;
			pulse_ch_1.UpdateVolume();
			break;

		case Bus::Addr::SQ1_SWEEP: // $4001
			pulse_ch_1.sweep.shift_count = data;
			pulse_ch_1.sweep.negate = data & 0x08;
			pulse_ch_1.sweep.divider_period = data >> 4;
			pulse_ch_1.sweep.enabled = data & 0x80;
			pulse_ch_1.sweep.reload = true;
			pulse_ch_1.ComputeTargetTimerPeriod();
			break;

		case Bus::Addr::SQ1_LO: // $4002
			pulse_ch_1.timer_period = pulse_ch_1.timer_period & 0x700 | data;
			pulse_ch_1.UpdateSweepMuting();
			pulse_ch_1.ComputeTargetTimerPeriod();
			break;

		case Bus::Addr::SQ1_HI: // $4003
			pulse_ch_1.timer_period = pulse_ch_1.timer_period & 0xFF | data << 8;
			pulse_ch_1.UpdateSweepMuting();
			if (pulse_ch_1.enabled) {
				pulse_ch_1.length_counter.value = length_table[data >> 3];
				pulse_ch_1.UpdateVolume();
			}
			pulse_ch_1.duty_pos = 0;
			pulse_ch_1.envelope.start_flag = true;
			pulse_ch_1.ComputeTargetTimerPeriod();
			break;

		case Bus::Addr::SQ2_VOL: // $4004
			pulse_ch_2.envelope.divider_period = data;
			pulse_ch_2.envelope.const_vol = data & 0x10;
			pulse_ch_2.length_counter.write_to_halt_next_cpu_cycle = true;
			pulse_ch_2.length_counter.bit_to_write_to_halt = data & 0x20;
			pulse_ch_2.duty = data >> 6;
			pulse_ch_2.UpdateVolume();
			break;

		case Bus::Addr::SQ2_SWEEP: // $4005
			pulse_ch_2.sweep.shift_count = data;
			pulse_ch_2.sweep.negate = data & 0x08;
			pulse_ch_2.sweep.divider_period = data >> 4;
			pulse_ch_2.sweep.enabled = data & 0x80;
			pulse_ch_2.sweep.reload = true;
			pulse_ch_2.ComputeTargetTimerPeriod();
			break;

		case Bus::Addr::SQ2_LO: // $4006
			pulse_ch_2.timer_period = pulse_ch_2.timer_period & 0x700 | data;
			pulse_ch_2.UpdateSweepMuting();
			pulse_ch_2.ComputeTargetTimerPeriod();
			break;

		case Bus::Addr::SQ2_HI: // $4007
			pulse_ch_2.timer_period = pulse_ch_2.timer_period & 0xFF | data << 8;
			pulse_ch_2.UpdateSweepMuting();
			if (pulse_ch_2.enabled) {
				pulse_ch_2.length_counter.value = length_table[data >> 3];
				pulse_ch_2.UpdateVolume();
			}
			pulse_ch_2.duty_pos = 0;
			pulse_ch_2.envelope.start_flag = true;
			pulse_ch_2.ComputeTargetTimerPeriod();
			break;

		case Bus::Addr::TRI_LINEAR: // $4008
			triangle_ch.linear_counter.reload_value = data;
			triangle_ch.length_counter.write_to_halt_next_cpu_cycle = true;
			triangle_ch.length_counter.bit_to_write_to_halt = data & 0x80; // Doubles as the linear counter control flag
			break;

		case Bus::Addr::TRI_LO: // $400A
			triangle_ch.timer_period = triangle_ch.timer_period & 0x700 | data;
			break;

		case Bus::Addr::TRI_HI: // $400B
			triangle_ch.timer_period = triangle_ch.timer_period & 0xFF | data << 8;
			if (triangle_ch.enabled) {
				triangle_ch.length_counter.value = length_table[data >> 3];
			}
			triangle_ch.linear_counter.reload = true;
			break;

		case Bus::Addr::NOISE_VOL: // $400C
			noise_ch.envelope.divider_period = data;
			noise_ch.envelope.const_vol = data & 0x10;
			noise_ch.length_counter.write_to_halt_next_cpu_cycle = true;
			noise_ch.length_counter.bit_to_write_to_halt = data & 0x20;
			noise_ch.UpdateVolume();
			break;

		case Bus::Addr::NOISE_LO: // $400E
			noise_ch.timer_period = System::standard.noise_period_table[data & 0xF];
			noise_ch.mode = data & 0x80;
			break;

		case Bus::Addr::NOISE_HI: // $400F
			if (noise_ch.enabled) {
				noise_ch.length_counter.value = length_table[data >> 3];
				noise_ch.UpdateVolume();
			}
			noise_ch.envelope.start_flag = true;
			break;

		case Bus::Addr::DMC_FREQ: // $4010
			dmc.period = System::standard.dmc_rate_table[data & 0xF];
			dmc.loop = data & 0x40;
			dmc.irq_enable = data & 0x80;
			if (!dmc.irq_enable) {
				SetDmcIrqHigh();
			}
			break;

		case Bus::Addr::DMC_RAW: // $4011
			dmc.output_level = data;
			break;

		case Bus::Addr::DMC_START: // $4012
			dmc.sample_addr_start = 0xC000 | data << 6;
			break;

		case Bus::Addr::DMC_LEN: // $4013
			dmc.sample_length = 1 | data << 4;
			break;

		case Bus::Addr::APU_STAT: // $4015
			/* If a channel goes from disabled to enabled, and if the length counter is 0, it seems that the length counter will then keep muting the channel.
			   This doesn't say on nesdev, but if you were to "unmute" the noise channel once it gets enabled, it sounds very wrong in SMB1. */
			pulse_ch_1.enabled = data & 0x01;
			if (!pulse_ch_1.enabled) {
				pulse_ch_1.length_counter.value = 0;
				pulse_ch_1.volume = 0;
			}
			pulse_ch_2.enabled = data & 0x02;
			if (!pulse_ch_2.enabled) {
				pulse_ch_2.length_counter.value = 0;
				pulse_ch_2.volume = 0;
			}
			triangle_ch.enabled = data & 0x04;
			if (!triangle_ch.enabled) {
				triangle_ch.length_counter.value = 0;
			}
			noise_ch.enabled = data & 0x08;
			if (!noise_ch.enabled) {
				noise_ch.length_counter.value = 0;
				noise_ch.volume = 0;
			}
			dmc.enabled = data & 0x10;
			if (dmc.enabled) {
				if (dmc.bytes_remaining == 0) {
					dmc.RestartSample();
				}
				if (dmc.sample_buffer_is_empty) {
					dmc.ReadSampleByte();
				}
			}
			else {
				dmc.bytes_remaining = 0;
				dmc.sample_buffer_is_empty = true;
			}
			SetDmcIrqHigh();
			break;

		case Bus::Addr::FRAME_CNT: // $4017
			/* If the write occurs during an APU cycle, the effects occur 2 CPU cycles after the $4017 write cycle,
			   and if the write occurs between APU cycles, the effects occurs 4 CPU cycles after the write cycle. */
			frame_counter.pending_4017_write = true;
			frame_counter.cpu_cycles_until_apply_4017_write = on_apu_cycle ? 3 : 4;
			frame_counter.data_written_to_4017 = data;
			/* Writing to $4017 with bit 7 set should clock all units immediately. */
			if (data & 0x80) {
				ClockEnvelopeUnits();
				ClockLengthUnits();
				ClockLinearUnits();
				ClockSweepUnits();
			}
			break;

		default:
			break;
		}
	}


	void FrameCounter::Step()
	{
		// If $4017 was written to, the write doesn't apply until a few cpu cycles later.
		if (pending_4017_write && --cpu_cycles_until_apply_4017_write == 0) {
			/* If bit 6 is set, the frame interrupt flag is cleared, otherwise it is unaffected. */
			interrupt_inhibit = data_written_to_4017 & 0x40;
			if (interrupt_inhibit) {
				SetFrameCounterIrqHigh();
			}
			mode = data_written_to_4017 & 0x80;
			pending_4017_write = false;
			cpu_cycle_count = 0;
			return;
		}

		// The frame counter is clocked on every other CPU cycle, i.e. on every APU cycle.
		// This function is called every CPU cycle.
		// Therefore, the APU cycle counts from https://wiki.nesdev.org/w/index.php?title=APU_Frame_Counter have been doubled.
		cpu_cycle_count++;

		const auto* table = System::standard.frame_counter_step_cycle_table.data();
		/* NTSC: cycle 7457/22371. PAL: cycle 8313/24939. */
		if (cpu_cycle_count == table[0] || cpu_cycle_count == table[2]) {
			ClockEnvelopeUnits();
			ClockLinearUnits();
		}
		/* NTSC: cycle 14913/37281. PAL: cycle 16627/41565. */
		else if (cpu_cycle_count == table[1] || cpu_cycle_count == table[6]) {
			ClockEnvelopeUnits();
			ClockLengthUnits();
			ClockLinearUnits();
			ClockSweepUnits();
		}
		/* NTSC: cycle 29828. PAL: cycle 33252. */
		else if (cpu_cycle_count == table[3]) {
			if (mode == 0 && !interrupt_inhibit) {
				SetFrameCounterIrqLow();
			}
		}
		/* NTSC: cycle 29829. PAL: cycle 33253. */
		else if (cpu_cycle_count == table[4]) {
			if (mode == 0) {
				ClockEnvelopeUnits();
				ClockLengthUnits();
				ClockLinearUnits();
				ClockSweepUnits();
				if (!interrupt_inhibit) {
					SetFrameCounterIrqLow();
				}
			}
		}
		/* NTSC: cycle 29830. PAL: cycle 33254. */
		else if (cpu_cycle_count == table[5]) {
			if (mode == 0 && !interrupt_inhibit) {
				SetFrameCounterIrqLow();
				cpu_cycle_count = 0;
			}
		}
		/* NTSC: cycle 37282. PAL: cycle 41566. */
		else if (cpu_cycle_count == table[7]) {
			cpu_cycle_count = 0;
		}
	}


	template<uint id>
	void PulseChannel<id>::ClockEnvelope()
	{
		if (!envelope.start_flag) {
			if (envelope.divider == 0) {
				envelope.divider = envelope.divider_period;
				if (envelope.decay_level_cnt == 0) {
					if (length_counter.halt) { // doubles as the envelope loop flag
						envelope.decay_level_cnt = 15;
						UpdateVolume();
					}
				}
				else {
					envelope.decay_level_cnt--;
					UpdateVolume();
				}
			}
			else {
				envelope.divider--;
			}
		}
		else {
			envelope.start_flag = 0;
			envelope.decay_level_cnt = 15;
			envelope.divider = envelope.divider_period;
			UpdateVolume();
		}
	}


	template<uint id>
	void PulseChannel<id>::ClockLength()
	{
		if (length_counter.halt || length_counter.value == 0) {
			return;
		}
		/* Technically, the length counter silences the channel when clocked while already zero.
		   The values in the length table are the actual values the length counter gets loaded with plus one,
		   to allow us to use a model where the channel is silenced when the length counter becomes zero. */
		if (--length_counter.value == 0) {
			volume = 0;
		}
	}


	template<uint id>
	void PulseChannel<id>::ClockSweep()
	{
		if (sweep.divider == 0 && sweep.enabled && !sweep.muting) {
			ComputeTargetTimerPeriod();
			// If the shift count is zero, the channel's period is never updated.
			if (sweep.shift_count != 0) {
				timer_period = sweep.target_timer_period;
				/* The target period is continuously recomputed. However, parameters change only ever so often,
				   so the function only needs to be called when this happens.
				   The current period is one of the parameters. */
				ComputeTargetTimerPeriod();
			}
		}
		if (sweep.divider == 0 || sweep.reload) {
			sweep.divider = sweep.divider_period;
			sweep.reload = false;
		}
		else {
			sweep.divider--;
		}
	}

	
	template<uint id>
	void PulseChannel<id>::ComputeTargetTimerPeriod()
	{
		int timer_period_change = timer_period >> sweep.shift_count;
		if (sweep.negate) {
			timer_period_change = -timer_period_change;
			// If the change amount is negative, pulse 1 adds the one's complement (-c-1), while pulse 2 adds the two's complement (-c).
			// TODO: not sure of the actual width of 'timer_period_change' in HW. overflows/underflows?
			static_assert(id == 1 || id == 2);
			if constexpr (id == 1) {
				timer_period_change--;
			}
		}
		sweep.target_timer_period = std::max(0, (int)timer_period + timer_period_change);
		UpdateSweepMuting();
	}


	template<uint id>
	u8 PulseChannel<id>::GetOutput()
	{
		return output * volume;
	}

	
	template<uint id>
	void PulseChannel<id>::Step()
	{
		static constexpr std::array<u8, 32> pulse_duty_table = {
			0, 1, 0, 0, 0, 0, 0, 0,
			0, 1, 1, 0, 0, 0, 0, 0,
			0, 1, 1, 1, 1, 0, 0, 0,
			1, 0, 0, 1, 1, 1, 1, 1
		};
		if (timer == 0) {
			timer = timer_period;
			duty_pos++;
			output = pulse_duty_table[duty * 8 + duty_pos];
		}
		else {
			timer--;
		}
	}


	template<uint id>
	void PulseChannel<id>::UpdateSweepMuting()
	{
		sweep.muting = sweep.target_timer_period > 0x7FF || timer_period < 8;
		UpdateVolume();
	}


	template<uint id>
	void PulseChannel<id>::UpdateVolume()
	{
		if (length_counter.value == 0 || sweep.muting) {
			volume = 0;
		}
		else if (envelope.const_vol) {
			volume = envelope.divider_period; // Doubles as the channel's volume when in constant volume mode
		}
		else {
			volume = envelope.decay_level_cnt;
		}
	}


	void TriangleChannel::ClockLength()
	{
		if (length_counter.halt || length_counter.value == 0) {
			return;
		}
		--length_counter.value;
		/* If the counter becomes 0, the volume is not set to 0; silencing the triangle channel merely halts it. It will continue to output its last value, rather than 0. */
	}


	void TriangleChannel::ClockLinear()
	{
		if (linear_counter.reload) {
			linear_counter.value = linear_counter.reload_value;
		}
		else if (linear_counter.value > 0) {
			--linear_counter.value;
		}
		if (!length_counter.halt) { // Note: this flag doubles as the linear counter control flag.
			linear_counter.reload = false;
		}
	}


	u8 TriangleChannel::GetOutput()
	{
		return output;
	}


	void TriangleChannel::Step()
	{
		static constexpr std::array<u8, 32> triangle_duty_table = {
			15, 14, 13, 12, 11, 10, 9, 8, 7, 6,  5,  4,  3,  2,  1,  0,
			 0,  1,  2,  3,  4,  5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
		};
		if (timer == 0) {
			timer = timer_period;
			// The sequencer is clocked by the timer as long as both the linear counter and the length counter are nonzero.
			if (linear_counter.value != 0 && length_counter.value != 0) {
				duty_pos++;
				output = triangle_duty_table[duty_pos];
			}
		}
		else {
			timer--;
		}
	}


	void NoiseChannel::ClockEnvelope()
	{
		if (!envelope.start_flag) {
			if (envelope.divider == 0) {
				envelope.divider = envelope.divider_period;
				if (envelope.decay_level_cnt == 0) {
					if (length_counter.halt) { // doubles as the envelope loop flag
						envelope.decay_level_cnt = 15;
						UpdateVolume();
					}
				}
				else {
					envelope.decay_level_cnt--;
					UpdateVolume();
				}
			}
			else {
				envelope.divider--;
			}
		}
		else {
			envelope.start_flag = 0;
			envelope.decay_level_cnt = 15;
			envelope.divider = envelope.divider_period;
			UpdateVolume();
		}
	}


	void NoiseChannel::ClockLength()
	{
		if (length_counter.halt || length_counter.value == 0) {
			return;
		}
		if (--length_counter.value == 0) {
			volume = 0;
		}
	}


	u8 NoiseChannel::GetOutput()
	{
		return output * volume;
	}


	void NoiseChannel::Step()
	{
		if (timer == 0) {
			timer = timer_period;
			output = (lfsr & 1) ^ (mode ? (lfsr >> 6 & 1) : (lfsr >> 1 & 1));
			lfsr >>= 1;
			lfsr |= output << 14;
			UpdateVolume();
		}
		else {
			timer--;
		}
	}


	void NoiseChannel::UpdateVolume()
	{
		if (length_counter.value == 0 || (lfsr & 1)) { // The volume is 0 if bit 0 of the LFSR is set
			volume = 0;
		}
		else if (envelope.const_vol) {
			volume = envelope.divider_period; // Doubles as the channel's volume when in constant volume mode
		}
		else {
			volume = envelope.decay_level_cnt;
		}
	}


	u8 DMC::GetOutput()
	{
		return output_level;
	}


	void DMC::ReadSampleByte()
	{
		sample_buffer = Cartridge::ReadPRG(current_sample_addr);
		current_sample_addr = (current_sample_addr + 1) | 0x8000; // If the address exceeds $FFFF, it is wrapped around to $8000.
		sample_buffer_is_empty = false;
		if (--bytes_remaining == 0) {
			if (loop) {
				RestartSample();
			}
			else if (irq_enable) {
				SetDmcIrqLow();
			}
		}
		CPU::Stall();
	}


	void DMC::RestartSample()
	{
		current_sample_addr = sample_addr_start;
		bytes_remaining = sample_length;
	}


	void DMC::Step()
	{
		if (--apu_cycles_until_step > 0) {
			return;
		}
		if (!silence_flag) {
			int new_output_level = output_level + ((shift_register & 1) ? 2 : -2);
			if (new_output_level >= 0 && new_output_level <= 127) {
				output_level = new_output_level;
			}
		}
		shift_register >>= 1;
		if (--bits_remaining == 0) {
			bits_remaining = 8;
			if (sample_buffer_is_empty) {
				silence_flag = true;
			}
			else {
				silence_flag = false;
				shift_register = sample_buffer;
				if (bytes_remaining > 0) {
					ReadSampleByte();
				}
				else {
					sample_buffer_is_empty = true;
				}
			}
		}
		apu_cycles_until_step = period;
	}


	void LengthCounter::UpdateHaltFlag()
	{
		if (write_to_halt_next_cpu_cycle) {
			halt = bit_to_write_to_halt;
			write_to_halt_next_cpu_cycle = false;
		}
	}


	void ClockEnvelopeUnits()
	{
		pulse_ch_1.ClockEnvelope();
		pulse_ch_2.ClockEnvelope();
		noise_ch.ClockEnvelope();
	}


	void ClockLengthUnits()
	{
		pulse_ch_1.ClockLength();
		pulse_ch_2.ClockLength();
		triangle_ch.ClockLength();
		noise_ch.ClockLength();
	}


	void ClockLinearUnits()
	{
		triangle_ch.ClockLinear();
	}


	void ClockSweepUnits()
	{
		pulse_ch_1.ClockSweep();
		pulse_ch_2.ClockSweep();
	}


	void SampleAndMix()
	{
		// https://wiki.nesdev.org/w/index.php?title=APU_Mixer

		static constexpr std::array pulse_table = [] {
			// https://wiki.nesdev.org/w/index.php?title=APU_Mixer#Lookup_Table
			std::array<f32, 31> table{};
			table[0] = 0.0f;
			for (size_t i = 1; i < table.size(); ++i) {
				table[i] = 95.52f / (8128.0f / f32(i) + 100.0f);
			}
			return table;
		}();

		static constexpr std::array tnd_table = [] {
			std::array<f32, 203> table{};
			table[0] = 0.0f;
			for (size_t i = 1; i < table.size(); ++i) {
				table[i] = 163.67f / (24329.0f / f32(i) + 100.0f);
			}
			return table;
		}();

		auto pulse_sum = pulse_ch_1.GetOutput() + pulse_ch_2.GetOutput();
		auto pulse_out = pulse_table[pulse_sum];

		auto tnd_sum = 3 * triangle_ch.GetOutput() + 2 * noise_ch.GetOutput() + dmc.GetOutput();
		auto tnd_out = tnd_table[tnd_sum];

		auto output = pulse_out + tnd_out; /* [0, 2] */

		Audio::EnqueueSample(output);
		Audio::EnqueueSample(output);
	}


	void SetDmcIrqLow()
	{
		dmc.interrupt = 1;
		CPU::SetIrqLow(CPU::IrqSource::ApuDmc);
	}


	void SetDmcIrqHigh()
	{
		dmc.interrupt = 0;
		CPU::SetIrqHigh(CPU::IrqSource::ApuDmc);
	}


	void SetFrameCounterIrqLow()
	{
		frame_counter.interrupt = 1;
		CPU::SetIrqLow(CPU::IrqSource::ApuFrame);
	}


	void SetFrameCounterIrqHigh()
	{
		frame_counter.interrupt = 0;
		CPU::SetIrqHigh(CPU::IrqSource::ApuFrame);
	}


	void StreamState(SerializationStream& stream)
	{
		// TODO
	}
}