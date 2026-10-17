/* The sound driver: a 68k sequencer in the style of Sega's SMPS, run once a frame from the
 * vertical blank (UpdateSound). The Z80 only plays samples; the 68k reads the music and the
 * effects and writes the YM2612 and the PSG itself, taking the Z80's bus while the Z80 is not
 * busy with a sample.
 *
 * Its RAM is $FFF800-$FFFCBF: the globals, then 24 tracks of $30 bytes (TR_*): the DAC, FM 1-6
 * and PSG 1-3 of the music; FM and PSG effects (3 each) that take over a music track's channel
 * (its flag TF_TAKEN); 8 special effects' tracks, of which the FM one at $FFFB40 and the noise
 * one at $FFFB70 are used. A track is bytes of data: notes ($80 a rest, $81-$DF), durations
 * ($00-$7F, times the track's tempo divider), and coordination flags ($E0-$FF, SoundFlag). */
#include "game.h"

/* ---- RAM -------------------------------------------------------------------------------------- */
#define S_PRIORITY      0xF800   /* the playing effect's priority */
#define S_TEMPO_COUNT   0xF801
#define S_TEMPO         0xF802   /* every this many frames each track's note lasts one frame more */
#define S_F803          0xF803   /* (flag $E2 sets it; nothing reads it) */
#define S_FADE_STEPS    0xF804   /* a fade out: steps left */
#define S_FADE_DELAY    0xF806   /* frames to the next step */
#define S_DAC_PLAYED    0xF808   /* set when the DAC track read a note this frame */
#define S_REQUEST       0xF809   /* the sound to start this frame */
#define S_QUEUE         0xF80A   /* four sounds asked for (QueueSound) */
#define S_GROUP         0xF80E   /* the tracks being updated: 0 the music, $80 effects, $40 special */
#define S_FM3_SPECIAL   0xF80F   /* FM 3 in its special mode (four operator frequencies) */
#define S_FM3_FREQS     0xF810   /* the four frequencies: the music's; the effects' at $F818 */
#define S_MUSIC_VOICES  0xF820   /* long: the playing song's FM voices */
#define S_SFX_VOICES    0xF824   /* long: the special effects' voices */
#define S_SLIDE         0xF828   /* a volume slide: 1 (rising), $80 (falling), 2 (done) */
#define S_SLIDE_FM      0xF829   /* the step for FM, */
#define S_SLIDE_PSG     0xF82A   /* and for PSG */
#define TRACKS          0xF840
#define T_DAC           0xF840
#define T_FM1           0xF870
#define T_FM4           0xF900
#define T_PSG1          0xF990
#define T_PSG3          0xF9F0
#define T_SFX           0xFA20   /* FM effects at $FA20-$FA80, PSG at $FAB0-$FB10 */
#define T_SFX_FM2       0xFA50
#define T_SFX_PSG3      0xFB10
#define T_SPECIAL_FM    0xFB40
#define T_SPECIAL_PSG   0xFB70
#define TRACK_SIZE      0x30

/* a track */
#define TR_FLAGS        0x00
#define TR_CHANNEL      0x01     /* FM: 0-2, 4-6 (the YM2612's key-on value); PSG: $80, $A0, $C0, $E0 (noise) */
#define TR_DIVIDER      0x02     /* durations are times this */
#define TR_ENV_SCALE    0x03     /* the frequency envelope's values are times this */
#define TR_DATA         0x04     /* long: the next byte */
#define TR_TRANSPOSE    0x08
#define TR_VOLUME       0x09     /* attenuation: FM added to the carriers' total level, PSG 0-15 */
#define TR_ENVELOPE     0x0A     /* bit 7: modulation on; bits 0-6: the frequency envelope, from 1 */
#define TR_VOICE        0x0B     /* FM: the voice; PSG: the volume envelope, from 1 */
#define TR_VOL_POS      0x0C     /* the volume envelope's position */
#define TR_STACK        0x0D     /* the call stack's top: return addresses below it in the track */
#define TR_TIMER        0x0E     /* frames left of the note */
#define TR_DURATION     0x0F
#define TR_FREQ         0x10     /* word: FM block and number, the PSG period; the DAC track's note */
#define TR_FILL         0x12     /* frames until the note is cut (note fill) */
#define TR_FILL_START   0x13
#define TR_MOD_DATA     0x14     /* long: the modulation's parameters */
#define TR_MOD_WAIT     0x18
#define TR_MOD_SPEED    0x19
#define TR_MOD_DELTA    0x1A
#define TR_MOD_STEPS    0x1B
#define TR_MOD_SUM      0x1C     /* word */
#define TR_DETUNE       0x1E
#define TR_PAN_MODE     0x1F     /* the pan animation: 0 none, 1-3 */
#define TR_PAN_TABLE    0x20     /* (an effect's FM voices here instead: a long) */
#define TR_PAN_POS      0x21
#define TR_PAN_LIMIT    0x22
#define TR_PAN_RATE     0x23
#define TR_PAN_TIMER    0x24
#define TR_ALGORITHM    0x25     /* FM: the voice's feedback and algorithm; noise: its mode */
#define TR_ENV_POS      0x26     /* the frequency envelope's position */
#define TR_PAN          0x27     /* register $B4: pan, AMS, FMS */
#define TR_LOOPS        0x28     /* loop counters (flag $F7) */

#define TF_PLAYING 0x80
#define TF_TIE     0x10          /* the next note continues this one (no key off and on) */
#define TF_PAUSED  0x01          /* stopped by flag $FF 1 */
#define TF_TAKEN   0x04          /* an effect uses the channel */
#define TF_REST    0x02
#define TB_TAKEN   2
#define TB_REST    1

/* ---- ROM -------------------------------------------------------------------------------------- */
#define DAC_SAMPLES     0x06575Au    /* the DAC track's notes $89 and up: the sample to play */
#define SONGS           0x068F42u    /* long pointers, $81-$9B */
#define PRIORITIES      0x068FAEu    /* a byte for each sound $81-$FF */
#define EFFECTS         0x069028u    /* long pointers, $A0-$DF */
#define SPECIALS        0x069128u    /* long pointers, $DF- (never reached) */
#define MUSIC_TRACK     0x065EACu    /* long pointers: each channel's music track */
#define EFFECT_TRACK    0x065ECCu    /* and its effect track */
#define FM_IDS          0x065DDAu    /* the music's FM tracks' channels */
#define PSG_IDS         0x065DE2u
#define FM3_REGS        0x065A14u    /* FM 3's special mode: its four operators' frequency registers */
#define PAN_TABLES      0x065AB4u
#define FM_FREQS        0x066468u    /* a word for each of the 12 notes of an octave */
#define PSG_PERIODS     0x066628u
#define Z80_PROGRAM     0x0666B4u
#define Z80_PROGRAM_SIZE 0x1F28
#define CARRIERS        0x06881Eu    /* each algorithm's carrier operators (bits) */
#define VOICE_REGS      0x06889Eu    /* the registers a voice's bytes go to, after $B0 */
#define TL_REGS         0x0688B2u
#define E9_REGS         0x068758u
#define FE_FREQS        0x068A88u
#define FF1_REGS        0x068AACu
#define FREQ_ENVELOPES  0x068BD4u
#define VOL_ENVELOPES   0x068C98u
#define VOICE_SIZE      25

/* The Z80's mailbox: its flag at $1FFD while it is busy, the sample to play at $1FFF (and $1FF6
 * set once the request is taken), a sample's bank at $01A6 */
#define Z80_BUSY 0x1FFD
#define Z80_SAMPLE 0x1FFF
#define Z80_SAMPLE_TAKEN 0x1FF6
#define Z80_BANK 0x01A6

static uint8_t flags(uint16_t t) { return rd8(t); }
static uint8_t channel(uint16_t t) { return rd8((uint16_t)(t + TR_CHANNEL)); }
static int is_psg(uint16_t t) { return channel(t) & 0x80; }
static uint8_t tr8(uint16_t t, int field) { return rd8((uint16_t)(t + field)); }
static void tr8w(uint16_t t, int field, uint8_t v) { wr8((uint16_t)(t + field), v); }
static uint8_t next(uint32_t *data) { return bus_rd8((*data)++); }
static void sound_pause(void);
static void sound_stop_all(void);
static void fm_write_freq(uint16_t t);
static void psg_write_freq(uint16_t t);
static void psg_envelope(uint16_t t);
static void sound_init(void);
static void start_song(uint8_t id);
static void start_effect(uint8_t id);
static void start_special(uint8_t id);
static int sound_flag_ff(uint16_t t, uint32_t *data);

/* ---- the chips -------------------------------------------------------------------------------- */

/* $06639E. The Z80 off the bus, once it is not busy with a sample. */
void SoundZ80Stop(void)
{
    for (;;) {
        z80_busreq(0x100);
        while (z80_busack() & 1) {}
        if (!(z80_ram_read8(Z80_BUSY) & 0x80)) return;
        z80_busreq(0);
    }
}

static void z80_go(void) { z80_busreq(0); }
static void ym_ready(void) { while (ym_read8(0) & 0x80) {} }

/* $06635A. A register of the YM2612's first part (channels 0-2). */
void SoundYm1(uint8_t reg, uint8_t value)
{
    ym_ready();
    ym_write8(0, reg);
    ym_ready();
    ym_write8(1, value);
}

/* $066380. Of the second part (channels 4-6). */
void SoundYm2(uint8_t reg, uint8_t value)
{
    ym_ready();
    ym_write8(2, reg);
    ym_ready();
    ym_write8(3, value);
}

/* $06634E. A register of the track's channel, in the part it belongs to. */
void SoundYmChannel(uint16_t t, uint8_t reg, uint8_t value)
{
    uint8_t ch = channel(t);
    if (ch & 4) SoundYm2((uint8_t)(reg + (ch & ~4)), value);
    else SoundYm1((uint8_t)(reg + ch), value);
}

/* $066324. The same with the Z80 held, unless an effect has the channel. */
void SoundYmChannelBus(uint16_t t, uint8_t reg, uint8_t value)
{
    if (flags(t) & TF_TAKEN) return;
    SoundZ80Stop();
    SoundYmChannel(t, reg, value);
    z80_go();
}

/* $06633C. A register of the first part, with the Z80 held. */
void SoundYmBus(uint8_t reg, uint8_t value)
{
    SoundZ80Stop();
    SoundYm1(reg, value);
    z80_go();
}

/* $06630C. Key off, unless the note is tied or an effect has the channel. */
void SoundNoteOff(uint16_t t)
{
    if (flags(t) & (TF_TIE | TF_TAKEN)) return;
    SoundKeyOff(t);
}

/* $066318 */
void SoundKeyOff(uint16_t t)
{
    SoundYmBus(0x28, channel(t));
}

/* key on (all four operators), unless resting or the channel is taken */
static void key_on(uint16_t t)
{
    if (flags(t) & (TF_REST | TF_TAKEN)) return;
    SoundYmBus(0x28, channel(t) | 0xF0);
}

/* $0665FA. The PSG channel silent, unless an effect has it. */
void SoundPsgNoteOff(uint16_t t)
{
    if (flags(t) & TF_TAKEN) return;
    SoundPsgOff(t);
}

/* $066600 */
void SoundPsgOff(uint16_t t)
{
    psg_write(channel(t) | 0x1F);
}

static void psg_silence(void)
{
    psg_write(0x9F);
    psg_write(0xBF);
    psg_write(0xDF);
    psg_write(0xFF);
}

/* $0665A8. The PSG channel's volume (0 loudest, 15 silent), unless it rests or is taken; a
 * tied note keeps quiet past its note fill. */
void SoundPsgSetVolume(uint16_t t, uint8_t volume)
{
    uint8_t f = flags(t);
    if (f & (TF_REST | TF_TAKEN)) return;
    if ((f & TF_TIE) && tr8(t, TR_FILL_START) && tr8(t, TR_FILL)) return;
    psg_write((uint8_t)((volume | channel(t)) + 0x10));
}

/* $0661DC. All six FM channels keyed off, every operator's total level at its quietest. */
void SoundSilenceFm(void)
{
    SoundZ80Stop();
    for (int ch = 2; ch >= 0; ch--) {
        SoundYm1(0x28, (uint8_t)ch);
        SoundYm1(0x28, (uint8_t)(ch + 4));
    }
    uint8_t reg = 0x40;
    for (int ch = 0; ch < 3; ch++) {
        for (int op = 0; op < 4; op++) {
            SoundYm1(reg, 0x7F);
            SoundYm2(reg, 0x7F);
            reg += 4;
        }
        reg -= 0x0F;
    }
    z80_go();
}

/* ---- the frame -------------------------------------------------------------------------------- */

/* $000C88. Each vertical blank: a sound the game asked for after a delay goes into the queue
 * (v_soundPending, after v_soundDelay frames), then the driver runs, unless it is still running
 * from the vertical blank this one interrupted (v_soundBusy). */
void UpdateSound(void)
{
    uint16_t delay = rd16(0xF762);
    if (delay) wr16(0xF762, (uint16_t)(delay - 1));
    else {
        uint8_t id = rd8(0xF764);
        if (id && QueueSound(id)) wr8(0xF764, 0);
    }
    if (rd8(0xF745)) return;
    wr8(0xF745, 1);
    SoundDriver();
    wr8(0xF745, 0);
}

/* $003196. A sound into the first free slot of the queue (one holding the same sound counts as
 * free); 0 when the four are taken. */
int QueueSound(uint8_t id)
{
    for (int i = 0; i < 4; i++) {
        uint8_t slot = rd8((uint16_t)(S_QUEUE + i));
        if (!(slot & 0x80) || slot == id) {
            wr8((uint16_t)(S_QUEUE + i), id);
            return id != 0;
        }
    }
    return 0;
}

/* $065600. A frame of sound: the pause, the volume slide, the tempo, the fade out, the sound
 * asked for, then every playing track: the music's, the effects', the special effects'. */
void SoundDriver(void)
{
    wr8(S_GROUP, 0);
    if (rd8(v_soundPause)) {
        sound_pause();
        return;
    }
    SoundSlide();
    SoundTempo();
    SoundFadeOut();
    if (rd32(S_QUEUE)) SoundTakeRequests();
    if (SoundPlayRequest()) return;                  /* a song started: its tracks begin next frame */
    uint16_t t = T_DAC;
    if ((flags(t) & TF_PLAYING) && SoundDacTrack(t)) return;
    wr8(S_DAC_PLAYED, 0);
    for (int i = 0; i < 6; i++) { t += TRACK_SIZE; if (flags(t) & TF_PLAYING) SoundFmTrack(t); }
    for (int i = 0; i < 3; i++) { t += TRACK_SIZE; if (flags(t) & TF_PLAYING) SoundPsgTrack(t); }
    wr8(S_GROUP, 0x80);
    for (int i = 0; i < 3; i++) { t += TRACK_SIZE; if (flags(t) & TF_PLAYING) SoundFmTrack(t); }
    for (int i = 0; i < 3; i++) { t += TRACK_SIZE; if (flags(t) & TF_PLAYING) SoundPsgTrack(t); }
    wr8(S_GROUP, 0x40);
    for (int i = 0; i < 8; i++) {
        t += TRACK_SIZE;
        if (!(flags(t) & TF_PLAYING)) continue;
        if (is_psg(t)) SoundPsgTrack(t);
        else SoundFmTrack(t);
    }
}

/* The pause (v_soundPause 1: every frame while it lasts, every channel's pan off, every key off
 * and the PSG silent) and the resume ($80: the pans back). */
static void sound_pause(void)
{
    if (!(rd8(v_soundPause) & 0x80)) {
        SoundZ80Stop();
        for (uint8_t reg = 0xB4; reg < 0xB7; reg++) {
            SoundYm1(reg, 0);
            SoundYm2(reg, 0);
        }
        for (int ch = 2; ch >= 0; ch--) {
            SoundYm1(0x28, (uint8_t)ch);
            SoundYm1(0x28, (uint8_t)(ch + 4));
        }
        z80_go();
        psg_silence();
        return;
    }
    wr8(v_soundPause, 0);
    SoundZ80Stop();
    static const struct { uint16_t first; int n; } groups[] = { { T_DAC, 7 }, { T_SFX, 3 }, { T_SPECIAL_FM, 1 } };
    for (int g = 0; g < 3; g++) {
        uint16_t t = groups[g].first;
        for (int i = 0; i < groups[g].n; i++, t += TRACK_SIZE)
            if ((flags(t) & TF_PLAYING) && !(flags(t) & TF_TAKEN)) SoundYmChannel(t, 0xB4, tr8(t, TR_PAN));
    }
    z80_go();
}

/* $066174. Every v_tempo frames, every note of the music's ten tracks lasts a frame more. */
void SoundTempo(void)
{
    if (!rd8(S_TEMPO)) return;
    wr8(S_TEMPO_COUNT, (uint8_t)(rd8(S_TEMPO_COUNT) - 1));
    if (rd8(S_TEMPO_COUNT)) return;
    wr8(S_TEMPO_COUNT, rd8(S_TEMPO));
    for (uint16_t t = TRACKS, i = 0; i < 10; i++, t += TRACK_SIZE)
        if (flags(t) & TF_PLAYING) tr8w(t, TR_TIMER, (uint8_t)(tr8(t, TR_TIMER) + 1));
}

/* $0660FA. The fade out: every 3 frames, the music's FM and PSG a step quieter; tracks that
 * reach silence stop; after the last step, everything stops. */
void SoundFadeOut(void)
{
    if (!rd8(S_FADE_STEPS)) return;
    if (rd8(S_FADE_DELAY)) {
        wr8(S_FADE_DELAY, (uint8_t)(rd8(S_FADE_DELAY) - 1));
        return;
    }
    wr8(S_FADE_STEPS, (uint8_t)(rd8(S_FADE_STEPS) - 1));
    if (!rd8(S_FADE_STEPS)) {
        sound_stop_all();
        return;
    }
    wr8(S_FADE_DELAY, 3);
    uint16_t t = T_FM1;
    for (int i = 0; i < 6; i++, t += TRACK_SIZE) {
        if (!(flags(t) & TF_PLAYING)) continue;
        tr8w(t, TR_VOLUME, (uint8_t)(tr8(t, TR_VOLUME) + 1));
        if (tr8(t, TR_VOLUME) & 0x80) wr8(t, flags(t) & ~TF_PLAYING);
        else SoundFmVolume(t);
    }
    for (int i = 0; i < 3; i++, t += TRACK_SIZE) {
        if (!(flags(t) & TF_PLAYING)) continue;
        tr8w(t, TR_VOLUME, (uint8_t)(tr8(t, TR_VOLUME) + 1));
        if (tr8(t, TR_VOLUME) >= 0x10) wr8(t, flags(t) & ~TF_PLAYING);
        else SoundPsgSetVolume(t, tr8(t, TR_VOLUME));
    }
}

/* $0663D0. A volume slide (flag $FF 3 and 4, no recorded run used one): each frame the music's
 * FM and PSG louder ($80) or quieter (1) by the slide's steps, once. */
void SoundSlide(void)
{
    uint8_t mode = rd8(S_SLIDE);
    if (!mode || mode == 2) return;
    uint8_t step = rd8(S_SLIDE_FM);
    uint16_t t = T_FM1;
    for (int i = 0; i < 6; i++, t += TRACK_SIZE) {
        if (!(flags(t) & TF_PLAYING)) continue;
        if (rd8(S_SLIDE) & 0x80) tr8w(t, TR_VOLUME, (uint8_t)(tr8(t, TR_VOLUME) - step));
        else {
            tr8w(t, TR_VOLUME, (uint8_t)(tr8(t, TR_VOLUME) + step));
            if (tr8(t, TR_VOLUME) & 0x80) continue;
        }
        SoundFmVolume(t);
    }
    step = rd8(S_SLIDE_PSG);
    for (int i = 0; i < 3; i++, t += TRACK_SIZE) {
        if (!(flags(t) & TF_PLAYING)) continue;
        if (rd8(S_SLIDE) & 0x80) tr8w(t, TR_VOLUME, (uint8_t)(tr8(t, TR_VOLUME) - step));
        else {
            tr8w(t, TR_VOLUME, (uint8_t)(tr8(t, TR_VOLUME) + step));
            if (tr8(t, TR_VOLUME) >= 0x10) continue;
        }
        SoundPsgSetVolume(t, tr8(t, TR_VOLUME));
    }
    wr8(S_SLIDE, rd8(S_SLIDE) & 0x80 ? 0 : 2);
}

/* ---- notes ------------------------------------------------------------------------------------ */

/* $0657F0. The note's length: n times the track's divider (a byte: 0 is 256 times). */
void SoundSetDuration(uint16_t t, uint8_t n)
{
    uint8_t d = n;
    for (uint8_t k = tr8(t, TR_DIVIDER); --k;) d = (uint8_t)(d + n);
    tr8w(t, TR_DURATION, d);
    tr8w(t, TR_TIMER, d);
}

/* $065810. A note starts: the track's position kept, its timer from its duration; unless tied,
 * the note fill, the envelopes and the modulation start again. */
void SoundNoteStart(uint16_t t, uint32_t data)
{
    wr32((uint16_t)(t + TR_DATA), data);
    tr8w(t, TR_TIMER, tr8(t, TR_DURATION));
    if (flags(t) & TF_TIE) return;
    tr8w(t, TR_FILL, tr8(t, TR_FILL_START));
    tr8w(t, TR_VOL_POS, 0);
    tr8w(t, TR_ENV_POS, 0);
    tr8w(t, TR_ENV_SCALE, 0);
    if (!(tr8(t, TR_ENVELOPE) & 0x80)) return;
    uint32_t mod = rd32((uint16_t)(t + TR_MOD_DATA));
    tr8w(t, TR_MOD_WAIT, bus_rd8(mod));
    tr8w(t, TR_MOD_SPEED, bus_rd8(mod + 1));
    tr8w(t, TR_MOD_DELTA, bus_rd8(mod + 2));
    tr8w(t, TR_MOD_STEPS, (uint8_t)(bus_rd8(mod + 3) >> 1));
    wr16((uint16_t)(t + TR_MOD_SUM), 0);
}

/* $065858. The note fill: when its count runs out, the note is cut (and the track's update ends
 * for this frame: returns 1). */
int SoundNoteFill(uint16_t t)
{
    if (!tr8(t, TR_FILL)) return 0;
    tr8w(t, TR_FILL, (uint8_t)(tr8(t, TR_FILL) - 1));
    if (tr8(t, TR_FILL)) return 0;
    wr8(t, flags(t) | TF_REST);
    if (is_psg(t)) SoundPsgNoteOff(t);
    else SoundNoteOff(t);
    return 1;
}

/* $065880. The modulation: after its wait, every `speed` frames the sum moves by delta, for
 * `steps`, then delta turns round. */
void SoundModulate(uint16_t t)
{
    if (!(tr8(t, TR_ENVELOPE) & 0x80)) return;
    if (tr8(t, TR_MOD_WAIT)) {
        tr8w(t, TR_MOD_WAIT, (uint8_t)(tr8(t, TR_MOD_WAIT) - 1));
        return;
    }
    tr8w(t, TR_MOD_SPEED, (uint8_t)(tr8(t, TR_MOD_SPEED) - 1));
    if (tr8(t, TR_MOD_SPEED)) return;
    uint32_t mod = rd32((uint16_t)(t + TR_MOD_DATA));
    tr8w(t, TR_MOD_SPEED, bus_rd8(mod + 1));
    if (!tr8(t, TR_MOD_STEPS)) {
        tr8w(t, TR_MOD_STEPS, bus_rd8(mod + 3));
        tr8w(t, TR_MOD_DELTA, (uint8_t)-tr8(t, TR_MOD_DELTA));
        return;
    }
    tr8w(t, TR_MOD_STEPS, (uint8_t)(tr8(t, TR_MOD_STEPS) - 1));
    wr16((uint16_t)(t + TR_MOD_SUM), (uint16_t)(rd16((uint16_t)(t + TR_MOD_SUM)) + (int8_t)tr8(t, TR_MOD_DELTA)));
}

/* $06592E. The frequency to write: the frequency envelope's next value (times its scale), the
 * detune, the note's frequency and the modulation. The envelope's commands: $80 from the start,
 * $81 hold, $82 n go to n, $84 n the scale plus n, $83 the note ends (rests: what is returned
 * then is the $83). */
uint16_t SoundFreq(uint16_t t)
{
    uint16_t f = 0;
    uint8_t env = tr8(t, TR_ENVELOPE) & 0x7F;
    if (env) {
        uint32_t table = rom32(FREQ_ENVELOPES + 4u * (env - 1));
        for (;;) {
            uint8_t pos = tr8(t, TR_ENV_POS);
            tr8w(t, TR_ENV_POS, (uint8_t)(pos + 1));
            uint8_t v = bus_rd8(table + pos);
            if (v == 0x80) { tr8w(t, TR_ENV_POS, 0); continue; }
            if (v == 0x81) { tr8w(t, TR_ENV_POS, (uint8_t)(tr8(t, TR_ENV_POS) - 2)); continue; }
            if (v == 0x83) {
                wr8(t, flags(t) | TF_REST);
                if (is_psg(t)) SoundPsgNoteOff(t);
                else SoundNoteOff(t);
                return v;
            }
            if (v == 0x82) { tr8w(t, TR_ENV_POS, bus_rd8(table + pos + 1)); continue; }
            if (v == 0x84) {
                tr8w(t, TR_ENV_SCALE, (uint8_t)(tr8(t, TR_ENV_SCALE) + bus_rd8(table + pos + 1)));
                tr8w(t, TR_ENV_POS, (uint8_t)(tr8(t, TR_ENV_POS) + 1));
                continue;
            }
            f = (uint16_t)((uint16_t)(int8_t)v * (uint16_t)(int8_t)tr8(t, TR_ENV_SCALE));
            break;
        }
    }
    f = (uint16_t)(f + (int8_t)tr8(t, TR_DETUNE) + rd16((uint16_t)(t + TR_FREQ)));
    if (tr8(t, TR_ENVELOPE) & 0x80) f = (uint16_t)(f + rd16((uint16_t)(t + TR_MOD_SUM)));
    return f;
}

/* ---- the DAC track ------------------------------------------------------------------------------ */

/* $0656C0. Its notes are samples: $81-$88 the Z80's own, $89 and up through a table, the bank
 * at $01A6 and sample $85. A flag that stops it returns 1: the 68k then leaves the whole
 * frame's update (the stop pops two return addresses, here the driver's own). */
int SoundDacTrack(uint16_t t)
{
    tr8w(t, TR_TIMER, (uint8_t)(tr8(t, TR_TIMER) - 1));
    if (tr8(t, TR_TIMER)) return 0;
    wr8(S_DAC_PLAYED, 0x80);
    uint32_t data = rd32((uint16_t)(t + TR_DATA));
    uint8_t b;
    while ((b = next(&data)) >= 0xE0)
        if (SoundFlag(t, b, &data)) return 1;
    if (b & 0x80) {
        tr8w(t, TR_FREQ, b);
        b = next(&data);
        if (b & 0x80) {
            data--;
            tr8w(t, TR_TIMER, tr8(t, TR_DURATION));
        } else {
            SoundSetDuration(t, b);
        }
    } else {
        SoundSetDuration(t, b);
    }
    wr32((uint16_t)(t + TR_DATA), data);
    uint8_t note = tr8(t, TR_FREQ);
    if (note == 0x80) return 0;
    if (note < 0x89) {
        SoundZ80Stop();
        if (!z80_ram_read8(Z80_SAMPLE_TAKEN)) z80_ram_write8(Z80_SAMPLE, note);
        z80_go();
        return 0;
    }
    uint8_t sample = rom8(DAC_SAMPLES + (uint8_t)(note - 0x89));
    SoundZ80Stop();
    if (!z80_ram_read8(Z80_SAMPLE_TAKEN)) {
        z80_ram_write8(Z80_BANK, sample);
        z80_ram_write8(Z80_SAMPLE, 0x85);
    }
    z80_go();
    return 0;
}

/* ---- FM tracks ---------------------------------------------------------------------------------- */

/* $06575E. A frame of an FM track: at the note's end the next note (key off, the frequency, the
 * pan animation, key on); else the note fill, the pan animation, the modulation. */
void SoundFmTrack(uint16_t t)
{
    tr8w(t, TR_TIMER, (uint8_t)(tr8(t, TR_TIMER) - 1));
    if (!tr8(t, TR_TIMER)) {
        wr8(t, flags(t) & ~TF_TIE);
        if (SoundFmNextNote(t)) return;
        SoundFmNoteFreq(t);
        SoundPanStart(t);
        key_on(t);
        return;
    }
    if (SoundNoteFill(t)) return;
    SoundPanStep(t);
    SoundModulate(t);
    if (tr8(t, TR_ENVELOPE)) fm_write_freq(t);
}

/* $065788. The track's next note: its flags, key off, the note's frequency (or a rest), its
 * duration (else the last one). Returns 1 when a flag stopped the track. */
int SoundFmNextNote(uint16_t t)
{
    uint32_t data = rd32((uint16_t)(t + TR_DATA));
    wr8(t, flags(t) & ~TF_REST);
    uint8_t b;
    while ((b = next(&data)) >= 0xE0)
        if (SoundFlag(t, b, &data)) return 1;
    SoundNoteOff(t);
    if (b & 0x80) {
        SoundFmNote(t, b, data);
        b = next(&data);
        if (b & 0x80) {
            SoundNoteStart(t, data - 1);
            return 0;
        }
    }
    SoundSetDuration(t, b);
    SoundNoteStart(t, data);
    return 0;
}

/* $0657BE. A note's FM frequency (block and number) from its octave and key, transposed; $80 a
 * rest (and the note starts there). */
void SoundFmNote(uint16_t t, uint8_t note, uint32_t data)
{
    uint8_t n = (uint8_t)(note - 0x80);
    if (!n) {
        wr8(t, flags(t) | TF_REST);
        wr16((uint16_t)(t + TR_FREQ), 0);
        SoundNoteStart(t, data);
        return;
    }
    n = (uint8_t)(n + tr8(t, TR_TRANSPOSE)) & 0x7F;
    wr16((uint16_t)(t + TR_FREQ), (uint16_t)(rom16(FM_FREQS + 2u * (n % 12)) | (n / 12 & 7) << 11));
}

/* The frequency into the channel's registers ($A4 then $A0), unless resting or taken; FM 3 in its
 * special mode writes each operator's (never in a recorded run). */
static void fm_write_freq(uint16_t t)
{
    if (flags(t) & (TF_REST | TF_TAKEN)) return;
    uint16_t f = SoundFreq(t);
    if (rd8(S_FM3_SPECIAL) && channel(t) == 2) {
        uint16_t freqs = rd8(S_GROUP) ? S_FM3_FREQS + 8 : S_FM3_FREQS;
        SoundZ80Stop();
        for (int op = 0; op < 4; op++) {
            uint16_t v = (uint16_t)(f + rd16((uint16_t)(freqs + 2 * op)));
            SoundYm1(rom8(FM3_REGS + 2u * op), (uint8_t)(v >> 8));
            SoundYm1(rom8(FM3_REGS + 2u * op + 1), (uint8_t)v);
        }
        z80_go();
        return;
    }
    SoundZ80Stop();
    SoundYmChannel(t, 0xA4, (uint8_t)(f >> 8));
    SoundYmChannel(t, 0xA0, (uint8_t)f);
    z80_go();
}

/* $0658D0. The new note's frequency: none is a rest. */
void SoundFmNoteFreq(uint16_t t)
{
    if (!rd16((uint16_t)(t + TR_FREQ))) {
        wr8(t, flags(t) | TF_REST);
        return;
    }
    fm_write_freq(t);
}

/* The pan animation (flag $E4): every `rate` frames the next pan from the table, up to `limit`,
 * then from the start again (mode 1 and 3) or holding (mode 2). */
static void pan_step(uint16_t t)
{
    if (tr8(t, TR_PAN_TIMER) == tr8(t, TR_PAN_RATE)) {
        if ((int8_t)(tr8(t, TR_PAN_LIMIT) - tr8(t, TR_PAN_POS)) < 0) {
            if (tr8(t, TR_PAN_MODE) == 2) return;
            tr8w(t, TR_PAN_POS, 0);
        }
        tr8w(t, TR_PAN_TIMER, 0);
        tr8w(t, TR_PAN_POS, (uint8_t)(tr8(t, TR_PAN_POS) + 1));
    }
    uint32_t table = rom32(PAN_TABLES + (uint32_t)(int16_t)((tr8(t, TR_PAN_TABLE) - 1) * 4));
    uint8_t pan = bus_rd8(table + (uint32_t)(int16_t)(tr8(t, TR_PAN_POS) - 1));
    SoundYmChannelBus(t, 0xB4, pan | (tr8(t, TR_PAN) & 0x37));
    tr8w(t, TR_PAN_TIMER, (uint8_t)(tr8(t, TR_PAN_TIMER) + 1));
}

static void pan_restart(uint16_t t)
{
    tr8w(t, TR_PAN_TIMER, tr8(t, TR_PAN_RATE));
    tr8w(t, TR_PAN_POS, 0);
    pan_step(t);
}

/* $065A1C. At a new note: modes 2 and 3 start the animation again, mode 1 steps it. */
void SoundPanStart(uint16_t t)
{
    if (flags(t) & TF_REST) return;
    switch (tr8(t, TR_PAN_MODE)) {
    case 0: return;
    case 1: SoundPan1Start(t); return;
    case 2: SoundPan2Start(t); return;
    case 3: SoundPan3Start(t); return;
    default: call68k_missing(SYM_SoundPanStart); return;     /* past the table: not in the game */
    }
}

/* $065A30, $065A32, $065A34: the table's entries */
void SoundPan1Start(uint16_t t) { pan_step(t); }
void SoundPan2Start(uint16_t t) { pan_restart(t); }
void SoundPan3Start(uint16_t t) { pan_restart(t); }

/* $065A36. Each frame: modes 2 and 3 step the animation. */
void SoundPanStep(uint16_t t)
{
    if (flags(t) & TF_REST) return;
    switch (tr8(t, TR_PAN_MODE)) {
    case 0: case 1: return;
    case 2: SoundPan2Step(t); return;
    case 3: SoundPan3Step(t); return;
    default: call68k_missing(SYM_SoundPanStep); return;
    }
}

/* $065A4C, $065A4E */
void SoundPan2Step(uint16_t t) { pan_step(t); }
void SoundPan3Step(uint16_t t) { pan_step(t); }

/* The voice table the track's voice numbers index: the song's, or for an effect its own (or
 * the special effects'). */
static uint32_t voices(uint16_t t)
{
    uint8_t group = rd8(S_GROUP);
    if (!group) return rd32(S_MUSIC_VOICES);
    if (group & 0x80) return rd32((uint16_t)(t + TR_PAN_TABLE));
    return rd32(S_SFX_VOICES);
}

/* $0687B4. FM voice `voice` of the table at `table` into the channel: feedback and algorithm,
 * the 20 operator registers, the 4 total levels (the carriers' plus the track's volume), the
 * pan. */
void SoundLoadVoice(uint16_t t, uint8_t voice, uint32_t table)
{
    if (voice) table += (uint32_t)voice * VOICE_SIZE;
    SoundZ80Stop();
    uint8_t algo = bus_rd8(table++);
    tr8w(t, TR_ALGORITHM, algo);
    SoundYmChannel(t, 0xB0, algo);
    uint32_t regs = VOICE_REGS;
    for (int i = 0; i < 20; i++) SoundYmChannel(t, rom8(regs++), bus_rd8(table++));
    uint8_t carriers = rom8(CARRIERS + (algo & 7)), volume = tr8(t, TR_VOLUME);
    for (int i = 0; i < 4; i++) {
        uint8_t reg = rom8(regs++), tl = bus_rd8(table++);
        if (carriers & 1) tl = (uint8_t)(tl + volume);
        carriers >>= 1;
        SoundYmChannel(t, reg, tl);
    }
    SoundYmChannel(t, 0xB4, tr8(t, TR_PAN));
    z80_go();
}

/* $068826. The carriers' total levels again with the track's volume (one past 255 is left
 * as it was), unless taken or the volume is past 127. */
void SoundFmVolume(uint16_t t)
{
    if (flags(t) & TF_TAKEN) return;
    uint8_t voice = tr8(t, TR_VOICE);
    uint32_t table = voices(t);
    if (voice) table += (uint32_t)voice * VOICE_SIZE;
    table += 21;
    uint8_t carriers = rom8(CARRIERS + (tr8(t, TR_ALGORITHM) & 7)), volume = tr8(t, TR_VOLUME);
    if (volume & 0x80) return;
    SoundZ80Stop();
    for (int i = 0; i < 4; i++) {
        uint8_t reg = rom8(TL_REGS + (uint32_t)i), tl = bus_rd8(table + (uint32_t)i);
        int carrier = carriers & 1;
        carriers >>= 1;
        if (carrier && tl + volume < 0x100) SoundYmChannel(t, reg, (uint8_t)(tl + volume));
    }
    z80_go();
}

/* ---- PSG tracks --------------------------------------------------------------------------------- */

/* $066480. A frame of a PSG track: at the note's end the next note, its period, its volume
 * envelope from the start; else the note fill, the volume envelope, the modulation. */
void SoundPsgTrack(uint16_t t)
{
    tr8w(t, TR_TIMER, (uint8_t)(tr8(t, TR_TIMER) - 1));
    if (!tr8(t, TR_TIMER)) {
        wr8(t, flags(t) & ~TF_TIE);
        if (SoundPsgNextNote(t)) return;
        SoundPsgNoteFreq(t);
        psg_envelope(t);
        return;
    }
    if (SoundNoteFill(t)) return;
    SoundPsgVolume(t);
    SoundModulate(t);
    if (tr8(t, TR_ENVELOPE)) psg_write_freq(t);
}

/* $0664A6. As SoundFmNextNote, with PSG notes (and no key off). */
int SoundPsgNextNote(uint16_t t)
{
    wr8(t, flags(t) & ~TF_REST);
    uint32_t data = rd32((uint16_t)(t + TR_DATA));
    uint8_t b;
    while ((b = next(&data)) >= 0xE0)
        if (SoundFlag(t, b, &data)) return 1;
    if (b & 0x80) {
        SoundPsgNote(t, b, data);
        b = next(&data);
        if (b & 0x80) {
            SoundNoteStart(t, data - 1);
            return 0;
        }
    }
    SoundSetDuration(t, b);
    SoundNoteStart(t, data);
    return 0;
}

/* $0664DA. A note's PSG period, transposed; $80 a rest (period $FFFF, the channel silent). The
 * note starts here too. */
void SoundPsgNote(uint16_t t, uint8_t note, uint32_t data)
{
    if (note < 0x81) {
        wr8(t, flags(t) | TF_REST);
        wr16((uint16_t)(t + TR_FREQ), 0xFFFF);
        SoundNoteStart(t, data);
        SoundPsgNoteOff(t);
        return;
    }
    uint8_t n = (uint8_t)(note - 0x81 + tr8(t, TR_TRANSPOSE)) & 0x7F;
    wr16((uint16_t)(t + TR_FREQ), rom16(PSG_PERIODS + 2u * n));
    SoundNoteStart(t, data);
}

/* the period into the channel: its low 4 bits with the channel (noise: as tone 3), then the
 * rest; not when resting or taken */
static void psg_write_freq(uint16_t t)
{
    if (flags(t) & (TF_REST | TF_TAKEN)) return;
    uint16_t f = SoundFreq(t);
    uint8_t ch = channel(t);
    if (ch == 0xE0) ch = 0xC0;
    psg_write(ch | (f & 0x0F));
    psg_write((uint8_t)(f >> 4) & 0x3F);
}

/* $06650A. The new note's period: $FFFF (negative) is a rest. */
void SoundPsgNoteFreq(uint16_t t)
{
    if ((int16_t)rd16((uint16_t)(t + TR_FREQ)) < 0) {
        wr8(t, flags(t) | TF_REST);
        return;
    }
    psg_write_freq(t);
}

/* The volume envelope's next value added to the track's volume (15 at most). Its commands: $80
 * from the start, $81 hold, $82 n go to (the 68k reads n from past the table: never used), $83
 * the note ends. */
static void psg_envelope(uint16_t t)
{
    uint8_t volume = tr8(t, TR_VOLUME), env = tr8(t, TR_VOICE);
    if (!env) {
        SoundPsgSetVolume(t, volume);
        return;
    }
    uint32_t table = rom32(VOL_ENVELOPES + 4u * (env - 1));
    for (;;) {
        uint8_t pos = tr8(t, TR_VOL_POS);
        tr8w(t, TR_VOL_POS, (uint8_t)(pos + 1));
        uint8_t v = bus_rd8(table + pos);
        if (v == 0x83) {
            tr8w(t, TR_VOL_POS, (uint8_t)(tr8(t, TR_VOL_POS) - 2));
            wr8(t, flags(t) | TF_REST);
            SoundPsgNoteOff(t);
            return;
        }
        if (v == 0x81) { tr8w(t, TR_VOL_POS, (uint8_t)(tr8(t, TR_VOL_POS) - 2)); return; }
        if (v == 0x82) { tr8w(t, TR_VOL_POS, bus_rd8(table + 1 + v)); continue; }
        if (v == 0x80) { tr8w(t, TR_VOL_POS, 0); continue; }
        uint8_t sum = (uint8_t)(volume + v);
        SoundPsgSetVolume(t, sum >= 0x10 ? 15 : sum);
        return;
    }
}

/* $066556. Each frame: the volume envelope, if the track has one. */
void SoundPsgVolume(uint16_t t)
{
    if (!tr8(t, TR_VOICE)) return;
    psg_envelope(t);
}

/* ---- sounds asked for ---------------------------------------------------------------------------- */

/* $065B86. Of the queued sounds, the one of highest priority (and not below the playing
 * effect's) is the one to start; the queue empties. */
void SoundTakeRequests(void)
{
    uint8_t best = rd8(S_PRIORITY);
    for (int i = 0; i < 4; i++) {
        uint8_t id = rd8((uint16_t)(S_QUEUE + i));
        wr8((uint16_t)(S_QUEUE + i), 0);
        if (id < 0x81) continue;
        uint8_t p = rom8(PRIORITIES + ((id - 0x81) & 0x7F));
        if (p < best) continue;
        best = p;
        wr8(S_REQUEST, id);
    }
    if (!(best & 0x80)) wr8(S_PRIORITY, best);
}

/* $065BC8. The sound asked for: 0 starts the driver (the Z80's program loaded), $01-$7F stops
 * everything, $81-$9B a song, $A0-$DF an effect, $E0-$F5 a sample, $F6 fades out, $F7 stops the
 * effects, $F8 the special effects, $F9 everything. Returns 1 when a song started (the 68k
 * leaves the frame's update then). */
int SoundPlayRequest(void)
{
    uint8_t id = rd8(S_REQUEST);
    if (!id) {
        sound_init();
        return 0;
    }
    wr8(S_REQUEST, 0x80);
    if (id == 0x80) return 0;
    if (id < 0x80) { sound_stop_all(); return 0; }
    if (id <= 0x9B) { start_song(id); return 1; }
    if (id < 0xA0) return 0;
    if (id <= 0xDF) { start_effect(id); return 0; }
    if (id < 0xE0) { start_special(id); return 0; }     /* (never: $DF is an effect) */
    if (id < 0xF6) {
        SoundZ80Stop();
        z80_ram_write8(Z80_SAMPLE, (uint8_t)(id + 0xA8));
        z80_ram_write8(Z80_SAMPLE_TAKEN, 0x80);
        z80_go();
        return 0;
    }
    switch (id) {
    case 0xF6: SoundCmdFadeOut(); break;
    case 0xF7: SoundCmdStopEffects(); break;
    case 0xF8: SoundCmdStopSpecial(); break;
    case 0xF9: SoundCmdStop(); break;
    }
    return 0;
}

/* $065C26-$065C32: the commands $F6-$F9 (a table of branches) */
void SoundCmdFadeOut(void)
{
    SoundStopSpecial();
    wr8(S_FADE_DELAY, 3);
    wr8(S_FADE_STEPS, 0x28);
    wr8(T_DAC, 0);
}
void SoundCmdStopEffects(void) { SoundStopEffects(); }
void SoundCmdStopSpecial(void) { SoundStopSpecial(); }
void SoundCmdStop(void) { sound_stop_all(); }

/* A song: the effects and the old song stopped; its header: voices (an offset), the FM and PSG
 * tracks' numbers, the divider, the tempo; then each track's start (an offset) and transpose
 * and volume (PSG: and envelope and volume envelope). FM 6 is the DAC's unless the song has 7
 * FM tracks. The music tracks under a playing effect are marked taken; they all key off. */
static void start_song(uint8_t id)
{
    SoundStopEffects();
    SoundStopSpecial();
    SoundClearMusic();
    uint32_t song = rom32(SONGS + 4u * (id - 0x81)), hdr = song + 6;
    wr32(S_MUSIC_VOICES, song + bus_rd16(song));
    wr8(S_TEMPO, bus_rd8(song + 5));
    wr8(S_TEMPO_COUNT, bus_rd8(song + 5));
    uint8_t fm = bus_rd8(song + 2), psg = bus_rd8(song + 3), divider = bus_rd8(song + 4);
    uint16_t t = T_DAC;
    for (int i = 0; i < fm; i++, t += TRACK_SIZE) {
        wr8(t, TF_PLAYING);
        tr8w(t, TR_CHANNEL, rom8(FM_IDS + (uint32_t)i));
        tr8w(t, TR_DIVIDER, divider);
        tr8w(t, TR_STACK, TRACK_SIZE);
        tr8w(t, TR_PAN, 0xC0);
        tr8w(t, TR_TIMER, 1);
        wr32((uint16_t)(t + TR_DATA), song + bus_rd16(hdr));
        wr16((uint16_t)(t + TR_TRANSPOSE), bus_rd16(hdr + 2));
        hdr += 4;
    }
    if (fm) {
        if (fm == 7) {
            SoundYmBus(0x2B, 0);                             /* the DAC off */
        } else {
            SoundYmBus(0x28, 6);
            SoundZ80Stop();
            SoundYm2(0xB6, 0xC0);
            z80_go();
        }
    }
    t = T_PSG1;
    for (int i = 0; i < psg; i++, t += TRACK_SIZE) {
        /* (with no FM track, the 68k's registers here would be what was left in them) */
        wr8(t, TF_PLAYING);
        tr8w(t, TR_CHANNEL, rom8(PSG_IDS + (uint32_t)i));
        tr8w(t, TR_DIVIDER, divider);
        tr8w(t, TR_STACK, TRACK_SIZE);
        tr8w(t, TR_TIMER, 1);
        wr32((uint16_t)(t + TR_DATA), song + bus_rd16(hdr));
        wr16((uint16_t)(t + TR_TRANSPOSE), bus_rd16(hdr + 2));
        tr8w(t, TR_ENVELOPE, bus_rd8(hdr + 4));
        tr8w(t, TR_VOICE, bus_rd8(hdr + 5));
        hdr += 6;
    }
    t = T_SFX;
    for (int i = 0; i < 6; i++, t += TRACK_SIZE) {
        if (!(flags(t) & TF_PLAYING)) continue;
        uint8_t ch = channel(t);
        uint8_t k = ch & 0x80 ? (uint8_t)(ch >> 3) : (uint8_t)((uint8_t)(ch - 2) << 2);
        set8((uint16_t)rom32(MUSIC_TRACK + k), TB_TAKEN);
    }
    if (rd16(T_SPECIAL_FM) & 0x8000) set8(T_FM4, TB_TAKEN);
    if (rd16(T_SPECIAL_PSG) & 0x8000) set8(T_PSG3, TB_TAKEN);
    t = T_FM1;
    for (int i = 0; i < 6; i++, t += TRACK_SIZE) SoundNoteOff(t);
    for (int i = 0; i < 3; i++, t += TRACK_SIZE) SoundPsgNoteOff(t);
    if (!(rd8(T_PSG3) & TF_TAKEN)) psg_write(0xFF);
}

/* An effect: its voices (an offset), divider, tracks; each track's channel's music track marked
 * taken (the noise channel also silences tone 3), the effect track for that channel set up:
 * cleared, the voices, flags and channel, divider, start, transpose and volume. */
static void start_effect(uint8_t id)
{
    uint32_t fx = rom32(EFFECTS + 4u * (id - 0xA0)), hdr = fx + 2;
    uint32_t voice_table = fx + bus_rd16(fx);
    uint8_t divider = bus_rd8(hdr++), n = bus_rd8(hdr++);
    for (int i = 0; i < n; i++) {
        uint8_t ch = bus_rd8(hdr + 1);
        int k;
        if (!(ch & 0x80)) {
            k = (ch - 2) * 4;
            set8((uint16_t)rom32(MUSIC_TRACK + (uint32_t)k), TB_TAKEN);
        } else {
            k = ch >> 3;
            set8((uint16_t)rom32(MUSIC_TRACK + (uint32_t)k), TB_TAKEN);
            if (ch == 0xC0) {
                psg_write(0xDF);
                psg_write(0xFF);
            }
        }
        uint16_t t = (uint16_t)rom32(EFFECT_TRACK + (uint32_t)k);
        for (int b = 0; b < TRACK_SIZE; b++) wr8((uint16_t)(t + b), 0);
        wr32((uint16_t)(t + TR_PAN_TABLE), voice_table);
        wr16(t, bus_rd16(hdr));
        tr8w(t, TR_DIVIDER, divider);
        wr32((uint16_t)(t + TR_DATA), fx + bus_rd16(hdr + 2));
        wr16((uint16_t)(t + TR_TRANSPOSE), bus_rd16(hdr + 4));
        tr8w(t, TR_TIMER, 1);
        tr8w(t, TR_STACK, TRACK_SIZE);
        if (!(ch & 0x80)) tr8w(t, TR_PAN, 0xC0);
        hdr += 6;
    }
    if (rd8(T_SFX_FM2) & 0x80) set8(T_SPECIAL_FM, TB_TAKEN);
    if (rd8(T_SFX_PSG3) & 0x80) set8(T_SPECIAL_PSG, TB_TAKEN);
}

/* A special effect: its tracks on the special FM track (FM 4's) or the special noise track
 * (PSG 3's). Nothing in the game reaches it. */
static void start_special(uint8_t id)
{
    uint32_t fx = rom32(SPECIALS + 4u * (uint8_t)(id - 0xDF)), hdr = fx + 2;
    wr32(S_SFX_VOICES, fx + bus_rd16(fx));
    uint8_t divider = bus_rd8(hdr++), n = bus_rd8(hdr++);
    for (int i = 0; i < n; i++) {
        uint8_t ch = bus_rd8(hdr + 1);
        uint16_t t;
        if (!(ch & 0x80)) { set8(T_FM4, TB_TAKEN); t = T_SPECIAL_FM; }
        else { set8(T_PSG3, TB_TAKEN); t = T_SPECIAL_PSG; }
        for (int b = 0; b < TRACK_SIZE; b++) wr8((uint16_t)(t + b), 0);
        wr16(t, bus_rd16(hdr));
        tr8w(t, TR_DIVIDER, divider);
        wr32((uint16_t)(t + TR_DATA), fx + bus_rd16(hdr + 2));
        wr16((uint16_t)(t + TR_TRANSPOSE), bus_rd16(hdr + 4));
        tr8w(t, TR_TIMER, 1);
        tr8w(t, TR_STACK, TRACK_SIZE);
        if (!(ch & 0x80)) tr8w(t, TR_PAN, 0xC0);
        hdr += 6;
    }
    if (rd8(T_SFX_FM2) & 0x80) set8(T_SPECIAL_FM, TB_TAKEN);
    if (rd8(T_SFX_PSG3) & 0x80) {
        set8(T_SPECIAL_PSG, TB_TAKEN);
        uint8_t v = (uint8_t)(bus_rd8(hdr - 6 + 1) | 0x1F);  /* (the last track's channel byte) */
        psg_write(v);
        psg_write(v ^ 0x20);
    }
}

/* ---- stopping ------------------------------------------------------------------------------------ */

/* $065FB8. The effects stop: each one's music track gets its channel back (an FM one its voice
 * again), resting until its next note. */
void SoundStopEffects(void)
{
    wr8(S_PRIORITY, 0);
    SoundYmBus(0x27, 0);
    uint16_t t = T_SFX;
    for (int i = 0; i < 6; i++, t += TRACK_SIZE) {
        if (!(flags(t) & TF_PLAYING)) continue;
        wr8(t, flags(t) & ~TF_PLAYING);
        uint8_t ch = channel(t);
        if (!(ch & 0x80)) {
            SoundNoteOff(t);
            uint16_t m;
            uint32_t table;
            if (ch == 4 && (rd8(T_SPECIAL_FM) & 0x80)) {
                m = T_SPECIAL_FM;                                /* (never in a recorded run) */
                table = rd32(S_SFX_VOICES);
            } else {
                m = (uint16_t)rom32(MUSIC_TRACK + (uint8_t)((uint8_t)(ch - 2) << 2));
                table = rd32(S_MUSIC_VOICES);
            }
            wr8(m, (flags(m) & ~TF_TAKEN) | TF_REST);
            SoundLoadVoice(m, tr8(m, TR_VOICE), table);
        } else {
            SoundPsgNoteOff(t);
            uint16_t m = T_SPECIAL_PSG;
            if (ch != 0xE0 && ch != 0xC0) m = (uint16_t)rom32(MUSIC_TRACK + (uint8_t)(ch >> 3));
            wr8(m, (flags(m) & ~TF_TAKEN) | TF_REST);
            if (channel(m) == 0xE0) psg_write(tr8(m, TR_ALGORITHM));
        }
    }
}

/* $06606A. The special effects stop; FM 4 and PSG 3 get their channels back. */
void SoundStopSpecial(void)
{
    uint16_t t = T_SPECIAL_FM;
    if ((flags(t) & TF_PLAYING)) {
        wr8(t, flags(t) & ~TF_PLAYING);
        if (!(flags(t) & TF_TAKEN)) {
            SoundKeyOff(t);
            t = T_FM4;
            wr8(t, (flags(t) & ~TF_TAKEN) | TF_REST);
            if (flags(t) & TF_PLAYING) SoundLoadVoice(t, tr8(t, TR_VOICE), rd32(S_MUSIC_VOICES));
        }
    }
    t = T_SPECIAL_PSG;
    if ((flags(t) & TF_PLAYING)) {
        wr8(t, flags(t) & ~TF_PLAYING);
        if (!(flags(t) & TF_TAKEN)) {
            SoundPsgOff(t);
            t = T_PSG3;
            wr8(t, (flags(t) & ~TF_TAKEN) | TF_REST);
            if ((flags(t) & TF_PLAYING) && channel(t) == 0xE0) psg_write(tr8(t, TR_ALGORITHM));
        }
    }
}

/* $06624E. The song's state cleared (the effect's priority kept). */
void SoundClearMusic(void)
{
    SoundYmBus(0x27, 0);
    uint8_t priority = rd8(S_PRIORITY);
    for (uint16_t a = 0xF800; a < 0xFA20; a++) wr8(a, 0);
    wr8(S_PRIORITY, priority);
    wr8(S_REQUEST, 0x80);
}

/* Everything stops: the DAC on, timers off, the driver's RAM cleared (to $FB8F), FM and PSG
 * silent. */
static void sound_stop_all(void)
{
    SoundYmBus(0x2B, 0x80);
    SoundYmBus(0x27, 0);
    for (uint16_t a = 0xF800; a < 0xFB90; a++) wr8(a, 0);
    wr8(S_REQUEST, 0x80);
    SoundSilenceFm();
    psg_silence();
}

/* The driver's start: the Z80's program into its RAM (the sample player), its mailbox, a reset
 * of the Z80 and the YM2612; then everything stopped. */
static void sound_init(void)
{
    z80_busreq(0x100);
    while (z80_busack() & 1) {}
    for (uint16_t i = 0; i < Z80_PROGRAM_SIZE; i++) z80_ram_write8(i, rom8(Z80_PROGRAM + i));
    z80_ram_write8(0x1FF8, 0x14);
    z80_ram_write8(0x1FF9, 0x89);
    z80_ram_write8(0x1FFA, 0x07);
    z80_ram_write8(0x1FFB, 0x80);
    z80_reset(0);
    z80_reset(0x100);
    z80_busreq(0);
    sound_stop_all();
}

/* $065604. The driver started from outside (the reset). */
void SoundInit(void)
{
    sound_init();
}

/* ---- coordination flags --------------------------------------------------------------------------- */

static int jump(uint32_t *data)
{
    uint16_t off = (uint16_t)(next(data) << 8);
    off |= next(data);
    *data += (uint32_t)(int16_t)off - 1;
    return 0;
}

/* $0685DC. Flag $E0-$FF of a track (data past it): returns 1 when it stopped the track. */
int SoundFlag(uint16_t t, uint8_t flag, uint32_t *data)
{
    switch (flag) {
    case 0xE0: return SoundFlagPan(t, data);
    case 0xE1: return SoundFlagDetune(t, data);
    case 0xE2: return SoundFlagE2(t, data);
    case 0xE3: return SoundFlagSilence(t, data);
    case 0xE4: return SoundFlagPanAnim(t, data);
    case 0xE5: return SoundFlagVolumeFmPsg(t, data);
    case 0xE6: return SoundFlagVolumeFm(t, data);
    case 0xE7: return SoundFlagTie(t, data);
    case 0xE8: return SoundFlagNoteFill(t, data);
    case 0xE9: return SoundFlagLfo(t, data);
    case 0xEA: return SoundFlagTempo(t, data);
    case 0xEB: return SoundFlagQueue(t, data);
    case 0xEC: return SoundFlagVolume(t, data);
    case 0xED: return SoundFlagYm(t, data);
    case 0xEE: return SoundFlagYm1(t, data);
    case 0xEF: return SoundFlagVoice(t, data);
    case 0xF0: return SoundFlagModulation(t, data);
    case 0xF1: return SoundFlagEnvelopeFmPsg(t, data);
    case 0xF2: return SoundFlagStop(t, data);
    case 0xF3: return SoundFlagNoise(t, data);
    case 0xF4: return SoundFlagEnvelope(t, data);
    case 0xF5: return SoundFlagVolEnvelope(t, data);
    case 0xF6: return SoundFlagJump(t, data);
    case 0xF7: return SoundFlagLoop(t, data);
    case 0xF8: return SoundFlagCall(t, data);
    case 0xF9: return SoundFlagReturn(t, data);
    case 0xFA: return SoundFlagDivider(t, data);
    case 0xFB: return SoundFlagTranspose(t, data);
    case 0xFC: return SoundFlagModOn(t, data);
    case 0xFD: return SoundFlagModOff(t, data);
    case 0xFE: return SoundFlagFm3(t, data);
    default: return sound_flag_ff(t, data);
    }
}

/* $0685E6. $E0 p: the pan (FM). */
int SoundFlagPan(uint16_t t, uint32_t *data)
{
    uint8_t pan = next(data);
    if (is_psg(t)) return 0;
    pan |= tr8(t, TR_PAN) & 0x37;
    tr8w(t, TR_PAN, pan);
    SoundYmChannelBus(t, 0xB4, pan);
    return 0;
}

/* $0685EA. $E1 d: the detune. */
int SoundFlagDetune(uint16_t t, uint32_t *data) { tr8w(t, TR_DETUNE, next(data)); return 0; }

/* $0685EE. $E2 n: into $F803, which nothing reads. */
int SoundFlagE2(uint16_t t, uint32_t *data) { (void)t; wr8(S_F803, next(data)); return 0; }

/* $0685F2. $E3: the voice silenced, then the track stops. */
int SoundFlagSilence(uint16_t t, uint32_t *data)
{
    SoundVoiceSilence(t);
    return SoundFlagStop(t, data);
}

/* $0661A8. Every operator's total level at its quietest and its release at its fastest. */
void SoundVoiceSilence(uint16_t t)
{
    SoundZ80Stop();
    for (uint8_t reg = 0x40; reg < 0x50; reg += 4) SoundYmChannel(t, reg, 0x7F);
    for (uint8_t reg = 0x80; reg < 0x90; reg += 4) SoundYmChannel(t, reg, 0x0F);
    z80_go();
}

/* $0685F6. $E4 m [table pos limit rate]: the pan animation (m 0: off, the pan written again). */
int SoundFlagPanAnim(uint16_t t, uint32_t *data)
{
    uint8_t mode = next(data);
    tr8w(t, TR_PAN_MODE, mode);
    if (!mode) {
        SoundYmChannelBus(t, 0xB4, tr8(t, TR_PAN));
        return 0;
    }
    tr8w(t, TR_PAN_TABLE, next(data));
    tr8w(t, TR_PAN_POS, next(data));
    tr8w(t, TR_PAN_LIMIT, next(data));
    tr8w(t, TR_PAN_RATE, bus_rd8(*data));
    tr8w(t, TR_PAN_TIMER, next(data));
    return 0;
}

/* $0685FA. $E5 p f: the volume plus p on PSG, plus f on FM. */
int SoundFlagVolumeFmPsg(uint16_t t, uint32_t *data)
{
    uint8_t v = next(data);
    if (!is_psg(t)) return SoundFlagVolumeFm(t, data);
    tr8w(t, TR_VOLUME, (uint8_t)(tr8(t, TR_VOLUME) + v));
    (*data)++;
    return 0;
}

/* $0685FE. $E6 v: the volume plus v, the carriers written. */
int SoundFlagVolumeFm(uint16_t t, uint32_t *data)
{
    tr8w(t, TR_VOLUME, (uint8_t)(tr8(t, TR_VOLUME) + next(data)));
    SoundFmVolume(t);
    return 0;
}

/* $068602. $E7: the next note is tied to this one. */
int SoundFlagTie(uint16_t t, uint32_t *data) { (void)data; wr8(t, flags(t) | TF_TIE); return 0; }

/* $068606. $E8 n: the note fill. */
int SoundFlagNoteFill(uint16_t t, uint32_t *data)
{
    tr8w(t, TR_FILL, bus_rd8(*data));
    tr8w(t, TR_FILL_START, next(data));
    return 0;
}

/* $06860A. $E9 m l p: the voice's operators' AM bits (m's top four bits, the voice's decay
 * rates from the effect's voice table), the LFO (l), the AMS and FMS bits of the pan (p). No
 * recorded run used it. */
int SoundFlagLfo(uint16_t t, uint32_t *data)
{
    uint32_t table = rd32((uint16_t)(t + TR_PAN_TABLE));
    uint16_t mask = bus_rd8(*data);
    for (int i = 0; i < 4; i++) {
        uint8_t v = bus_rd8(table++), reg = rom8(E9_REGS + (uint32_t)i);
        if (mask & 0x80) SoundYmChannelBus(t, reg, v | 0x80);
        mask = (uint16_t)(mask << 1);
    }
    SoundYmBus(0x22, next(data));
    uint8_t pan = next(data) | (tr8(t, TR_PAN) & 0xC0);
    tr8w(t, TR_PAN, pan);
    SoundYmChannelBus(t, 0xB4, pan);
    return 0;
}

/* $06860E. $EA n: the tempo. */
int SoundFlagTempo(uint16_t t, uint32_t *data)
{
    (void)t;
    wr8(S_TEMPO, bus_rd8(*data));
    wr8(S_TEMPO_COUNT, next(data));
    return 0;
}

/* $068612. $EB s: sound s into the queue's first slot. */
int SoundFlagQueue(uint16_t t, uint32_t *data) { (void)t; wr8(S_QUEUE, next(data)); return 0; }

/* $068616. $EC v: the volume plus v. */
int SoundFlagVolume(uint16_t t, uint32_t *data)
{
    tr8w(t, TR_VOLUME, (uint8_t)(tr8(t, TR_VOLUME) + next(data)));
    return 0;
}

/* $06861A. $ED r v: register r (plus the channel) set to v. */
int SoundFlagYm(uint16_t t, uint32_t *data)
{
    uint8_t reg = next(data), v = next(data);
    SoundYmChannelBus(t, reg, v);
    return 0;
}

/* $06861E. $EE r v: register r of the first part set to v. */
int SoundFlagYm1(uint16_t t, uint32_t *data)
{
    (void)t;
    uint8_t reg = next(data), v = next(data);
    SoundYmBus(reg, v);
    return 0;
}

/* $068622. $EF v: the FM voice. */
int SoundFlagVoice(uint16_t t, uint32_t *data)
{
    uint8_t voice = next(data);
    tr8w(t, TR_VOICE, voice);
    if (flags(t) & TF_TAKEN) return 0;
    SoundLoadVoice(t, voice, voices(t));
    return 0;
}

/* $068626. $F0 wait speed delta steps: the modulation, on. */
int SoundFlagModulation(uint16_t t, uint32_t *data)
{
    tr8w(t, TR_ENVELOPE, tr8(t, TR_ENVELOPE) | 0x80);
    wr32((uint16_t)(t + TR_MOD_DATA), *data);
    tr8w(t, TR_MOD_WAIT, next(data));
    tr8w(t, TR_MOD_SPEED, next(data));
    tr8w(t, TR_MOD_DELTA, next(data));
    tr8w(t, TR_MOD_STEPS, (uint8_t)(next(data) >> 1));
    wr16((uint16_t)(t + TR_MOD_SUM), 0);
    return 0;
}

/* $06862A. $F1 p f: the frequency envelope, p on PSG, f on FM. */
int SoundFlagEnvelopeFmPsg(uint16_t t, uint32_t *data)
{
    uint8_t v = next(data);
    if (!is_psg(t)) return SoundFlagEnvelope(t, data);
    tr8w(t, TR_ENVELOPE, v);
    (*data)++;
    return 0;
}

/* $06862E. $F2: the track stops (key off, unless the DAC took FM 6 this frame). An effect's
 * track gives its channel back: the music's track rests, an FM one with its voice loaded
 * again. Returns 1: the 68k pops two return addresses, ending the track's frame. */
int SoundFlagStop(uint16_t t, uint32_t *data)
{
    (void)data;
    wr8(t, flags(t) & ~(TF_PLAYING | TF_TIE));
    if (!is_psg(t)) {
        if (rd8(S_DAC_PLAYED) & 0x80) return 1;
        SoundNoteOff(t);
    } else {
        SoundPsgNoteOff(t);
    }
    if (!(rd8(S_GROUP) & 0x80)) return 1;
    wr8(S_PRIORITY, 0);
    uint8_t ch = channel(t);
    if (!(ch & 0x80)) {
        uint16_t m;
        uint32_t table;
        if (ch == 4 && (rd8(T_SPECIAL_FM) & 0x80)) {
            m = T_SPECIAL_FM;                                    /* (never in a recorded run) */
            table = rd32(S_SFX_VOICES);
        } else {
            m = (uint16_t)rom32(MUSIC_TRACK + (uint8_t)((uint8_t)(ch - 2) << 2));
            table = rd32(S_MUSIC_VOICES);
            if (!(flags(m) & TF_PLAYING)) m = 0;
        }
        if (m) {
            wr8(m, (flags(m) & ~TF_TAKEN) | TF_REST);
            SoundLoadVoice(m, tr8(m, TR_VOICE), table);
        }
        if (channel(t) == 2 && !rd8(S_FM3_SPECIAL)) SoundYmBus(0x27, 0);
        return 1;
    }
    uint16_t m = T_SPECIAL_PSG;
    if (!(flags(m) & TF_PLAYING) || (ch != 0xE0 && ch != 0xC0)) m = (uint16_t)rom32(MUSIC_TRACK + (uint8_t)(ch >> 3));
    wr8(m, (flags(m) & ~TF_TAKEN) | TF_REST);
    if (channel(m) == 0xE0) psg_write(tr8(m, TR_ALGORITHM));
    return 1;
}

/* $068632. $F3 n: the track plays the noise channel, in mode n. */
int SoundFlagNoise(uint16_t t, uint32_t *data)
{
    tr8w(t, TR_CHANNEL, 0xE0);
    uint8_t mode = next(data);
    tr8w(t, TR_ALGORITHM, mode);
    if (!(flags(t) & TF_TAKEN)) psg_write(mode);
    return 0;
}

/* $068636. $F4 e: the frequency envelope (and the modulation bit). */
int SoundFlagEnvelope(uint16_t t, uint32_t *data) { tr8w(t, TR_ENVELOPE, next(data)); return 0; }

/* $06863A. $F5 e: the PSG volume envelope (or the FM voice number, without loading it). */
int SoundFlagVolEnvelope(uint16_t t, uint32_t *data) { tr8w(t, TR_VOICE, next(data)); return 0; }

/* $06863E. $F6 o: the data goes on at o from the offset's second byte. */
int SoundFlagJump(uint16_t t, uint32_t *data) { (void)t; return jump(data); }

/* $068642. $F7 i n o: loop counter i set to n the first time; while it counts down, jump. */
int SoundFlagLoop(uint16_t t, uint32_t *data)
{
    uint8_t i = next(data), n = next(data);
    uint16_t counter = (uint16_t)(t + TR_LOOPS + i);
    if (!rd8(counter)) wr8(counter, n);
    wr8(counter, (uint8_t)(rd8(counter) - 1));
    if (rd8(counter)) return jump(data);
    *data += 2;
    return 0;
}

/* $068646. $F8 o: a call: the data's place onto the track's stack, then the jump. */
int SoundFlagCall(uint16_t t, uint32_t *data)
{
    uint8_t sp = (uint8_t)(tr8(t, TR_STACK) - 4);
    wr32((uint16_t)(t + sp), *data);
    tr8w(t, TR_STACK, sp);
    return jump(data);
}

/* $06864A. $F9: back after the call's offset. */
int SoundFlagReturn(uint16_t t, uint32_t *data)
{
    uint8_t sp = tr8(t, TR_STACK);
    *data = rd32((uint16_t)(t + sp)) + 2;
    tr8w(t, TR_STACK, (uint8_t)(sp + 4));
    return 0;
}

/* $06864E. $FA d: the duration divider. */
int SoundFlagDivider(uint16_t t, uint32_t *data) { tr8w(t, TR_DIVIDER, next(data)); return 0; }

/* $068652. $FB n: the transpose plus n. */
int SoundFlagTranspose(uint16_t t, uint32_t *data)
{
    tr8w(t, TR_TRANSPOSE, (uint8_t)(tr8(t, TR_TRANSPOSE) + next(data)));
    return 0;
}

/* $068656. $FC: the modulation on. */
int SoundFlagModOn(uint16_t t, uint32_t *data) { (void)data; tr8w(t, TR_ENVELOPE, tr8(t, TR_ENVELOPE) | 0x80); return 0; }

/* $06865A. $FD: off. */
int SoundFlagModOff(uint16_t t, uint32_t *data) { (void)data; tr8w(t, TR_ENVELOPE, tr8(t, TR_ENVELOPE) & 0x7F); return 0; }

/* $06865E. $FE a b c d: FM 3 in its special mode, its operators' frequencies from a table. */
int SoundFlagFm3(uint16_t t, uint32_t *data)
{
    (void)t;
    uint16_t freqs = S_FM3_FREQS + 8;
    if (!rd8(S_GROUP)) {
        freqs = S_FM3_FREQS;
        wr8(S_FM3_SPECIAL, 0x80);
    }
    for (int i = 0; i < 4; i++) wr16((uint16_t)(freqs + 2 * i), rom16(FE_FREQS + 2u * next(data)));
    SoundYmBus(0x27, 0x40);
    return 0;
}

/* $FF c ...: commands no recorded run used (the 68k's code at $068662 onward, not a routine of
 * its own): 0: four operators' SSG-EG and attack rate; 1 n: the music tracks paused (n != 0)
 * or resumed; 2 d: every music track's divider; 3 f p: a volume slide down by f and p; 4: one
 * up. */
static int sound_flag_ff(uint16_t t, uint32_t *data)
{
    uint8_t cmd = next(data);
    switch (cmd) {
    case 0:
        for (int i = 0; i < 4; i++) {
            SoundYmChannelBus(t, rom8(FF1_REGS + 2u * i), next(data));
            SoundYmChannelBus(t, rom8(FF1_REGS + 2u * i + 1), 0x1F);
        }
        return 0;
    case 1: {
        uint16_t m = T_DAC;
        if (next(data)) {
            if (flags(m) & TF_PLAYING) wr8(m, (flags(m) & ~TF_PLAYING) | TF_PAUSED);
            for (int i = 0; i < 6; i++) {
                m += TRACK_SIZE;
                if (!(flags(m) & TF_PLAYING)) continue;
                wr8(m, (flags(m) & ~TF_PLAYING) | TF_PAUSED);
                SoundYmChannelBus(m, 0xB4, 0);
                SoundNoteOff(m);
            }
            for (int i = 0; i < 3; i++) {
                m += TRACK_SIZE;
                if (!(flags(m) & TF_PLAYING)) continue;
                wr8(m, (flags(m) & ~TF_PLAYING) | TF_PAUSED);
                SoundPsgNoteOff(m);
            }
        } else {
            if (flags(m) & TF_PAUSED) {
                wr8(m, (flags(m) | TF_PLAYING) & ~TF_PAUSED);
                SoundYmChannelBus(m, 0xB4, tr8(m, TR_PAN));
            }
            for (int i = 0; i < 6; i++) {
                m += TRACK_SIZE;
                if (!(flags(m) & TF_PAUSED)) continue;
                wr8(m, (flags(m) | TF_PLAYING) & ~TF_PAUSED);
                SoundYmChannelBus(m, 0xB4, tr8(m, TR_PAN));
            }
            for (int i = 0; i < 3; i++) {
                m += TRACK_SIZE;
                if (flags(m) & TF_PAUSED) wr8(m, (flags(m) | TF_PLAYING) & ~TF_PAUSED);
            }
        }
        return 0;
    }
    case 2: {
        uint8_t d = next(data);
        for (uint16_t m = T_DAC, i = 0; i < 10; i++, m += TRACK_SIZE) tr8w(m, TR_DIVIDER, d);
        return 0;
    }
    case 3:
        if (rd8(S_SLIDE) != 2) {
            wr8(S_SLIDE, 1);
            wr8(S_SLIDE_FM, next(data));
            wr8(S_SLIDE_PSG, next(data));
        }
        return 0;
    case 4:
        wr8(S_SLIDE, 0x80);
        return 0;
    default:
        call68k_missing(0x068662);                       /* past the table: not in the game */
        return 0;
    }
}
