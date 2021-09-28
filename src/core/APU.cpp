#include "APU.h"


void APU::Initialize()
{

}


void APU::Power()
{
    noise_ch.LFSR = 1;
    dmc.output_level = 0;
    WriteRegister(Bus::Addr::SND_CHN, 0x00);
}


void APU::Reset()
{
    // TODO: move this to somewhere else
    audio_spec.freq = sample_rate;
    audio_spec.format = AUDIO_F32;
    audio_spec.channels = 1;
    audio_spec.samples = sample_buffer_size;
    audio_spec.callback = NULL;

    SDL_AudioSpec obtainedSpec;
    SDL_OpenAudio(&audio_spec, &obtainedSpec);
    SDL_PauseAudio(0);

    WriteRegister(Bus::Addr::SND_CHN, 0x00);
}


void APU::Update()
{
    StepFrameCounter();
    dmc.Step();
    triangle_ch.Step();

    // APU::Update() is called every CPU cycle, and 2 CPU cycles = 1 APU cycle
    if (!on_apu_cycle)
    {
        on_apu_cycle = true;
    }
    else
    {
        // The pulse channels' timers are clocked every apu cycle.
        pulse_ch_1.Step();
        pulse_ch_2.Step();
    }

    if (--cpu_cycles_until_sample == 0)
    {
        Mix();
        cpu_cycles_until_sample = cpu_cycles_per_sample;
    }
}


u8 APU::ReadRegister(u16 addr)
{
    // Only $4015 is readable, the rest are write only.
    if (addr == Bus::Addr::SND_CHN)
    {
        u8 ret = (pulse_ch_1.len_cnt > 0 )
               | (pulse_ch_2.len_cnt > 0 ) << 1
               | (triangle_ch.len_cnt > 0) << 2
               | (noise_ch.len_cnt > 0   ) << 3
               | (dmc.bytes_remaining > 0) << 4
               | (0x01                   ) << 5 // TODO: unclear if this should return 0 or 1
               | (frame_counter.interrupt) << 6
               | (dmc.IRQ_enable         ) << 7;
        frame_counter.interrupt = false;
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
        pulse_ch_1.const_vol = data & 0x10;
        if (pulse_ch_1.const_vol)
            pulse_ch_1.volume = pulse_ch_1.envelope.divider_period; // The divider period doubles as the volume.
        else
            pulse_ch_1.volume = pulse_ch_1.envelope.decay_level_cnt;
        pulse_ch_1.len_cnt_halt = data & 0x20;
        pulse_ch_1.duty = data >> 6;
        break;

    case Bus::Addr::SQ1_SWEEP: // $4001
        pulse_ch_1.sweep.shift_count = data;
        pulse_ch_1.sweep.negate = data & 0x08;
        pulse_ch_1.sweep.divider_period = data >> 4;
        pulse_ch_1.sweep.enabled = data & 0x80;
        pulse_ch_1.sweep.reload = true;
        break;

    case Bus::Addr::SQ1_LO: // $4002
        pulse_ch_1.timer_period = pulse_ch_1.timer_period & 0x700 | data;
        if (pulse_ch_1.timer_period < 8)
            pulse_ch_1.muted = true;
        break;

    case Bus::Addr::SQ1_HI: // $4003
        pulse_ch_1.timer_period = pulse_ch_1.timer_period & 0xFF | data << 8;
        if (pulse_ch_1.timer_period < 8)
            pulse_ch_1.muted = true;
        if (pulse_ch_1.enabled)
            pulse_ch_1.len_cnt = data >> 3;
        pulse_ch_1.envelope.start_flag = true;
        break;

    case Bus::Addr::SQ2_VOL: // $4004
        pulse_ch_2.envelope.divider_period = data;
        pulse_ch_2.const_vol = data & 0x10;
        if (pulse_ch_2.const_vol)
            pulse_ch_2.volume = pulse_ch_2.envelope.divider_period; // The divider period doubles as the volume.
        else
            pulse_ch_2.volume = pulse_ch_2.envelope.decay_level_cnt;
        pulse_ch_2.len_cnt_halt = data & 0x20;
        pulse_ch_2.duty = data >> 6;
        break;

    case Bus::Addr::SQ2_SWEEP: // $4005
        pulse_ch_2.sweep.shift_count = data;
        pulse_ch_2.sweep.negate = data & 0x08;
        pulse_ch_2.sweep.divider_period = data >> 4;
        pulse_ch_2.sweep.enabled = data & 0x80;
        pulse_ch_2.sweep.reload = true;
        break;

    case Bus::Addr::SQ2_LO: // $4006
        pulse_ch_2.timer_period = pulse_ch_2.timer_period & 0x700 | data;
        if (pulse_ch_2.timer_period < 8)
            pulse_ch_2.muted = true;
        break;

    case Bus::Addr::SQ2_HI: // $4007
        pulse_ch_2.timer_period = pulse_ch_2.timer_period & 0xFF | data << 8;
        if (pulse_ch_2.timer_period < 8)
            pulse_ch_2.muted = true;
        if (pulse_ch_2.enabled)
            pulse_ch_2.len_cnt = data >> 3;
        pulse_ch_2.envelope.start_flag = true;
        break;

    case Bus::Addr::TRI_LINEAR: // $4008
        triangle_ch.linear_cnt_reload = data;
        triangle_ch.linear_cnt = data;
        triangle_ch.linear_cnt_control = data & 0x80;
        triangle_ch.linear_cnt_reload_flag = triangle_ch.linear_cnt_control;
        break;

    case Bus::Addr::TRI_LO: // $400A
        triangle_ch.timer_period = triangle_ch.timer_period & 0x700 | data;
        break;

    case Bus::Addr::TRI_HI: // $400B
        triangle_ch.timer_period = triangle_ch.timer_period & 0xFF | data << 8;
        if (triangle_ch.enabled)
            triangle_ch.len_cnt = data >> 3;
        triangle_ch.linear_cnt_reload_flag = true;
        break;

    case Bus::Addr::NOISE_VOL: // $400C
        noise_ch.div_period = data;
        noise_ch.const_vol = data & 0x10;
        if (noise_ch.const_vol)
            noise_ch.volume = noise_ch.envelope.divider_period; // The divider period doubles as the volume.
        else
            noise_ch.volume = noise_ch.envelope.decay_level_cnt;
        noise_ch.len_cnt_halt = data & 0x20;
        break;

    case Bus::Addr::NOISE_LO: // $400E
        noise_ch.timer_period = noise_period_ntsc[data & 0xF];
        noise_ch.loop_noise = data & 0x80;
        break;

    case Bus::Addr::NOISE_HI: // $400F
        if (noise_ch.enabled)
            noise_ch.len_cnt = noise_length[data >> 3];
        noise_ch.envelope.start_flag = true;
        break;

    case Bus::Addr::DMC_FREQ: // $4010
        dmc.rate = dmc_rate_ntsc[data & 0xF];
        dmc.loop = data & 0x40;
        dmc.IRQ_enable = data & 0x80;
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

    case Bus::Addr::SND_CHN: // $4015
        pulse_ch_1.enabled = data & 0x01;
        if (!pulse_ch_1.enabled)
            pulse_ch_1.len_cnt = 0;

        pulse_ch_2.enabled = data & 0x02;
        if (!pulse_ch_2.enabled)
            pulse_ch_2.len_cnt = 0;

        triangle_ch.enabled = data & 0x04;
        if (!triangle_ch.enabled)
            triangle_ch.len_cnt = 0;

        noise_ch.enabled = data & 0x08;
        if (!noise_ch.enabled)
            noise_ch.len_cnt = 0;

        dmc.enabled = data & 0x10;
        // todo: not clear if the xor between the new dmc.enabled and previous one matters
        if (dmc.enabled)
        {
            if (dmc.bytes_remaining > 0)
            {
                dmc.current_sample_addr = dmc.sample_addr;
                dmc.bytes_remaining = dmc.sample_len;
                // todo If there are bits remaining in the 1-byte sample buffer, these will finish playing before the next sample is fetched.
            }
        }
        else
        {
            dmc.bytes_remaining = 0;
            // TODO: DMC bytes remaining will be set to 0 and the DMC will silence when it empties.
        }
        dmc.interrupt_flag = false;

        break;

    case Bus::Addr::FRAME_CNT: // $4017
        /* If the write occurs during an APU cycle, the effects occur 3 CPU cycles after the $4017 write cycle, 
           and if the write occurs between APU cycles, the effects occurs 4 CPU cycles after the write cycle. */
        frame_counter.pending_4017_write = true;
        frame_counter.cpu_cycles_until_apply_4017_write = on_apu_cycle ? 3 : 4;
        frame_counter.data_written_to_4017 = data;
        break;

    default:
        break;
    }
}


void APU::StepFrameCounter()
{
    // If $4017 was written to, the write doesn't apply until a few cpu cycles later.
    if (frame_counter.pending_4017_write && --frame_counter.cpu_cycles_until_apply_4017_write == 0)
    {
        frame_counter.interrupt_inhibit = frame_counter.data_written_to_4017 & 0x40;
        frame_counter.mode = frame_counter.data_written_to_4017 & 0x80;
        frame_counter.pending_4017_write = false;
        frame_counter.cpu_cycle_count = 0;
    }

    // The frame counter is clocked on every other CPU cycle, i.e. on every APU cycle.
    // This function is called every CPU cycle.
    // Therefore, the APU cycle counts from https://wiki.nesdev.org/w/index.php?title=APU_Frame_Counter have been doubled.
    switch (++frame_counter.cpu_cycle_count)
    {
    case 7457:
        ClockEnvelopeUnits();
        ClockLinearUnits();
        break;

    case 14913:
        ClockEnvelopeUnits();
        ClockLengthUnits();
        ClockLinearUnits();
        ClockSweepUnits();
        break;

    case 22371:
        ClockEnvelopeUnits();
        ClockLinearUnits();
        break;

    case 29828:
        if (frame_counter.mode == 0 && frame_counter.interrupt_inhibit == 0)
            frame_counter.interrupt = 1;
        break;

    case 29829:
        if (frame_counter.mode == 0)
        {
            ClockEnvelopeUnits();
            ClockLengthUnits();
            ClockLinearUnits();
            ClockSweepUnits();
            if (frame_counter.interrupt_inhibit == 0)
                frame_counter.interrupt = 1;
        }
        break;

    case 29830:
        if (frame_counter.mode == 0)
        {
            if (frame_counter.interrupt_inhibit == 0)
                frame_counter.interrupt = 1;
            frame_counter.cpu_cycle_count = 0;
        }
        break;

    case 37281:
        ClockEnvelopeUnits();
        ClockLengthUnits();
        ClockLinearUnits();
        ClockSweepUnits();
        break;

    case 37282:
        frame_counter.cpu_cycle_count = 0;
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
                if (len_cnt_halt == 1)
                    envelope.decay_level_cnt = 15;
            }
            else
                envelope.decay_level_cnt--;
        }
        else
            envelope.divider--;
    }
    else
    {
        envelope.start_flag = 0;
        envelope.decay_level_cnt = 15;
        envelope.divider = envelope.divider_period;
    }
}


void APU::PulseCh::ClockLength()
{
    if (len_cnt != 0 && !len_cnt_halt)
    {
        len_cnt--;
        if (len_cnt == 0)
            volume = 0;
    }
}


void APU::PulseCh::ClockSweep()
{
    if (sweep.divider == 0 && sweep.enabled && !muted)
    {
        // If the shift count is zero, the channel's period is never updated.
        if (sweep.shift_count != 0)
        {
            timer_period = sweep.timer_target_period;
            if (timer_period < 8)
                muted = true;
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
    // TODO: when to call this function?

    int timer_period_change = timer_period >> sweep.shift_count;
    if (sweep.negate)
    {
        timer_period_change = -timer_period_change;
        // If the change amount is negative, pulse 1 adds the one's complement (-c-1), while pulse 2 adds the two's complement (-c).
        // TODO: not sure of the actual width of 'timer_period_change' in HW. overflows/underflows?
        if (id == 1)
            timer_period_change--;
    }
    sweep.timer_target_period = timer_period + timer_period_change;

    if (sweep.timer_target_period > 0x7FF)
        muted = true;
}


void APU::PulseCh::Step()
{
    if (timer == 0)
    {
        timer = timer_period;
        duty_pos--; // The counter counts downward rather than upward.
        output = pulse_duty_table[duty][duty_pos];
    }
    else
        timer--;
}


void APU::TriangleCh::ClockLength()
{
    if (len_cnt != 0 && !linear_cnt_control) // note: 'linear_cnt_control' doubles as the length counter halt flag
    {
        len_cnt--;
        if (len_cnt == 0)
            volume = 0;
    }
}


void APU::TriangleCh::ClockLinear()
{
    if (linear_cnt_reload_flag == 1)
    {
        if (linear_cnt == 0)
            linear_cnt = linear_cnt_reload;
        else
            linear_cnt--;
    }
    if (linear_cnt_control == 0)
        linear_cnt_reload_flag = 0;
}


void APU::TriangleCh::Step()
{
    // The sequencer is clocked by the timer as long as both the linear counter and the length counter are nonzero.
    // TODO not sure if I have understood it correctly
    if (linear_cnt == 0 || len_cnt == 0)
        return;

    if (timer == 0)
    {
        timer = timer_period;
        duty_pos++;
        output = triangle_duty_table[duty_pos];
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
                if (len_cnt_halt == 1)
                    envelope.decay_level_cnt = 15;
            }
            else
                envelope.decay_level_cnt--;
        }
        else
            envelope.divider--;
    }
    else
    {
        envelope.start_flag = 0;
        envelope.decay_level_cnt = 15;
        envelope.divider = envelope.divider_period;
    }
}


void APU::NoiseCh::ClockLength()
{
    if (len_cnt != 0 && !len_cnt_halt)
    {
        len_cnt--;
        if (len_cnt == 0)
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
    }
    else
        timer--;
    // TODO  The mixer receives the current envelope volume except when
    // 1) Bit 0 of the shift register is set, or 2)  The length counter is zero
}


void APU::DMC::Step()
{
    if (--cpu_cycles_until_step > 0)
        return;

    if (!silence_flag)
    {
        int new_output_level = output_level + ((shift_register & 1) ? 2 : -2);
        if (new_output_level >= 0 && new_output_level <= 127)
            output_level = new_output_level;
    }

    // TODO: The right shift register is clocked.

    if (--bits_remaining == 0)
    {
        bits_remaining = 8;
        if (sample_buffer_is_empty)
            silence_flag = true;
        else
        {
            silence_flag = false;
            shift_register = sample_buffer;
            sample_buffer_is_empty = true;
        }
    }

    cpu_cycles_until_step = rate;
}


void APU::ReadSample()
{
    dmc.sample_buffer = mapper->ReadPRG(dmc.current_sample_addr);
    dmc.current_sample_addr++;
    if (dmc.current_sample_addr == 0x0000)
        dmc.current_sample_addr = 0x8000;

    if (dmc.bytes_remaining == 0)
    {
        if (dmc.loop)
        {
            dmc.current_sample_addr = dmc.sample_addr;
            dmc.bytes_remaining = dmc.sample_len;
        }
        else if (dmc.IRQ_enable)
            dmc.interrupt_flag = true;
    }
    else
        dmc.bytes_remaining--;
}


void APU::Mix()
{
    // https://wiki.nesdev.org/w/index.php?title=APU_Mixer
    u8 pulse_sum = pulse_ch_1.output + pulse_ch_2.output;
    f32 pulse_out = pulse_table[pulse_sum];
    u16 tnd_sum = 3 * triangle_ch.output + 2 * noise_ch.output + dmc.output;
    f32 tnd_out = tnd_table[tnd_sum];

    f32 output = pulse_out + tnd_out;

    sample_buffer[sample_buffer_index++] = output;

    if (sample_buffer_index == sample_buffer_size)
    {
        while (SDL_GetQueuedAudioSize(1) > sample_buffer_size * sizeof(f32));
        SDL_QueueAudio(1, sample_buffer, sample_buffer_size * sizeof(f32));
        sample_buffer_index = 0;
    }
}


void APU::State(Serialization::BaseFunctor& functor)
{

}
