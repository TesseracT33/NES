#include "APU.h"


void APU::PowerOn(const System::VideoStandard standard)
{
	audio_spec.freq = sample_rate;
	audio_spec.format = AUDIO_F32;
	audio_spec.channels = 2;
	audio_spec.samples = sample_buffer_size;
	audio_spec.callback = NULL;

	SDL_AudioSpec obtainedSpec;
	SDL_OpenAudio(&audio_spec, &obtainedSpec);
	SDL_PauseAudio(0);

	Reset();

    for (u16 addr = 0x4000; addr <= 0x4013; addr++)
        WriteRegister(addr, 0x00);
    WriteRegister(0x4015, 0x00);
    WriteRegister(0x4017, 0x00);

    pulse_ch_1.timer = pulse_ch_1.timer_period;
    pulse_ch_2.timer = pulse_ch_2.timer_period;

    noise_ch.LFSR = 1;
    dmc.output_level = 0;

	switch (standard)
	{
	case System::VideoStandard::NTSC : this->standard = NTSC ; break;
	case System::VideoStandard::PAL  : this->standard = PAL  ; break;
	case System::VideoStandard::Dendy: this->standard = Dendy; break;
	}
}


void APU::Reset()
{
    WriteRegister(0x4015, 0x00);
}


void APU::Update()
{

    // APU::Update() is called every CPU cycle, and 2 CPU cycles = 1 APU cycle
	// Certain component update every APU cycle, others every CPU cycle.
    if (on_apu_cycle)
    {
		frame_counter.Step();
		pulse_ch_1.Step();
		pulse_ch_2.Step();
    }

    dmc.Step();
    triangle_ch.Step();

    cpu_cycle_sample_counter += sample_rate;
    if (cpu_cycle_sample_counter >= standard.cpu_cycles_per_sec)
    {
        MixAndSample();
        cpu_cycle_sample_counter -= standard.cpu_cycles_per_sec;
    }

	on_apu_cycle = !on_apu_cycle;
}


u8 APU::ReadRegister(u16 addr)
{
    // Only $4015 is readable, the rest are write only.
    if (addr == Bus::Addr::APU_STAT)
    {
        u8 ret = (pulse_ch_1.length_counter.value  > 0)
               | (pulse_ch_2.length_counter.value  > 0) << 1
               | (triangle_ch.length_counter.value > 0) << 2
               | (noise_ch.length_counter.value    > 0) << 3
               | (dmc.bytes_remaining > 0             ) << 4
               | (frame_counter.interrupt             ) << 6
               | (dmc.IRQ_enable                      ) << 7;
        frame_counter.interrupt = 0;
        nes->cpu->SetIRQHigh(IRQ_APU_FRAME_COUNTER_mask);
        // TODO If an interrupt flag was set at the same moment of the read, it will read back as 1 but it will not be cleared.
        return ret;
    }
    return 0xFF;
}


void APU::WriteRegister(u16 addr, u8 data)
{
    switch (addr)
    {
    case Bus::Addr::SQ1_VOL: // $4000
        pulse_ch_1.envelope.divider_period = data;
        pulse_ch_1.envelope.const_vol = data & 0x10;
        pulse_ch_1.length_counter.halt = data & 0x20;
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
        if (pulse_ch_1.enabled)
        {
            pulse_ch_1.length_counter.SetValue(length_table[data >> 3]);
            pulse_ch_1.UpdateVolume(); // TODO: possible to somehow call UpdateVolume from SetValue?
        }
		pulse_ch_1.duty_pos = 0;
		pulse_ch_1.envelope.start_flag = true;
        pulse_ch_1.ComputeTargetTimerPeriod();
        break;

    case Bus::Addr::SQ2_VOL: // $4004
        pulse_ch_2.envelope.divider_period = data;
        pulse_ch_2.envelope.const_vol = data & 0x10;
        pulse_ch_2.length_counter.halt = data & 0x20;
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
        if (pulse_ch_2.enabled)
        {
            pulse_ch_2.length_counter.SetValue(length_table[data >> 3]);
            pulse_ch_2.UpdateVolume();
        }
		pulse_ch_2.duty_pos = 0;
		pulse_ch_2.envelope.start_flag = true;
        pulse_ch_2.ComputeTargetTimerPeriod();
        break;

    case Bus::Addr::TRI_LINEAR: // $4008
        triangle_ch.linear_counter.reload_value = data;
        triangle_ch.length_counter.halt = data & 0x80; // Doubles as the linear counter control flag
        break;

    case Bus::Addr::TRI_LO: // $400A
        triangle_ch.timer_period = triangle_ch.timer_period & 0x700 | data;
        break;

    case Bus::Addr::TRI_HI: // $400B
        triangle_ch.timer_period = triangle_ch.timer_period & 0xFF | data << 8;
        if (triangle_ch.enabled)
        {
            triangle_ch.length_counter.SetValue(length_table[data >> 3]);
            triangle_ch.UpdateVolume();
        }
        triangle_ch.linear_counter.reload = true;
        break;

    case Bus::Addr::NOISE_VOL: // $400C
        noise_ch.envelope.divider_period = data;
        noise_ch.envelope.const_vol = data & 0x10;
        noise_ch.length_counter.halt = data & 0x20;
        noise_ch.UpdateVolume();
        break;

    case Bus::Addr::NOISE_LO: // $400E
        noise_ch.timer_period = standard.noise_period_table[data & 0xF];
        noise_ch.loop_noise = data & 0x80;
        break;

    case Bus::Addr::NOISE_HI: // $400F
        if (noise_ch.enabled)
        {
            noise_ch.length_counter.SetValue(length_table[data >> 3]);
            noise_ch.UpdateVolume();
        }
        noise_ch.envelope.start_flag = true;
        break;

    case Bus::Addr::DMC_FREQ: // $4010
        dmc.period = standard.dmc_rate_table[data & 0xF];
        dmc.loop = data & 0x40;
        dmc.IRQ_enable = data & 0x80;
        if (!dmc.IRQ_enable)
			nes->cpu->SetIRQHigh(IRQ_APU_DMC_mask);
        break;

    case Bus::Addr::DMC_RAW: // $4011
        dmc.output_level = data;
        break;

    case Bus::Addr::DMC_START: // $4012
        dmc.sample_addr = 0xC000 | data << 6;
        break;

    case Bus::Addr::DMC_LEN: // $4013
        dmc.sample_len = 1 | data << 4;
        break;

    case Bus::Addr::APU_STAT: // $4015
        pulse_ch_1.enabled = data & 0x01;
		if (!pulse_ch_1.enabled)
		{
			pulse_ch_1.length_counter.SetToZero();
			pulse_ch_1.volume = 0;
		}

        pulse_ch_2.enabled = data & 0x02;
		if (!pulse_ch_2.enabled)
		{
			pulse_ch_2.length_counter.SetToZero();
			pulse_ch_2.volume = 0;
		}

        triangle_ch.enabled = data & 0x04;
		if (!triangle_ch.enabled)
		{
			triangle_ch.length_counter.SetToZero();
			triangle_ch.volume = 0;
		}

        noise_ch.enabled = data & 0x08;
		if (!noise_ch.enabled)
		{
			noise_ch.length_counter.SetToZero();
			noise_ch.volume = 0;
		}

        dmc.enabled = data & 0x10;
        // todo: not clear if the xor between the new dmc.enabled and previous one matters
        if (dmc.enabled)
        {
            if (dmc.bytes_remaining == 0)
            {
                if (dmc.bits_remaining == 0)
                    dmc.RestartSample();
                else
                    dmc.restart_sample_after_buffer_is_emptied = true;
            }
        }
        else
            dmc.bytes_remaining = 0;

		dmc.interrupt = 0;
		nes->cpu->SetIRQHigh(IRQ_APU_DMC_mask);
        break;

    case Bus::Addr::FRAME_CNT: // $4017
        /* If the write occurs during an APU cycle, the effects occur 3 CPU cycles after the $4017 write cycle, 
           and if the write occurs between APU cycles, the effects occurs 4 CPU cycles after the write cycle. */
        frame_counter.pending_4017_write = true;
        frame_counter.cpu_cycles_until_apply_4017_write = on_apu_cycle ? 3 : 4;
        frame_counter.data_written_to_4017 = data;

        /* Writing $80 to $4017 should clock length immediately. Source: blarrg apu test */
        /* TODO: is only $80 allowed, or any number with bit 7 set? */
        if (data == 0x80)
            ClockLengthUnits();
        break;

    default:
        break;
    }
}


void APU::FrameCounter::Step()
{
    // If $4017 was written to, the write doesn't apply until a few cpu cycles later.
    if (pending_4017_write && --cpu_cycles_until_apply_4017_write == 0)
    {
		/* If bit 6 is set, the frame interrupt flag is cleared, otherwise it is unaffected. */
		interrupt_inhibit = data_written_to_4017 & 0x40;
		if (interrupt_inhibit)
		{
			apu->nes->cpu->SetIRQHigh(IRQ_APU_FRAME_COUNTER_mask); // TODO: make less ugly
			interrupt = false;
		}

        mode = data_written_to_4017 & 0x80;
        pending_4017_write = false;
        cpu_cycle_count = 0;
        return;
    }

    // The frame counter is clocked on every other CPU cycle, i.e. on every APU cycle.
    // This function is called every CPU cycle.
    // Therefore, the APU cycle counts from https://wiki.nesdev.org/w/index.php?title=APU_Frame_Counter have been doubled.
    switch (++cpu_cycle_count)
    {
    case 7459: // APU cycle 3728.5
        apu->ClockEnvelopeUnits();
        apu->ClockLinearUnits();
        break;

    case 14915: // APU cycle 7456.5
        apu->ClockEnvelopeUnits();
        apu->ClockLengthUnits();
        apu->ClockLinearUnits();
        apu->ClockSweepUnits();
        break;

    case 22373: // APU cycle 11185.5
        apu->ClockEnvelopeUnits();
        apu->ClockLinearUnits();
        break;

    case 29830: // APU cycle 14914
        if (mode == 0 && interrupt_inhibit == 0)
        {
            apu->nes->cpu->SetIRQLow(IRQ_APU_FRAME_COUNTER_mask); // TODO: make less ugly
            interrupt = true;
        }
        break;

    case 29831: // APU cycle 14914.5
        if (mode == 0)
        {
            apu->ClockEnvelopeUnits();
            apu->ClockLengthUnits();
            apu->ClockLinearUnits();
            apu->ClockSweepUnits();
            if (interrupt_inhibit == 0)
            {
                apu->nes->cpu->SetIRQLow(IRQ_APU_FRAME_COUNTER_mask); // TODO: make less ugly
                interrupt = true;
            }
        }
        break;

    case 29832: // APU cycle 14915
        if (mode == 0)
        {
            if (interrupt_inhibit == 0)
            {
                apu->nes->cpu->SetIRQLow(IRQ_APU_FRAME_COUNTER_mask); // TODO: make less ugly
                interrupt = true;
            }
            cpu_cycle_count = 1;
        }
        break;

    case 37283: // APU cycle 18640.5
        apu->ClockEnvelopeUnits();
        apu->ClockLengthUnits();
        apu->ClockLinearUnits();
        apu->ClockSweepUnits();
        break;

    case 37284: // APU cycle 18641
        cpu_cycle_count = 1;
        break;

    default:
        break;
    }
}


void APU::PulseCh::ClockEnvelope()
{
    if (envelope.start_flag == 0)
    {
        if (envelope.divider == 0)
        {
            envelope.divider = envelope.divider_period;
            if (envelope.decay_level_cnt == 0)
            {
                if (length_counter.halt == 1) // doubles as the envelope loop flag
                {
                    envelope.decay_level_cnt = 15;
                    UpdateVolume();
                }
            }
            else
            {
                envelope.decay_level_cnt--;
                UpdateVolume();
            }
        }
        else
            envelope.divider--;
    }
    else
    {
        envelope.start_flag = 0;
        envelope.decay_level_cnt = 15;
        envelope.divider = envelope.divider_period;
        UpdateVolume();
    }
}


void APU::PulseCh::ClockLength()
{
    if (length_counter.halt || length_counter.has_reached_zero)
        return;

    /* Technically, the length counter silences the channel when clocked while already zero.
	   The values in the length table are the actual values the length counter gets loaded with plus one,
	   to allow us to use a model where the channel is silenced when the length counter becomes zero. */
	if (--length_counter.value == 0)
	{
		length_counter.has_reached_zero = true;
		volume = 0;
	}
}


void APU::PulseCh::ClockSweep()
{
    if (sweep.divider == 0 && sweep.enabled && !sweep.muting)
    {
		ComputeTargetTimerPeriod();
		// If the shift count is zero, the channel's period is never updated.
		if (sweep.shift_count != 0)
		{
			timer_period = sweep.target_timer_period;
			/* The target period is continuously recomputed. However, parameters change only ever so often,
			   so the function only needs to be called when this happens.
			   The current period is one of the parameters. */
			ComputeTargetTimerPeriod();
		}
    }
    if (sweep.divider == 0 || sweep.reload)
    {
        sweep.divider = sweep.divider_period;
        sweep.reload = false;
    }
    else
        sweep.divider--;
}


void APU::PulseCh::ComputeTargetTimerPeriod()
{
    int timer_period_change = timer_period >> sweep.shift_count;
    if (sweep.negate)
    {
        timer_period_change = -timer_period_change;
        // If the change amount is negative, pulse 1 adds the one's complement (-c-1), while pulse 2 adds the two's complement (-c).
        // TODO: not sure of the actual width of 'timer_period_change' in HW. overflows/underflows?
        if (id == 1)
            timer_period_change--;
    }
    sweep.target_timer_period = std::max(0, (int)timer_period + timer_period_change);

	UpdateSweepMuting();
}


void APU::PulseCh::Step()
{
    if (timer == 0)
    {
        timer = timer_period;
        duty_pos++;
        output = pulse_duty_table[duty][duty_pos];
    }
    else
        timer--;
}


void APU::TriangleCh::ClockLength()
{
    if (length_counter.halt || length_counter.has_reached_zero)
        return;

    if (--length_counter.value == 0)
    {
        length_counter.has_reached_zero = true;
		volume = 0;
    }
}


void APU::TriangleCh::ClockLinear()
{
    if (linear_counter.reload)
    {
        linear_counter.Reload();
        UpdateVolume(); // TODO: what should happen when linear is reloaded, but length is still at 0? Currently, volume will still be 0.
    }
    else if (linear_counter.value != 0)
    {
        if (--linear_counter.value == 0)
        {
            linear_counter.has_reached_zero = true;
			volume = 0;
        }
    }
    if (!length_counter.halt) // Note: this flag doubles as the linear counter control flag.
        linear_counter.reload = false;
}


void APU::TriangleCh::Step()
{
    if (timer == 0)
    {
        timer = timer_period;
        // The sequencer is clocked by the timer as long as both the linear counter and the length counter are nonzero.
        if (linear_counter.value != 0 && length_counter.value != 0)
        {
            duty_pos++;
            output = triangle_duty_table[duty_pos];
        }
    }
    else
        timer--;
}


void APU::NoiseCh::ClockEnvelope()
{
    // TODO; code duplication with APU::PulseCh::ClockEnvelope
    if (envelope.start_flag == 0)
    {
        if (envelope.divider == 0)
        {
            envelope.divider = envelope.divider_period;
            if (envelope.decay_level_cnt == 0)
            {
                if (length_counter.halt == 1) // doubles as the envelope loop flag
                {
                    envelope.decay_level_cnt = 15;
                    UpdateVolume();
                }
            }
            else
            {
                envelope.decay_level_cnt--;
                UpdateVolume();
            }
        }
        else
            envelope.divider--;
    }
    else
    {
        envelope.start_flag = 0;
        envelope.decay_level_cnt = 15;
        envelope.divider = envelope.divider_period;
        UpdateVolume();
    }
}


void APU::NoiseCh::ClockLength()
{
    if (length_counter.halt || length_counter.has_reached_zero)
        return;

    if (--length_counter.value == 0)
    {
		length_counter.has_reached_zero = true;
		volume = 0;
    }
}


void APU::NoiseCh::Step()
{
    if (timer == 0)
    {
        timer = timer_period;
        output = (LFSR & 1) ^ (loop_noise ? (LFSR >> 6 & 1) : (LFSR >> 1 & 1));
        LFSR >>= 1;
        LFSR |= output << 14;
        UpdateVolume(); // The volume is set to 0 if bit 0 of the LFSR is set.
    }
    else
        timer--;
}


void APU::DMC::Step()
{
    if (read_sample_on_next_apu_cycle)
    {
        ReadSample();
        read_sample_on_next_apu_cycle = false;
    }

    if (--cpu_cycles_until_step > 0)
        return;

    if (!silence_flag)
    {
        int new_output_level = output_level + ((shift_register & 1) ? 2 : -2);
        if (new_output_level >= 0 && new_output_level <= 127)
            output_level = new_output_level;
    }

    shift_register >>= 1;

    if (--bits_remaining == 0)
    {
        bits_remaining = 8;
        if (restart_sample_after_buffer_is_emptied)
        {
            RestartSample();
            restart_sample_after_buffer_is_emptied = false;
        }
        if (sample_buffer_is_empty)
            silence_flag = true;
        else if (enabled)
        {
            silence_flag = false;
            shift_register = sample_buffer;
            sample_buffer_is_empty = true;
            if (bytes_remaining > 0)
            {
                if (apu->on_apu_cycle)
                    ReadSample();
                else
                    read_sample_on_next_apu_cycle = true;
            }
        }
    }

    cpu_cycles_until_step = period;
}


void APU::DMC::ReadSample()
{
    apu->nes->cpu->Stall(); // TODO: make less ugly
    sample_buffer = apu->nes->mapper->ReadPRG(current_sample_addr); // TODO: make less ugly
    sample_buffer_is_empty = false;
    current_sample_addr = (current_sample_addr + 1) | 0x8000; // If the address exceeds $FFFF, it is wrapped around to $8000.

    if (--bytes_remaining == 0)
    {
		if (loop)
			RestartSample();
		else if (IRQ_enable)
			apu->nes->cpu->SetIRQLow(IRQ_APU_DMC_mask); // TODO: make less ugly
    }
}


void APU::MixAndSample()
{
    // https://wiki.nesdev.org/w/index.php?title=APU_Mixer
    u8 pulse_sum = pulse_ch_1.output * pulse_ch_1.volume + pulse_ch_2.output * pulse_ch_2.volume;
    f32 pulse_out = pulse_table[pulse_sum];
    u16 tnd_sum = 3 * triangle_ch.output * triangle_ch.volume + 
        2 * noise_ch.output * noise_ch.volume + dmc.output_level * !dmc.silence_flag;
    f32 tnd_out = tnd_table[tnd_sum];

    f32 output = pulse_out + tnd_out;

    sample_buffer[sample_buffer_index++] = output;
    sample_buffer[sample_buffer_index++] = output;

    if (sample_buffer_index == sample_buffer_size)
    {
        while (SDL_GetQueuedAudioSize(1) > sample_buffer_size * sizeof(f32));
		SDL_QueueAudio(1, sample_buffer, sample_buffer_size * sizeof(f32));
        sample_buffer_index = 0;
    }
}


void APU::StreamState(SerializationStream& stream)
{

}
