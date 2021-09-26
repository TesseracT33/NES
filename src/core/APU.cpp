#include "APU.h"


void APU::Initialize()
{
    audio_spec.freq = sample_rate;
    audio_spec.format = AUDIO_F32;
    audio_spec.channels = 1;
    audio_spec.samples = sample_buffer_size;
    audio_spec.callback = NULL;

    SDL_AudioSpec obtainedSpec;
    SDL_OpenAudio(&audio_spec, &obtainedSpec);
    SDL_PauseAudio(0);
}


void APU::Power()
{
    noise_ch.LFSR = 1;
    WriteRegister(Bus::Addr::SND_CHN, 0x00);
}


void APU::Reset()
{
    WriteRegister(Bus::Addr::SND_CHN, 0x00);
}


void APU::Update()
{
    StepFrameCounter();

    // The triangle channel timer is clocked every cpu cycle.
    triangle_ch.Step();

    if (!on_apu_cycle)
    {
        on_apu_cycle = true;
    }
    else
    {
        // The pulse channels' timer is clocked every other cpu cycle (every apu cycle).
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
               | (dmc.active             ) << 4
               | (0x01                   ) << 5 // TODO: unclear if this should return 0 or 1
               | (frame_interrupt        ) << 6
               | (dmc.IRQ_enable         ) << 7;
        frame_interrupt = false;
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
        pulse_ch_1.len_cnt_halt = data & 0x20;
        pulse_ch_1.duty = data >> 6;
        break;

    case Bus::Addr::SQ1_SWEEP: // $4001
        pulse_ch_1.sweep.shift_count = data;
        pulse_ch_1.sweep.negate = data & 0x08;
        pulse_ch_1.sweep.divider_period = data >> 4;
        pulse_ch_1.sweep.enabled = data & 0x80;
        break;

    case Bus::Addr::SQ1_LO: // $4002
        pulse_ch_1.timer_reload = pulse_ch_1.timer_reload & 0x700 | data;
        break;

    case Bus::Addr::SQ1_HI: // $4003
        pulse_ch_1.timer_reload = pulse_ch_1.timer_reload & 0xFF | data << 8;
        pulse_ch_1.len_cnt_reload = data >> 3;
        if (pulse_ch_1.enabled) // TODO: is 'len_cnt_load' necessary as a variable?
            pulse_ch_1.len_cnt = pulse_ch_1.len_cnt_reload;
        pulse_ch_1.envelope.start_flag = true;
        break;

    case Bus::Addr::SQ2_VOL: // $4004
        pulse_ch_2.envelope.divider_period = data;
        pulse_ch_2.const_vol = data & 0x10;
        pulse_ch_2.len_cnt_halt = data & 0x20;
        pulse_ch_2.duty = data >> 6;
        break;

    case Bus::Addr::SQ2_SWEEP: // $4005
        pulse_ch_2.sweep.shift_count = data;
        pulse_ch_2.sweep.negate = data & 0x08;
        pulse_ch_2.sweep.divider_period = data >> 4;
        pulse_ch_2.sweep.enabled = data & 0x80;
        break;

    case Bus::Addr::SQ2_LO: // $4006
        pulse_ch_2.timer_reload = pulse_ch_2.timer_reload & 0x700 | data;
        break;

    case Bus::Addr::SQ2_HI: // $4007
        pulse_ch_2.timer_reload = pulse_ch_2.timer_reload & 0xFF | data << 8;
        pulse_ch_2.len_cnt_reload = data >> 3;
        if (pulse_ch_2.enabled) // TODO: is 'len_cnt_reload' necessary as a variable?
            pulse_ch_2.len_cnt = pulse_ch_2.len_cnt_reload;
        pulse_ch_2.envelope.start_flag = true;
        break;

    case Bus::Addr::TRI_LINEAR: // $4008
        triangle_ch.linear_cnt_reload = data;
        triangle_ch.linear_cnt = data;
        triangle_ch.linear_cnt_control = data & 0x80;
        triangle_ch.linear_cnt_reload_flag = triangle_ch.linear_cnt_control;
        break;

    case Bus::Addr::TRI_LO: // $400A
        triangle_ch.timer_reload = triangle_ch.timer_reload & 0x700 | data;
        break;

    case Bus::Addr::TRI_HI: // $400B
        triangle_ch.timer_reload = triangle_ch.timer_reload & 0xFF | data << 8;
        triangle_ch.len_cnt_reload = data >> 3;
        if (triangle_ch.enabled) // TODO: is 'len_cnt_reload' necessary as a variable?
            triangle_ch.len_cnt = triangle_ch.len_cnt_reload;
        break;

    case Bus::Addr::NOISE_VOL: // $400C
        noise_ch.div_period = data;
        noise_ch.const_vol = data & 0x10;
        noise_ch.len_cnt_halt = data & 0x20;
        break;

    case Bus::Addr::NOISE_LO: // $400E
        noise_ch.timer_reload = noise_period_ntsc[data & 0xF];
        noise_ch.loop_noise = data & 0x80;
        break;

    case Bus::Addr::NOISE_HI: // $400F
        noise_ch.len_cnt_reload = data >> 3;
        if (noise_ch.enabled) // TODO: is 'len_cnt_reload' necessary as a variable?
            noise_ch.len_cnt = noise_ch.len_cnt_reload;
        noise_ch.envelope.start_flag = true;
        break;

    case Bus::Addr::DMC_FREQ: // $4010
        dmc.frequency = data;
        dmc.loop = data & 0x40;
        dmc.IRQ_enable = data & 0x80;
        break;

    case Bus::Addr::DMC_RAW: // $4011
        dmc.load_cnt = data;
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
        dmc.IRQ_enable = false;
        break;

    case Bus::Addr::FRAME_CNT: // $4017
        frame_cnt_interrupt_inhibit = data & 0x40;
        frame_cnt_mode = data & 0x80;
        break;

    default:
        break;
    }
}


void APU::StepFrameCounter()
{
    switch (++cpu_cycle_count)
    {
    case 7457:
        pulse_ch_1.ClockEnvelope();
        pulse_ch_2.ClockEnvelope();
        noise_ch.ClockEnvelope();
        triangle_ch.ClockLinear();
        break;

    case 14913:
        pulse_ch_1.ClockEnvelope();
        pulse_ch_2.ClockEnvelope();
        noise_ch.ClockEnvelope();
        pulse_ch_1.ClockLength();
        pulse_ch_2.ClockLength();
        pulse_ch_1.ClockSweep();
        pulse_ch_2.ClockSweep();
        triangle_ch.ClockLinear();
        break;

    case 22371:
        pulse_ch_1.ClockEnvelope();
        pulse_ch_2.ClockEnvelope();
        noise_ch.ClockEnvelope();
        triangle_ch.ClockLinear();
        break;

    case 29828:
        // TODO: not sure if correct logic
        if (frame_cnt_mode == 0 && frame_cnt_interrupt_inhibit == 0)
            frame_interrupt = 1;
        break;

    case 29829:
        if (frame_cnt_mode == 0)
        {
            pulse_ch_1.ClockEnvelope();
            pulse_ch_2.ClockEnvelope();
            noise_ch.ClockEnvelope();
            pulse_ch_1.ClockLength();
            pulse_ch_2.ClockLength();
            pulse_ch_1.ClockSweep();
            pulse_ch_2.ClockSweep();
            triangle_ch.ClockLinear();
            if (frame_cnt_interrupt_inhibit == 0)
                frame_interrupt = 1;
        }
        break;

    case 29830:
        if (frame_cnt_mode == 0)
        {
            if (frame_cnt_interrupt_inhibit == 0)
                frame_interrupt = 1;
            cpu_cycle_count = 0;
        }
        break;

    case 37281:
        pulse_ch_1.ClockEnvelope();
        pulse_ch_2.ClockEnvelope();
        noise_ch.ClockEnvelope();
        pulse_ch_1.ClockLength();
        pulse_ch_2.ClockLength();
        pulse_ch_1.ClockSweep();
        pulse_ch_2.ClockSweep();
        triangle_ch.ClockLinear();
        break;

    case 37282:
        cpu_cycle_count = 0;
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
            ; // TODO: silence channel
    }
}


void APU::PulseCh::ClockSweep()
{
    if (sweep.divider == 0)
    {
        sweep.enabled = true;
        // TODO: adjust period.
    }
    if (sweep.divider == 0 || sweep.reload)
    {
        sweep.divider = sweep.divider_period;
        sweep.reload = false;
    }
    else
    {
        sweep.divider--;
    }
}


void APU::PulseCh::Step()
{
    if (timer == 0)
    {
        timer = timer_reload;
        duty_pos++;
        output = pulse_duty_table[duty][duty_pos];
    }
    else
        timer--;
}


void APU::TriangleCh::ClockLength()
{
    if (len_cnt != 0 && !linear_cnt_control)
    {
        len_cnt--;
        if (len_cnt == 0)
            ; // TODO: silence channel
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
        timer = timer_reload;
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


void APU::NoiseCh::Step()
{
    if (timer == 0)
    {
        timer = timer_reload;
        output = (LFSR & 1) ^ (loop_noise ? (LFSR >> 6 & 1) : (LFSR >> 1 & 1));
        LFSR >>= 1;
        LFSR |= output << 14;
    }
    else
        timer--;
    // TODO  The mixer receives the current envelope volume except when
    // 1) Bit 0 of the shift register is set, or 2)  The length counter is zero
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


void APU::OutputSample()
{
    
}


void APU::State(Serialization::BaseFunctor& functor)
{

}