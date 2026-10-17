/* Gunstar Heroes in C: the ported routines, by the part of the game they belong to. Each is the
 * 68k routine of the same name (tools/disasm/names.txt). The comment after each prototype gives
 * its address and registers, "$003156 a2 -> d0.b": the registers its parameters were (results
 * aside), then its results (the return value, then the pointer parameters), a flag Z / N / C
 * when it returns one. The tests' adapters are generated from these (tools/gen_ports.py); a
 * comment with "manual" has its adapter in test/ports.c. */
#pragma once
#include <stdint.h>
#include "mem.h"
#include "hal.h"
#include "symbols_gen.h"

/* VDP register n's word in v_vdpRegs ($8rvv; its value at +1) */
#define VDP_REG(n) (v_vdpRegs + 2 * (n))

/* Registers the 68k keeps from one routine to another, kept in C globals (states carry them) */
extern uint32_t reg_a6;     /* a6: no routine saves it, so it outlives them; the H-interrupt
                               handlers read their table through it (HintEffect sets it), and
                               the Treasure logo's routines leave their pointers in it */
extern uint32_t reg_sp;     /* a7: the 68k's stack pointer, kept (by the callers on the way, with
                               WITH_SP) for the routines that leave stack addresses in RAM: the
                               Treasure logo's polygons. Natively a vertical blank's in the main
                               loop */
/* a call the 68k makes with `bytes` more on its stack (its return address, registers saved) */
#define WITH_SP(bytes, call) do { reg_sp -= (bytes); call; reg_sp += (bytes); } while (0)

/* the objects: $60 bytes from $FFC800 ($FFA800 one more; the players $200 at v_player1, 2) */
#define OBJECT_ROUTINES 0x00054Cu    /* dc.l: each type's routine (the type is its offset) */
#define O_TYPE     0x00              /* word: the type; 0: no object */
#define O_FLAGS    0x02              /* word: bit 15 shown, bit 12 to be removed, bit 9 removed
                                        when off the screen */
#define O_STATE    0x04              /* word: the routine's step */
#define O_X        0x10              /* long: x, 16.16 */
#define O_Y        0x14              /* long: y */
#define O_VITALITY 0x24

/* ---- the reset and the main loop (boot.c) ---- */
extern int checksum_check;
void Start(void);                                                    /* $000200 */
void MainLoop(void);                                                 /* $000434 */
void VdpInitRegs(void);                                              /* $002AB4 */
void IoInit(void);                                                   /* $002B2A */
void FrameInit(void);                                                /* $002A7C */
void ClearDmaArea(void);                                             /* $002DE0 */
void ClearRamF000(void);                                             /* $002DC8 */
void ClearRamB800(void);                                             /* $002D00 */
void ResetPads(void);                                                /* $003050 */

/* ---- the vertical blank (vblank.c) ---- */
void VBlank(void);                                                   /* $000BB2 a6=reg_a6, a7=reg_sp -> a6=reg_a6 */
void CheckPause(void);                                               /* $000C36 */

/* ---- the VDP and the palette (vdp.c) ---- */
void VBlankDma(void);                                                /* $000D36 */
void VdpWriteRegs(void);                                             /* $000F4C */
void VdpWriteReg0(void);                                             /* $000F9C */
void FadePalette(void);                                              /* $000FB4 */
void QueueHScroll(void);                                             /* $0012F4 */
void QueueVScroll(void);                                             /* $00134A */
uint32_t Random(void);                                               /* $003630 -> d0 */
uint8_t bcd_add8(uint8_t a, uint8_t b, int *x);
uint8_t bcd_sub8(uint8_t dst, uint8_t src, int *x);
void QueueDma(uint32_t command, uint32_t src, uint32_t length_regs); /* (a helper: a DMA into the queue) */
void LoadPalettes(uint32_t list);                                    /* $001BE4 a0 */
void LoadPaletteLine(uint32_t *list, uint16_t *pal, uint16_t *target);   /* $001BFE -> a0, a2, a3 */
uint16_t AdjustColour(uint16_t colour, uint16_t delta, uint16_t mask);   /* $003774 d3.w, d4.w, d5.w -> d2.w */
void AdjustLine(uint16_t src, uint16_t dst, uint16_t delta, uint16_t mask);   /* $003820 a0, a1, a2, a3 */
void AdjustPalettes(void);                                           /* $0036A4 */

/* ---- background graphics: the job list, command lists, the decoders (jobs.c) ---- */
void RunCommands(uint32_t list);                                     /* $0022DE a0 */
uint32_t CmdCopyToRam(uint32_t list);                                /* $002310 a0 -> a0 */
uint32_t CmdCopyToVram(uint32_t list);                               /* $002326 a0 -> a0 */
uint32_t CmdDecodeToRam(uint32_t list);                              /* $002374 a0 -> a0 */
uint32_t CmdDecodeToVram(uint32_t list);                             /* $0023A0 a0 -> a0 */
uint32_t CmdRleToRam(uint32_t list);                                 /* $0023CC a0 -> a0 */
uint32_t CmdRleToVram(uint32_t list);                                /* $0023E6 a0 -> a0 */
uint16_t NextJob(uint32_t *list);                                    /* $002478 -> d0.w, a0 */
void RunJobs(void);                                                  /* $002506 */
void JobCopyToRam(void);                                             /* $00254C */
void JobCopyToVram(void);                                            /* $002562 */
void JobDecodeToRam(void);                                           /* $00258A */
void JobDecodeToVram(void);                                          /* $0025A6 */
void JobRleToRam(void);                                              /* $0025FC */
void JobRleToVram(void);                                             /* $002616 */
void RleChunk(uint32_t end, uint32_t *src, uint32_t *dst);           /* $002660 a4 -> a1, a2 */
void RleRun(uint32_t *src, uint32_t *dst);                           /* $00267E -> a1, a2 */
void ClearTilePixels(void);                                          /* $002702 */
uint32_t DecodeTile(uint32_t src);                                   /* $002712 a1 -> a1 */
uint32_t PackTile(uint32_t dst);                                     /* $002886 a2 -> a2 */
uint32_t WriteTileVram(uint32_t vram);                               /* $0028B2 a3 -> a3 */
void QueueTileDma(uint32_t bytes, uint32_t *src, uint32_t *vram);    /* $00290C d1 -> a2, a3 */

/* ---- H-interrupt effects (hint.c) ---- */
void HintEffect(void);                                               /* $001416 a6=reg_a6 -> a6=reg_a6 */
void HintNone(void);                                                 /* $001440 */
void HintEffect1(void);                                              /* $001462 -> a6=reg_a6 */
void HintEffect3(void);                                              /* $0014A6 -> a6=reg_a6 */
void HintEffect4(void);                                              /* $0014EA -> a6=reg_a6 */
void HintEffect2(void);                                              /* $001534 -> a6=reg_a6 */
void HintEffect5(void);                                              /* $0015C8 */
void HintEffect6(void);                                              /* $00161A -> a6=reg_a6 */
void HintEffect7(void);                                              /* $00165E -> a6=reg_a6 */
void HintShiftLines(uint16_t end);                                   /* $00176E d0.w */

/* ---- the pads (pads.c) ---- */
void ReadPads(void);                                                 /* $003066 */
void ReadPad1(void);                                                 /* $003094 */
void ReadPad2(void);                                                 /* $0030B6 */
uint8_t RemapButtons(uint8_t buttons, uint16_t map);                 /* $00311E d0.b, a3 -> d0.b */
uint8_t CopyBit(uint8_t buttons, uint8_t raw, uint8_t to, uint8_t from);   /* $003148 d0.b, d1.b, d2.b, d3.b -> d0.b */
uint8_t PadId(uint32_t port);                                        /* $003156 a2 -> d0.b */

/* ---- the sound driver (sound.c) ---- */
void UpdateSound(void);                                              /* $000C88 */
int QueueSound(uint8_t id);                                          /* $003196 d0.b -> !Z */
void SoundDriver(void);                                              /* $065600 */
void SoundInit(void);                                                /* $065604 */
int SoundDacTrack(uint16_t t);                                       /* $0656C0 a5 -> - */
void SoundFmTrack(uint16_t t);                                       /* $06575E a5 */
int SoundFmNextNote(uint16_t t);                                     /* $065788 a5 -> - */
void SoundFmNote(uint16_t t, uint8_t note, uint32_t data);           /* $0657BE a5, d5.b, a4 */
void SoundSetDuration(uint16_t t, uint8_t n);                        /* $0657F0 a5, d5.b */
void SoundNoteStart(uint16_t t, uint32_t data);                      /* $065810 a5, a4 */
int SoundNoteFill(uint16_t t);                                       /* $065858 a5 -> - */
void SoundModulate(uint16_t t);                                      /* $065880 a5 */
void SoundFmNoteFreq(uint16_t t);                                    /* $0658D0 a5 */
uint16_t SoundFreq(uint16_t t);                                      /* $06592E a5 -> d6.w */
void SoundPanStart(uint16_t t);                                      /* $065A1C a5 */
void SoundPan0Start(uint16_t t);                                     /* $065A2E a5 */
void SoundPan1Start(uint16_t t);                                     /* $065A30 a5 */
void SoundPan0Step(uint16_t t);                                      /* $065A48 a5 */
void SoundPan1Step(uint16_t t);                                      /* $065A4A a5 */
void SoundPan2Start(uint16_t t);                                     /* $065A32 a5 */
void SoundPan3Start(uint16_t t);                                     /* $065A34 a5 */
void SoundPanStep(uint16_t t);                                       /* $065A36 a5 */
void SoundPan2Step(uint16_t t);                                      /* $065A4C a5 */
void SoundPan3Step(uint16_t t);                                      /* $065A4E a5 */
void SoundTakeRequests(void);                                        /* $065B86 */
int SoundPlayRequest(void);                                          /* $065BC8 -> - */
void SoundCmdFadeOut(void);                                          /* $065C26 */
void SoundCmdStopEffects(void);                                      /* $065C2A */
void SoundCmdStopSpecial(void);                                      /* $065C2E */
void SoundCmdStop(void);                                             /* $065C32 */
void SoundStopEffects(void);                                         /* $065FB8 */
void SoundStopSpecial(void);                                         /* $06606A */
void SoundFadeOut(void);                                             /* $0660FA */
void SoundTempo(void);                                               /* $066174 */
void SoundVoiceSilence(uint16_t t);                                  /* $0661A8 a5 */
void SoundSilenceFm(void);                                           /* $0661DC */
void SoundClearMusic(void);                                          /* $06624E */
void SoundNoteOff(uint16_t t);                                       /* $06630C a5 */
void SoundKeyOff(uint16_t t);                                        /* $066318 a5 */
void SoundYmChannelBus(uint16_t t, uint8_t reg, uint8_t value);      /* $066324 a5, d0.b, d1.b */
void SoundYmBus(uint8_t reg, uint8_t value);                         /* $06633C d0.b, d1.b */
void SoundYmChannel(uint16_t t, uint8_t reg, uint8_t value);         /* $06634E a5, d0.b, d1.b */
void SoundYm1(uint8_t reg, uint8_t value);                           /* $06635A d0.b, d1.b */
void SoundYm2(uint8_t reg, uint8_t value);                           /* $066380 d0.b, d1.b */
void SoundZ80Stop(void);                                             /* $06639E */
void SoundSlide(void);                                               /* $0663D0 */
void SoundPsgTrack(uint16_t t);                                      /* $066480 a5 */
int SoundPsgNextNote(uint16_t t);                                    /* $0664A6 a5 -> - */
void SoundPsgNote(uint16_t t, uint8_t note, uint32_t data);          /* $0664DA a5, d5.b, a4 */
void SoundPsgNoteFreq(uint16_t t);                                   /* $06650A a5 */
void SoundPsgVolume(uint16_t t);                                     /* $066556 a5 */
void SoundPsgSetVolume(uint16_t t, uint8_t volume);                  /* $0665A8 a5, d6.b */
void SoundPsgNoteOff(uint16_t t);                                    /* $0665FA a5 */
void SoundPsgOff(uint16_t t);                                        /* $066600 a5 */
int SoundFlag(uint16_t t, uint8_t flag, uint32_t *data);             /* $0685DC a5, d5.b -> -, a4 */
void SoundLoadVoice(uint16_t t, uint8_t voice, uint32_t table);      /* $0687B4 a5, d0.b, a1 */
void SoundFmVolume(uint16_t t);                                      /* $068826 a5 */
int SoundFlagPan(uint16_t t, uint32_t *data);                        /* $0685E6 a5 -> -, a4 */
int SoundFlagDetune(uint16_t t, uint32_t *data);                     /* $0685EA a5 -> -, a4 */
int SoundFlagE2(uint16_t t, uint32_t *data);                         /* $0685EE a5 -> -, a4 */
int SoundFlagSilence(uint16_t t, uint32_t *data);                    /* $0685F2 a5 -> -, a4 */
int SoundFlagPanAnim(uint16_t t, uint32_t *data);                    /* $0685F6 a5 -> -, a4 */
int SoundFlagVolumeFmPsg(uint16_t t, uint32_t *data);                /* $0685FA a5 -> -, a4 */
int SoundFlagVolumeFm(uint16_t t, uint32_t *data);                   /* $0685FE a5 -> -, a4 */
int SoundFlagTie(uint16_t t, uint32_t *data);                        /* $068602 a5 -> -, a4 */
int SoundFlagNoteFill(uint16_t t, uint32_t *data);                   /* $068606 a5 -> -, a4 */
int SoundFlagLfo(uint16_t t, uint32_t *data);                        /* $06860A a5 -> -, a4 */
int SoundFlagTempo(uint16_t t, uint32_t *data);                      /* $06860E a5 -> -, a4 */
int SoundFlagQueue(uint16_t t, uint32_t *data);                      /* $068612 a5 -> -, a4 */
int SoundFlagVolume(uint16_t t, uint32_t *data);                     /* $068616 a5 -> -, a4 */
int SoundFlagYm(uint16_t t, uint32_t *data);                         /* $06861A a5 -> -, a4 */
int SoundFlagYm1(uint16_t t, uint32_t *data);                        /* $06861E a5 -> -, a4 */
int SoundFlagVoice(uint16_t t, uint32_t *data);                      /* $068622 a5 -> -, a4 */
int SoundFlagModulation(uint16_t t, uint32_t *data);                 /* $068626 a5 -> -, a4 */
int SoundFlagEnvelopeFmPsg(uint16_t t, uint32_t *data);              /* $06862A a5 -> -, a4 */
int SoundFlagStop(uint16_t t, uint32_t *data);                       /* $06862E a5 -> -, a4 */
int SoundFlagNoise(uint16_t t, uint32_t *data);                      /* $068632 a5 -> -, a4 */
int SoundFlagEnvelope(uint16_t t, uint32_t *data);                   /* $068636 a5 -> -, a4 */
int SoundFlagVolEnvelope(uint16_t t, uint32_t *data);                /* $06863A a5 -> -, a4 */
int SoundFlagJump(uint16_t t, uint32_t *data);                       /* $06863E a5 -> -, a4 */
int SoundFlagLoop(uint16_t t, uint32_t *data);                       /* $068642 a5 -> -, a4 */
int SoundFlagCall(uint16_t t, uint32_t *data);                       /* $068646 a5 -> -, a4 */
int SoundFlagReturn(uint16_t t, uint32_t *data);                     /* $06864A a5 -> -, a4 */
int SoundFlagDivider(uint16_t t, uint32_t *data);                    /* $06864E a5 -> -, a4 */
int SoundFlagTranspose(uint16_t t, uint32_t *data);                  /* $068652 a5 -> -, a4 */
int SoundFlagModOn(uint16_t t, uint32_t *data);                      /* $068656 a5 -> -, a4 */
int SoundFlagModOff(uint16_t t, uint32_t *data);                     /* $06865A a5 -> -, a4 */
int SoundFlagFm3(uint16_t t, uint32_t *data);                        /* $06865E a5 -> -, a4 */

/* ---- clearing (clear.c) ---- */
void ScreenReset(void);                                              /* $0029EA */
void ScreenResetAlt(void);                                           /* $002A12 */
void ClearVideo(void);                                               /* $002A32 */
void ClearObjects(void);                                             /* $002A56 */
void ClearPalettes(void);                                            /* $002A6C */
void ClearBuffers(void);                                             /* $002AAA */
void VdpShadowRegs(void);                                            /* $002AD6 */
void VdpShadowRegsAlt(void);                                         /* $002ADE */
void ClearRamFF0000(void);                                           /* $002B68 */
void ClearRamFF8000(void);                                           /* $002B9C */
void ClearRamA000(void);                                             /* $002BB4 */
void ClearRamA200(void);                                             /* $002BE2 */
void ClearPlayers(void);                                             /* $002C5A */
void ClearRamA800(void);                                             /* $002C88 */
void ClearRamA800Head(void);                                         /* $002CA0 */
void ClearRamA900(void);                                             /* $002CB8 */
void ClearTileBuffer(void);                                          /* $002CE8 */
void ClearRamBE00(void);                                             /* $002D18 */
void ClearObjectRam(void);                                           /* $002D30 */
void ClearSprites(void);                                             /* $002D68 */
void ClearPaletteRam(void);                                          /* $002D80 */
void ClearHScroll(void);                                             /* $002D98 */
void ClearVScroll(void);                                             /* $002DB0 */
void ClearVram(void);                                                /* $002E28 */
void ClearVramC000(void);                                            /* $002EB8 */
void ClearVramD000(void);                                            /* $002EE8 */
void ClearVramE000(void);                                            /* $002F18 */
void ClearVramF000(void);                                            /* $002F48 */
void ClearVramF400(void);                                            /* $002F78 */

/* ---- sprites (sprites.c) ---- */
void BuildSprites(void);                                             /* $001C32 */
void SpritesStart(void);                                             /* $001E6E */
uint32_t Animate(uint16_t obj, uint32_t script);                     /* $001DE4 a5, a4 -> a4 */
uint32_t AnimateFrames(uint16_t obj, uint32_t script);               /* $001E10 a5, a4 -> a4 */
void DrawSprite(uint16_t obj, uint16_t *count, uint16_t *at);        /* $001EE2 a5 -> d4.w, a3 */
void DrawMapping(uint16_t obj, uint32_t map, uint16_t *count, uint16_t *at);   /* $001F50 a5, a4 -> d4.w, a3 */
void DrawMappingDma(uint16_t obj, uint32_t map, uint16_t *count, uint16_t *at, uint16_t *dma);   /* $002010 a5, a4 -> d4.w, a3, a2 */
void DrawMappingDma2(uint16_t obj, uint32_t map, uint16_t *count, uint16_t *at, uint16_t *dma);  /* $0020D6 a5, a4 -> d4.w, a3, a2 */
void DrawPlayerSprites(uint16_t obj, uint16_t *count, uint16_t *at); /* $001CBA a5 -> d4.w, a3 */
void DrawObjectSprites(uint16_t obj, uint16_t *count, uint16_t *at); /* $001D1A a5 -> d4.w, a3 */
void QueueDmaBlock(uint16_t *q, uint32_t src, uint16_t *vram);       /* $001B86 a1 -> a0, d0.w */
void SelectAnimation(uint16_t obj, uint32_t table);                  /* $0021E8 a5, a0 */
void DrawSpriteList(uint32_t list);                                  /* $00222C a0 */

/* ---- the modes (game.c) ---- */
void GameFrame(void);                                                /* $000CCA a6=reg_a6, a7=reg_sp -> a6=reg_a6 */
void Mode0(void);                                                    /* $00048C */
void StageStart(void);                                               /* $003900 */
void StageFrame(void);                                               /* $00391C */

/* ---- text (text.c) ---- */
void DrawText(uint32_t list);                                        /* $00187C a0 */
void DrawTextPlain(uint32_t list, uint16_t attr);                    /* $0018F8 a0, d0.w */

/* ---- arithmetic (math.c) ---- */
uint16_t Atan2(int16_t dx, uint32_t *d1);                            /* $003202 d0.w -> d2.w, d1 */
void AddScore(uint16_t obj, uint32_t points);                        /* $0035D6 a5, d0 */
void AddBcd(uint16_t end, uint32_t points);                          /* $003602 a0, d0 */

/* ---- the SEGA logo (logo.c) ---- */
void Mode4(void);                                                    /* $01B45E -> a6=reg_a6 */
void Mode4Start(void);                                               /* $01B478 */
void Mode4Shrink(void);                                              /* $01B4F2 */
void Mode4Show(void);                                                /* $01B508 */
void Mode4Animate(void);                                             /* $01B52E -> a6=reg_a6 */
void LogoAnimStart(void);                                            /* $020256 */
void LogoAnimEnd(void);                                              /* $020270 */
void ClearLineTable(void);                                           /* $020360 */
void LogoStepParams(uint16_t *acc, uint16_t *frac, uint16_t *stride, uint16_t *count);/* $0205A4 -> d5.w, d6.w, d4.w, d7.w */
uint16_t LogoNext(uint16_t *acc, uint16_t frac);                     /* $02044A d6.w -> d0.w, d5.w */
void LogoCoords(uint16_t *acc, uint16_t frac, uint16_t *x, uint16_t *y);/* $02043C d6.w -> d5.w, d2.w, d3.w */
uint16_t LogoSources(uint16_t x, uint16_t y, uint32_t *a, uint32_t *b);/* $020460 d2.w, d3.w -> d0.w, a0, a1 */
void LogoCopyHL(uint16_t rows, uint16_t stride, uint32_t *a, uint32_t *b, uint16_t *dst);/* $0204C4 d2.w, d4.w -> a0, a1, a2 */
void LogoCopyLL(uint16_t rows, uint16_t stride, uint32_t *a, uint32_t *b, uint16_t *dst);/* $0204FC d2.w, d4.w -> a0, a1, a2 */
void LogoCopyHH(uint16_t rows, uint16_t stride, uint32_t *a, uint32_t *b, uint16_t *dst);/* $020532 d2.w, d4.w -> a0, a1, a2 */
void LogoCopyLH(uint16_t rows, uint16_t stride, uint32_t *a, uint32_t *b, uint16_t *dst);/* $02056C d2.w, d4.w -> a0, a1, a2 */
void LogoCopy(uint16_t which, uint16_t stride, uint32_t a, uint32_t b, uint16_t dst);/* $0204A8 d0.w, d4.w, a0, a1, a2 */
void LogoShrinkStep(void);                                           /* $0201C2 */
void LogoStretch(void);                                              /* $020374 */
void LogoBrighten(void);                                             /* $020646 */
void LogoGrow(void);                                                 /* $0202B0 -> a6=reg_a6 */
void LogoHold(void);                                                 /* $0202D8 -> a6=reg_a6 */
void LogoShrinkBack(void);                                           /* $0202F6 -> a6=reg_a6 */
void LogoBounce(void);                                               /* $020314 -> a6=reg_a6 */
void LogoFade(void);                                                 /* $020340 -> a6=reg_a6 */

/* ---- the Treasure logo (treasure.c) ---- */
void Mode48(void);                                                   /* $062892 a7=reg_sp -> a6=reg_a6 */
void Mode48Start(void);                                              /* $0633B0 */
void Mode48Init(void);                                               /* $0628BC */
void Mode48Approach(void);                                           /* $062902 a7=reg_sp -> a6=reg_a6 */
void Mode48Unfold(void);                                             /* $062958 a7=reg_sp -> a6=reg_a6 */
void Mode48Settle(void);                                             /* $06299C a7=reg_sp -> a6=reg_a6 */
void Mode48Morph(void);                                              /* $062A74 a7=reg_sp -> a6=reg_a6 */
void Mode48Hold(void);                                               /* $062AE0 */
void TreasureDraw(void);                                             /* $0629B0 a7=reg_sp -> a6=reg_a6 */
void TreasurePalette(void);                                          /* $0629D4 */
void TreasureSnapshot(void);                                         /* $062A10 */
int16_t TreasureSinCos(uint16_t angle, int16_t *sin);                /* $063392 d0.w -> d0.w, d1.w */
void TreasureSineTable(void);                                        /* $063366 */
void TreasureSolidTiles(void);                                       /* $063408 */
void TreasureQueueDma(uint32_t tmpl);                                /* $06343E a0 */
void TreasureClear(void);                                            /* $06347C -> a6=reg_a6 */
void TreasureQueue(void);                                            /* $06349A a6=reg_a6 */
void TreasureSprites(void);                                          /* $0634C8 */
void TreasureCopyFill(void);                                         /* $062F64 */
void TreasureBlend(uint16_t t, uint16_t from, uint32_t to, uint16_t dst);/* $062F40 d5.w, a2, a3, a5 */
void TreasureLerp(uint16_t t, uint16_t *from, uint32_t *to, uint16_t *dst);/* $062F50 d5.w -> a2, a3, a5 */
void TreasureFadeList(uint16_t t, uint32_t list, uint16_t dst, uint16_t from);/* $062D78 d4.w, a1, a2, a3 */
void TreasureMixEntry(uint16_t t, uint16_t dst, uint32_t *list);     /* $062D8A d4.w, a2 -> a1 */
void TreasureMix(uint16_t from, uint16_t t, uint16_t at, uint16_t dst, uint32_t *list);/* $062D8E d3.w, d4.w, d5.w, a2 -> a1 */
uint16_t TreasureMixComponent(uint16_t mask, uint16_t from, uint16_t t, uint16_t to);/* $062DAE d2.w, d3.w, d4.w, d6.w -> d0.w */
void TreasureMixLine(uint16_t from, uint16_t t, uint16_t dst, uint32_t *list);/* $062DC2 d3.w, d4.w, a2 -> a1 */
void TreasureTransform(uint16_t dst, uint32_t model);                /* $062DD4 a1, a4 */
void TreasureProject(uint32_t px, uint32_t pz, uint32_t turn, uint16_t *dst, uint32_t *model);/* $062E68 d4, d5, d7 -> a1, a4 */
void TreasureDrawPolys(uint32_t list, uint16_t verts);               /* $062ED2 a4, a5, a6=reg_a6, a7=reg_sp -> a6=reg_a6 */
void TreasureFillRam(uint16_t first, uint16_t end);                  /* $062FCC a0, a1, a6=reg_a6 -> a6=reg_a6 */
/* the fill, in RAM (TreasureCopyFill) */
void TreasureFill(uint16_t first, uint16_t end);                     /* $FF2058 a0, a1, a6=reg_a6 -> a6=reg_a6 */

/* ---- the players (player.c) ---- */
void PlayersUpdate(void);                                            /* $00F0E2 */
void PlayerRemove(uint16_t obj);                                     /* $00F12E a5 */
void PlayerClampVitality(uint16_t obj);                              /* $00F146 a5 */
void PlayerFrame(uint16_t obj);                                      /* $00F158 a5 */
void PlayerMotionInput(uint16_t obj);                                /* $00F194 a5 */
void MotionStart(uint16_t obj);                                      /* $00F1A8 a5 */
void MotionSecond(uint16_t obj);                                     /* $00F1D0 a5 */
void MotionWait(uint16_t obj);                                       /* $00F1FC a5 */
void PlayerInput(uint16_t obj);                                      /* $00F212 a5 */
void PlayerInputPad(uint16_t obj);                                   /* $00F24C a5 */
void PlayerInputNone(uint16_t obj);                                  /* $00F264 a5 */
void ReadPlayerPad1(uint16_t obj);                                   /* $00F272 a5 */
void ReadPlayerPad2(uint16_t obj);                                   /* $00F286 a5 */
void PlayerInputDemo(uint16_t obj);                                  /* $00F29A a5 */
void PlayerInputWalkTo(uint16_t obj);                                /* $00F32C a5 */
void PlayerInputC(uint16_t obj);                                     /* $00F39E a5 */
void PlayerInputDirections(uint16_t obj);                            /* $00F404 a5 */
void PlayerInput14(uint16_t obj);                                    /* $00F434 a5 */
void PlayerInput18(uint16_t obj);                                    /* $00F478 a5 */
void PlayerInput1C(uint16_t obj);                                    /* $00F498 a5 */
void PlayerInput20(uint16_t obj);                                    /* $00F4D2 a5 */
void PlayerSceneType(uint16_t obj);                                  /* $00F4EC a5 */
void PlayerWeaponButton(uint16_t obj);                               /* $00F500 a5 */
void PlayerFloorKind(uint16_t obj);                                    /* $00F5D0 a5 */
void PlayerCollide(uint16_t obj);                                    /* $01212A a5 */
void CollideNone(uint16_t obj);                                      /* $01218E a5 */
void CollideGround(uint16_t obj);                                    /* $012190 a5 */
void CollideAir(uint16_t obj);                                       /* $01219C a5 */
void CollideAirNoGrab(uint16_t obj);                                 /* $0121C6 a5 */
void CollideHanging(uint16_t obj);                                   /* $0121D6 a5 */
void PlayerWallsFeet(uint16_t obj);                                  /* $01220C a5 */
void PlayerWalls(uint16_t obj);                                      /* $0122A0 a5 */
void PlayerWallsHead(uint16_t obj);                                  /* $012324 a5 */
void PlayerFloor3(uint16_t obj);                                     /* $0123BC a5 */
void PlayerFloor2(uint16_t obj);                                     /* $01241A a5 */
void PlayerFloor1(uint16_t obj);                                     /* $012464 a5 */
void PlayerKnockTime(uint16_t obj);                                  /* $012B8E a5 */
void PlayerCharacter(uint16_t obj);                                  /* $00EFF6 a5 */
void PlayerInit(uint16_t obj);                                       /* $00EE94 a5 */
void Player1Init(void);                                              /* $00EE50 */
void Player2Init(void);                                              /* $00EE72 */
void PlayersInit(void);                                              /* $00EE2A */
void PlayerUpAgain(uint16_t obj);                                    /* $012B78 a5 */
void PlayerLimits(uint16_t obj);                                     /* $012BA0 a5 */
void PlayerLimitsX(uint16_t obj);                                    /* $012BB4 a5 */
void PlayerLimitsY(uint16_t obj);                                    /* $012C02 a5 */
void PlayerSetAnim(uint16_t obj);                                    /* $016BF6 a5 */
void ObjectClear(uint16_t obj);                                      /* $01ADB0 a5 */

/* ---- the players' states (player_states.c) ---- */
void PlayerStates(uint16_t obj);                                     /* $00F64E a5 */
void PlayerKnockTimer(uint16_t obj);                                 /* $00F782 a5 */
void PlayerKnockSide(uint16_t obj, uint16_t by);                     /* $00F7D4 a5, a0 */
void PlayerPickUp(uint16_t obj);                                     /* $00F86A a5 */
void PlayerHidden(uint16_t obj);                                     /* $00F94E a5 */
void PlayerStart(uint16_t obj);                                      /* $00F97C a5 */
void PlayerStand(uint16_t obj);                                      /* $00F9AC a5 */
void PlayerShootStand(uint16_t obj);                                 /* $00FABC a5 */
void PlayerTurning(uint16_t obj);                                    /* $00FC0E a5 */
void PlayerCrouch(uint16_t obj);                                     /* $00FD0A a5 */
void PlayerShootCrouch(uint16_t obj);                                /* $00FDCC a5 */
void PlayerTurningCrouched(uint16_t obj);                            /* $00FF0A a5 */
void PlayerWalk(uint16_t obj);                                       /* $00FFD2 a5 */
void PlayerWalkSpeed(uint16_t obj);                                  /* $0100C2 a5 */
void PlayerShootWalk(uint16_t obj);                                  /* $0100FA a5 */
void PlayerDash(uint16_t obj);                                       /* $0101EA a5 */
void PlayerDashSpeed(uint16_t obj);                                  /* $010256 a5 */
void PlayerGuard(uint16_t obj);                                      /* $0102A8 a5 */
void PlayerPunch(uint16_t obj);                                      /* $010380 a5 */
void PlayerPunchAir(uint16_t obj);                                   /* $01042E a5 */
void PlayerJump(uint16_t obj);                                       /* $0104D6 a5 */
void PlayerJumpHurt(uint16_t obj);                                   /* $010540 a5 */
void PlayerJumpDir(uint16_t obj);                                    /* $010552 a5 */
void PlayerFall(uint16_t obj);                                       /* $0105E0 a5 */
void PlayerFallDir(uint16_t obj);                                    /* $010658 a5 */
void PlayerShootJump(uint16_t obj);                                  /* $0106F2 a5 */
void PlayerShootFall(uint16_t obj);                                  /* $01078C a5 */
void PlayerAirAttack(uint16_t obj);                                  /* $010874 a5 */
void PlayerHitSpark(uint16_t obj);                                   /* $010938 a5 */
void PlayerAirAttackBox(uint16_t obj);                               /* $010960 a5 */
void PlayerAirAttackSpeed(uint16_t obj);                             /* $0109B4 a5 */
void PlayerWallJumpSpeed(uint16_t obj);                              /* $0109DC a5 */
void PlayerOnWall(uint16_t obj);                                     /* $010A50 a5 */
void PlayerFaceFromWall(uint16_t obj);                               /* $010A88 a5 */
void PlayerSlideSpeed(uint16_t obj);                                 /* $010AEA a5 */
void PlayerSlide(uint16_t obj);                                      /* $010B08 a5 */
void PlayerAttackPause(uint16_t obj);                                /* $010B7C a5 */
void PlayerHang(uint16_t obj);                                       /* $010BE2 a5 */
void PlayerHangShoot(uint16_t obj);                                  /* $010C92 a5 */
void PlayerClimb(uint16_t obj);                                      /* $010D4A a5 */
void PlayerDrop(uint16_t obj);                                       /* $010DFE a5 */
void PlayerHurt(uint16_t obj);                                       /* $010E6A a5 */
void PlayerHurtSlope(uint16_t obj);                                  /* $010EBE a5 */
void PlayerHurtHanging(uint16_t obj);                                /* $010F00 a5 */
void PlayerDown(uint16_t obj);                                       /* $010F56 a5 */
void PlayerKnockSpeed(uint16_t obj);                                 /* $010F70 a5 */
void PlayerHurtAir(uint16_t obj);                                    /* $010FDA a5 */
void PlayerKnocked(uint16_t obj);                                    /* $01106C a5 */
void PlayerLying(uint16_t obj);                                      /* $0110B2 a5 */
void PlayerGettingUp(uint16_t obj);                                  /* $01110C a5 */
void PlayerDead(uint16_t obj);                                       /* $01115A a5 */
void PlayerFallenOut(uint16_t obj);                                  /* $0111E8 a5 */
void PlayerComingBack(uint16_t obj);                                 /* $011240 a5 */
void PlayerThrow(uint16_t obj);                                      /* $0112AC a5 */
void PlayerThrowAir(uint16_t obj);                                   /* $011350 a5 */
void PlayerHeldByEnemy(uint16_t obj);                                /* $0113C2 a5 */
void PlayerHeldByPlayer(uint16_t obj);                               /* $011440 a5 */
void PlayerThrownByPlayer(uint16_t obj);                             /* $0114CC a5 */
void PlayerThrownByEnemy(uint16_t obj);                              /* $011534 a5 */
void PlayerStandAnim(uint16_t obj);                                  /* $0115F0 a5 */
void PlayerWalkAnim(uint16_t obj);                                   /* $011AD4 a5 */
int PlayerAttackDir(uint16_t obj);                                   /* $012922 a5 -> !Z */
void PlayerAim(uint16_t obj);                                        /* $012A96 a5 */
void PlayerAimPad(uint16_t obj);                                     /* $012A9A a5 */
void PlayerAimStraight(uint16_t obj);                                /* $012AD6 a5 */
void PlayerFace(uint16_t obj);                                       /* $012AF2 a5 */
void PlayerAttackPower(uint16_t obj);                                /* $012C66 a5 */
int ObjectFindEffect(uint16_t obj, uint16_t *left, uint16_t *slot);  /* $01A21C a5 -> Z, d2.w, a4 */

/* ---- the players' movement (player_move.c) ---- */
void PlayerMove(uint16_t obj);                                       /* $011560 a5 */
void PlayerPushed(uint16_t obj);                                     /* $0115CC a5 */
void MoveNone(uint16_t obj);                                         /* $0115E6 a5 */
void MoveStand(uint16_t obj);                                        /* $0115E8 a5 */
void PlayerSlopeSpeed(uint16_t obj);                                 /* $011616 a5 */
void MoveStill(uint16_t obj);                                        /* $011644 a5 */
void MoveNone3(uint16_t obj);                                        /* $01164A a5 */
void MoveShootStand(uint16_t obj);                                   /* $01164C a5 */
void AimFlat(uint16_t obj);                                          /* $01166A a5 */
void AimFlatAnim(uint16_t obj, uint16_t i);                          /* $0116BC a5, d0.w */
void AimSlopeUp(uint16_t obj);                                       /* $011764 a5 */
void AimSlopeUpAnim(uint16_t obj, uint16_t i);                       /* $0117CC a5, d0.w */
void AimSlopeDown(uint16_t obj);                                     /* $011864 a5 */
void AimSlopeDownAnim(uint16_t obj, uint16_t i);                     /* $0118CC a5, d0.w */
void MoveNone2(uint16_t obj);                                        /* $0119A6 a5 */
void MoveShootCrouch(uint16_t obj);                                  /* $0119A8 a5 */
void MoveTurn(uint16_t obj);                                         /* $011A10 a5 */
void MoveDash(uint16_t obj);                                         /* $011A3C a5 */
void PlayerFriction(uint16_t obj);                                   /* $011A50 a5 */
void MoveWalk(uint16_t obj);                                         /* $011A7A a5 */
void PlayerSlopeVy(uint16_t obj);                                    /* $011A7E a5 */
void SlopeVyFlat(uint16_t obj);                                      /* $011AA0 a5 */
void SlopeVyRising(uint16_t obj);                                    /* $011AA6 a5 */
void SlopeVyRisingHalf(uint16_t obj);                                /* $011AB2 a5 */
void SlopeVyFalling(uint16_t obj);                                   /* $011AC0 a5 */
void SlopeVyFallingHalf(uint16_t obj);                               /* $011AC8 a5 */
void MoveShootWalk(uint16_t obj);                                    /* $011B30 a5 */
void AimWalk(uint16_t obj);                                          /* $011B3C a5 */
void AimNone(uint16_t obj);                                          /* $011B84 a5 */
void AimWalkFlat(uint16_t obj);                                      /* $011B86 a5 */
void AimWalkAnim(uint16_t obj, uint16_t i);                          /* $011BDC a5, d0.w */
void AimWalkLegs(uint16_t obj);                                      /* $011C84 a5 */
void MoveAir2(uint16_t obj);                                         /* $011CBA a5 */
void MoveHurtAir(uint16_t obj);                                      /* $011CBE a5 */
void MoveJump(uint16_t obj);                                         /* $011CCC a5 */
void PlayerJumpAnim(uint16_t obj);                                   /* $011CD0 a5 */
void MoveFall(uint16_t obj);                                         /* $011CE6 a5 */
void MoveAir(uint16_t obj);                                          /* $011D14 a5 */
void PlayerGravity(uint16_t obj);                                    /* $011D18 a5 */
void PlayerSteer(uint16_t obj);                                      /* $011D4A a5 */
void MoveShootAir(uint16_t obj);                                     /* $011D8C a5 */
void AimAirAnim(uint16_t obj, uint16_t i);                           /* $011DE0 a5, d0.w */
void MoveGravity2(uint16_t obj);                                     /* $011E88 a5 */
void MoveSlide(uint16_t obj);                                        /* $011E92 a5 */
void PlayerSlideFriction(uint16_t obj);                              /* $011EA0 a5 */
void MoveHang(uint16_t obj);                                         /* $011EC6 a5 */
void HangAnim(uint16_t obj);                                         /* $011ED2 a5 */
void HangSpeed(uint16_t obj);                                        /* $011EFE a5 */
void MoveShootHang(uint16_t obj);                                    /* $011F2C a5 */
void AimHangAnim(uint16_t obj, uint16_t i);                          /* $011F7E a5, d0.w */
void MoveNone4(uint16_t obj);                                        /* $012026 a5 */
void MoveNone5(uint16_t obj);                                        /* $012028 a5 */
void MoveGravity(uint16_t obj);                                      /* $01202A a5 */
void PlayerFreeAimGun(uint16_t obj);                                 /* $011964 a5 */
void MoveThrow(uint16_t obj);                                        /* $012034 a5 */
void PlayerPlaceHeld(uint16_t obj, uint16_t held);                   /* $0120BA a5, a2 */
void MoveNone6(uint16_t obj);                                        /* $01211E a5 */
void MoveGravity3(uint16_t obj);                                     /* $012120 a5 */
void PlayerSlideDust(uint16_t obj);                                  /* $012970 a5 */
void PlayerSlopeDust(uint16_t obj);                                  /* $01299A a5 */
void PlayerDust(uint16_t obj, uint16_t e);                           /* $0129EC a5, a0 */
void PlayerSplash(uint16_t obj);                                     /* $012A24 a5 */
int PlayerFindEffect(uint16_t obj, uint16_t *slot);                  /* $0189C6 a5 -> Z, a0 */

/* ---- the weapons (weapons.c) ---- */
void PlayerShoot(uint16_t obj);                                      /* $01776A a5 */
void WeaponFire(uint16_t obj);                                       /* $0177EC a5 */
void WeaponSecond(uint16_t obj);                                     /* $01781A a5 */
void WeaponNone(uint16_t obj);                                       /* $017842 a5 */
void WeaponWithForce(uint16_t obj);                                  /* $01784C a5 */
void WeaponWithLightning(uint16_t obj);                              /* $017874 a5 */
void WeaponWithFire(uint16_t obj);                                   /* $01789C a5 */
void WeaponWithChaser(uint16_t obj);                                 /* $0178C4 a5 */
void WeaponForce(uint16_t obj);                                      /* $0178EC a5 */
void ForceStart(uint16_t obj);                                       /* $017906 a5 */
void ForceHold(uint16_t obj);                                        /* $017918 a5 */
void ForceFire(uint16_t obj);                                        /* $017942 a5 */
void ForceRelease(uint16_t obj);                                     /* $0179A0 a5 */
void WeaponAim(uint16_t obj);                                        /* $0179C6 a5 */
void WeaponAimTurn(uint16_t obj, uint16_t step);                     /* $017A18 a5, d7.w */
void WeaponTurn(uint16_t obj, uint16_t step);                        /* $017A80 a5, d7.w */
void WeaponFaceAim(uint16_t obj);                                    /* $017AF6 a5 */
void WeaponForceForce(uint16_t obj);                                 /* $017B22 a5 */
void ForceForceStart(uint16_t obj);                                  /* $017B3C a5 */
void ForceForceHold(uint16_t obj);                                   /* $017B4E a5 */
void ForceForceFire(uint16_t obj);                                   /* $017B78 a5 */
void ForceForceRelease(uint16_t obj);                                /* $017BD6 a5 */
void WeaponChaser(uint16_t obj);                                     /* $017BFC a5 */
void ChaserStart(uint16_t obj);                                      /* $017C16 a5 */
void ChaserHold(uint16_t obj);                                       /* $017C28 a5 */
void ChaserFire(uint16_t obj);                                       /* $017C52 a5 */
void ChaserRelease(uint16_t obj);                                    /* $017CB0 a5 */
void WeaponChaserChaser(uint16_t obj);                               /* $017CD6 a5 */
void ChaserChaserStart(uint16_t obj);                                /* $017CF0 a5 */
void ChaserChaserHold(uint16_t obj);                                 /* $017D02 a5 */
void ChaserChaserFire(uint16_t obj);                                 /* $017D2C a5 */
void ChaserChaserRelease(uint16_t obj);                              /* $017D8A a5 */
void WeaponForceChaser(uint16_t obj);                                /* $017DB0 a5 */
void ForceChaserStart(uint16_t obj);                                 /* $017DCA a5 */
void ForceChaserHold(uint16_t obj);                                  /* $017DDC a5 */
void ForceChaserFire(uint16_t obj);                                  /* $017E0A a5 */
void ForceChaserRelease(uint16_t obj);                               /* $017E78 a5 */
void ChaserTarget(uint16_t obj);                                     /* $017E9E a5 */
void WeaponLightning(uint16_t obj);                                  /* $017ED0 a5 */
void LightningStart(uint16_t obj);                                   /* $017EEE a5 */
void LightningHold(uint16_t obj);                                    /* $017F00 a5 */
void LightningWait(uint16_t obj);                                    /* $017F2E a5 */
void LightningFire(uint16_t obj);                                    /* $017F50 a5 */
void LightningRelease(uint16_t obj);                                 /* $017F9C a5 */
void WeaponLightningChaser(uint16_t obj);                            /* $017FC2 a5 */
void LightningChaserStart(uint16_t obj);                             /* $017FE0 a5 */
void LightningChaserHold(uint16_t obj);                              /* $017FF2 a5 */
void LightningChaserWait(uint16_t obj);                              /* $018024 a5 */
void LightningChaserFire(uint16_t obj);                              /* $018048 a5 */
void LightningChaserRelease(uint16_t obj);                           /* $0180B4 a5 */
void LightningChaserSound(uint16_t obj);                             /* $0180DA a5 */
void WeaponLightningFire(uint16_t obj);                              /* $0180F8 a5 */
void LightningFireStart(uint16_t obj);                               /* $018116 a5 */
void LightningFireHold(uint16_t obj);                                /* $018128 a5 */
void LightningFireWait(uint16_t obj);                                /* $018150 a5 */
void LightningFireFire(uint16_t obj);                                /* $018176 a5 */
void LightningFireRelease(uint16_t obj);                             /* $0181DE a5 */
void WeaponLightningLightning(uint16_t obj);                         /* $018204 a5 */
void LightningLightningStart(uint16_t obj);                          /* $01821E a5 */
void LightningLightningHold(uint16_t obj);                           /* $018230 a5 */
void LightningLightningFire(uint16_t obj);                           /* $018260 a5 */
void LightningLightningRelease(uint16_t obj);                        /* $0182C4 a5 */
void WeaponForceLightning(uint16_t obj);                             /* $0182EA a5 */
void ForceLightningStart(uint16_t obj);                              /* $018304 a5 */
void ForceLightningHold(uint16_t obj);                               /* $018316 a5 */
void ForceLightningFire(uint16_t obj);                               /* $018346 a5 */
void ForceLightningRelease(uint16_t obj);                            /* $01839E a5 */
void WeaponFlame(uint16_t obj);                                      /* $0183C4 a5 */
void FlameStart(uint16_t obj);                                       /* $0183E2 a5 */
void FlameHold(uint16_t obj);                                        /* $0183F4 a5 */
void FlameWait(uint16_t obj);                                        /* $01841C a5 */
void FlameFire(uint16_t obj);                                        /* $01844A a5 */
void FlameRelease(uint16_t obj);                                     /* $0184D0 a5 */
void FlameSound(uint16_t obj);                                       /* $0184F6 a5 */
uint16_t FlameSpacing(uint16_t k);                                   /* $018514 d0.w -> d0.w */
void FlameTrail(uint16_t obj);                                       /* $01852C a5 */
void WeaponFireFire(uint16_t obj);                                   /* $018576 a5 */
void FireFireStart(uint16_t obj);                                    /* $018594 a5 */
void FireFireHold(uint16_t obj);                                     /* $0185A6 a5 */
void FireFireWait(uint16_t obj);                                     /* $0185CE a5 */
void FireFireFire(uint16_t obj);                                     /* $0185FC a5 */
void FireFireRelease(uint16_t obj);                                  /* $018682 a5 */
void FireFireSound(uint16_t obj);                                    /* $0186A8 a5 */
void WeaponForceFire(uint16_t obj);                                  /* $0186C6 a5 */
void ForceFireStart(uint16_t obj);                                   /* $0186E0 a5 */
void ForceFireHold(uint16_t obj);                                    /* $0186F2 a5 */
void ForceFireFire(uint16_t obj);                                    /* $018728 a5 */
void ForceFireRelease(uint16_t obj);                                 /* $018832 a5 */
void WeaponChaserFire(uint16_t obj);                                 /* $018858 a5 */
void ChaserFireStart(uint16_t obj);                                  /* $01887A a5 */
void ChaserFireHold(uint16_t obj);                                   /* $01888C a5 */
void ChaserFireWait(uint16_t obj);                                   /* $0188B4 a5 */
void ChaserFireFire(uint16_t obj);                                   /* $0188E2 a5 */
void ChaserFireHolding(uint16_t obj);                                /* $018958 a5 */
void ChaserFireRelease(uint16_t obj);                                /* $018982 a5 */
void ChaserFireSound(uint16_t obj);                                  /* $0189A8 a5 */
int ShotSlot(uint16_t obj, uint16_t *slot);                          /* $0189F6 a5 -> Z, a0 */
int ShotLingering(uint16_t obj);                                     /* $018A70 a5 -> !Z */
int ShotSlotsFree(uint16_t obj);                                     /* $018ACC a5 -> Z */
void ShotMuzzle(uint16_t obj);                                       /* $018B08 a5 */
void MuzzleFlash(uint16_t obj);                                      /* $018B3A a5 */
void MuzzlePriority(uint16_t obj, uint16_t m);                       /* $018BC2 a5, a4 */
void ShotPriority(uint16_t obj, uint16_t s);                         /* $018BD4 a5, a0 */

/* ---- the players in the scenes (scenes.c) ---- */
void ScenePlayer1(uint16_t obj);                                     /* $020DDA a5 */
void ScenePlayer2(uint16_t obj);                                     /* $020E14 a5 */
void SceneTitle1(uint16_t obj);                                      /* $020E4E a5 */
void SceneTitle2(uint16_t obj);                                      /* $020EAA a5 */
void SceneRun1(uint16_t obj);                                        /* $020F06 a5 */
void SceneRun1Start(uint16_t obj);                                   /* $020F20 a5 */
void SceneRun1Running(uint16_t obj);                                 /* $020F5A a5 */
void SceneRun1Stopped(uint16_t obj);                                 /* $020F82 a5 */
void SceneRun1Posed(uint16_t obj);                                   /* $020F9E a5 */
void SceneRun2(uint16_t obj);                                        /* $020FA0 a5 */
void SceneRun2Start(uint16_t obj);                                   /* $020FBA a5 */
void SceneRun2Running(uint16_t obj);                                 /* $020FF4 a5 */
void SceneRun2Stopped(uint16_t obj);                                 /* $02101C a5 */
void SceneRun2Posed(uint16_t obj);                                   /* $021038 a5 */
void SceneWait1(uint16_t obj);                                       /* $02103A a5 */
void SceneWait1Start(uint16_t obj);                                  /* $021054 a5 */
void SceneWait1First(uint16_t obj);                                  /* $021090 a5 */
void SceneWait1Pose(uint16_t obj);                                   /* $0210B4 a5 */
void SceneWait1End(uint16_t obj);                                    /* $0210D2 a5 */
void SceneWait2(uint16_t obj);                                       /* $0210D8 a5 */
void SceneWalkIn1(uint16_t obj);                                     /* $021146 a5 */
void SceneWalkIn1Start(uint16_t obj);                                /* $02116A a5 */
void SceneWalkIn1Walking(uint16_t obj);                              /* $0211A8 a5 */
void SceneWalkIn1Waiting(uint16_t obj);                              /* $0211C4 a5 */
void SceneWalkIn1On(uint16_t obj);                                   /* $0211E6 a5 */
void SceneWalkIn2(uint16_t obj);                                     /* $0211F6 a5 */
void SceneWalkIn2Start(uint16_t obj);                                /* $02121A a5 */
void SceneWalkIn2Walking(uint16_t obj);                              /* $021258 a5 */
void SceneWalkIn2Waiting(uint16_t obj);                              /* $021274 a5 */
void SceneWalkIn2On(uint16_t obj);                                   /* $021296 a5 */
void SceneJumpIn1(uint16_t obj);                                     /* $0212A6 a5 */
void SceneJumpIn2(uint16_t obj);                                     /* $021332 a5 */
void SceneTakeItem(uint16_t obj);                                    /* $0213BE a5 */
void SceneTakeStart(uint16_t obj, uint32_t item);                    /* $0213E6 a5, a4 */
void SceneTakeWalk(uint16_t obj, uint32_t item);                     /* $0213F8 a5, a4 */
void SceneTakeTaking(uint16_t obj, uint32_t item);                   /* $021452 a5, a4 */
void SceneTakeTaken(uint16_t obj, uint32_t item);                    /* $021480 a5, a4 */
uint32_t SceneItem(void);                                            /* $02148A -> a4 */
void SceneAfterItem(uint16_t obj);                                   /* $0214A4 a5 */
void SceneAfterStart(uint16_t obj);                                  /* $0214C4 a5 */
void SceneAfterWait(uint16_t obj);                                   /* $0214D6 a5 */
void SceneAfterEnd(uint16_t obj);                                    /* $0214E8 a5 */
void SceneCue(uint16_t obj);                                         /* $0214EA a5 */
void SceneCueStart(uint16_t obj);                                    /* $02150E a5 */
void SceneCueWait(uint16_t obj);                                     /* $021524 a5 */
void SceneCueGesture(uint16_t obj);                                  /* $021538 a5 */
void SceneCueEnd(uint16_t obj);                                      /* $02154A a5 */
void SceneAnimRun(uint16_t obj);                                     /* $02154C a5 */
void SceneAnimStand(uint16_t obj);                                   /* $021570 a5 */
void SceneAnimTake(uint16_t obj);                                    /* $021594 a5 */
void SceneAnimPose(uint16_t obj);                                    /* $0215B8 a5 */
void SceneAnimGesture(uint16_t obj);                                 /* $0215DC a5 */
void SceneVoice(uint16_t obj);                                       /* $021600 a5 */
void SceneRed(uint16_t obj);                                         /* $02161C a5 */
void SceneBlue(uint16_t obj);                                        /* $02162A a5 */

/* ---- the engine's frame (engine.c) ---- */
void CameraMove(void);                                               /* $00A630 */
void DrawListStart(void);                                            /* $01F65E */
void DrawListEnd(void);                                              /* $01F66E */
void ObjectsMove(void);                                              /* $01F6F6 */
void PlayerMoveLimited(uint16_t obj, uint8_t f);                     /* $01F862 a5, d2.b */
void PlayerMoveWithFloor(uint16_t obj, uint8_t f, uint16_t dx, uint16_t dy);/* $01F8C0 a5, d2.b, d0.w, d1.w */
void ScreenShake(void);                                              /* $01F914 */
void EffectsUpdate(void);                                            /* $018BE6 */
void ObjectsUpdate(void);                                            /* $01ABC8 */
void ObjectsSpawn(void);                                             /* $01ACF0 */
void ObjectSpawn(uint32_t entry);                                    /* $01AD32 a4 */
int SlotFree(uint16_t from, uint16_t to, uint16_t *slot);            /* $01AD86 a5, a0 -> Z, a5 */
int SlotReusable(uint16_t from, uint16_t to, uint16_t *slot);        /* $01AD9A a5, a0 -> Z, a5 */

/* ---- the objects' collisions (collide.c) ---- */
void Collisions(void);                                               /* $00B188 */
void CollisionLists(void);                                           /* $00B1A6 */
void CollisionListsDrawn(uint16_t ends[8]);                          /* $00B250 manual */
void CollisionAdd(uint16_t obj, uint16_t ends[8]);                   /* $00B260 manual */
void CollideHurt(void);                                              /* $00B3F8 */
void CollideTouch(void);                                             /* $00B532 */
void CollideGrab(void);                                              /* $00B60E */
void SolidBox(uint16_t obj, uint16_t *top, uint16_t *bottom, uint16_t *left, uint16_t *right);/* $00B764 a5 -> d0.w, d1.w, d2.w, d3.w */
void SolidBoxHurt(uint16_t obj, uint16_t *top, uint16_t *bottom, uint16_t *left, uint16_t *right);/* $00B76C a5 -> d0.w, d1.w, d2.w, d3.w */
void SolidBoxPlayer(uint16_t obj, uint16_t *top, uint16_t *bottom, uint16_t *left, uint16_t *right);/* $00B770 a5 -> d0.w, d1.w, d2.w, d3.w */
void SolidPush(uint16_t obj, uint16_t solid);                        /* $00B78C a5, a4 */
void SolidPushBox(uint16_t obj, uint16_t solid);                     /* $00B798 a5, a4 */
void SolidPushPlayer(uint16_t obj, uint16_t solid);                  /* $00B79C a5, a4 */
void PlayerOnSolid(uint16_t obj, uint16_t solid);                    /* $012574 a5, a4 */
void SolidNone(uint16_t obj, uint16_t solid);                        /* $0125E2 a5, a4 */
void SolidGround(uint16_t obj, uint16_t solid);                      /* $0125E4 a5, a4 */
void SolidAir(uint16_t obj, uint16_t solid);                         /* $0125F0 a5, a4 */
void SolidAirNoGrab(uint16_t obj, uint16_t solid);                   /* $01261A a5, a4 */
void SolidHanging(uint16_t obj, uint16_t solid);                     /* $01262A a5, a4 */
void SolidSides(uint16_t obj, uint16_t solid);                       /* $01263C a5, a4 */
void SolidTop(uint16_t obj, uint16_t solid);                         /* $012710 a5, a4 */

/* ---- the planes' scrolling (scroll.c) ---- */
void ScrollRowA(void);                                               /* $00976C */
void ScrollRowAShort(void);                                          /* $009790 */
void ScrollColumnA(void);                                            /* $0097B4 */
void ScrollRowB(void);                                               /* $0097D8 */
void ScrollColumnB(void);                                            /* $0097F4 */
void ScrollRowBDma(void);                                            /* $009870 */
void ScrollColumnBDma(void);                                         /* $00988C */
uint16_t QueueVram(uint16_t q, uint16_t vram);                       /* $009DF0 a0, d2.w -> a0 */
uint16_t QueueSource(uint16_t q, uint32_t src);                      /* $009E0A a0, d2 -> a0 */
uint16_t ColumnGather(uint16_t col, uint16_t y, uint16_t to);        /* $009F8E d0.w, d1.w, a1 -> a1 */
void ColumnGather2(uint16_t col, uint16_t y, uint16_t *even, uint16_t *odd);/* $00A040 d0.w, d1.w -> a4, a1 */
void ScrollFrame(void);                                              /* $00A37A */
void ScrollPlaneAColumns(void);                                      /* $00A3B8 */
void ScrollPlaneB(void);                                             /* $00A3F6 */
void ScrollPlaneBColumns(void);                                      /* $00A418 */
void HScrollTable(void);                                             /* $00A434 */
void VScrollTable(void);                                             /* $00A53E */
void PlaneAFill(void);                                               /* $0098AC */
void PlaneBFill(void);                                               /* $009AD0 */
void PlaneADmaFirst(uint16_t top);                                   /* $0099D2 d4.w */
void PlaneADmaSecond(uint16_t top);                                  /* $009A4E d4.w */
void PlaneADma(void);                                                /* $009974 */
void PlaneBDma(void);                                                /* $009B94 */
void PlaneAFillAhead(void);                                          /* $00A6B2 */
void PlaneBFillAhead(void);                                          /* $00A6C8 */
void PlaneARedraw(void);                                             /* $00A692 */
void PlaneBRedraw(void);                                             /* $00A6A2 */

/* ---- the level's collision (map.c) ---- */
uint16_t MapAttribute(uint16_t x, uint16_t y);                       /* $00B95A d0.w, d1.w -> d2.w */
uint16_t MapBoundsY(uint16_t x, uint16_t *y);                       /* $00C31C d0.w -> d2.w, d1.w */
uint16_t MapAttr(uint16_t x, uint16_t y);                            /* $00B94A d0.w, d1.w -> d2.w */
uint16_t MapMarked(uint16_t x, uint16_t y);                          /* $00BB74 d0.w, d1.w -> d1.w */
uint16_t MapAttrHigh(uint16_t x, uint16_t y);                        /* $00BA4C d0.w, d1.w -> d2.w */
uint16_t MapFloor(uint16_t x, uint16_t y);                           /* $00BA74 d0.w, d1.w -> d2.w */
uint16_t MapThinFloor(uint16_t x, uint16_t y);                       /* $00BAA4 d0.w, d1.w -> d2.w */
uint16_t MapWallRight(uint16_t x, uint16_t y);                       /* $00BAFC d0.w, d1.w -> d2.w */
uint16_t MapWallLeft(uint16_t x, uint16_t y);                        /* $00BB34 d0.w, d1.w -> d2.w */
void MapSnap(uint16_t *x, uint16_t *y, uint16_t *attr);              /* $00BC2E -> d0.w, d1.w, d2.w */
uint16_t MapFloorTop(uint16_t y);                                    /* $00BD2E d1.w -> d1.w */
uint16_t MapFloorBottom(uint16_t y);                                 /* $00BD3C d1.w -> d1.w */
uint16_t MapSlopeUp(uint16_t x, uint16_t y, uint16_t *d2);           /* $00BD4C d0.w, d1.w -> d1.w, d2.w */
uint16_t MapSlopeUpLow(uint16_t x, uint16_t y, uint16_t *d2);        /* $00BD6A d0.w, d1.w -> d1.w, d2.w */
uint16_t MapSlopeUpHigh(uint16_t x, uint16_t y, uint16_t *d2);       /* $00BD8C d0.w, d1.w -> d1.w, d2.w */
uint16_t MapFloorMiddle(uint16_t y);                                 /* $00BDAC d1.w -> d1.w */
uint16_t MapFloorMiddleHigh(uint16_t y);                             /* $00BDBC d1.w -> d1.w */
uint16_t MapSlopeDown(uint16_t x, uint16_t y, uint16_t *d2);         /* $00BDCC d0.w, d1.w -> d1.w, d2.w */
uint16_t MapSlopeDownLow(uint16_t x, uint16_t y, uint16_t *d2);      /* $00BDE6 d0.w, d1.w -> d1.w, d2.w */
uint16_t MapSlopeDownHigh(uint16_t x, uint16_t y, uint16_t *d2);     /* $00BE04 d0.w, d1.w -> d1.w, d2.w */

/* ---- the stages' own frames (stages.c) ---- */
void CameraSpeed(uint32_t x, uint32_t y);                            /* $004188 d0, d1 */
void PlayersCentre(uint16_t *x, uint16_t *y);                        /* $008DEE -> d0.w, d1.w */
void ClampFour(uint16_t *x, uint16_t *y);                            /* $008EAE -> d0.w, d1.w */
void CameraStop(void);                                               /* $0041AC */
void CameraFollowRight(void);                                        /* $004094 */
void PlayersInFront(void);                                           /* $0041BE */
void PlayersFree(void);                                              /* $0041FE */
void PlayersFixed(void);                                             /* $00428C */
void StageSceneStart(void);                                          /* $004038 */
void StageSceneEnd(void);                                            /* $004068 */
void Stage0Frame(void);                                              /* $003B38 */
void Stage0Camera(void);                                             /* $003B40 */
void Stage0Cam00(void);                                              /* $003B7C */
void Stage0Cam04(void);                                              /* $003BAC */
void Stage0Cam08(void);                                              /* $003BBA */
void Stage0Cam0C(void);                                              /* $003C00 */
void Stage0Cam10(void);                                              /* $003C0E */
void Stage0Cam14(void);                                              /* $003C3E */
void Stage0Cam18(void);                                              /* $003C58 */
void Stage0Cam1C(void);                                              /* $003C7A */
void Stage0Cam20(void);                                              /* $003C8E */
void Stage0Cam24(void);                                              /* $003CB2 */
void Stage0Cam28(void);                                              /* $003D08 */
void Stage0Cam2C(void);                                              /* $003D40 */
void Stage0Cam30(void);                                              /* $003D82 */
void Stage0Cam34(void);                                              /* $003DDC */
void Stage0Cam38(void);                                              /* $003E08 */
void Stage0Cam3C(void);                                              /* $003E6A */
void Stage0Cam40(void);                                              /* $003E9C */
void Stage0Cam44(void);                                              /* $003EC6 */
void Stage0Cam48(void);                                              /* $003EEE */
void Stage0BossTiles(void);                                          /* $0040D8 */
void Stage0Cam4C(void);                                              /* $003F22 */
void Stage0Cam50(void);                                              /* $003F3E */
void Stage0Cam54(void);                                              /* $003F98 */
void Stage0Cam58(void);                                              /* $003FE6 */
void Stage0ShaftAttributes(void);                                    /* $0042BA */
void BackQuarterHalf(void);                                          /* $004440 */
void BackQuarterHalfCamera(void);                                    /* $00445A */
void BackStop(void);                                                 /* $00442E */
void BackLayersStart(void);                                          /* $004470 */
void Stage0Back00(void);                                             /* $00430A */
void Stage0Back04(void);                                             /* $004338 */
void Stage0Back08(void);                                             /* $004350 */
void Stage0Back0C(void);                                             /* $004388 */
void Stage0Back10(void);                                             /* $0043BA */
void Stage0Back14(void);                                             /* $0043E2 */
void Stage0Back18(void);                                             /* $00441C */
void Stage0Back1C(void);                                             /* $00442C */
void PlayersUnpin(void);                                             /* $00424E */
void PlayersPin(void);                                               /* $00422C */
void Stage1Frame(void);                                              /* $0047FC */
void Stage1Camera(void);                                             /* $004808 */
void Stage1Back(void);                                               /* $0055B8 */
void Stage1Cam00(void);                                              /* $004866 */
void Stage1Cam04(void);                                              /* $0048B0 */
void Stage1Cam08(void);                                              /* $004906 */
void Stage1Cam0C(void);                                              /* $00495C */
void Stage1Cam10(void);                                              /* $00496E */
void Stage1Cam14(void);                                              /* $004A1E */
void Stage1Cam18(void);                                              /* $004A44 */
void Stage1Cam1C(void);                                              /* $004A82 */
void Stage1Cam20(void);                                              /* $004AC4 */
void Stage1Cam24(void);                                              /* $004B0A */
void Stage1Cam28(void);                                              /* $004B3C */
void Stage1Cam2C(void);                                              /* $004B62 */
void Stage1Cam30(void);                                              /* $004BA0 */
void Stage1Cam34(void);                                              /* $004BD8 */
void Stage1Cam38(void);                                              /* $004C22 */
void Stage1Cam3C(void);                                              /* $004C54 */
void Stage1Cam40(void);                                              /* $004C86 */
void Stage1Cam44(void);                                              /* $004CC0 */
void Stage1Cam48(void);                                              /* $004D12 */
void Stage1Go4C(void);                                               /* $004D24 */
void Stage1Cam4C(void);                                              /* $004D66 */
void Stage1Stay(void);                                               /* $004D72 */
void Stage1Go50(void);                                               /* $004D8E */
void Stage1Cam50(void);                                              /* $004DD0 */
void Stage1Stay2(void);                                              /* $004DDC */
void Stage1Go54(void);                                               /* $004DF8 */
void Stage1Cam54(void);                                              /* $004E3A */
void Stage1Stay3(void);                                              /* $004E46 */
void Stage1Junction54(void);                                         /* $004E4E */
void Stage1Go58(void);                                               /* $004E62 */
void Stage1Cam58(void);                                              /* $004EA4 */
void Stage1Stay4(void);                                              /* $004EB0 */
void Stage1Go5C(void);                                               /* $004ECC */
void Stage1Cam5C(void);                                              /* $004F3A */
void Stage1Cam60(void);                                              /* $004F72 */
void Stage1Go64(void);                                               /* $004FA0 */
void Stage1Cam64(void);                                              /* $00500E */
void Stage1Cam68(void);                                              /* $005046 */
void Stage1Go6C(void);                                               /* $005074 */
void Stage1Cam6C(void);                                              /* $0050E2 */
void Stage1Cam70(void);                                              /* $00511A */
void Stage1Go74(void);                                               /* $005148 */
void Stage1Cam74(void);                                              /* $0051B6 */
void Stage1Cam78(void);                                              /* $0051EE */
void Stage1Go7C(void);                                               /* $00521C */
void Stage1Cam7C(void);                                              /* $00528A */
void Stage1Go84(void);                                               /* $0052C6 */
void Stage1Cam84(void);                                              /* $005334 */
void Stage1Go8C(void);                                               /* $005370 */
void Stage1Cam8C(void);                                              /* $0053DE */
void Stage1Go94(void);                                               /* $00541A */
void Stage1Cam94(void);                                              /* $005488 */
void Stage1Cam9C(void);                                              /* $0054C4 */
void Stage1Back00(void);                                             /* $0055CE */
void Stage1Back04(void);                                             /* $0055E0 */
void Stage1Back08(void);                                             /* $005600 */
void Stage1Back0C(void);                                             /* $00561E */
void ChunkDma(uint32_t src, uint16_t vram);                          /* $001AB0 d0, d1.w */
void PlaneAChunk(uint32_t src, uint16_t vram);                       /* $001AB8 d0, d1.w */
void PlayerPlace(uint16_t obj, uint16_t x, uint16_t y, uint16_t state); /* $00F0BC a5, d0.w, d1.w, d2.w */
void Player1Place(uint16_t x, uint16_t y, uint16_t state);           /* $00F074 d0.w, d1.w, d2.w */
void Player2Place(uint16_t x, uint16_t y, uint16_t state);           /* $00F098 d0.w, d1.w, d2.w */
void PlayersPlace(uint16_t x, uint16_t y, uint16_t state);           /* $00F070 d0.w, d1.w, d2.w */
void PlayersStageHold(void);                                         /* $00427E */
void BackSpeed(uint32_t x, uint32_t y);                              /* $00419A d0, d1 */
void Stage2Frame(void);                                              /* $0058F4 */
void Stage2Camera(void);                                             /* $0058FC */
void Stage2Rumble(void);                                             /* $005E3C */
void Stage2Bob(void);                                                /* $005F66 */
void Stage2BowPull(void);                                            /* $005FB4 */
void Stage2BowColumn(void);                                          /* $005FD2 */
void Stage2BowColumnPlain(void);                                     /* $006030 */
void Stage2Lights(void);                                             /* $006060 */
void Stage2SideA(uint16_t vram, uint16_t frame, uint16_t y);         /* $0061E2 d1.w, d2.w, d3.w */
void Stage2SideB(uint16_t vram, uint16_t frame, uint16_t y);         /* $0061C2 d1.w, d2.w, d3.w */
void Stage2SideC(uint16_t vram, uint16_t frame, uint16_t y);         /* $0061A2 d1.w, d2.w, d3.w */
void Stage2SideD(uint16_t vram, uint16_t frame, uint16_t y);         /* $006182 d1.w, d2.w, d3.w */
void Stage2SideE(uint16_t vram, uint16_t frame, uint16_t y);         /* $006162 d1.w, d2.w, d3.w */
void Stage2Side(void);                                               /* $0060FA */
void Stage2DeckA(uint16_t vram, uint16_t frame, uint16_t x);         /* $006376 d1.w, d2.w, d3.w */
void Stage2DeckB(uint16_t vram, uint16_t frame, uint16_t x);         /* $006356 d1.w, d2.w, d3.w */
void Stage2DeckC(uint16_t vram, uint16_t frame, uint16_t x);         /* $006336 d1.w, d2.w, d3.w */
void Stage2DeckD(uint16_t vram, uint16_t frame, uint16_t x);         /* $006316 d1.w, d2.w, d3.w */
void Stage2DeckE(uint16_t vram, uint16_t frame, uint16_t x);         /* $0062F6 d1.w, d2.w, d3.w */
void Stage2DeckF(uint16_t vram, uint16_t frame, uint16_t x);         /* $0062D6 d1.w, d2.w, d3.w */
void Stage2DeckG(uint16_t vram, uint16_t frame, uint16_t x);         /* $0062B6 d1.w, d2.w, d3.w */
void Stage2DeckH(uint16_t vram, uint16_t frame, uint16_t x);         /* $006296 d1.w, d2.w, d3.w */
void Stage2DeckI(uint16_t vram, uint16_t frame, uint16_t x);         /* $006276 d1.w, d2.w, d3.w */
void Stage2ColumnScroll(void);                                       /* $006396 */
void Stage2Catch(void);                                              /* $006402 */
void Stage2Catch2(void);                                             /* $006442 */
void PlayersUnlimitY(void);                                          /* $006474 */
void PlayersLimitY(void);                                            /* $006496 */
void Stage2SceneInput(void);                                         /* $005E76 */
void Stage2StateX(void);                                             /* $005EA4 */
void Player2PinOnFloor(void);                                        /* $005F0A */
void Stage2Cam00(void);                                              /* $005932 */
void Stage2Cam04(void);                                              /* $0059A4 */
void Stage2Cam08(void);                                              /* $005A16 */
void Stage2Cam0C(void);                                              /* $005A82 */
void Stage2Cam10(void);                                              /* $005AE4 */
void Stage2Cam14(void);                                              /* $005B22 */
void Stage2Cam18(void);                                              /* $005B96 */
void Stage2Cam1C(void);                                              /* $005BB8 */
void Stage2Cam20(void);                                              /* $005C16 */
void Stage2Cam24(void);                                              /* $005C3E */
void Stage2Cam28(void);                                              /* $005C68 */
void Stage2Cam2C(void);                                              /* $005CAC */
void Stage2Cam30(void);                                              /* $005CC2 */
void Stage2Cam34(void);                                              /* $005CFC */
void Stage2Cam38(void);                                              /* $005D36 */
void Stage2Cam3C(void);                                              /* $005DB0 */
void Stage2Cam40(void);                                              /* $005DC4 */
void Stage2Cam44(void);                                              /* $005DEC */
void Stage2Cam48(void);                                              /* $005E26 */
void Stage2Cam4C(void);                                              /* $005E3A */
void Stage2BandsStart(void);                                         /* $006A06 */
void Stage2Band(uint16_t top, uint16_t bottom, uint16_t scroll, uint16_t from, uint16_t to, uint32_t table); /* $006AA0 d0.w, d1.w, d2.w, d3.w, d4.w, a0 */
void Stage2Bands(void);                                              /* $006A20 */
void Stage2Back00(void);                                             /* $006758 */
void Stage2Back04(void);                                             /* $00676A */
void Stage2Back08(void);                                             /* $006796 */
void Stage2Back0C(void);                                             /* $0067C8 */
void Stage2Back10(void);                                             /* $0067E8 */
void Stage2Back14(void);                                             /* $00680E */
void Stage2Back18(void);                                             /* $00688E */
void Stage2Back1C(void);                                             /* $0068C2 */
void Stage2Back20(void);                                             /* $006914 */
void Stage2Back24(void);                                             /* $006962 */
void Stage2Back28(void);                                             /* $006986 */
void Stage2Back2C(void);                                             /* $0069C6 */
void Stage2Back30(void);                                             /* $0069E6 */
void Stage2Back34(void);                                             /* $0069E8 */
void CameraPlace(uint16_t ax, uint16_t ay, uint16_t bx, uint16_t by); /* $008EE8 d0.w, d1.w, d2.w, d3.w */
void Stage3Frame(void);                                              /* $006D1C */
void Stage3Camera(void);                                             /* $006D24 */
void Stage3Cam00(void);                                              /* $006D42 */
void Stage3Cam04(void);                                              /* $006D72 */
void Stage3Cam08(void);                                              /* $006D84 */
void Stage3Cam0C(void);                                              /* $006DB4 */
void Stage3Cam10(void);                                              /* $006DE0 */
void Stage3Cam14(void);                                              /* $006E7E */
void Stage3Cam18(void);                                              /* $006E9C */
void Stage3Cam1C(void);                                              /* $006EAA */
void Stage3Back00(void);                                             /* $006EC0 */
void Stage3Back04(void);                                             /* $006ED6 */
void Stage3Back08(void);                                             /* $006ED8 */
void Stage4Frame(void);                                              /* $00715C */
void Stage4Camera(void);                                             /* $0071BA */
void Stage4Follow(void);                                             /* $007558 */
void Stage4Cam00(void);                                              /* $0071EE */
void Stage4Cam04(void);                                              /* $007206 */
void Stage4Cam08(void);                                              /* $007244 */
void Stage4Cam0C(void);                                              /* $007252 */
void Stage4Cam10(void);                                              /* $00726A */
void Stage4Cam14(void);                                              /* $0072A8 */
void Stage4Cam18(void);                                              /* $0072B6 */
void Stage4Cam1C(void);                                              /* $0072CE */
void Stage4Cam20(void);                                              /* $00730C */
void Stage4Cam24(void);                                              /* $00731A */
void Stage4Cam28(void);                                              /* $007332 */
void Stage4ClearCells(void);                                         /* $00751E */
void Stage4ClearCollision(void);                                     /* $0080D0 */
void Stage4Cam2C(void);                                              /* $007370 */
void Stage4Cam30(void);                                              /* $00737C */
void Stage4Cam34(void);                                              /* $0073B2 */
void Stage4Cam38(void);                                              /* $0073C0 */
void Stage4Cam3C(void);                                              /* $00741E */
void Stage4Cam40(void);                                              /* $007462 */
void Stage4Cam44(void);                                              /* $007486 */
void Stage4Cam48(void);                                              /* $0074B6 */
void Stage4Back00(void);                                             /* $00769A */
void Stage4Back04(void);                                             /* $0076CC */
void Stage4Back08(void);                                             /* $0076E8 */
void Stage4StripsLikeA(void);                                        /* $0077DA */
void Stage4Back0C(void);                                             /* $007700 */
void Stage4Back10(void);                                             /* $00771C */
void Stage4Back14(void);                                             /* $00771E */
void EffectsClear(void);                                             /* $002D50 */
void PlayersAt(uint16_t x, uint16_t y);                              /* $008E62 d0.w, d1.w */
void PlayersSet20(uint8_t v);                                        /* $0041E0 d0.b */
void Stage5Drift(void);                                              /* $007F54 */
void Stage5Frame(void);                                              /* $007A3A */
void Stage5Camera(void);                                             /* $007A42 */
void Stage5Cam00(void);                                              /* $007A72 */
void Stage5Cam04(void);                                              /* $007AB2 */
void Stage5Cam08(void);                                              /* $007AE8 */
void Stage5Cam0C(void);                                              /* $007B08 */
void Stage5Cam10(void);                                              /* $007B60 */
void Stage5Cam14(void);                                              /* $007BB8 */
void Stage5Cam18(void);                                              /* $007BEE */
void Stage5Cam1C(void);                                              /* $007C0E */
void Stage5Cam20(void);                                              /* $007C66 */
void Stage5Cam24(void);                                              /* $007CBE */
void Stage5Cam28(void);                                              /* $007CF4 */
void Stage5Cam2C(void);                                              /* $007D14 */
void Stage5Cam30(void);                                              /* $007D3E */
void Stage5Cam34(void);                                              /* $007E52 */
void Stage5Cam38(void);                                              /* $007E76 */
void Stage5Cam3C(void);                                              /* $007F50 */
void Stage5Cam40(void);                                              /* $007F52 */
void Stage5Tiles(uint16_t i);                                        /* $008534 d0.w */
void Stage5Back00(void);                                             /* $008106 */
void Stage5Back04(void);                                             /* $00811C */
void Stage5Back08(void);                                             /* $008140 */
void Stage5Back0C(void);                                             /* $008160 */
void Stage5Back10(void);                                             /* $008174 */
void Stage5Back14(void);                                             /* $0081AA */
void Stage5Back18(void);                                             /* $0081AC */
void Stage6Frame(void);                                              /* $00872E */
void Stage6Camera(void);                                             /* $008736 */
void Stage6Lamps(uint32_t pal, uint32_t target);                     /* $008D52 a0, a1 */
void CameraFollowUp(void);                                           /* $008CB4 */
void Stage6Gate(void);                                               /* $008B58 */
void Stage6GateOpen(void);                                           /* $008C06 */
void PlayersToScene(uint16_t s);                                     /* $008AFC d0.w */
void PlayersCharacters(void);                                        /* $008B2E */
void Stage6Cam00(void);                                              /* $00876A */
void Stage6Cam04(void);                                              /* $0087A2 */
void Stage6Cam08(void);                                              /* $0087AE */
void Stage6Cam0C(void);                                              /* $0087D6 */
void Stage6Cam10(void);                                              /* $0087FE */
void Stage6Cam14(void);                                              /* $008836 */
void Stage6Cam18(void);                                              /* $008842 */
void Stage6Cam1C(void);                                              /* $00887A */
void Stage6Cam20(void);                                              /* $008886 */
void Stage6Cam24(void);                                              /* $0088CE */
void Stage6Cam28(void);                                              /* $0088EC */
void Stage6Cam2C(void);                                              /* $008916 */
void Stage6Cam30(void);                                              /* $00894E */
void Stage6Cam34(void);                                              /* $0089AA */
void Stage6Cam38(void);                                              /* $0089DE */
void Stage6Cam3C(void);                                              /* $008A02 */
void Stage6Cam40(void);                                              /* $008A28 */
void Stage6Cam44(void);                                              /* $008AF6 */
void Stage6Cam48(void);                                              /* $008AFA */
void StageTilesJapan(void);                                          /* $01B7EA */
void PlayerVitality(uint16_t obj);                                   /* $00EFA0 a5 */
void PlayersVitality(void);                                          /* $00EF94 */
void StageColoursCopy(uint32_t colours, uint16_t stage);             /* $02A13C a0, d0.w */
void StageColoursFor(uint16_t stage);                                /* $02A132 d0.w */
void StageColours(void);                                             /* $02A12E */
void StageColoursCopy2(uint32_t colours);                            /* $02A1DA a0 */
void StageColours2(void);                                            /* $02A1C8 */
void StageLayout(void);                                              /* $01ACDE */
int SlotFreeLow(uint16_t *slot);                                     /* $01ABAC -> Z, a0.w */
void Spawn1F0(void);                                                 /* $033A96 */
void Stage1DirectorAtCheckpoint(void);                               /* $03351C */
void Stage2Objects(void);                                            /* $0064B8 */
void StarsStart(void);                                               /* $0081D0 */
void Stage0Start(void);                                              /* $003942 */
void Stage1Start(void);                                              /* $0045A6 */
void Stage2Start(void);                                              /* $0056AA */
void Stage3Start(void);                                              /* $006B46 */
void Stage4Start(void);                                              /* $006F66 */
void Stage5Start(void);                                              /* $00785C */
void Stage6Start(void);                                              /* $00855C */

/* ---- the objects' shared helpers (objects.c) ---- */
int SlotFreeMid(uint16_t *slot, uint16_t *type);                     /* $055F6A -> Z, a0.w, d0.w */
int SlotFreeHigh(uint16_t *slot, uint16_t *type);                    /* $055FC0 -> Z, a0.w, d0.w */
int SlotFreeOrPuff(uint16_t *slot, uint16_t *type, uint16_t *d1, uint32_t *a1); /* $055F84 -> Z, a0.w, d0.w, d1.w, a1 */
int SlotFree24(uint16_t *slot, uint16_t *type);                      /* $055FEC -> Z, a0.w, d0.w */
int SlotFree12(uint16_t *slot, uint16_t *type);                      /* $056064 -> Z, a0.w, d0.w */
int SlotFree9(uint16_t *slot, uint16_t *type);                       /* $056082 -> Z, a0.w, d0.w */
int SlotFree6(uint16_t *slot, uint16_t *type);                       /* $0560A0 -> Z, a0.w, d0.w */
int SlotFree4(uint16_t *slot, uint16_t *type);                       /* $0560B4 -> Z, a0.w, d0.w */
uint16_t PlayerTarget(void);                                         /* $029E9C -> a0.w */
void ObjectSetup(uint16_t obj, uint32_t *data, uint16_t *count);     /* $04C4AC a5 -> a0, d7.w */
void ObjectSetupNext(uint16_t obj, uint32_t *data, uint16_t *count); /* $04C4A8 a5 -> a0, d7.w */
void ObjectSetupAll(uint16_t obj, uint32_t *data, uint16_t *count);  /* $04C4A6 a5 -> a0, d7.w */
uint16_t AngleIndex(uint16_t *angle);                                /* $02A252 -> d0.w, d2.w */
void ObjectRemoveOffscreen(uint16_t obj);                            /* $04C3EA a5 */
void ObjectVanish(uint16_t slot, uint16_t keep1, uint16_t keep2);    /* $02A10E a0, d0.w, d1.w */
void ObjectsVanishFrom(uint16_t from, uint16_t keep1, uint16_t keep2); /* $02A0E0 a0, d0.w, d1.w */
uint16_t EffectOwner(uint16_t obj);                                  /* $019D88 a5 -> a0.w */
void EffectAtHand(uint16_t obj, uint16_t player);                    /* $019DA6 a5, a0 */
void EffectPriority(uint16_t obj);                                   /* $019DFA a5 */
void EffectFlicker(uint16_t obj);                                    /* $019E16 a5 */
void SpeedFromAngle(uint16_t obj, uint16_t shift);                   /* $019142 a5, d7.w */
uint16_t NearestPlayer(uint16_t obj, uint16_t *dx, uint16_t *dy, uint16_t *player); /* $029F7A a5 -> d0.w, d1.w, d5.w, a0.w */
int SpawnAt(uint16_t obj, uint16_t type, uint16_t dy, uint16_t dx, uint16_t arg, uint16_t *slot); /* $056AF4 a5, d4.w, d5.w, d6.w, d7.w -> Z, a0.w */
int SpawnTowardPlayer(uint16_t obj, uint16_t *slot);                 /* $04C16E a5 -> Z, a0.w */
void AnimListStart(uint16_t obj);                                    /* $056206 a5 */
void AnimList(uint16_t obj);                                         /* $0561F4 a5 */
int SlotFreeBelowCE(uint16_t *slot, uint16_t *type);                 /* $032288 -> Z, a0.w, d0.w */
uint16_t AngleEighthOf(uint16_t index);                              /* $02A234 d2.w -> d0.w */
uint16_t AngleEighth(uint16_t *angle);                               /* $02A22E -> d0.w, d2.w */
int Spawn4A8(uint16_t angle, uint16_t x, uint16_t y, uint32_t ytable, uint32_t xtable, uint16_t *slot); /* $04C2F0 d5.w, d6.w, d7.w, a1, a2 -> Z, a0.w */
int Spawn4A8Round(uint16_t angle, uint16_t x, uint16_t y, uint16_t *slot); /* $04C2E4 d5.w, d6.w, d7.w -> Z, a0.w */
void FaceAngle(uint16_t obj, uint16_t dir, uint32_t table, uint16_t *d6, uint16_t *d7); /* $02A6CA a5, d0.w, a1 -> d6.w, d7.w */
void FaceToward(uint16_t obj, uint16_t target, uint32_t table, uint16_t *d0, uint16_t *d5, uint16_t *d6, uint16_t *d7); /* $02A6A8 a5, a0, a1 -> d0.w, d5.w, d6.w, d7.w */
int ObjFloor4A(uint16_t obj, uint16_t x, uint16_t y, uint16_t attr, uint16_t mask, uint16_t *d4); /* $029CD8 a5, d0.w, d1.w, d3.w, d7.w -> Z, d4.w */
int ObjFloor4B(uint16_t obj, uint16_t x, uint16_t y, uint16_t attr, uint16_t mask, uint16_t *d4); /* $029CF2 a5, d0.w, d1.w, d3.w, d7.w -> Z, d4.w */
int ObjFloor4C(uint16_t obj, uint16_t x, uint16_t y, uint16_t attr, uint16_t mask, uint16_t *d4); /* $029D10 a5, d0.w, d1.w, d3.w, d7.w -> Z, d4.w */
int ObjFloor8(uint16_t obj, uint16_t x, uint16_t y, uint16_t attr, uint16_t mask, uint16_t *d4); /* $029D2C a5, d0.w, d1.w, d3.w, d7.w -> Z, d4.w */
int ObjFloorC(uint16_t obj, uint16_t x, uint16_t y, uint16_t attr, uint16_t mask, uint16_t *d4); /* $029D4E a5, d0.w, d1.w, d3.w, d7.w -> Z, d4.w */
int ObjFloor10A(uint16_t obj, uint16_t x, uint16_t y, uint16_t attr, uint16_t mask, uint16_t *d4); /* $029D6E a5, d0.w, d1.w, d3.w, d7.w -> Z, d4.w */
int ObjFloor10B(uint16_t obj, uint16_t x, uint16_t y, uint16_t attr, uint16_t mask, uint16_t *d4); /* $029D94 a5, d0.w, d1.w, d3.w, d7.w -> Z, d4.w */
int ObjFloor14A(uint16_t obj, uint16_t x, uint16_t y, uint16_t attr, uint16_t mask, uint16_t *d4); /* $029DBC a5, d0.w, d1.w, d3.w, d7.w -> Z, d4.w */
int ObjFloor14B(uint16_t obj, uint16_t x, uint16_t y, uint16_t attr, uint16_t mask, uint16_t *d4); /* $029DE0 a5, d0.w, d1.w, d3.w, d7.w -> Z, d4.w */
int ObjectFloor(uint16_t obj, uint16_t dy, uint16_t *d3, uint16_t *d4); /* $029B82 a5, d1.w -> Z, d3.w, d4.w */
int ObjectFloor16(uint16_t obj, uint16_t *d3, uint16_t *d4);         /* $029B7E a5 -> Z, d3.w, d4.w */
int ObjectCeiling(uint16_t obj, uint16_t dy, uint16_t *d3, uint16_t *d4); /* $029C48 a5, d1.w -> Z, d3.w, d4.w */
int ObjectFloorUp16(uint16_t obj, uint16_t *d3, uint16_t *d4);       /* $029AF0 a5 -> Z, d3.w, d4.w */
int ObjectFloorUp30(uint16_t obj, uint16_t *d3, uint16_t *d4);       /* $029AF8 a5 -> Z, d3.w, d4.w */
int ObjectWalls(uint16_t obj, uint16_t dy, uint16_t *d3, uint16_t *d4); /* $029A42 a5, d1.w -> Z, d3.w, d4.w */
int ObjectWalls8(uint16_t obj, uint16_t *d3, uint16_t *d4);          /* $029A40 a5 -> Z, d3.w, d4.w */
int ObjectWallsAny(uint16_t obj, uint16_t dy, uint16_t *d3, uint16_t *d4); /* $029AA2 a5, d1.w -> Z, d3.w, d4.w */
int ObjectWallsAny8(uint16_t obj, uint16_t *d3, uint16_t *d4);       /* $029AA0 a5 -> Z, d3.w, d4.w */
uint16_t PlayerBySlot(uint16_t obj);                                 /* $029F04 a5 -> a0.w */

/* ---- the soldiers' spawner (spawner.c) ---- */
void SpawnTimer(uint32_t table, uint16_t timer);                     /* $03210A a0, a1 */
void SpawnTimersTick(void);                                          /* $032302 */
void SpawnRight(uint32_t kinds);                                     /* $032316 d7 */
void SpawnRightLow(uint32_t kinds);                                  /* $032350 d7 */
int PlayersLeftEdge(void);                                           /* $03229E -> N */
void SpawnLeft(uint16_t kind);                                       /* $032376 d7.w */
int SpawnSlot(uint16_t *slot, uint16_t *type);                       /* $032282 -> Z, a0.w, d0.w */
void SpawnPatternA(uint16_t obj);                                    /* $031FF8 a5 */
void SpawnPatternB(uint16_t obj);                                    /* $03205A a5 */
void SpawnPatternC(uint16_t obj);                                    /* $032028 a5 */
void SpawnPatternD(uint16_t obj);                                    /* $0320D4 a5 */
void SpawnPatternE(uint16_t obj);                                    /* $0320CA a5 */
void SpawnPatternF(uint16_t obj);                                    /* $0320B6 a5 */
void SpawnPatternG(uint16_t obj);                                    /* $0320C0 a5 */
void SpawnPatternH(uint16_t obj);                                    /* $03208C a5 */
void SpawnerSetupNone(void);                                         /* $03222C */
void SpawnerSetupC(void);                                            /* $032238 */
void SpawnerSetup8(void);                                            /* $032240 */
void SpawnerSetupBase(void);                                         /* $032248 */
void SpawnerStart(uint16_t obj);                                     /* $031F5E a5 */
void SpawnerWait(uint16_t obj);                                      /* $031F72 a5 */
void SpawnerPattern(uint16_t obj);                                   /* $031F92 a5 */
void SpawnerAgain(uint16_t obj);                                     /* $031FEC a5 */
void SpawnerBecome1F8(uint16_t obj);                                 /* $0321A0 a5 */
void SpawnerEnd(uint16_t obj);                                       /* $0321AC a5 */
void SpawnerBoss(uint16_t obj);                                      /* $0321B4 a5 */
void Spawner3C4(uint16_t obj);                                       /* $0321C8 a5 */
void Spawner(uint16_t obj);                                          /* $031F3A a5 */

/* ---- the text box (textbox.c) ---- */
uint8_t TextButtonsHeld(uint16_t obj);                               /* $00C6D6 a5 -> d0.b */
uint8_t TextButtonsPressed(uint16_t obj);                            /* $00C6FE a5 -> d0.b */
void TextSound(uint16_t obj);                                        /* $00C6A8 a5 */
void TextVramAddress(uint16_t obj);                                  /* $00C6BC a5 */
void TextClear8(void);                                               /* $00C726 */
void TextClear2(void);                                               /* $00C8A0 */
void TextBoxPrint(uint16_t obj);                                     /* $00C580 a5 */
void TextBoxRun(uint16_t obj);                                       /* $00C488 a5 */
void TextBoxStart(uint16_t obj);                                     /* $00C46C a5 */
void TextBox(uint16_t obj);                                          /* $00C45A a5 */

/* ---- the items (items.c) ---- */
void ItemLook(uint16_t obj);                                         /* $01A5E4 a5 */
void ItemStart(uint16_t obj);                                        /* $01A2D0 a5 */
void ItemFall(uint16_t obj);                                         /* $01A2F6 a5 */
void ItemRest(uint16_t obj);                                         /* $01A392 a5 */
void ItemDrift(uint16_t obj);                                        /* $01A5C0 a5 */
void ItemMineStart(uint16_t obj);                                    /* $01A41C a5 */
void ItemMineFall(uint16_t obj);                                     /* $01A442 a5 */
void ItemMineRest(uint16_t obj);                                     /* $01A4D8 a5 */
void ItemSpaceStart(uint16_t obj);                                   /* $01A56A a5 */
void ItemSpaceRest(uint16_t obj);                                    /* $01A596 a5 */
void Item(uint16_t obj);                                             /* $01A2A2 a5 */
void CarrierFace(uint16_t obj);                                      /* $01A8CE a5 */
void CarrierDrop(uint16_t obj, uint16_t slot);                       /* $01A8F6 a5, a0 */
void CarrierBob(uint16_t obj);                                       /* $01A836 a5 */
void CarrierFly(uint16_t obj);                                       /* $01A6E2 a5 */
void CarrierFall(uint16_t obj);                                      /* $01A77E a5 */
void CarrierWait(uint16_t obj);                                      /* $01A82E a5 */
void CarrierThrown(uint16_t obj);                                    /* $01A8A6 a5 */
void CarrierStart(uint16_t obj);                                     /* $01A66A a5 */
void Carrier(uint16_t obj);                                          /* $01A64C a5 */

/* ---- the story scenes' actors (actors.c) ---- */
void ActorA(uint16_t obj);                                           /* $02191E a5 */
void ActorTypeA(uint16_t obj);                                       /* $021900 a5 */
void ActorB(uint16_t obj);                                           /* $021FBE a5 */
void ActorTypeB(uint16_t obj);                                       /* $021FA0 a5 */
void BannerStart(uint16_t obj);                                      /* $020D30 a5 */
void BannerWait(uint16_t obj);                                       /* $020D60 a5 */
void BannerFlip(uint16_t obj);                                       /* $020D62 a5 */
void BannerDone(uint16_t obj);                                       /* $020D76 a5 */
void BannerA(uint16_t obj);                                          /* $020D16 a5 */
void BannerBStart(uint16_t obj);                                     /* $020D92 a5 */
void BannerBWait(uint16_t obj);                                      /* $020DC2 a5 */
void BannerBFlip(uint16_t obj);                                      /* $020DC4 a5 */
void BannerBDone(uint16_t obj);                                      /* $020DD8 a5 */
void BannerB(uint16_t obj);                                          /* $020D78 a5 */
void FigureC(uint16_t obj);                                          /* $021C08 a5 */
void FigureTypeC(uint16_t obj);                                      /* $021BEA a5 */
void FigureD1(uint16_t obj);                                         /* $021EAA a5 */
void FigureD2(uint16_t obj);                                         /* $021EEE a5 */
void FigureD3(uint16_t obj);                                         /* $021F32 a5 */
void FigureTypeD(uint16_t obj);                                      /* $021E84 a5 */
void CraftBob(uint16_t obj);                                         /* $0216F2 a5 */
void CraftA(uint16_t obj);                                           /* $021662 a5 */
void CraftB(uint16_t obj);                                           /* $021718 a5 */
void CraftC(uint16_t obj);                                           /* $0217A2 a5 */
void CraftD0(uint16_t obj);                                          /* $0217FE a5 */
void CraftD1(uint16_t obj);                                          /* $021836 a5 */
void CraftD2(uint16_t obj);                                          /* $02184E a5 */
void CraftD3(uint16_t obj);                                          /* $021878 a5 */
void CraftD4(uint16_t obj);                                          /* $02189C a5 */
void CraftD5(uint16_t obj);                                          /* $0218D0 a5 */
void CraftD(uint16_t obj);                                           /* $0217E4 a5 */
void CraftType(uint16_t obj);                                        /* $021638 a5 */
void DebrisA(uint16_t obj);                                          /* $0219D8 a5 */
void DebrisB(uint16_t obj);                                          /* $021A34 a5 */
void DebrisC(uint16_t obj);                                          /* $021A90 a5 */
void DebrisType(uint16_t obj);                                       /* $0219B2 a5 */
void FigureE1(uint16_t obj);                                         /* $021C80 a5 */
void FigureE2(uint16_t obj);                                         /* $021CC2 a5 */
void FigureE3(uint16_t obj);                                         /* $021D3A a5 */
void FigureE4(uint16_t obj);                                         /* $021D7C a5 */
void FigureE5(uint16_t obj);                                         /* $021DBE a5 */
void FigureE6(uint16_t obj);                                         /* $021E00 a5 */
void FigureE7(uint16_t obj);                                         /* $021E42 a5 */
void FigureTypeE(uint16_t obj);                                      /* $021C4A a5 */
void SetA1(uint16_t obj);                                            /* $0220AC a5 */
void SetA2(uint16_t obj);                                            /* $02210E a5 */
void SetA3(uint16_t obj);                                            /* $022150 a5 */
void SetA4(uint16_t obj);                                            /* $022192 a5 */
void SetA5(uint16_t obj);                                            /* $0221D4 a5 */
void SetA6(uint16_t obj);                                            /* $022216 a5 */
void SetA7(uint16_t obj);                                            /* $022258 a5 */
void SetA8(uint16_t obj);                                            /* $02229A a5 */
void SetA9(uint16_t obj);                                            /* $0222DC a5 */
void SetA10(uint16_t obj);                                           /* $02231E a5 */
void SetTypeA(uint16_t obj);                                         /* $02206A a5 */
void CloudBob(uint16_t obj);                                         /* $022402 a5 */
void Cloud(uint16_t obj);                                            /* $022386 a5 */
void CloudType(uint16_t obj);                                        /* $022368 a5 */
void CloudRider(uint16_t obj);                                       /* $022446 a5 */
void CloudRiderType(uint16_t obj);                                   /* $022428 a5 */
void FigureF(uint16_t obj);                                          /* $02249A a5 */
void FigureTypeF(uint16_t obj);                                      /* $02247C a5 */
void FigureG1(uint16_t obj);                                         /* $022A90 a5 */
void FigureG2a(uint16_t obj);                                        /* $022AEC a5 */
void FigureG2b(uint16_t obj);                                        /* $022B24 a5 */
void FigureG2c(uint16_t obj);                                        /* $022B3C a5 */
void FigureG2d(uint16_t obj);                                        /* $022B50 a5 */
void FigureG2e(uint16_t obj);                                        /* $022B68 a5 */
void FigureG2f(uint16_t obj);                                        /* $022B78 a5 */
void FigureG2(uint16_t obj);                                         /* $022AD2 a5 */
void FigureTypeG(uint16_t obj);                                      /* $022A6E a5 */

/* ---- the soldier and what it becomes (soldier.c) ---- */
uint16_t ObjectRelease(uint16_t obj);                                /* $02A906 a5 -> d0.w */
/* labels other objects jump into (no routines of their own: their code runs in the caller's) */
void AimedSpawn(uint16_t obj, uint16_t target, uint32_t faces, uint32_t offsets, uint16_t type);
void SineTableStart(uint16_t at, uint16_t radius);                   /* $0296DA d0.w, d1.w */
int SineTableBuild(void);                                            /* $0296F6 -> N */
void AimedSpawnHighToward(uint16_t obj, uint16_t target, uint32_t faces, uint32_t offsets, uint16_t type);
void AimFacing(uint16_t obj, uint16_t target, uint32_t faces, uint16_t *d0, uint16_t *d5, uint16_t *d6, uint16_t *d7); /* $02A84E a5, a0, a1 -> d0.w, d5.w, d6.w, d7.w */
void AimedSpawnHigh(uint16_t obj, uint16_t angle, uint16_t dir, uint16_t type, uint32_t offsets); /* $02A822 a5, d5.w, d6.w, d7.w, a2 */
void ThrowAt(uint16_t obj, uint16_t target, uint16_t type);
void SoldierDying(uint16_t obj);
void LampColours(void);
void HintHandler(void);                  /* the H-interrupt handler copied to v_hintCode, as C (native) */
void FrameFlash(void);                   /* loc_0013A0: the frame's end in a stage (a flash) */
int ObjectFloorA0(uint16_t obj, uint16_t dy, uint16_t *d3, uint16_t *d4, uint32_t *a0);
void DebrisEmit(uint16_t obj);
void ThrownFly(uint16_t obj);                                        /* $02AA00 a5 */
void Thrown(uint16_t obj);                                           /* $02A918 a5 */
void KnockedStart(uint16_t obj);                                     /* $02ABFA a5 */
void KnockedHeld(uint16_t obj);                                      /* $02AC1A a5 */
void KnockedFly(uint16_t obj);                                       /* $02AC44 a5 */
void KnockedFall(uint16_t obj);                                      /* $02AC9A a5 */
void KnockedBounce(uint16_t obj);                                    /* $02ACF4 a5 */
void Knocked(uint16_t obj);                                          /* $02ABBA a5 */
void SoldierFall(uint16_t obj);                                      /* $02CB80 a5 */
void SoldierTire(uint16_t obj);                                      /* $02C7CA a5 */
void SoldierTireCrouch(uint16_t obj);                                /* $02C8AA a5 */
void SoldierHop(uint16_t obj);                                       /* $02CC06 a5 */
void SoldierStagger(uint16_t obj);                                   /* $02CE60 a5 */
void SoldierKnocked(uint16_t obj);                                   /* $02CEA2 a5 */
void SoldierGrab(uint16_t obj);                                      /* $02CF18 a5 */
void SoldierSpark(uint16_t obj, uint16_t vx, uint16_t vy, uint32_t data); /* $02C6B8 a5, d2.w, d3.w, d5 */
void SoldierSlide(uint16_t obj);                                     /* $02C5EE a5 */
void SoldierThinkNone(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy); /* $02D02A a5, d0.w, d1.w, d5.w */
void SoldierThinkA(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy); /* $02D02C a5, d0.w, d1.w, d5.w */
void SoldierThinkB(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy); /* $02D072 a5, d0.w, d1.w, d5.w */
void SoldierThinkC(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy); /* $02D094 a5, d0.w, d1.w, d5.w */
void SoldierThinkD(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy); /* $02D0B2 a5, d0.w, d1.w, d5.w */
void SoldierThinkE(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy); /* $02D0DE a5, d0.w, d1.w, d5.w */
void SoldierThinkF(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy); /* $02D0FE a5, d0.w, d1.w, d5.w */
void SoldierThinkG(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy); /* $02D118 a5, d0.w, d1.w, d5.w */
void SoldierThinkH(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy); /* $02D126 a5, d0.w, d1.w, d5.w */
void SoldierThinkI(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy); /* $02D134 a5, d0.w, d1.w, d5.w */
void SoldierThinkJ(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy); /* $02D142 a5, d0.w, d1.w, d5.w */
void SoldierThinkK(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy); /* $02D162 a5, d0.w, d1.w, d5.w */
void SoldierThinkL(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy); /* $02D182 a5, d0.w, d1.w, d5.w */
void SoldierThinkM(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy); /* $02D1A4 a5, d0.w, d1.w, d5.w */
void SoldierThinkN(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy); /* $02D1B2 a5, d0.w, d1.w, d5.w */
void SoldierS04(uint16_t obj);                                       /* $02B29C a5 */
void SoldierS104(uint16_t obj);                                      /* $02B2C2 a5 */
void SoldierS0C(uint16_t obj);                                       /* $02B310 a5 */
void SoldierS14(uint16_t obj);                                       /* $02B2E8 a5 */
void SoldierS1C(uint16_t obj);                                       /* $02B2F0 a5 */
void SoldierS08(uint16_t obj);                                       /* $02B3CA a5 */
void SoldierS18(uint16_t obj);                                       /* $02B3A2 a5 */
void SoldierS10(uint16_t obj);                                       /* $02B3AA a5 */
void SoldierSA0(uint16_t obj);                                       /* $02B48A a5 */
void SoldierSA4(uint16_t obj);                                       /* $02B50A a5 */
void SoldierS9C(uint16_t obj);                                       /* $02B45C a5 */
void SoldierSA8(uint16_t obj);                                       /* $02B57E a5 */
void SoldierS10C(uint16_t obj);                                      /* $02B5D0 a5 */
void SoldierS110(uint16_t obj);                                      /* $02B5FC a5 */
void SoldierS100(uint16_t obj);                                      /* $02B62A a5 */
void SoldierS30(uint16_t obj);                                       /* $02B68C a5 */
void SoldierPushed(uint16_t obj, uint16_t by);                       /* $02B6F0 a5, a0 */
void SoldierSB8(uint16_t obj);                                       /* $02B718 a5 */
void SoldierSBC(uint16_t obj);                                       /* $02B74A a5 */
void SoldierSB4(uint16_t obj);                                       /* $02B7D4 a5 */
void SoldierSD4(uint16_t obj);                                       /* $02B89C a5 */
void SoldierSD8(uint16_t obj);                                       /* $02B8FC a5 */
void SoldierSDC(uint16_t obj);                                       /* $02B974 a5 */
void SoldierS118(uint16_t obj);                                      /* $02BA04 a5 */
void SoldierS11C(uint16_t obj);                                      /* $02BA68 a5 */
void SoldierS120(uint16_t obj);                                      /* $02BAB2 a5 */
void SoldierSB0(uint16_t obj);                                       /* $02BB1E a5 */
void SoldierSD0(uint16_t obj);                                       /* $02BB60 a5 */
void SoldierS44(uint16_t obj);                                       /* $02BC14 a5 */
void SoldierSC0(uint16_t obj);                                       /* $02BC28 a5 */
void SoldierSE8(uint16_t obj);                                       /* $02BC50 a5 */
void SoldierS34(uint16_t obj);                                       /* $02BCAE a5 */
void SoldierS38(uint16_t obj);                                       /* $02BCC2 a5 */
void SoldierS3C(uint16_t obj);                                       /* $02BCF6 a5 */
void SoldierSE0(uint16_t obj);                                       /* $02BD00 a5 */
void SoldierSE4(uint16_t obj);                                       /* $02BD34 a5 */
void SoldierS48(uint16_t obj);                                       /* $02BD3E a5 */
void SoldierS50(uint16_t obj);                                       /* $02BD5A a5 */
void SoldierS54(uint16_t obj);                                       /* $02BDEA a5 */
void SoldierS58(uint16_t obj);                                       /* $02BE46 a5 */
void SoldierS5C(uint16_t obj);                                       /* $02BEA2 a5 */
void SoldierS60(uint16_t obj);                                       /* $02C000 a5 */
void SoldierS64(uint16_t obj);                                       /* $02C080 a5 */
void SoldierS68(uint16_t obj);                                       /* $02C100 a5 */
void SoldierS6C(uint16_t obj);                                       /* $02C11C a5 */
void SoldierS74(uint16_t obj);                                       /* $02C178 a5 */
void SoldierS78(uint16_t obj);                                       /* $02C1D8 a5 */
void SoldierS7C(uint16_t obj);                                       /* $02C232 a5 */
void SoldierS84(uint16_t obj);                                       /* $02C270 a5 */
void SoldierS88(uint16_t obj);                                       /* $02C282 a5 */
void SoldierS8C(uint16_t obj);                                       /* $02C2AA a5 */
void SoldierS98(uint16_t obj);                                       /* $02C406 a5 */
void SoldierSC4(uint16_t obj);                                       /* $02C410 a5 */
void SoldierSCC(uint16_t obj);                                       /* $02C4BC a5 */
int ChanceRoll(uint16_t obj, uint16_t chance);                       /* $02D018 a5, d6.w -> C */
void ThrownStart(uint16_t obj);                                      /* $02A92A a5 */
void KnockedRise(uint16_t obj);                                      /* $02AC5E a5 */
void SoldierAimSetup(uint16_t obj, uint16_t *target, uint16_t *type, uint32_t *faces, uint32_t *muzzles); /* $02C652 a5 -> a0.w, d7.w, a1, a2 */
void SoldierS108(uint16_t obj);                                      /* $02B6A0 a5 */
void SoldierS70(uint16_t obj);                                       /* $02B7FA a5 */
void SoldierS114(uint16_t obj);                                      /* $02B990 a5 */
void SoldierS4C(uint16_t obj);                                       /* $02BACE a5 */
void SoldierS40(uint16_t obj);                                       /* $02BB7C a5 */
void SoldierSF4(uint16_t obj);                                       /* $02BF84 a5 */
void SoldierSF0(uint16_t obj);                                       /* $02BEC6 a5 */
void SoldierS80(uint16_t obj);                                       /* $02C23C a5 */
void SoldierS94(uint16_t obj);                                       /* $02C32E a5 */
void SoldierS90(uint16_t obj);                                       /* $02C2AC a5 */
void SoldierSC8(uint16_t obj);                                       /* $02C434 a5 */
void SoldierSEC(uint16_t obj);                                       /* $02C4D2 a5 */
void SoldierS00(uint16_t obj);                                       /* $02B1DC a5 */
void Soldier(uint16_t obj);                                          /* $02AECE a5 */
void RiderStart(uint16_t obj);                                       /* $02A436 a5 */
void RiderMachine(uint16_t obj);                                     /* $02A652 a5 */
uint16_t RiderSpawn(uint16_t obj);                                   /* $02A4B6 a5 -> a0.w */
void RiderA(uint16_t obj);                                           /* $02A48C a5 */
void RiderB(uint16_t obj);                                           /* $02A4FC a5 */
void RiderC(uint16_t obj);                                           /* $02A538 a5 */
void RiderD(uint16_t obj);                                           /* $02A552 a5 */
void RiderFall(uint16_t obj);                                        /* $02A5BA a5 */
void RiderOff(uint16_t obj);                                         /* $02A5EC a5 */
void RiderHit(uint16_t obj);                                         /* $02A642 a5 */
void Rider(uint16_t obj);                                            /* $02A3C4 a5 */

/* ---- small objects and the orbiting segments (parts.c) ---- */
void AnimListFall(uint16_t obj);                                     /* $0561D8 a5 */
void HopperStart(uint16_t obj);                                      /* $037D54 a5 */
void HopperDone(uint16_t obj);                                       /* $037D82 a5 */
void HopperWait(uint16_t obj);                                       /* $037D84 a5 */
void Hopper(uint16_t obj);                                           /* $037D3E a5 */
uint32_t BossColours(uint32_t colours);                              /* $0277BE a4 -> a4 */
uint16_t OrbiterSetup(uint16_t parent, uint32_t maps, uint32_t radii, uint32_t links, uint16_t count, uint16_t slot, uint32_t *look); /* $0272E0 a5, a0, a1, a2, d7.w, a4.w -> a4.w, a3, a6=reg_a6 */
void Orbiter(uint16_t obj);                                          /* $027144 a5 */
void OrbitersStart(uint16_t obj);                                    /* $0277E8 a5 */

/* ---- the scenery's figures (figures.c) ---- */
void FrameList(uint16_t obj);                                        /* $05801A a5 */
void SelectAnimationKeep(uint16_t obj, uint32_t table);              /* $00219C a5, a0 */
void BouncerStart(uint16_t obj);                                     /* $0381E8 a5 */
void BouncerWalk(uint16_t obj);                                      /* $038268 a5 */
void BouncerRise(uint16_t obj);                                      /* $0382A2 a5 */
void BouncerFall(uint16_t obj);                                      /* $0382B4 a5 */
void BouncerStill(uint16_t obj);                                     /* $0382CC a5 */
void BouncerSparks(uint16_t obj);                                    /* $0382CE a5 */
void BouncerBurst(uint16_t obj);                                     /* $03833A a5 */
void BouncerBurstWait(uint16_t obj);                                 /* $03839A a5 */
void BouncerBounceStart(uint16_t obj);                               /* $0383AA a5 */
void BouncerBounce(uint16_t obj);                                    /* $0383C6 a5 */
void Bouncer(uint16_t obj);                                          /* $0381B2 a5 */
void FlyerStart(uint16_t obj);                                       /* $03792E a5 */
void FlyerFly(uint16_t obj);                                         /* $0379A4 a5 */
void FlyerDart(uint16_t obj);                                        /* $037AAA a5 */
void FlyerAim(uint16_t obj);                                         /* $037AC8 a5 */
void FlyerFire(uint16_t obj);                                        /* $037AFC a5 */
void FlyerStep(uint16_t obj);                                        /* $037910 a5 */
void Flyer(uint16_t obj);                                            /* $037858 a5 */
void CaughtStart(uint16_t obj);                                      /* $037C08 a5 */
void CaughtHeld(uint16_t obj);                                       /* $037C2E a5 */
void CaughtRise(uint16_t obj);                                       /* $037C98 a5 */
void CaughtFall(uint16_t obj);                                       /* $037CC6 a5 */
void CaughtBounce(uint16_t obj);                                     /* $037D20 a5 */
void FlyerCaught(uint16_t obj);                                      /* $037BE2 a5 */
void DebrisBurst(uint16_t obj);                                      /* $057BC2 a5 */
void WallStart(uint16_t obj);                                        /* $037E00 a5 */
void WallWhole(uint16_t obj);                                        /* $037E38 a5 */
void WallDamaged(uint16_t obj);                                      /* $037E58 a5 */
void WallBroken(uint16_t obj);                                       /* $037E70 a5 */
void WallFallStart(uint16_t obj);                                    /* $037E7C a5 */
void WallFall(uint16_t obj);                                         /* $037EAE a5 */
void WallSmokeStart(uint16_t obj);                                   /* $037F0A a5 */
void WallSmoke(uint16_t obj);                                        /* $037F9E a5 */
void WallCrumbleStart(uint16_t obj);                                 /* $037FA4 a5 */
void WallCrumble(uint16_t obj);                                      /* $037F42 a5 */
void Wall(uint16_t obj);                                             /* $037DC2 a5 */
void RockStart(uint16_t obj);                                        /* $037332 a5 */
void RockRise(uint16_t obj);                                         /* $03737A a5 */
void RockWait(uint16_t obj);                                         /* $037398 a5 */
void RockDrop(uint16_t obj);                                         /* $0373F6 a5 */
void Rock(uint16_t obj);                                             /* $037318 a5 */
void WormStart(uint16_t obj);                                        /* $03745C a5 */
void WormDrop(uint16_t obj);                                         /* $0374B2 a5 */
void WormWait(uint16_t obj);                                         /* $0374CE a5 */
void WormSegments(uint16_t obj);                                     /* $0374DC a5 */
void WormStretch(uint16_t obj);                                      /* $0375AA a5 */
void WormPlace(uint16_t obj);                                        /* $037690 a5 */
void WormSway(uint16_t obj);                                         /* $037630 a5 */
void Worm(uint16_t obj);                                             /* $03743A a5 */
void WormSegment(uint16_t obj);                                      /* $0377EC a5 */

/* ---- the effects (effects.c) ---- */
void Glint(uint16_t obj);                                            /* $056130 a5 */
void GlintFall(uint16_t obj);                                        /* $056128 a5 */
void AnimListLater(uint16_t obj);                                    /* $0561B2 a5 */
void AnimListBehind(uint16_t obj);                                   /* $0561CC a5 */
void AnimListSpeeding(uint16_t obj);                                 /* $0561E4 a5 */
void AnimListRepeat(uint16_t obj);                                   /* $056264 a5 */
void AnimListLow(uint16_t obj);                                      /* $0562E6 a5 */
void AnimLoop(uint16_t obj);                                         /* $056360 a5 */
void AnimLoopFor(uint16_t obj);                                      /* $05631E a5 */
void AnimLoopForFall(uint16_t obj);                                  /* $056302 a5 */
void AnimLoopForSpeeding(uint16_t obj);                              /* $05630E a5 */
void Blinker(uint16_t obj);                                          /* $056824 a5 */
void Reach(uint16_t obj);                                            /* $05684E a5 */
void SparkFall(uint16_t obj);                                        /* $0568CA a5 */
void Spark(uint16_t obj);                                            /* $0568DE a5 */
void Flash(uint16_t obj);                                            /* $056922 a5 */
void FlashFall(uint16_t obj);                                        /* $056912 a5 */
void Chip(uint16_t obj);                                             /* $056946 a5 */
void Lasting(uint16_t obj);                                          /* $0569A2 a5 */
void LastingSpeeding(uint16_t obj);                                  /* $056992 a5 */
void BlinkWhile(uint16_t obj);                                       /* $0569B2 a5 */
void Circler(uint16_t obj);                                          /* $056A06 a5 */
void FallThenDie(uint16_t obj);                                      /* $056AA8 a5 */
void Pellet(uint16_t obj);                                           /* $056B36 a5 */
void PelletHarmless(uint16_t obj);                                   /* $056B20 a5 */
void PelletBehind(uint16_t obj);                                     /* $056B2A a5 */
void PelletAimed(uint16_t obj);                                      /* $056C92 a5 */
void Bolt(uint16_t obj);                                             /* $056E10 a5 */
void Ember(uint16_t obj);                                            /* $056F1E a5 */
void SeekerStart(uint16_t obj);                                      /* $056FD4 a5 */
void SeekerFly(uint16_t obj);                                        /* $05703E a5 */
void Seeker(uint16_t obj);                                           /* $056FBA a5 */
void BombStart(uint16_t obj);                                        /* $0571F2 a5 */
void BombBurst(uint16_t obj);                                        /* $057282 a5 */
void BombFall(uint16_t obj);                                         /* $05722C a5 */
void BombFire(uint16_t obj);                                         /* $057336 a5 */
void BombFireStart(uint16_t obj);                                    /* $05732C a5 */
void BombNow(uint16_t obj);                                          /* $0571C2 a5 */
void Bomb(uint16_t obj);                                             /* $0571D8 a5 */
void CinderStart(uint16_t obj);                                      /* $0573FA a5 */
void CinderFall(uint16_t obj);                                       /* $05743A a5 */
void Cinder(uint16_t obj);                                           /* $0573E8 a5 */
void ShellStart(uint16_t obj);                                       /* $0574D6 a5 */
void ShellDown(uint16_t obj);                                        /* $05750E a5 */
void ShellUp(uint16_t obj);                                          /* $057526 a5 */
void Shell(uint16_t obj);                                            /* $0574C0 a5 */
void FrameNext(uint16_t obj);                                        /* $0560F4 a5 */
void Blast(uint16_t obj);                                            /* $057574 a5 */
void Spiral(uint16_t obj);                                           /* $057632 a5 */
void Ring(uint16_t obj);                                             /* $0576F4 a5 */
void RingKept(uint16_t obj);                                         /* $0576EC a5 */
void Emitter(uint16_t obj);                                          /* $057790 a5 */
void Pieces(uint16_t obj);                                           /* $05790E a5 */
void Crash(uint16_t obj);                                            /* $057872 a5 */
void Scatter(uint16_t obj);                                          /* $05798C a5 */
void Wheel(uint16_t obj);                                            /* $057A4C a5 */
void Burster(uint16_t obj);                                          /* $057B96 a5 */
void SparkThrower(uint16_t obj);                                     /* $057C4C a5 */
void Fan(uint16_t obj);                                              /* $057CD0 a5 */
void Splash(uint16_t obj);                                           /* $057D7E a5 */
void SplashUp(uint16_t obj);                                         /* $057D68 a5 */
void RadialBurster(uint16_t obj);                                    /* $057E18 a5 */
void Burst4(uint16_t obj);                                           /* $057E80 a5 */
void Scrap(uint16_t obj);                                            /* $057F10 a5 */
void Sparkle(uint16_t obj);                                          /* $01A246 a5 */
void FrameFrom(uint16_t obj, uint32_t list);                         /* $056100 a5, a0 */
int SparkThrow(uint16_t obj, uint16_t *slot);                        /* $057C88 a5 -> Z, a0.w */
void RadialBurst(uint16_t obj, uint32_t sines, uint32_t cosines, uint16_t angle, uint16_t step, uint16_t rx, uint16_t ry, uint32_t list, uint16_t mask, uint16_t count); /* $057E46 a5, a1, a2, d1.w, d2.w, d3.w, d4.w, d5, d6.w, d7.w */

/* ---- the title screen (title.c) ---- */
void SpriteColumn(uint32_t buf, uint16_t x, uint16_t attr, uint32_t list, uint16_t y, uint16_t count); /* $01B908 a0, d0.w, d1.w, a2, d3.w, d4.w */
void TitleMenu(void);                                                /* $01B922 */
void TitleCursor(void);                                              /* $01B830 */
void TitleSelect(void);                                              /* $01B828 */
void TitleChoose(void);                                              /* $01B800 */
void CubeShow(void);                                                 /* $01BD40 */
void CubeClear(void);                                                /* $01BD08 */
uint16_t CubeShade(uint16_t bit, uint16_t angle);                    /* $01BD1E d0.w, d7.w -> d0.w */
void CubeEdge(uint16_t angle, uint16_t size, uint16_t centre, uint16_t base, uint16_t *up, uint16_t *down, uint16_t *uprow, uint16_t *downrow, uint16_t *d1, uint16_t *d2, uint16_t *d3); /* $01BC46 d6.w, d1.w, d5.w, d3.w -> a0.w, a1.w, a3.w, a4.w, d1.w, d2.w, d3.w */
uint32_t CubeSlope(uint16_t angle, uint16_t height, uint32_t *count); /* $01BC9C d6.w, d7.w -> d0, d7 */
void CubeRows(uint16_t up, uint16_t down, uint16_t uprow, uint16_t downrow, uint16_t step, uint16_t d1, uint16_t d2, uint16_t scroll, uint16_t count); /* $01BCC8 a0.w, a1.w, a3.w, a4.w, d0.w, d1.w, d2.w, d3.w, d7.w */
void CubeFaceA(uint16_t angle);                                      /* $01BB6E d7.w */
void CubeFaceB(uint16_t angle);                                      /* $01BBA4 d7.w */
void CubeFaceC(uint16_t angle);                                      /* $01BBDA d7.w */
void CubeFaceD(uint16_t angle);                                      /* $01BC10 d7.w */
void CubeTurn(void);                                                 /* $01BA74 */
void TitleCube(void);                                                /* $01BA6E */
void TitleSetup(void);                                               /* $01B7AA */
void TitleInput(void);                                               /* $01B67E */
void Title(void);                                                    /* $01B5B0 */
void PlaneAReset(void);                                              /* $00970C */
void PlaneBReset(void);                                              /* $00973C */

/* ---- the status bar (hud.c), the players joining (player.c) ---- */
void HudClear(void);                                                 /* $00A7B8 */
void HudSprites(void);                                               /* $00A7CC */
void HudBlank(uint16_t buf);                                         /* $00AF54 a4.w */
void HudIconsOff(uint16_t p);                                        /* $00AF72 a5 */
void HudIcons(uint16_t p);                                           /* $00AF94 a5 */
void HudFrame(uint16_t p, uint16_t buf);                             /* $00AB60 a5, a4.w */
uint16_t HudCanJoin(uint16_t p);                                     /* $00A964 a5 -> d0.w */
void HudAbsent(uint16_t p, uint16_t buf);                            /* $00A8B6 a5, a4.w */
void HudSwapCharacter(uint16_t p);                                   /* $00AA32 a5 */
void HudPickWeapon(uint16_t p);                                      /* $00AAA6 a5 */
void HudBlinkCharacter(uint16_t p, uint16_t buf);                    /* $00AB94 a5, a4.w */
void HudBlinkWeapon(uint16_t p, uint16_t buf);                       /* $00AC5A a5, a4.w */
void HudFace(uint16_t p, uint16_t buf);                              /* $00AD22 a5, a4.w */
void HudDigit(uint16_t digit, uint16_t *at, uint16_t *shown);        /* $00ADD4 d1.w -> a0.w, d3.w */
void HudScore(uint16_t p, uint16_t buf);                             /* $00AD8A a5, a4.w */
void HudVitality(uint16_t p, uint16_t buf);                          /* $00ADF0 a5, a4.w */
void HudHealth(uint16_t p);                                          /* $00AE50 a5 */
void HudIcon(uint16_t at, uint16_t weapon);                          /* $00AF42 a0.w, d0.w */
void HudWeapons(uint16_t p);                                         /* $00AEEA a5 */
void HudSelectCharacter(uint16_t p, uint16_t buf);                   /* $00A9E8 a5, a4.w */
void HudSelectWeapon(uint16_t p, uint16_t buf);                      /* $00AA5C a5, a4.w */
int HudOut(uint16_t p);                                              /* $00AB48 a5 -> Z */
uint16_t HudFlashLine(uint16_t p);                                   /* $00B0FE a5 -> d2.w */
void HudTint(uint16_t at, uint16_t count, uint16_t pal);             /* $00B0EE a0.w, d1.w, d2.w */
void HudPlayer(uint16_t p, uint16_t buf);                            /* $00AB08 a5, a4.w */
void HudPlayer1(void);                                               /* $00A86C */
void HudUpdate(void);                                                /* $00A82A */
void HudStart(void);                                                 /* $00A6DE */
void Player1JoinReset(void);                                         /* $00EBF6 */
void Player2JoinReset(void);                                         /* $00EC0A */
void PlayerJoinReset(uint16_t p);                                    /* $00EBEE a5 */
void PlayerJoin(uint16_t p);                                         /* $00EC3C a5 */

/* ---- the demo (demo.c), the difficulty (engine.c) ---- */
void DemoStage(void);                                                /* $01CC9C */
void DemoInput(void);                                                /* $01CCF4 */
void DemoStart(void);                                                /* $01CBF4 */
void Demo(void);                                                     /* $01CD42 */
void DifficultyAdjust(void);                                         /* $029636 */
void DifficultyUpdate(void);                                         /* $0295D0 */

/* ---- the story (story.c) ---- */
void SceneStill(void);                                               /* $01DD66 */
void SceneTextClear(void);                                           /* $01DE32 */
void SceneText(uint16_t text);                                       /* $01DD94 d0.w */
void SceneTextB(uint16_t text);                                      /* $01DDCA d0.w */
void SceneTextC(uint16_t text);                                      /* $01DE00 d0.w */
void SceneSprite(uint16_t x, uint16_t y);                            /* $01DFAC d0.w, d1.w */
void PlanePriority(uint32_t map, uint16_t count);                    /* $01DFD8 a0, d0.w */
void StoryA1Start(void);                                             /* $01CFA2 */
void StoryA1(void);                                                  /* $01D094 */
void StoryA2Start(void);                                             /* $01D142 */
void StoryA2Top(void);                                               /* $01D2E0 */
void StoryA2Climb(void);                                             /* $01D2C4 */
void StoryA2Wait(void);                                              /* $01D284 */
void StoryA2Pause(void);                                             /* $01D270 */
void StoryA2Done(void);                                              /* $01D30A */
void StoryA2(void);                                                  /* $01D21A */
void StoryA3Start(void);                                             /* $01D34E */
void StoryA3(void);                                                  /* $01D474 */
void StoryA4Actors(void);                                            /* $01D656 */
void StoryA4Start(void);                                             /* $01D534 */
void StoryA4(void);                                                  /* $01D5F8 */
void StoryPauseStart(void);                                          /* $01DCEE */
void StoryPause(void);                                               /* $01DCF8 */
void StoryToTitle(void);                                             /* $01DD3A */
void StoryToDemo(void);                                              /* $01DD50 */
void StoryB1Actors(void);                                            /* $01D778 */
void StoryB1Start(void);                                             /* $01D692 */
void StoryB1(void);                                                  /* $01D73E */
void StoryBob(void);                                                 /* $01D968 */
void StoryB2Soldiers(void);                                          /* $01D990 */
void StoryB2Others(void);                                            /* $01D9FA */
void StoryB2Start(void);                                             /* $01D7DC */
void StoryB2(void);                                                  /* $01D8B0 */
void StoryB3Actors(void);                                            /* $01DB66 */
void StoryB3Start(void);                                             /* $01DA44 */
void StoryB3(void);                                                  /* $01DB22 */
void StoryB4Start(void);                                             /* $01DBA2 */
void StoryB4(void);                                                  /* $01DC6E */
void StoryAStep(void);                                               /* $01CEA2 */
void StoryBStep(void);                                               /* $01CF72 */
void StoryA(void);                                                   /* $01CDFE */
void StoryB(void);                                                   /* $01CEDA */

/* ---- scattered types (objects20.c) ---- */
void BowPart(uint16_t obj);                                          /* $00650E a5 */
void BowPartStart(uint16_t obj);                                     /* $006524 a5 */
void BowPartMove(uint16_t obj);                                      /* $00652E a5 */
void BowPartIdle(uint16_t obj);                                      /* $0065F8 a5 */
void BowPartGone(uint16_t obj);                                      /* $006608 a5 */
void BowPartSetup(uint16_t obj);                                     /* $00660A a5 */
void Stage4Block(uint16_t obj);                                      /* $007656 a5 */
void CarrierB(uint16_t obj);                                         /* $01A946 a5 */
void CarrierBStart(uint16_t obj);                                    /* $01A964 a5 */
void CarrierBFly(uint16_t obj);                                      /* $01A9CE a5 */
void CarrierBFall(uint16_t obj);                                     /* $01AA6A a5 */
void CarrierBWait(uint16_t obj);                                     /* $01AAF0 a5 */
void CarrierBThrown(uint16_t obj);                                   /* $01AB4E a5 */
void CarrierBDrop(uint16_t obj, uint16_t slot);                      /* $01AB5E a5, a0 */
void Cue49C(uint16_t obj);                                           /* $0208E8 a5 */
void Cue49CNone(uint16_t obj);                                       /* $020940 a5 */
void Cue49CCue4(uint16_t obj);                                       /* $02095C a5 */
void Cue49CPose1(uint16_t obj);                                      /* $02096E a5 */
void Cue49CHold1(uint16_t obj);                                      /* $020980 a5 */
void Cue49CCue8(uint16_t obj);                                       /* $020982 a5 */
void Cue49CPose2(uint16_t obj);                                      /* $020994 a5 */
void Cue49CWait(uint16_t obj);                                       /* $0209A8 a5 */
void Cue49CCueC(uint16_t obj);                                       /* $0209B8 a5 */
void Cue49CPose3(uint16_t obj);                                      /* $0209CA a5 */
void Cue49CHold3(uint16_t obj);                                      /* $0209DC a5 */
void Cue49CCue10(uint16_t obj);                                      /* $0209DE a5 */
void Cue49CPose4(uint16_t obj);                                      /* $0209F0 a5 */
void Cue49CHold4(uint16_t obj);                                      /* $020A02 a5 */
void Cue4A0(uint16_t obj);                                           /* $020A04 a5 */
void Cue4A0None(uint16_t obj);                                       /* $020A5C a5 */
void Cue4A0Cue4(uint16_t obj);                                       /* $020A74 a5 */
void Cue4A0Pose1(uint16_t obj);                                      /* $020A8A a5 */
void Cue4A0Wait1(uint16_t obj);                                      /* $020AA2 a5 */
void Cue4A0Wait2(uint16_t obj);                                      /* $020AC4 a5 */
void Cue4A0Cue8(uint16_t obj);                                       /* $020ACC a5 */
void Cue4A0Pose3(uint16_t obj);                                      /* $020AE2 a5 */
void Cue4A0Wait3(uint16_t obj);                                      /* $020AF6 a5 */
void Cue4A0Wait4(uint16_t obj);                                      /* $020B16 a5 */
void Cue4A0CueC(uint16_t obj);                                       /* $020B26 a5 */
void Cue4A0Pose5(uint16_t obj);                                      /* $020B3C a5 */
void Cue4A0Play(uint16_t obj);                                       /* $020B4E a5 */
void Cue4A0Done(uint16_t obj);                                       /* $020B68 a5 */
void Cue4FC(uint16_t obj);                                           /* $020B6A a5 */
void Cue4FCNone(uint16_t obj);                                       /* $020BBE a5 */
void Cue4FCHide(uint16_t obj);                                       /* $020BDE a5 */
void Cue4FCShow1(uint16_t obj);                                      /* $020BEA a5 */
void Cue4FCShow2(uint16_t obj);                                      /* $020C0C a5 */
void Cue4FCShow3(uint16_t obj);                                      /* $020C3A a5 */
void Cue4FCShow4(uint16_t obj);                                      /* $020C68 a5 */
void BlueType(uint16_t obj);                                         /* $02250E a5 */
void BlueScene0(uint16_t obj);                                       /* $02252C a5 */
void BlueStart(uint16_t obj);                                        /* $022542 a5 */
void BlueWait(uint16_t obj);                                         /* $022580 a5 */
void BlueSink(uint16_t obj);                                         /* $0225A6 a5 */
void BlueDrift(uint16_t obj);                                        /* $0225C6 a5 */
void BlueDriftY(uint16_t obj, uint8_t n);                            /* $0225F0 a5, d0.b */
void StillType(uint16_t obj);                                        /* $02260C a5 */
void Still0(uint16_t obj);                                           /* $022642 a5 */
void Still4(uint16_t obj);                                           /* $02267C a5 */
void Still8(uint16_t obj);                                           /* $022696 a5 */
void StillC(uint16_t obj);                                           /* $0226B0 a5 */
void Still18(uint16_t obj);                                          /* $0226CA a5 */
void Still10(uint16_t obj);                                          /* $0226E4 a5 */
void Still14(uint16_t obj);                                          /* $0226FE a5 */
void StillAt(uint16_t obj);                                          /* $02274C a5 */
void BearerType(uint16_t obj);                                       /* $022778 a5 */
void BearerScene0(uint16_t obj);                                     /* $022796 a5 */
void BearerStart(uint16_t obj);                                      /* $0227B0 a5 */
void BearerIn(uint16_t obj);                                         /* $0227FE a5 */
void BearerWait(uint16_t obj);                                       /* $022818 a5 */
void BearerShow(uint16_t obj);                                       /* $022850 a5 */
void BearerBob2(uint16_t obj);                                       /* $022888 a5 */
void BearerShow2(uint16_t obj);                                      /* $0228C0 a5 */
void BearerBob(uint16_t obj);                                        /* $0228D2 a5 */
void CarriedType(uint16_t obj);                                      /* $02292C a5 */
void CarriedHidden(uint16_t obj);                                    /* $022956 a5 */
void CarriedShown4(uint16_t obj);                                    /* $022972 a5 */
void CarriedShown8(uint16_t obj);                                    /* $0229A8 a5 */
void CarriedShownC(uint16_t obj);                                    /* $0229DE a5 */
void CraftRiderType(uint16_t obj);                                   /* $022BB2 a5 */
void CraftRiderA(uint16_t obj);                                      /* $022BDC a5 */
void CraftRiderAStart(uint16_t obj);                                 /* $022BF4 a5 */
void CraftRiderAPose2(uint16_t obj);                                 /* $022C20 a5 */
void CraftRiderAPose3(uint16_t obj);                                 /* $022C3A a5 */
void CraftRiderASlow(uint16_t obj);                                  /* $022C54 a5 */
void CraftRiderATurn(uint16_t obj);                                  /* $022CA6 a5 */
void CraftRiderB(uint16_t obj);                                      /* $022CD6 a5 */
void CraftRiderBStart(uint16_t obj);                                 /* $022CEE a5 */
void CraftRiderBPose2(uint16_t obj);                                 /* $022D1A a5 */
void CraftRiderBPose3(uint16_t obj);                                 /* $022D34 a5 */
void CraftRiderBSlow(uint16_t obj);                                  /* $022D4E a5 */
void CraftRiderBTurn(uint16_t obj);                                  /* $022DA0 a5 */
void CraftRiderC(uint16_t obj);                                      /* $022DD0 a5 */
void CraftRiderCStart(uint16_t obj);                                 /* $022DE8 a5 */
void CraftRiderCPose2(uint16_t obj);                                 /* $022E14 a5 */
void CraftRiderCPose3(uint16_t obj);                                 /* $022E2E a5 */
void CraftRiderCSlow(uint16_t obj);                                  /* $022E48 a5 */
void CraftRiderCTurn(uint16_t obj);                                  /* $022E9A a5 */
void CraftRiderD(uint16_t obj);                                      /* $022ECA a5 */
void CraftRiderDStart(uint16_t obj);                                 /* $022EE2 a5 */
void CraftRiderDPose2(uint16_t obj);                                 /* $022F0E a5 */
void CraftRiderDPose3(uint16_t obj);                                 /* $022F28 a5 */
void CraftRiderDSlow(uint16_t obj);                                  /* $022F42 a5 */
void CraftRiderDGone(uint16_t obj);                                  /* $022F8E a5 */
void SmokerType(uint16_t obj);                                       /* $022FCE a5 */
void SmokerHidden(uint16_t obj);                                     /* $022FFC a5 */
void Smoker4(uint16_t obj);                                          /* $02300A a5 */
void Smoker8(uint16_t obj);                                          /* $023058 a5 */
void SmokerC(uint16_t obj);                                          /* $0230A6 a5 */
void Smoker10(uint16_t obj);                                         /* $0230F4 a5 */
void SmokerBob(uint16_t obj);                                        /* $02318C a5 */

/* ---- types $28016-$2D40E (objects28.c) ---- */
void MaskBlinker(uint16_t obj);                                      /* $028016 a5 */
void SatellitePair(uint16_t obj);                                    /* $028076 a5 */
void FallBurst(uint16_t obj);                                        /* $0281E6 a5 */
void Drifter(uint16_t obj);                                          /* $028262 a5 */
void BlinkOut(uint16_t obj);                                         /* $0282DA a5 */
void Swayer(uint16_t obj);                                           /* $028318 a5 */
void Sprayer(uint16_t obj);                                          /* $0283CC a5 */
void BoxBouncer(uint16_t obj);                                       /* $028504 a5 */
void ClusterTimer(uint16_t obj);                                     /* $028838 a5 */
void ClusterStart(uint16_t obj);                                     /* $028664 a5 */
void ClusterSteer(uint16_t obj);                                     /* $028866 a5 */
void ClusterSpread(uint16_t obj);                                    /* $028904 a5 */
void ClusterBlink(uint16_t obj);                                     /* $0289EA a5 */
void ClusterLooks(uint16_t obj);                                     /* $028A1C a5 */
void ClusterFireBurst(uint16_t obj);                                 /* $028AB0 a5 */
void ClusterFire(uint16_t obj);                                      /* $028AB6 a5 */
void ClusterMove(uint16_t obj);                                      /* $02873E a5 */
void ClusterWarn(uint16_t obj);                                      /* $028768 a5 */
void ClusterAfterBurst(uint16_t obj);                                /* $0287B2 a5 */
void ClusterSpray(uint16_t obj);                                     /* $0287D0 a5 */
void ClusterClose(uint16_t obj);                                     /* $0287F2 a5 */
void Cluster(uint16_t obj);                                          /* $028642 a5 */
void ClusterShotStart(uint16_t obj);                                 /* $028B24 a5 */
void ClusterShotFly(uint16_t obj);                                   /* $028B4C a5 */
void ClusterShotEnd(uint16_t obj);                                   /* $028BE6 a5 */
void ClusterShotPuff(uint16_t obj, uint16_t dx, uint16_t dy, uint32_t list);/* $028D2A a5, d1.w, d2.w, d3 */
void ClusterShotBurst(uint16_t obj);                                 /* $028BEE a5 */
void ClusterShot(uint16_t obj);                                      /* $028B0A a5 */
void OrbStart(uint16_t obj);                                         /* $028DA6 a5 */
void OrbAim(uint16_t obj);                                           /* $028E74 a5 */
void OrbHeld(uint16_t obj);                                          /* $028DE0 a5 */
void OrbFly(uint16_t obj);                                           /* $028E30 a5 */
void OrbFade(uint16_t obj);                                          /* $028E5E a5 */
void Orb(uint16_t obj);                                              /* $028D8C a5 */
void LauncherSpeed(uint16_t obj);                                    /* $029288 a5 */
void LauncherOut(uint16_t obj);                                      /* $02929E a5 */
void LauncherRideTime(uint16_t obj);                                 /* $02935E a5 */
void LauncherCarrier(uint16_t obj);                                  /* $0291F2 a5 */
void LauncherFire(uint16_t obj);                                     /* $0292C2 a5 */
void LauncherStart(uint16_t obj);                                    /* $028EEE a5 */
void LauncherRide(uint16_t obj);                                     /* $028F44 a5 */
void LauncherCross(uint16_t obj);                                    /* $028F76 a5 */
void LauncherAway(uint16_t obj);                                     /* $028FB0 a5 */
void Launcher(uint16_t obj);                                         /* $028ED4 a5 */
void LauncherShot(uint16_t obj);                                     /* $028FCC a5 */
void SmokerLook(uint16_t obj);                                       /* $029422 a5 */
void SmokerPuff(uint16_t obj);                                       /* $0293CC a5 */
void SmokerWeave(uint16_t obj);                                      /* $029380 a5 */
void SmokerStart(uint16_t obj);                                      /* $02904E a5 */
void SmokerRide(uint16_t obj);                                       /* $02907A a5 */
void SmokerCross(uint16_t obj);                                      /* $0290D0 a5 */
void Smoker(uint16_t obj);                                           /* $029038 a5 */
void PairRiderStart(uint16_t obj);                                   /* $029108 a5 */
void PairRiderLower(uint16_t obj);                                   /* $02913A a5 */
void PairRiderRide(uint16_t obj);                                    /* $029172 a5 */
void PairRiderCross(uint16_t obj);                                   /* $0291C6 a5 */
void PairRider(uint16_t obj);                                        /* $0290EE a5 */
void Shell25C(uint16_t obj);                                         /* $029466 a5 */
void PadMover(uint16_t obj);                                         /* $02A2C2 a5 */
void ObjectNothing(uint16_t obj);                                    /* $02A68A a5 */
int SlotFreeFrom(uint16_t *slot, uint16_t *type);                    /* $0560DE -> Z, a0.w, d0.w */
int SlotFreeFromC980(uint16_t *slot, uint16_t *type);                /* $0560DA -> Z, a0.w, d0.w */

/* ---- the boss on foot, type $380 (objects2f.c) ---- */
int FrameChance(void);                                               /* $02A2AA -> C */
void StageColoursFirst(void);                                        /* $02A1C2 */
void Boss380RideY(uint16_t obj);                                     /* $03074A a5 */
void Boss380Priority(uint16_t obj);                                  /* $030B76 a5 */
void Boss380Sparks(uint16_t obj);                                    /* $030AF0 a5 */
void Boss380BeamClear(void);                                         /* $030A4E */
void Boss380Down(uint16_t obj);                                      /* $030A6C a5 */
void Boss380DownFlying(uint16_t obj);                                /* $030ABC a5 */
void Boss380Blink(uint16_t obj);                                     /* $030D2A a5 */
void Boss380LandStand(uint16_t obj);                                 /* $030C7C a5 */
void Boss380StandStart(uint16_t obj);                                /* $030CC2 a5 */
uint16_t Boss380FacePlayer(uint16_t obj);                            /* $030962 a5 -> d0.w */
void Boss380Grunt(void);                                             /* $03127C */
void Boss380SmokeSlow(uint16_t obj);                                 /* $031216 a5 */
uint8_t Boss380Smoke(uint16_t obj);                                  /* $031222 a5 -> d0.b */
void Boss380FlyX(uint16_t obj);                                      /* $031108 a5 */
void Boss380FlyY(uint16_t obj);                                      /* $03107A a5 */
void Boss380Hover(uint16_t obj);                                     /* $03118C a5 */
void Boss380Home(uint16_t obj);                                      /* $030D4A a5 */
void Boss380SwoopX(uint16_t obj);                                    /* $030E46 a5 */
void Boss380SwoopY(uint16_t obj);                                    /* $030DB8 a5 */
void Boss380Shot(uint16_t obj, uint32_t vx, uint16_t type, uint16_t x, uint16_t arg); /* $030330 a5, d1, d2.w, d3.w, d4.w */
void Boss380Setup(uint16_t obj);                                     /* $02FB18 a5 */
void Boss380Stand(uint16_t obj);                                     /* $02FBC0 a5 */
void Boss380HitFlying(uint16_t obj);                                 /* $02FC64 a5 */
void Boss380Hit(uint16_t obj);                                       /* $02FCAC a5 */
void Boss380Knocked(uint16_t obj);                                   /* $02FCE8 a5 */
void Boss380Grabbed(uint16_t obj);                                   /* $02FD5E a5 */
void Boss380Thrown(uint16_t obj);                                    /* $02FD98 a5 */
void Boss380GrabbedFlying(uint16_t obj);                             /* $02FDF2 a5 */
void Boss380ThrownFlying(uint16_t obj);                              /* $02FE08 a5 */
void Boss380Tumble(uint16_t obj);                                    /* $02FE44 a5 */
void Boss380Rise(uint16_t obj);                                      /* $02FE72 a5 */
void Boss380Fall(uint16_t obj);                                      /* $02FE84 a5 */
void Boss380Fly(uint16_t obj);                                       /* $02FEDA a5 */
void Boss380FlyHold(uint16_t obj);                                   /* $02FF16 a5 */
void Boss380Swoop(uint16_t obj);                                     /* $02FF3C a5 */
void Boss380SwoopSparks(uint16_t obj);                               /* $02FF90 a5 */
void Boss380Charge(uint16_t obj);                                    /* $02FFBC a5 */
void Boss380Dash(uint16_t obj);                                      /* $02FFD6 a5 */
void Boss380Leap(uint16_t obj);                                      /* $030026 a5 */
void Boss380LeapRise(uint16_t obj);                                  /* $03004A a5 */
void Boss380Lift(uint16_t obj);                                      /* $03008C a5 */
void Boss380Throw(uint16_t obj);                                     /* $030116 a5 */
void Boss380AfterThrow(uint16_t obj);                                /* $030174 a5 */
void Boss380Approach(uint16_t obj);                                  /* $0301B2 a5 */
void Boss380Retreat(uint16_t obj);                                   /* $0301E2 a5 */
void Boss380JumpFall(uint16_t obj);                                  /* $030212 a5 */
void Boss380Crouch(uint16_t obj);                                    /* $030266 a5 */
void Boss380BeamWait(uint16_t obj);                                  /* $030280 a5 */
void Boss380Fire(uint16_t obj);                                      /* $0302AC a5 */
void Boss380Intro(uint16_t obj);                                     /* $030360 a5 */
void Boss380IntroText(uint16_t obj);                                 /* $0303C0 a5 */
void Boss380IntroWait(uint16_t obj);                                 /* $0303D4 a5 */
void Boss380IntroWalk(uint16_t obj);                                 /* $0303E8 a5 */
void Boss380IntroPause(uint16_t obj);                                /* $03042A a5 */
void Boss380IntroLook(uint16_t obj);                                 /* $03044C a5 */
void Boss380IntroReach(uint16_t obj);                                /* $030468 a5 */
void Boss380IntroToss(uint16_t obj);                                 /* $0304AC a5 */
void Boss380IntroWatch(uint16_t obj);                                /* $0304D8 a5 */
void Boss380IntroGone(uint16_t obj);                                 /* $0304F6 a5 */
void Boss380IntroPose(uint16_t obj);                                 /* $03052A a5 */
void Boss380IntroTalk(uint16_t obj);                                 /* $03054C a5 */
void Boss380IntroEnd(uint16_t obj);                                  /* $030572 a5 */
void Boss380Beaten(uint16_t obj);                                    /* $030754 a5 */
void Boss380BeatenLand(uint16_t obj);                                /* $030794 a5 */
void Boss380Recover(uint16_t obj);                                   /* $0307DA a5 */
void Boss380DefeatLand(uint16_t obj);                                /* $030834 a5 */
void Boss380DefeatTalk(uint16_t obj);                                /* $030890 a5 */
void Boss380DefeatLeave(uint16_t obj);                               /* $0308BA a5 */
void Boss380DefeatHop(uint16_t obj);                                 /* $030926 a5 */
void Boss380Ejected(uint16_t obj);                                   /* $030592 a5 */
void Boss380EjectedFall(uint16_t obj);                               /* $0305DA a5 */
void Boss380Return(uint16_t obj);                                    /* $030608 a5 */
void Boss380ReturnWalk(uint16_t obj);                                /* $030654 a5 */
void Boss380ReturnJump(uint16_t obj);                                /* $03067E a5 */
void Boss380ReturnLand(uint16_t obj);                                /* $0306B2 a5 */
void Boss380ReturnPose(uint16_t obj);                                /* $030700 a5 */
void Boss380ReturnDone(uint16_t obj);                                /* $03072E a5 */
void Boss380Idle(uint16_t obj);                                      /* $030748 a5 */
void Boss380Think(uint16_t obj);                                     /* $02F80C a5 */
void Boss380(uint16_t obj);                                          /* $02F804 a5 */

/* ---- types $31292-$36E88 (objects31.c) ---- */
void HaulerStart(uint16_t obj);                                      /* $032438 a5 */
void HaulerWait(uint16_t obj);                                       /* $0324C8 a5 */
void Hauler(uint16_t obj);                                           /* $032412 a5 */
uint16_t AboveCamera348(uint16_t d0);                                /* $03F768 d0.w -> d0.w */
void SetYAboveCamera348(uint16_t obj, uint16_t d0);                  /* $03F75E a5, d0.w */
uint16_t BelowCamera220(uint16_t d1);                                /* $040AC6 d1.w -> d1.w */
void Boss380Spawn(uint16_t obj);                                     /* $031292 a5 */
void Lobbed(uint16_t obj);                                           /* $0312BC a5 */
void FlameBox(uint16_t o);                                           /* $031406 a0 */
void FlameBob(uint16_t obj);                                         /* $031356 a5 */
void FlameHoming(uint16_t obj);                                      /* $031422 a5 */
void RiderSoldierDown(uint16_t obj);                                 /* $03176A a5 */
void RiderSoldierFlung(uint16_t obj);                                /* $031794 a5 */
uint16_t RiderSoldierAim(uint16_t obj, uint16_t *d7, uint32_t *a1, uint32_t *a2);/* $0317EE a5 -> a0.w, d7.w, a1, a2 */
void RiderSoldierWait(uint16_t obj);                                 /* $03180C a5 */
uint16_t RiderSoldierRide(uint16_t obj);                             /* $03183A a5 -> a0.w */
void RiderSoldierStart(uint16_t obj);                                /* $031588 a5 */
void RiderSoldierLie(uint16_t obj);                                  /* $0315E4 a5 */
void RiderSoldierFly(uint16_t obj);                                  /* $0315EE a5 */
void RiderSoldierFall(uint16_t obj);                                 /* $031616 a5 */
void RiderSoldierAimStart(uint16_t obj);                             /* $031676 a5 */
void RiderSoldierAiming(uint16_t obj);                               /* $03168C a5 */
void RiderSoldierShot(uint16_t obj);                                 /* $0316B2 a5 */
void RiderSoldierDropStart(uint16_t obj);                            /* $0316C6 a5 */
void RiderSoldierDrop(uint16_t obj);                                 /* $0316E0 a5 */
void RiderSoldier(uint16_t obj);                                     /* $031488 a5 */
void TrailDust(uint16_t obj);                                        /* $034A28 a5 */
void OnlookerPlace(uint16_t obj);                                    /* $031924 a5 */
void Onlooker(uint16_t obj);                                         /* $03184C a5 */
void WaveSpawn(uint16_t obj);                                        /* $0319C8 a5 */
void WaveEnd(uint16_t obj);                                          /* $031994 a5 */
void WaveSpawning(uint16_t obj);                                     /* $03197E a5 */
void WaveStart(uint16_t obj);                                        /* $03195E a5 */
void Wave(uint16_t obj);                                             /* $031948 a5 */
void Idle190(uint16_t obj);                                          /* $031A26 a5 */
void DropshipDrop(uint16_t obj);                                     /* $031DBC a5 */
void DropshipExhaust(uint16_t obj);                                  /* $031DF6 a5 */
void DropshipStart(uint16_t obj);                                    /* $031A9E a5 */
void DropshipWait(uint16_t obj);                                     /* $031AEE a5 */
void DropshipRise(uint16_t obj);                                     /* $031B0E a5 */
void DropshipClimb(uint16_t obj);                                    /* $031B30 a5 */
void DropshipAway(uint16_t obj);                                     /* $031B5E a5 */
void DropshipSwoopDown(uint16_t obj);                                /* $031B96 a5 */
void DropshipSwoopUp(uint16_t obj);                                  /* $031BC6 a5 */
void DropshipHit(uint16_t obj);                                      /* $031BFE a5 */
void DropshipThrown(uint16_t obj);                                   /* $031C32 a5 */
void DropshipBurning(uint16_t obj);                                  /* $031C78 a5 */
void DropshipLeave(uint16_t obj);                                    /* $031CE2 a5 */
void Dropship(uint16_t obj);                                         /* $031A28 a5 */
void LeaperStart(uint16_t obj);                                      /* $031E76 a5 */
void LeaperHop(uint16_t obj);                                        /* $031EF4 a5 */
void Leaper(uint16_t obj);                                           /* $031E42 a5 */
void HoverMount(uint16_t obj, uint16_t which);                       /* $032C1A a5, d7.w */
void HoverMountAt(uint16_t obj, uint16_t dx, uint16_t dy);           /* $032C96 a5, d2.w, d3.w */
void HoverMountPlace(uint16_t obj);                                  /* $032C90 a5 */
void HoverPuff(uint16_t obj);                                        /* $032CC0 a5 */
void HoverSparks(uint16_t obj);                                      /* $032D1A a5 */
void HoverFace(uint16_t obj);                                        /* $032E12 a5 */
void HoverPick(uint16_t obj);                                        /* $032E48 a5 */
void HoverSteer(uint16_t obj);                                       /* $032E72 a5 */
void HoverFacePlayer(uint16_t obj);                                  /* $032F32 a5 */
void HoverDrift(uint16_t obj);                                       /* $032F4C a5 */
void HoverHum(void);                                                 /* $032FCE */
void HoverWait(uint16_t obj);                                        /* $032FE6 a5 */
void CarrierRise(uint16_t obj);                                      /* $0324E8 a5 */
void CarrierClimb(uint16_t obj);                                     /* $03254E a5 */
void CarrierTroops(uint16_t obj);                                    /* $03255E a5 */
void CarrierBoss(uint16_t obj);                                      /* $032594 a5 */
void CarrierEnd(uint16_t obj);                                       /* $0325BE a5 */
void EscortStart(uint16_t obj);                                      /* $0325FA a5 */
void EscortWait(uint16_t obj);                                       /* $03260E a5 */
void EscortRise(uint16_t obj);                                       /* $032650 a5 */
void Escort(uint16_t obj);                                           /* $0325D8 a5 */
void HoverSoldierStart(uint16_t obj);                                /* $032716 a5 */
void HoverSoldierFly(uint16_t obj);                                  /* $03275C a5 */
void HoverSoldierDrift(uint16_t obj);                                /* $032784 a5 */
void HoverSoldierShoot(uint16_t obj);                                /* $0327A8 a5 */
void HoverSoldierLeave(uint16_t obj);                                /* $0327CE a5 */
void HoverSoldier(uint16_t obj);                                     /* $0326A6 a5 */

/* ---- types near $3D000 (objects38.c) ---- */
void DebrisPuff(uint16_t obj);                                       /* $03D2A6 a5 */
void SparksAt(uint16_t x, uint16_t y);                               /* $03D342 d1.w, d2.w */
void SparksAtDE80(void);                                             /* $03D33A */
void GroupPlace(uint16_t obj, uint16_t count);                       /* $043906 a5, d7.w */
void TimerStart(uint16_t obj);                                       /* $03910C a5 */
void TimerDown(uint16_t obj);                                        /* $039166 a5 */
void TimerOut(uint16_t obj);                                         /* $03915A a5 */
void Timer(uint16_t obj);                                            /* $0390EA a5 */
void ClockStart(uint16_t obj);                                       /* $0392BC a5 */
void ClockUp(uint16_t obj);                                          /* $0392DC a5 */
void ClockOut(uint16_t obj);                                         /* $0392CC a5 */
void ClockShow(uint16_t obj);                                        /* $0392D8 a5 */
void Clock(uint16_t obj);                                            /* $039296 a5 */
void CounterStart(uint16_t obj);                                     /* $039390 a5 */
void CounterWait(uint16_t obj);                                      /* $03939A a5 */
void CounterUp(uint16_t obj);                                        /* $0393C0 a5 */
void CounterDown(uint16_t obj);                                      /* $0393EA a5 */
void Counter(uint16_t obj);                                          /* $039376 a5 */
void Splinter(uint16_t obj);                                         /* $03AB84 a5 */
void Beacon(uint16_t obj);                                           /* $03DB94 a5 */
void WeightStart(uint16_t obj);                                      /* $03DC28 a5 */
void WeightFall(uint16_t obj);                                       /* $03DC4E a5 */
void WeightRest(uint16_t obj);                                       /* $03DC9C a5 */
void Weight(uint16_t obj);                                           /* $03DC04 a5 */
void Creeper(uint16_t obj);                                          /* $03F564 a5 */
void Afterimage(uint16_t obj);                                       /* $03F592 a5 */
void EmberPlace(uint16_t obj, uint16_t radius);                      /* $03F6CE a5, d7.w */
void EmberDrift(uint16_t obj);                                       /* $03F734 a5 */
void Fleck(uint16_t obj);                                            /* $03F68A a5 */
void EmberRing(uint16_t obj);                                        /* $03F65A a5 */
void FurnaceStart(uint16_t obj);                                     /* $03A5C4 a5 */
void FurnaceWait(uint16_t obj);                                      /* $03A616 a5 */
void FurnaceOpen(uint16_t obj);                                      /* $03A64E a5 */
void FurnaceOpenOn(uint16_t obj);                                    /* $03A644 a5 */
void FurnaceBreathe(uint16_t obj);                                   /* $03A688 a5 */
void FurnaceBreatheOn(uint16_t obj);                                 /* $03A67E a5 */
void FurnaceBeaten(uint16_t obj);                                    /* $03A6EE a5 */
void FurnaceGone(uint16_t obj);                                      /* $03A74A a5 */
void FurnaceBossWait(uint16_t obj);                                  /* $03A76E a5 */
void FurnaceBossOpen(uint16_t obj);                                  /* $03A7BC a5 */
void FurnaceBossRoll(uint16_t obj);                                  /* $03A80A a5 */
void FurnaceBossJolt(uint16_t obj);                                  /* $03A886 a5 */
void Furnace(uint16_t obj);                                          /* $03A53C a5 */
void ClawerStart(uint16_t obj);                                      /* $03F802 a5 */
void ClawerWait(uint16_t obj);                                       /* $03F810 a5 */
void ClawerMessage(uint16_t obj);                                    /* $03F832 a5 */
void ClawerArrive(uint16_t obj);                                     /* $03F848 a5 */
void ClawerCrouch(uint16_t obj);                                     /* $03F88C a5 */
void ClawerLeap(uint16_t obj);                                       /* $03F8A8 a5 */
void ClawerLand(uint16_t obj);                                       /* $03F8FE a5 */
void ClawerFall(uint16_t obj);                                       /* $03F978 a5 */
void ClawerRise(uint16_t obj);                                       /* $03F9C6 a5 */
void ClawerShudder(uint16_t obj);                                    /* $03FA60 a5 */
void ClawerGlow(uint16_t obj);                                       /* $03FA20 a5 */
void ClawerSpark(uint16_t obj);                                      /* $03FADC a5 */
void ClawerStood(uint16_t obj);                                      /* $03FB12 a5 */
void ClawerChoose(uint16_t obj);                                     /* $03FB44 a5 */
void ClawerLiftRight(uint16_t obj);                                  /* $03FBA0 a5 */
void ClawerPause(uint16_t obj);                                      /* $03FBDC a5 */
void ClawerGrindLeft(uint16_t obj);                                  /* $03FC04 a5 */
void ClawerStampRight(uint16_t obj);                                 /* $03FC80 a5 */
void ClawerLiftLeft(uint16_t obj);                                   /* $03FC98 a5 */
void ClawerGrindRight(uint16_t obj);                                 /* $03FCD4 a5 */
void ClawerStampLeft(uint16_t obj);                                  /* $03FD3A a5 */
void ClawerLiftLeftOut(uint16_t obj);                                /* $03FD5C a5 */
void ClawerStampLeftOut(uint16_t obj);                               /* $03FD98 a5 */
void ClawerLiftRightIn(uint16_t obj);                                /* $03FDB0 a5 */
void ClawerStampRightIn(uint16_t obj);                               /* $03FDEC a5 */
void Clawer(uint16_t obj);                                           /* $03F774 a5 */

/* ---- types $48044-$4B9B2 (objects48.c) ---- */
void BrawlerAnim(uint16_t obj);                                      /* $047F8C a5 */
void BrawlerAnimStart(uint16_t obj);                                 /* $047F86 a5 */
void BrawlerAnimOnFlipped(uint16_t obj);                             /* $047F7A a5 */
void BrawlerAnimOnBySpeed(uint16_t obj);                             /* $047F70 a5 */
void BrawlerAnimBySpeed(uint16_t obj);                               /* $047F4C a5 */
void BrawlerAnimFacing(uint16_t obj);                                /* $047F3E a5 */
int BrawlerFall(uint16_t obj);                                       /* $047F0E a5 -> Z */
void BrawlerWalls(uint16_t obj);                                     /* $047E50 a5 */
void BrawlerHitJump(uint16_t obj);                                   /* $047E9A a5 */
int BrawlerNear(uint16_t obj);                                       /* $047CE0 a5, a6=reg_a6 -> Z */
void BrawlerPuff(uint16_t obj);                                      /* $047D4C a5 */
void DiceRoomClear(void);                                            /* $041044 */
void DiceRoomMark(void);                                             /* $041E4A */
void DiceRoomEnd(uint16_t obj);                                      /* $041E10 a5 */
void BlastSound8(void);                                              /* $02A262 */
void Brawler4CStart(uint16_t obj);                                   /* $048120 a5 */
void Brawler4CStand(uint16_t obj);                                   /* $048160 a5 */
void Brawler4CWalkLeft(uint16_t obj);                                /* $0481A0 a5 */
void Brawler4CWalkRight(uint16_t obj);                               /* $0481BE a5 */
void Brawler4CKnocked(uint16_t obj);                                 /* $0481D8 a5 */
void Brawler4CLying(uint16_t obj);                                   /* $0481E2 a5 */
void Brawler4CHit(uint16_t obj);                                     /* $0481EC a5 */
void Brawler4CThrown(uint16_t obj);                                  /* $048200 a5 */
void Brawler4CFall(uint16_t obj);                                    /* $04821C a5 */
void Brawler4CCrouch(uint16_t obj, uint32_t d0);                     /* $048258 a5, d0 */
void Brawler4CJump(uint16_t obj);                                    /* $0482B2 a5 */
void Brawler4CJumpFall(uint16_t obj);                                /* $0482DA a5 */
void Brawler4CAttack(uint16_t obj);                                  /* $0482E8 a5 */
void Brawler4C(uint16_t obj);                                        /* $048044 a5 */
void FireSpinner(uint16_t obj);                                      /* $048328 a5 */

/* ---- the selects and the stage's start (modes.c) ---- */
void SelectColours(void);                                            /* $01F4BE */
void SelectTextBox(uint16_t box);                                    /* $01F456 a0.w */
void SelectTextBoxB(uint16_t box);                                   /* $01F482 a0.w */
void SelectScrollLine(void);                                         /* $01F4B0 */
void Player2KeepChoices(void);                                       /* $00EE0E */
void PlayersKeepChoices(void);                                       /* $00EDEE */
void PlayersStart(void);                                             /* $00EB58 */
void SelectArrows(void);                                             /* $01E2AC */
void WeaponSelectStart(void);                                        /* $01E18C */
uint16_t WeaponPending(void);                                        /* $01E4AC -> d0.w */
void WeaponPlayersReady(void);                                       /* $01E368 */
void WeaponTextCharacter(void);                                      /* $01E5DA */
void WeaponTextWeapon(void);                                         /* $01E614 */
void StageText(void);                                                /* $01E63E */
void WeaponNothing(uint16_t p, uint16_t arrow);                      /* $01E5D8 a5, a4.w */
void WeaponPickCharacter(uint16_t p, uint16_t arrow);                /* $01E4E2 a5, a4.w */
void WeaponPickWeapon(uint16_t p, uint16_t arrow);                   /* $01E552 a5, a4.w */
void WeaponPlayer(uint16_t p, uint16_t arrow);                       /* $01E4CC a5, a4.w */
void WeaponArrowDone(uint16_t p, uint16_t arrow);                    /* $01E6B0 a5, a4.w */
void WeaponArrowCharacter(uint16_t p, uint16_t arrow);               /* $01E676 a5, a4.w */
void WeaponArrowWeapon(uint16_t p, uint16_t arrow);                  /* $01E68A a5, a4.w */
void WeaponArrow(uint16_t p, uint16_t arrow);                        /* $01E660 a5, a4.w */
void WeaponArrow2Done(uint16_t p, uint16_t arrow);                   /* $01E70E a5, a4.w */
void WeaponArrow2Character(uint16_t p, uint16_t arrow);              /* $01E6D4 a5, a4.w */
void WeaponArrow2Weapon(uint16_t p, uint16_t arrow);                 /* $01E6E8 a5, a4.w */
void WeaponFadeIn(void);                                             /* $01E354 */
void WeaponScrollRows(void);                                         /* $01E42E */
void WeaponScroll(void);                                             /* $01E412 */
void WeaponChoose(void);                                             /* $01E38A */
void WeaponStep(void);                                               /* $01E33E */
void SelectPalette(void);                                            /* $01E728 */
void WeaponMark1(void);                                              /* $01E746 */
void SelectCards(uint16_t x);                                        /* $01E7CE d0.w */
void WeaponCards(void);                                              /* $01E7A6 */
void WeaponSelectStep(void);                                         /* $01E332 */
void WeaponSelect(void);                                             /* $01E2D6 */
void StageBlocks(void);                                              /* $01EAA6 */
void StageChunk(uint16_t stage);                                     /* $01EB1C d0.w */
void StageClearedChunks(void);                                       /* $01EAE0 */
void StageIcons(void);                                               /* $01EE1E */
void StageIconsMoved(void);                                          /* $01EE46 */
void StageIconsPlace(void);                                          /* $01F000 */
void StageSelectStart(void);                                         /* $01E8C4 */
uint8_t SelectPressed(void);                                         /* $01EEC8 -> d0.b */
void StageFadeIn(void);                                              /* $01EBD8 */
void StageScrollIn(void);                                            /* $01EC04 */
void StageArrive(void);                                              /* $01EBE2 */
void StageNothing(uint16_t p);                                       /* $01EDD2 a5 */
void StagePick(uint16_t p);                                          /* $01ED44 a5 */
void StageMove(uint16_t p);                                          /* $01ED32 a5 */
void StageChoose(void);                                              /* $01EC46 */
void StageDescribe(void);                                            /* $01ECC2 */
void StageLeave(void);                                               /* $01ED0C */
void StageMarks(void);                                               /* $01EDDC */
void StageStep(void);                                                /* $01EBB6 */
void StageSelectStep(void);                                          /* $01EBAE */
void StageSelect(void);                                              /* $01EB52 */
void FirstWait(void);                                                /* $01F3D0 */
void FirstSwapped(void);                                             /* $01F390 */
void FirstToggle(void);                                              /* $01F3DE */
void FirstChoose(void);                                              /* $01F36A */
void FirstReady(void);                                               /* $01F34A */
void FirstShow(void);                                                /* $01F32E */
void FirstAsk(void);                                                 /* $01F304 */
void FirstText(void);                                                /* $01F2EC */
void FirstScene(void);                                               /* $01F280 */
void FirstFadeIn(void);                                              /* $01F26C */
void FirstLeave(void);                                               /* $01F2A0 */
void FirstStep(void);                                                /* $01F256 */
void FirstSelect(void);                                              /* $01F1FA */
void StageClearTime(void);                                           /* $01F548 */
void StageEnter(void);                                               /* $01F4DE */
void ScrollFrameSelect(void);                                        /* $00A2FE */

/* ---- the stage play (modes.c) and its messages (messages.c) ---- */
void StagePlayNothing(void);                                         /* $01F8EA */
void PauseText(void);                                                /* $01F686 */
void StagePlay(void);                                                /* $01F566 */
void MessageLayout(uint32_t *text, uint16_t *list, uint16_t *y, uint16_t *x); /* $038EF8 -> a0, a1.w, d1.w, d2.w */
void MessageLayoutTop(uint32_t *text, uint16_t *list, uint16_t *y, uint16_t *x); /* $038EF2 -> a0, a1.w, d1.w, d2.w */
void MessageNone(void);                                              /* $038926 */
void MessageStart(void);                                             /* $038928 */
void MessageWait(void);                                              /* $038978 */
void MessageType(void);                                              /* $0389C2 */
void MessageBlink(void);                                             /* $0389AC */
void MessageVitality(void);                                          /* $038AB0 */
void MessageVitalityBlink(void);                                     /* $038AD0 */
void MessageVitalityCount(void);                                     /* $038B22 */
void MessageVitalityStart(void);                                     /* $038BA0 */
void MessageVitalityShow(void);                                      /* $038BE8 */
void MessageShow(void);                                              /* $038E6E */
void MessageBonusStart(void);                                        /* $038C44 */
void MessageBonus(void);                                             /* $038C4E */
void AddBcdAt(uint16_t end_at, uint32_t points);                     /* $038E42 a0.w, d0 */
void MessageBonusCount(void);                                        /* $038CAC */
void MessageBonusDone(void);                                         /* $038E24 */
void PalettesFull(void);                                             /* $00369E */
void QueueDmaList(uint32_t list);                                    /* $03985C a3 */
void RingLetter(void);                                               /* $0396A4 */
void RingLetterBack(void);                                           /* $0396E6 */
void MessageRing(void);                                              /* $03958E */
void MessageWarpStart(void);                                         /* $038F38 */
void MessageWarp(void);                                              /* $038F6A */
void MessageFlash(void);                                             /* $03902A */
void MessageFlashStart(void);                                        /* $039016 */
void MessageOutroStart(void);                                        /* $039052 */
void MessageOutro(void);                                             /* $039070 */
void MessageOutroWait(void);                                         /* $03905E */
void MessageArrow(void);                                             /* $03947C */
void MessageArrowDown(void);                                         /* $039466 */
void MessageArrowOther(void);                                        /* $03944C */
void Messages(void);                                                 /* $038882 */

/* ---- a stage cleared, the continue (modes.c) ---- */
void StageClearedMark(void);                                         /* $020156 */
void StageGemMark(void);                                             /* $02013A */
void StageCleared(void);                                             /* $0200AC */
void GameOverWaves(void);                                            /* $020172 */
void ContinueCursor(void);                                           /* $01FE04 */
void ContinueColours(void);                                          /* $01FEB6 */
void ContinueFlash(void);                                            /* $01FE72 */
void ContinueArrow(void);                                            /* $01FF3C */
void ContinueText(void);                                             /* $01FF56 */
void ContinueInput(void);                                            /* $01FCAA */
void GameOverWait(void);                                             /* $01FD62 */
void GameOver(void);                                                 /* $01FBEE */

/* ---- types $32000-$36FFF (objects32.c) ---- */
void Pebble(uint16_t obj);                                           /* $033270 a5 */
void MessageCue(uint16_t obj);                                       /* $03577E a5 */
void FallingPiece(uint16_t obj);                                     /* $036E54 a5 */
void FollowerStart(uint16_t obj);                                    /* $03567E a5 */
void FollowerSteer(uint16_t obj);                                    /* $0356B6 a5 */
void FollowerMove(uint16_t obj);                                     /* $0356B0 a5 */
void Follower(uint16_t obj);                                         /* $03566C a5 */
void Balloon(uint16_t obj);                                          /* $0350CC a5 */
void PartPlace(uint16_t obj, uint16_t i);                            /* $034122 a5, d0.w, a6=reg_a6 */
void GunboatStart(uint16_t obj);                                     /* $0351A8 a5 -> a6=reg_a6 */
void GunboatFly(uint16_t obj);                                       /* $035286 a5, a6=reg_a6 */
void Gunboat(uint16_t obj);                                          /* $03518A a5 -> a6=reg_a6 */
void TwinCraftStart(uint16_t obj);                                   /* $0353FC a5 */
void TwinCraftParts(uint16_t obj);                                   /* $035584 a5 */
void TwinCraftLight(uint16_t obj);                                   /* $0355CC a5 */
void TwinCraftFly(uint16_t obj);                                     /* $035522 a5 */
void TwinCraft(uint16_t obj);                                        /* $0353EA a5 */
void MastSetY(uint16_t obj, uint16_t y);                             /* $033262 a5, d0.w */
void MastY(uint16_t obj);                                            /* $03325A a5 */
void MastStart(uint16_t obj);                                        /* $03302A a5 */
void MastWait(uint16_t obj);                                         /* $03306E a5 */
void MastRiseWait(uint16_t obj);                                     /* $03306A a5 */
void MastRise(uint16_t obj);                                         /* $03307C a5 */
void MastFight(uint16_t obj);                                        /* $03311A a5 */
void MastSink(uint16_t obj);                                         /* $0331B6 a5 */
void MastEnd(uint16_t obj);                                          /* $0331EA a5 */
void Mast(uint16_t obj);                                             /* $033004 a5 */
void DropItem(uint16_t obj);                                         /* $05861A a5 */
void DropItemRandom(uint16_t obj);                                   /* $0585DC a5 */
void HoverBossWait(uint16_t obj);                                    /* $0328E2 a5 */
void HoverBossDrift(uint16_t obj);                                   /* $032950 a5 */
void HoverBossSteer(uint16_t obj);                                   /* $032982 a5 */
void HoverBossLanded(uint16_t obj);                                  /* $0329B0 a5 */
void HoverBossRise(uint16_t obj);                                    /* $0329E8 a5 */
void HoverBossLeap(uint16_t obj);                                    /* $032A16 a5 */
void HoverBossHeld(uint16_t obj);                                    /* $032A76 a5 */
void HoverBossSlamLeft(uint16_t obj);                                /* $032A96 a5 */
void HoverBossSlam(uint16_t obj);                                    /* $032A8A a5 */
void HoverBossBeaten(uint16_t obj);                                  /* $032AF0 a5 */
void HoverBoss(uint16_t obj);                                        /* $0327F2 a5 */
void JetTrooperStart(uint16_t obj);                                  /* $0343FE a5 */
void JetTrooperHover(uint16_t obj);                                  /* $034450 a5, a6=reg_a6 */
void JetTrooperLeft(uint16_t obj);                                   /* $03447A a5, a6=reg_a6 */
void JetTrooperRight(uint16_t obj);                                  /* $0344BA a5, a6=reg_a6 */
void JetTrooperPause(uint16_t obj);                                  /* $0344FA a5, a6=reg_a6 */
void JetTrooperAim(uint16_t obj);                                    /* $034512 a5, a6=reg_a6 */
void JetTrooperFired(uint16_t obj);                                  /* $034552 a5, a6=reg_a6 */
void JetTrooperFallLeft(uint16_t obj);                               /* $034566 a5 */
void JetTrooperFallRight(uint16_t obj);                              /* $034578 a5 */
void JetTrooperLeave(uint16_t obj);                                  /* $03458A a5, a6=reg_a6 */
void JetTrooper(uint16_t obj);                                       /* $03438A a5 -> a6=reg_a6 */
void HulkStart(uint16_t obj);                                        /* $036EB0 a5 */
void HulkWait(uint16_t obj);                                         /* $036EF6 a5 */
void HulkFight(uint16_t obj);                                        /* $036F36 a5 */
void HulkBeaten(uint16_t obj);                                       /* $036F6A a5 */
void HulkBlowUp(uint16_t obj);                                       /* $036FA8 a5 */
void HulkWreck(uint16_t obj);                                        /* $03705E a5 */
void Hulk(uint16_t obj);                                             /* $036E88 a5 */
void WagonDrive(uint16_t obj, uint16_t back, uint16_t ahead);        /* $03385E a5, d0.w, d1.w */
void WagonGroup(uint32_t list, uint16_t slot, uint16_t x, uint16_t y);/* $033908 a0, a1, d0.w, d1.w */
void WagonNext(void);                                                /* $033A8E */
void WagonStart(uint16_t obj);                                       /* $0348B8 a5 */
void WagonRise(uint16_t obj);                                        /* $034946 a5 */
void WagonAim(uint16_t obj);                                         /* $034980 a5 */
void WagonShot(uint16_t obj);                                        /* $0349C6 a5 */
void WagonDust(uint16_t obj);                                        /* $034A1E a5 */
void WagonEngine(uint16_t obj);                                      /* $034B58 a5 */
void WagonWheel(uint16_t obj);                                       /* $034A88 a5 */
void WagonArrive(uint16_t obj);                                      /* $034B9A a5 */
void WagonFlashUp(uint16_t obj);                                     /* $034BF6 a5 */
void WagonFlashDown(uint16_t obj);                                   /* $034C5A a5 */
void WagonWait(uint16_t obj);                                        /* $034CC2 a5 */
void WagonPace(uint16_t obj);                                        /* $034CEC a5 */
void WagonShelling(uint16_t obj);                                    /* $034D1E a5 */
void WagonRide(uint16_t obj);                                        /* $034AF6 a5 */
void WagonCargoStart(uint16_t obj);                                  /* $034B06 a5 */
void WagonCargo(uint16_t obj);                                       /* $034B42 a5 */
void WagonBeaten(uint16_t obj);                                      /* $034D72 a5 */
void WagonSinking(uint16_t obj);                                     /* $034DAC a5 */
void WagonSunk(uint16_t obj);                                        /* $034DCC a5 */
void WagonThrowerUp(uint16_t obj);                                   /* $034DF8 a5 */
void WagonThrown(uint16_t obj);                                      /* $034E92 a5 */
void WagonThrow(uint16_t obj);                                       /* $034E32 a5 */
void Wagon(uint16_t obj);                                            /* $034814 a5 */
void WagonDriveY(uint16_t obj, uint16_t back, uint16_t ahead);       /* $0338AE a5, d0.w, d1.w */
void Stage1DirectorRide1(uint16_t obj);                              /* $0336DE a5 */
void Stage1DirectorStop1(uint16_t obj);                              /* $0336EE a5 */
void Stage1DirectorRide2(uint16_t obj);                              /* $033762 a5 */
void Stage1DirectorStop2(uint16_t obj);                              /* $033772 a5 */
void Stage1DirectorRide3(uint16_t obj);                              /* $0337E4 a5 */
void Stage1DirectorWave3(uint16_t obj);                              /* $0337D6 a5 */
void Stage1DirectorRide4(uint16_t obj);                              /* $033802 a5 */
void Stage1DirectorWave4(uint16_t obj);                              /* $0337F4 a5 */
void Stage1DirectorRide5(uint16_t obj);                              /* $033820 a5 */
void Stage1DirectorWave5(uint16_t obj);                              /* $033812 a5 */
void Stage1DirectorBoss(uint16_t obj);                               /* $033830 a5 */
void Stage1DirectorStart(uint16_t obj);                              /* $03330A a5 */
void Stage1DirectorScroll(uint16_t obj);                             /* $033320 a5 */
void Stage1DirectorBegin(uint16_t obj);                              /* $03333A a5 */
void Stage1DirectorTroopers(uint16_t obj);                           /* $03335A a5 */
void Stage1DirectorCarts(uint16_t obj);                              /* $0333A0 a5 */
void Stage1DirectorRide(uint16_t obj);                               /* $0333D0 a5 */
void Stage1DirectorClear(uint16_t obj);                              /* $033404 a5 */
void Stage1DirectorCleared(uint16_t obj);                            /* $03341E a5 */
void Stage1DirectorPause(uint16_t obj);                              /* $03343E a5 */
void Stage1DirectorJets(uint16_t obj);                               /* $033452 a5 */
void Stage1DirectorClimb(uint16_t obj);                              /* $0334A2 a5 */
void Stage1DirectorTroopers2(uint16_t obj);                          /* $0334AA a5 */
void Stage1DirectorRides(uint16_t obj);                              /* $03350E a5 */
void Stage1DirectorBossWait(uint16_t obj);                           /* $033532 a5 */
void Stage1DirectorEnd(uint16_t obj);                                /* $033556 a5 */
void Stage1Director(uint16_t obj);                                   /* $0332B8 a5 */
void WallTrooperStart(uint16_t obj);                                 /* $033B60 a5 */
void WallTrooperHover(uint16_t obj);                                 /* $033BCA a5, a6=reg_a6 */
void WallTrooperUp(uint16_t obj);                                    /* $033BF0 a5, a6=reg_a6 */
void WallTrooperDown(uint16_t obj);                                  /* $033C30 a5, a6=reg_a6 */
void WallTrooperPause(uint16_t obj);                                 /* $033C70 a5, a6=reg_a6 */
void WallTrooperAim(uint16_t obj);                                   /* $033C88 a5, a6=reg_a6 */
void WallTrooperFired(uint16_t obj);                                 /* $033CC8 a5, a6=reg_a6 */
void WallTrooperFallUp(uint16_t obj);                                /* $033CE0 a5 */
void WallTrooperFallDown(uint16_t obj);                              /* $033D08 a5 */
void WallTrooperBurnUp(uint16_t obj);                                /* $033CDC a5 */
void WallTrooperBurnDown(uint16_t obj);                              /* $033D04 a5 */
void WallTrooperLeave(uint16_t obj);                                 /* $033D2C a5, a6=reg_a6 */
void WallTrooperKnockedUp(uint16_t obj);                             /* $033D54 a5, a6=reg_a6 */
void WallTrooperKnockedDown(uint16_t obj);                           /* $033D84 a5, a6=reg_a6 */
void WallTrooperBombDown(uint16_t obj);                              /* $033DB4 a5, a6=reg_a6 */
void WallTrooperBombUp(uint16_t obj);                                /* $033DE0 a5, a6=reg_a6 */
void WallTrooper(uint16_t obj);                                      /* $033ABA a5 -> a6=reg_a6 */
void WalkerWait(uint16_t obj);                                       /* $035792 a5 */
void WalkerBuild(uint16_t obj);                                      /* $0357BE a5 */
void WalkerGrow(uint16_t obj);                                       /* $0358AE a5 */
void WalkerDrop(uint16_t obj);                                       /* $0358D0 a5 */
void WalkerUnfold(uint16_t obj);                                     /* $0358FC a5 */
void WalkerLand(uint16_t obj);                                       /* $035940 a5 */
void WalkerStand(uint16_t obj);                                      /* $0359CE a5 */
void WalkerWalk(uint16_t obj);                                       /* $035A68 a5 */
void WalkerTurnLeft(uint16_t obj);                                   /* $035A22 a5 */
void WalkerTurnRight(uint16_t obj);                                  /* $035A48 a5 */
void WalkerLean(uint16_t obj);                                       /* $035C04 a5 */
void WalkerIdle(uint16_t obj);                                       /* $035C28 a5 */
void WalkerLeap(uint16_t obj);                                       /* $035C2A a5 */
void WalkerSpin(uint16_t obj);                                       /* $035C76 a5 */
void WalkerComeDown(uint16_t obj);                                   /* $035C96 a5 */
void WalkerSway(uint16_t obj);                                       /* $035D40 a5 */
void WalkerCurl(uint16_t obj);                                       /* $035DE6 a5 */
void WalkerTuck(uint16_t obj);                                       /* $035E3E a5 */
void WalkerRolling(uint16_t obj);                                    /* $035E6C a5 */
void WalkerUnroll(uint16_t obj);                                     /* $035F0A a5 */
void WalkerRight(uint16_t obj);                                      /* $035F32 a5 */
void WalkerTurnAbout(uint16_t obj);                                  /* $035F86 a5 */
void WalkerEnd(uint16_t obj);                                        /* $036000 a5 */
void WalkerFall(uint16_t obj);                                       /* $035FEA a5 */
void WalkerSettle(uint16_t obj);                                     /* $036054 a5 */
void Walker(uint16_t obj);                                           /* $0356F2 a5 */
void Stage1DirectorWave1(uint16_t obj);                              /* $0336CA a5 */
void Stage1DirectorWave2(uint16_t obj);                              /* $03374E a5 */

/* ---- types $40470-$465C0 (objects40.c) ---- */
void Spark2AC(uint16_t obj);                                         /* $040470 a5 */
void GunPodStart(uint16_t obj);                                      /* $0405D2 a5 */
void GunPodFire(uint16_t obj);                                       /* $04060E a5 */
void GunPod(uint16_t obj);                                           /* $04059A a5 */
void Lantern(uint16_t obj);                                          /* $041F1E a5 */
void Switch318(uint16_t obj);                                        /* $046094 a5 */
void BounceBall(uint16_t obj);                                       /* $0460CC a5 */
void PrizeRoomStart(uint16_t obj);                                   /* $043D5E a5 */
void PrizeRoomWave(uint16_t obj);                                    /* $043DAC a5 */
void PrizeRoom(uint16_t obj);                                        /* $043CDE a5 */
void SpikePodFlung(uint16_t obj);                                    /* $046674 a5 */
void SpikePodDrop(uint16_t obj);                                     /* $046614 a5 */
void SpikePodStart(uint16_t obj);                                    /* $0465E6 a5 */
void SpikePodRebound(uint16_t obj);                                  /* $046726 a5 */
void SpikePodBounce(uint16_t obj);                                   /* $046690 a5 */
void SpikePodClose(uint16_t obj);                                    /* $046784 a5 */
void SpikePodOpen(uint16_t obj);                                     /* $046760 a5 */
void SpikePod(uint16_t obj);                                         /* $0465C0 a5 */
void SwitchRoomStart(uint16_t obj);                                  /* $045D7E a5 */
void SwitchRoomPlay(uint16_t obj);                                   /* $045DAA a5 */
void SwitchRoomErase(uint16_t obj);                                  /* $045DFA a5 */
void SwitchRoomDraw(uint16_t obj);                                   /* $045E0E a5 */
void SwitchRoomWon(uint16_t obj);                                    /* $045E1C a5 */
void SwitchRoomLost(uint16_t obj);                                   /* $045E36 a5 */
void SwitchRoom(uint16_t obj);                                       /* $045D18 a5 */
void PalaceDieStart(uint16_t obj);                                   /* $041FF0 a5 */
void PalaceDieMessage(uint16_t obj);                                 /* $042022 a5 */
void PalaceDieDrop(uint16_t obj);                                    /* $04209E a5 */
void PalaceDieBounce(uint16_t obj);                                  /* $0420DC a5 */
void PalaceDieRoll(uint16_t obj);                                    /* $04216A a5 */
void PalaceDieStruck(uint16_t obj);                                  /* $04218A a5 */
void PalaceDie(uint16_t obj);                                        /* $041FB8 a5 */
void BlimpStart(uint16_t obj);                                       /* $046280 a5 */
void BlimpFly(uint16_t obj);                                         /* $046300 a5 */
void BlimpShake(uint16_t obj);                                       /* $0463B8 a5 */
void BlimpSink(uint16_t obj);                                        /* $0463EA a5 */
void Blimp(uint16_t obj);                                            /* $0461D2 a5 */
void StalkStart(uint16_t obj);                                       /* $0406EE a5 */
void StalkRise(uint16_t obj);                                        /* $0407C2 a5 */
void StalkSway(uint16_t obj);                                        /* $0407DA a5 */
void StalkAim(uint16_t obj);                                         /* $040804 a5 */
void StalkFire(uint16_t obj);                                        /* $040822 a5 */
void Stalk(uint16_t obj);                                            /* $0406B4 a5 */
void DiceBoardStart(uint16_t obj);                                   /* $040B2E a5 */
void DiceBoardShow(uint16_t obj);                                    /* $040B5C a5 */
void DiceBoardBegin(uint16_t obj);                                   /* $040B66 a5 */
void DiceBoardWait(uint16_t obj);                                    /* $040B80 a5 */
void DiceBoardRolled(uint16_t obj);                                  /* $040B9A a5 */
void DiceBoardPause(uint16_t obj);                                   /* $040BCC a5 */
void DiceBoardMove(uint16_t obj);                                    /* $040BEA a5 */
void DiceBoardAgain(uint16_t obj);                                   /* $040D32 a5 */
void DiceBoardReturn(uint16_t obj);                                  /* $040D52 a5 */
void DiceBoardFall(uint16_t obj);                                    /* $040D7E a5 */
void DiceBoardFallen(uint16_t obj);                                  /* $040DC4 a5 */
void DiceBoardToStart(uint16_t obj);                                 /* $040E2E a5 */
void DiceBoardFadeUp(uint16_t obj);                                  /* $040E4E a5 */
void DiceBoardWarp(uint16_t obj);                                    /* $040E8E a5 */
void DiceBoardWarping(uint16_t obj);                                 /* $040EDC a5 */
void DiceBoardLoad(uint16_t obj);                                    /* $040F26 a5 */
void DiceBoardEnter(uint16_t obj);                                   /* $040F48 a5 */
void DiceBoardArrive(uint16_t obj);                                  /* $040FBC a5 */
void DiceBoard(uint16_t obj);                                        /* $040AD8 a5 */

/* ---- types $48D3C-$4FB72 (objects4c.c) ---- */
void SpaceColourCopy(uint32_t src, uint16_t dst, uint16_t count);    /* $048FAE a0, a1.w, d7.w */
void SpaceColours(void);                                             /* $048F78 */
uint32_t SpaceColourTable(uint16_t d0);                              /* $04903C d0.w -> a0 */
void SpaceColoursFlash(void);                                        /* $048FEA */
void SpaceColoursSet(void);                                          /* $049018 */
void SpaceDirSprite(uint16_t obj);                                   /* $04905C a5 */
void SpaceDir00(uint16_t obj);                                       /* $048DA2 a5 */
void SpaceDir04(uint16_t obj);                                       /* $048DC2 a5 */
void SpaceDir08(uint16_t obj);                                       /* $048DD2 a5 */
void SpaceSpawnRight(uint16_t type, uint16_t d5, uint16_t d6);       /* $0493E0 d4.w, d5.w, d6.w */
void SpaceSpawnRightArg(uint16_t arg, uint16_t type, uint16_t d5, uint16_t d6); /* $049744 d3.w, d4.w, d5.w, d6.w */
uint16_t SpaceSpawn4B4(void);                                        /* $04950E -> a0.w */
int SpaceSpawnHigh(uint32_t list, uint16_t *slot);                   /* $04969C a1 -> Z, a0.w */
void SpaceEvent00(void);                                             /* $049402 */
void SpaceEvent04(uint16_t d5);                                      /* $04934A d5.w */
void SpaceEvent08(uint16_t d5);                                      /* $049356 d5.w */
void SpaceEvent0C(void);                                             /* $0497A2 */
void SpaceEvent10(uint16_t d5);                                      /* $0492FE d5.w */
void SpaceEvent14(uint16_t d5);                                      /* $04930A d5.w */
void SpaceEvent18(uint16_t d5);                                      /* $04936E d5.w */
void SpaceEvent1C(uint16_t d5);                                      /* $04937A d5.w */
void SpaceEvent20(uint16_t d5);                                      /* $049322 d5.w */
void SpaceEvent24(uint16_t d5);                                      /* $04933A d5.w */
void SpaceEvent28(uint16_t d5);                                      /* $049392 d5.w */
void SpaceEvent2C(void);                                             /* $0494E2 */
void SpaceEvent30(void);                                             /* $0495AE */
void SpaceEvent34(void);                                             /* $049404 */
void SpaceEvent38(void);                                             /* $04941A */
void SpaceEvent3C(void);                                             /* $049470 */
void SpaceEvent40(void);                                             /* $049476 */
void SpaceEvent44(void);                                             /* $04947C */
void SpaceEvent48(void);                                             /* $049482 */
void SpaceEvent4C(void);                                             /* $049488 */
void SpaceEvent50(void);                                             /* $04948E */
void SpaceEvent54(void);                                             /* $049494 */
void SpaceEvent58(void);                                             /* $0494B6 */
void SpaceEvent5C(void);                                             /* $0496C4 */
void SpaceEvent60(void);                                             /* $049764 */
void SpaceEvent64(void);                                             /* $049430 */
void SpaceEvent68(void);                                             /* $04954C */
void SpaceEvent6C(void);                                             /* $04955E */
void SpaceEvent70(void);                                             /* $0497B2 */
void SpaceEvent74(void);                                             /* $0494C0 */
void SpaceEvent78(void);                                             /* $0494F6 */
void SpaceEvent7C(void);                                             /* $049520 */
void SpaceEvent80(void);                                             /* $049726 */
void SpaceEvent84(uint16_t d5);                                      /* $0493C2 d5.w */
void SpaceEvent88(void);                                             /* $049578 */
void SpaceEvent8C(void);                                             /* $04958A */
void SpaceEvent90(void);                                             /* $04949A */
void SpaceEvent94(void);                                             /* $04953A */
void SpaceEvent98(void);                                             /* $049446 */
void SpaceEvent9C(void);                                             /* $0495F8 */
void SpaceEventA0(void);                                             /* $04945A */
void SpaceEventA4(void);                                             /* $049622 */
void SpaceEventA8(void);                                             /* $049648 */
void SpaceEventAC(void);                                             /* $04965A */
void SpaceEventB0(void);                                             /* $049664 */
void SpaceEventB4(void);                                             /* $04968A */
void SpaceEventB8(void);                                             /* $0492EC */
void SpaceEventBC(void);                                             /* $0494A0 */
void SpaceEventC0(void);                                             /* $04959C */
void SpaceEventC4(void);                                             /* $049780 */
void SpaceEventC8(void);                                             /* $04977A */
void SpaceDir0C(uint16_t obj);                                       /* $048DFC a5 */
void SpaceDir10(uint16_t obj);                                       /* $048E2A a5 */
void SpaceDir14(uint16_t obj);                                       /* $048E3C a5 */
void SpaceDir18(uint16_t obj);                                       /* $048E5A a5 */
void SpaceDir1C(uint16_t obj);                                       /* $048E7C a5 */
void SpaceDir20(uint16_t obj);                                       /* $048E96 a5 */
void SpaceDir24(uint16_t obj);                                       /* $048EB8 a5 */
void SpaceDir28(uint16_t obj);                                       /* $048EBE a5 */
void SpaceDir2C(uint16_t obj);                                       /* $048ED2 a5 */
void SpaceDir30(uint16_t obj);                                       /* $048EF4 a5 */
void SpaceDir34(uint16_t obj);                                       /* $048F0E a5 */
void SpaceDir38(uint16_t obj);                                       /* $048F30 a5 */
void SpaceDir3C(uint16_t obj);                                       /* $048F32 a5 */
void SpaceDir40(uint16_t obj);                                       /* $048F44 a5 */
void SpaceDir48(uint16_t obj);                                       /* $048F6A a5 */
void SpaceDir44(uint16_t obj);                                       /* $048F66 a5 */
void SpaceDirector(uint16_t obj);                                    /* $048D3C a5 */
void Space448Reload(uint16_t obj);                                   /* $04B354 a5 */
void Space448Volley(uint16_t obj);                                   /* $04B386 a5 */
void Space448Run(uint16_t obj);                                      /* $04B2E2 a5 */
void Space448(uint16_t obj);                                         /* $04B2DA a5 */
void Space44C(uint16_t obj);                                         /* $04B282 a5 */
void Space450Run(uint16_t obj);                                      /* $04B3C6 a5 */
void Space450(uint16_t obj);                                         /* $04B3A8 a5 */
void Space454(uint16_t obj);                                         /* $04C628 a5 */
void Space500(uint16_t obj);                                         /* $04C610 a5 */
void Space460(uint16_t obj);                                         /* $04C6A2 a5 */
void Space464(uint16_t obj);                                         /* $04C774 a5 -> a6=reg_a6 */
void Space47CSteer(uint16_t obj, uint16_t target);                   /* $04CAEE a5, a0.w */
void Space47C(uint16_t obj);                                         /* $04C9C6 a5 */
void Space480Start(uint16_t obj);                                    /* $04B49C a5 */
void Space480Wait(uint16_t obj);                                     /* $04B4A8 a5 */
void Space480Open(uint16_t obj);                                     /* $04B4BA a5 */
void Space480Fire(uint16_t obj);                                     /* $04B500 a5 */
void Space480Close(uint16_t obj);                                    /* $04B58C a5 */
void Space480Run(uint16_t obj);                                      /* $04B460 a5 */
void Space480(uint16_t obj);                                         /* $04B458 a5 */
void Space45CStep00(uint16_t obj);                                   /* $04B6E0 a5 */
void Space45CStep04(uint16_t obj);                                   /* $04B73A a5 */
void Space45CStep08(uint16_t obj);                                   /* $04B756 a5 */
void Space45CStep0C(uint16_t obj);                                   /* $04B770 a5 */
void Space45CStep10(uint16_t obj);                                   /* $04B798 a5, a6=reg_a6 */
void Space45CBomb(void);                                             /* $04BF3E */
void Space45CVolley00(uint16_t obj);                                 /* $04BC48 a5 -> a6=reg_a6 */
void Space45CVolley04(uint16_t obj);                                 /* $04BC64 a5 -> a6=reg_a6 */
void Space45CVolley08(uint16_t obj);                                 /* $04BCFC a5 -> a6=reg_a6 */
void Space45CVolley0C(uint16_t obj);                                 /* $04BD18 a5 -> a6=reg_a6 */
void Space45CVolley10(uint16_t obj);                                 /* $04BD34 a5 -> a6=reg_a6 */
void Space45CVolley14(uint16_t obj);                                 /* $04BD50 a5 -> a6=reg_a6 */
void Space45CVolley18(uint16_t obj);                                 /* $04BD7E a5 -> a6=reg_a6 */
void Space45CVolley1C(uint16_t obj);                                 /* $04BDAC a5 -> a6=reg_a6 */
void Space45CVolley20(uint16_t obj);                                 /* $04BDE6 a5 -> a6=reg_a6 */
void Space45CVolley24(uint16_t obj);                                 /* $04BE02 a5 -> a6=reg_a6 */
void Space45CVolley28(uint16_t obj);                                 /* $04BE12 a5 -> a6=reg_a6 */
void Space45CVolley2C(uint16_t obj);                                 /* $04BE4C a5 -> a6=reg_a6 */
void Space45CVolley30(uint16_t obj);                                 /* $04BE84 a5 -> a6=reg_a6 */
void Space45CVolley34(uint16_t obj);                                 /* $04BEB2 a5 -> a6=reg_a6 */
void Space45CVolley38(uint16_t obj);                                 /* $04BEEA a5 -> a6=reg_a6 */
void Space45CVolley3C(uint16_t obj);                                 /* $04BEFA a5 -> a6=reg_a6 */
void Space45CVolley40(uint16_t obj);                                 /* $04BF0A a5 -> a6=reg_a6 */
void Space45CVolley44(uint16_t obj);                                 /* $04BF3A a5 -> a6=reg_a6 */
void Space45CStep14(uint16_t obj);                                   /* $04B7F8 a5, a6=reg_a6 -> a6=reg_a6 */
void Space45CStep18(uint16_t obj);                                   /* $04B826 a5, a6=reg_a6 */
void Space45CStep1C(uint16_t obj);                                   /* $04B85E a5, a6=reg_a6 */
void Space45CStep20(uint16_t obj);                                   /* $04B8CC a5, a6=reg_a6 */
void Space45CStep24(uint16_t obj);                                   /* $04B904 a5 */
void Space45CStep28(uint16_t obj);                                   /* $04B914 a5 */
void Space45CStep2C(uint16_t obj);                                   /* $04B96C a5 */
void Space45CStep30(uint16_t obj);                                   /* $04B99C a5 */
void Space45C(uint16_t obj);                                         /* $04B5BA a5 -> a6=reg_a6 */
void Space484Start(uint16_t obj);                                    /* $04BA3A a5 */
void Space484Wait(uint16_t obj, uint16_t d7);                        /* $04BA3E a5, d7.w */
void Space484AtPlayer(uint16_t obj);                                 /* $04BB36 a5 */
void Space484Shot(uint16_t obj);                                     /* $04BB1C a5 */
void Space484Fight(uint16_t obj);                                    /* $04BA6E a5 */
void Space484Burst(uint16_t obj);                                    /* $04BA9E a5 */
void Space484End(uint16_t obj);                                      /* $04BB06 a5 */
void Space484(uint16_t obj);                                         /* $04B9B2 a5 */
void Space4A4Beam(uint16_t obj);                                     /* $04B20E a5 */
void Space4A4Start(uint16_t obj);                                    /* $04AF28 a5 */
void Space4A4Step04(uint16_t obj);                                   /* $04AF80 a5 */
void Space4A4Step3C(uint16_t obj);                                   /* $04AF94 a5 */
void Space4A4Step08(uint16_t obj);                                   /* $04AFA8 a5 */
void Space4A4Step0C(uint16_t obj);                                   /* $04AFCA a5 */
void Space4A4Step10(uint16_t obj);                                   /* $04AFE4 a5 */
void Space4A4Step18(uint16_t obj);                                   /* $04B01C a5 */
void Space4A4Step14(uint16_t obj);                                   /* $04B004 a5 */
void Space4A4Step50(uint16_t obj);                                   /* $04B060 a5 */
void Space4A4Step1C(uint16_t obj);                                   /* $04B072 a5 */
void Space4A4Step20(uint16_t obj);                                   /* $04B086 a5 */
void Space4A4Step24(uint16_t obj);                                   /* $04B0AE a5 */
void Space4A4Step28(uint16_t obj);                                   /* $04B0CC a5 */
void Space4A4Step2C(uint16_t obj);                                   /* $04B0DE a5 */
void Space4A4Step30(uint16_t obj);                                   /* $04B0EA a5 */
void Space4A4Step38(uint16_t obj);                                   /* $04B108 a5 */
void Space4A4Step40(uint16_t obj);                                   /* $04B114 a5 */
void Space4A4Step44(uint16_t obj);                                   /* $04B132 a5 */
void Space4A4Step48(uint16_t obj);                                   /* $04B148 a5 */
void Space4A4Step4C(uint16_t obj);                                   /* $04B16C a5 */
void Space4A4Step(uint16_t obj);                                     /* $04AECA a5 */
void Space4A4(uint16_t obj);                                         /* $04AE88 a5 */
void Space4A8(uint16_t obj);                                         /* $04CB36 a5 */
void Space4AC(uint16_t obj);                                         /* $04CC0C a5 */
void Space514(uint16_t obj);                                         /* $04CC1E a5 */
void Space50C(uint16_t obj);                                         /* $04CC46 a5 */
void Space51C(uint16_t obj);                                         /* $04CD34 a5 */
void Space4B0(uint16_t obj);                                         /* $04CDE2 a5 */
void Space4B4Rock(uint16_t obj, uint16_t slot);                      /* $04CF70 a5, a0.w */
void Space4B4(uint16_t obj);                                         /* $04CEE2 a5 */
void Space4B8(uint16_t obj);                                         /* $04CFE8 a5 */
void Space4BCPuff(uint16_t obj);                                     /* $04D0A2 a5 */
void Space4BC(uint16_t obj);                                         /* $04D08A a5 */
void Space4C0Spawn(uint16_t kind);                                   /* $04A058 d6.w */
void Space4C0SpawnHigh(uint16_t kind);                               /* $04A06C d6.w */
void Space4C0Start(uint16_t obj);                                    /* $049F74 a5 */
void Space4C0Step04(uint16_t obj);                                   /* $049F88 a5 */
void Space4C0Step08(uint16_t obj);                                   /* $049FB2 a5 */
void Space4C0Step0C(uint16_t obj);                                   /* $049FDC a5 */
void Space4C0Step10(uint16_t obj);                                   /* $04A006 a5 */
void Space4C0Step14(uint16_t obj);                                   /* $04A034 a5 */
void Space4C0(uint16_t obj);                                         /* $049F52 a5 */
void Space4C4Look(uint16_t obj);                                     /* $04A3DA a5 */
void Space4C4Move(uint16_t obj);                                     /* $04A3FA a5 */
void Space4C4Start(uint16_t obj);                                    /* $04A148 a5 */
void Space4C4Step04(uint16_t obj);                                   /* $04A162 a5 */
void Space4C4Step1C(uint16_t obj);                                   /* $04A17A a5 */
void Space4C4Step08(uint16_t obj, uint16_t d5);                      /* $04A192 a5, d5.w */
void Space4C4Step0C(uint16_t obj);                                   /* $04A1AC a5 */
void Space4C4Step24(uint16_t obj);                                   /* $04A1CA a5 */
void Space4C4Step10(uint16_t obj);                                   /* $04A1E8 a5 */
void Space4C4Step28(uint16_t obj);                                   /* $04A1FC a5 */
void Space4C4Step14(uint16_t obj, uint16_t d5);                      /* $04A210 a5, d5.w */
void Space4C4Step2C(uint16_t obj, uint16_t d5);                      /* $04A236 a5, d5.w */
void Space4C4Step18(uint16_t obj);                                   /* $04A250 a5 */
void Space4C4Step34(uint16_t obj);                                   /* $04A266 a5 */
void Space4C4Step44(uint16_t obj);                                   /* $04A27E a5 */
void Space4C4Step38(uint16_t obj);                                   /* $04A296 a5 */
void Space4C4Step3C(uint16_t obj);                                   /* $04A2AA a5 */
void Space4C4Step4C(uint16_t obj);                                   /* $04A2EC a5 */
void Space4C4Step40(uint16_t obj);                                   /* $04A304 a5 */
void Space4C4Step54(uint16_t obj);                                   /* $04A31A a5 */
void Space4C4Step58(uint16_t obj);                                   /* $04A332 a5 */
void Space4C4Step60(uint16_t obj);                                   /* $04A33A a5 */
void Space4C4Step64(uint16_t obj);                                   /* $04A358 a5 */
void Space4C4Step6C(uint16_t obj);                                   /* $04A362 a5 */
void Space4C4Step68(uint16_t obj);                                   /* $04A380 a5 */
void Space4C4Step70(uint16_t obj);                                   /* $04A3A0 a5 */
void Space4C4Step74(uint16_t obj);                                   /* $04A418 a5 */
void Space4C4Step78(uint16_t obj);                                   /* $04A44C a5 */
void Space4C4Step7C(uint16_t obj);                                   /* $04A4BE a5 */
void Space4C4Step80(uint16_t obj);                                   /* $04A4CE a5 */
void Space4C4(uint16_t obj);                                         /* $04A080 a5 */
void Space4CCLook(uint16_t obj);                                     /* $04A7CC a5 */
void Space4CCThrow(uint16_t obj);                                    /* $04A7E6 a5 */
void Space4CCRun(uint16_t obj);                                      /* $04A748 a5 */
void Space4CC(uint16_t obj);                                         /* $04A73E a5 */
void Space4D0Start(uint16_t obj);                                    /* $04A542 a5 */
void Space4D0Step04(uint16_t obj);                                   /* $04A55A a5 */
void Space4D0Step08(uint16_t obj);                                   /* $04A562 a5 */
void Space4D0Step0C(uint16_t obj);                                   /* $04A56A a5 */
void Space4D0Step10(uint16_t obj);                                   /* $04A572 a5 */
void Space4D0Step14(uint16_t obj);                                   /* $04A5A8 a5 */
void Space4D0Step18(uint16_t obj);                                   /* $04A63C a5 */
void Space4D0Step1C(uint16_t obj);                                   /* $04A66A a5 */
void Space4D0Step20(uint16_t obj);                                   /* $04A680 a5 */
void Space4D0(uint16_t obj);                                         /* $04A510 a5 */
void Space4D4Start(uint16_t obj);                                    /* $04A96C a5 */
void Space4D4Step04(uint16_t obj);                                   /* $04A984 a5 */
void Space4D4Step08(uint16_t obj);                                   /* $04A9A2 a5 */
void Space4D4Step0C(uint16_t obj);                                   /* $04A9D8 a5 */
void Space4D4Step10(uint16_t obj);                                   /* $04AA00 a5 */
void Space4D4Step14(uint16_t obj);                                   /* $04AA0A a5 */
void Space4D4Step18(uint16_t obj);                                   /* $04AA14 a5 */
void Space4D4Step1C(uint16_t obj);                                   /* $04AA6C a5 */
void Space4D4Step20(uint16_t obj);                                   /* $04AA8E a5 */
void Space4D4Step28(uint16_t obj);                                   /* $04AB3E a5 */
void Space4D4Step24(uint16_t obj);                                   /* $04AAF4 a5 */
void Space4D4Step2C(uint16_t obj);                                   /* $04AB40 a5 */
void Space4D4Step30(uint16_t obj);                                   /* $04ABBC a5 */
void Space4D4Step38(uint16_t obj);                                   /* $04AC58 a5 */
void Space4D4Step34(uint16_t obj);                                   /* $04AC0A a5 */
void Space4D4Step3C(uint16_t obj);                                   /* $04AD40 a5 */
void Space4D4Run(uint16_t obj);                                      /* $04A8E2 a5 */
void Space4D4(uint16_t obj);                                         /* $04A8AE a5 */
void Space4D8Start(uint16_t obj);                                    /* $04A84A a5 */
void Space4D8Run(uint16_t obj);                                      /* $04A856 a5 */
void Space4D8(uint16_t obj);                                         /* $04A838 a5 */
void SnakeTrailClear(void);                                          /* $049D46 */
void SnakeTrail(uint16_t obj);                                       /* $049D9A a5 */
void SnakeStart(uint16_t obj);                                       /* $049C70 a5 */
void SnakeBody(uint16_t obj);                                        /* $049D64 a5 */
void SnakeHead(uint16_t obj);                                        /* $049E2A a5 */
void SnakeLeave(uint16_t obj);                                       /* $049E18 a5 */
void SnakeTail(uint16_t obj);                                        /* $049F18 a5 */
void Snake(uint16_t obj);                                            /* $049C2E a5 */
void Space518Start(uint16_t obj);                                    /* $049B6A a5 */
void Space518Run(uint16_t obj);                                      /* $049B94 a5 */
void Space518Step(uint16_t obj);                                     /* $049B58 a5 */
void Space518(uint16_t obj);                                         /* $049B4A a5 */
void Space520(uint16_t obj);                                         /* $04AD4E a5 */
void Space530(uint16_t obj);                                         /* $04ADEE a5 */
void Stage6Trail(uint16_t obj, uint32_t d2, uint32_t d3);            /* $04FB3A a5, d2, d3 */
void Flame54C(uint16_t obj);                                         /* $04F74A a5 */
void Fade550(uint16_t obj);                                          /* $04F7F6 a5 */
void Fade554(uint16_t obj);                                          /* $04F9D4 a5 */
void Flame558Start(uint16_t obj);                                    /* $04F86C a5 */
void Flame558Left(uint16_t obj);                                     /* $04F8A6 a5 */
void Flame558Right(uint16_t obj);                                    /* $04F8AC a5 */
void Flame558Fall(uint16_t obj);                                     /* $04F8FA a5 */
void Flame558(uint16_t obj);                                         /* $04F82E a5 */
void Fire55C(uint16_t obj);                                          /* $04F93C a5 */
void Blast560Ring0(uint16_t obj);                                    /* $04FA1E a5 */
void Blast560Ring1(uint16_t obj);                                    /* $04FA28 a5 */
void Blast560Ring2(uint16_t obj);                                    /* $04FA46 a5 */
void Blast560Ring3(uint16_t obj);                                    /* $04FA62 a5 */
void Blast560Ring4(uint16_t obj);                                    /* $04FA6A a5 */
void Blast560(uint16_t obj);                                         /* $04FA00 a5 */
void Boss548Wait(uint16_t obj);                                      /* $04F502 a5 */
void Boss548Rest(uint16_t obj);                                      /* $04F4FC a5 */
void Boss548Blink(uint16_t obj);                                     /* $04F528 a5 */
int Boss548Floor(uint16_t obj);                                      /* $04F53A a5 -> Z */
void Boss548Dust(uint16_t obj, uint32_t d7);                         /* $04F688 a5, d7 */
void Boss548Dusts(uint16_t obj);                                     /* $04F678 a5 */
void Boss548DustAt(uint16_t at, uint32_t d7);                        /* $04F714 a1.w, d7 */
void Boss548DustsAt(uint16_t at);                                    /* $04F6F4 a0.w */
void Boss548Shadow(uint16_t obj);                                    /* $04F4CA a5 */
void Boss548Flame(uint16_t obj, uint16_t d1, uint16_t d2);           /* $04EE44 a5, d1.w, d2.w */
void Boss548Start(uint16_t obj);                                     /* $04E9D0 a5 */
void Boss548Enter(uint16_t obj);                                     /* $04EA3C a5 */
void Boss548Arrive(uint16_t obj);                                    /* $04EA5A a5 */
void Boss548Stand(uint16_t obj);                                     /* $04EA98 a5 */
void Boss548Walk(uint16_t obj);                                      /* $04EAC6 a5 */
void Boss548Knocked(uint16_t obj);                                   /* $04ECBA a5 */
void Boss548Flinch(uint16_t obj);                                    /* $04ECE8 a5 */
void Boss548Held(uint16_t obj);                                      /* $04ECFA a5 */
void Boss548Thrown(uint16_t obj);                                    /* $04ED0A a5 */
void Boss548Jump(uint16_t obj);                                      /* $04ED7C a5 */
void Boss548Crouch(uint16_t obj);                                    /* $04EDC8 a5 */
void Boss548Leap(uint16_t obj);                                      /* $04EDF0 a5 */
void Boss548Flames(uint16_t obj);                                    /* $04EE0E a5 */
void Boss548Turning(uint16_t obj);                                   /* $04EE6E a5 */
void Boss548Grab(uint16_t obj);                                      /* $04EEA4 a5 */
void Boss548Swing(uint16_t obj);                                     /* $04EF12 a5 */
void Boss548Throw(uint16_t obj);                                     /* $04F044 a5 */
void Boss548DashStart(uint16_t obj);                                 /* $04F096 a5 */
void Boss548Dash(uint16_t obj);                                      /* $04F11A a5 */
void Boss548Blast(uint16_t obj);                                     /* $04F366 a5 */
void Boss548Spin(uint16_t obj);                                      /* $04F19E a5 */
void Boss548Rise(uint16_t obj);                                      /* $04F220 a5 */
void Boss548Dive(uint16_t obj);                                      /* $04F300 a5 */
void Boss548Beaten(uint16_t obj);                                    /* $04F3BE a5 */
void Boss548Bounce(uint16_t obj);                                    /* $04F426 a5 */
void Boss548Run(uint16_t obj);                                       /* $04E7CE a5 */
void Boss548(uint16_t obj);                                          /* $04E7C4 a5 */
void Stage6AlliesStart(void);                                        /* $04FFFE */
void Stage6Dir00(uint16_t obj);                                      /* $04FBBA a5 */
void Stage6Dir02(uint16_t obj);                                      /* $04FBD6 a5 */
void Stage6Dir04(uint16_t obj);                                      /* $04FBEA a5 */
void Stage6Dir06(uint16_t obj);                                      /* $04FC12 a5 */
void Stage6Dir08(uint16_t obj);                                      /* $04FC28 a5 */
void Stage6Dir0C(uint16_t obj);                                      /* $04FC5E a5 */
void Stage6Dir0A(uint16_t obj);                                      /* $04FC56 a5 */
void Stage6Dir0E(uint16_t obj);                                      /* $04FCEC a5 */
void Stage6Dir10(uint16_t obj);                                      /* $04FD02 a5 */
void Stage6Dir12(uint16_t obj);                                      /* $04FD2A a5 */
void Stage6Dir14(uint16_t obj);                                      /* $04FD40 a5 */
void Stage6Dir16(uint16_t obj, uint16_t d7);                         /* $04FD5A a5, d7.w */
void Stage6Dir18(uint16_t obj);                                      /* $04FD7C a5 */
void Stage6Dir1A(uint16_t obj);                                      /* $04FDA4 a5 */
void Stage6Dir1C(uint16_t obj);                                      /* $04FDD2 a5 */
void Stage6Dir1E(uint16_t obj);                                      /* $04FDEC a5 -> a6=reg_a6 */
void Stage6Dir20(uint16_t obj);                                      /* $04FE30 a5 */
void Stage6Dir22(uint16_t obj);                                      /* $04FE6C a5 */
void Stage6Dir24(uint16_t obj);                                      /* $04FE98 a5 */
void Stage6Dir26(uint16_t obj);                                      /* $04FEDC a5 */
void Stage6Dir28(uint16_t obj);                                      /* $04FEF0 a5 */
void Stage6Dir2A(uint16_t obj);                                      /* $04FF1E a5 */
void Stage6Dir2C(uint16_t obj);                                      /* $04FF32 a5 */
void Stage6Dir2E(uint16_t obj);                                      /* $04FF68 a5 */
void Stage6Dir30(uint16_t obj);                                      /* $04FF9A a5 */
void Stage6Dir32(uint16_t obj);                                      /* $04FFA8 a5 */
void Stage6Dir34(uint16_t obj);                                      /* $04FFC8 a5 */
void Stage6Dir36(uint16_t obj);                                      /* $04FFFC a5 */
void Stage6Director(uint16_t obj);                                   /* $04FB72 a5 -> a6=reg_a6 */
int SlotFreeLast(uint16_t *slot);                                    /* $04BF98 -> Z, a0.w */
void SpaceWreck(uint16_t obj);                                       /* $04C3B8 a5 */
uint16_t SpaceAim(uint16_t obj);                                     /* $04C11C a5 -> d7.w */
void SpaceArmPart(uint16_t obj, uint16_t part);                      /* $04C142 a5, a0.w */
void SpaceArm(uint16_t obj, uint16_t d7);                            /* $04C01C a5, d7.w */
int SpaceArmExtend(uint16_t obj);                                    /* $04BFFE a5 -> N */
void SpaceShot454(uint16_t obj);                                     /* $04BFA4 a5 */
void SpaceRing(uint16_t obj);                                        /* $04C1A6 a5 */
void SpaceShot460(uint16_t obj, uint16_t angle);                     /* $04C44A a5, d0.w, a6=reg_a6 */
void SpaceShotIfHard(uint16_t obj);                                  /* $04C158 a5 */
void SpaceFire4B0(uint16_t obj, uint32_t d3, uint32_t d4, uint32_t d5, uint32_t d6, uint16_t d7); /* $04C20C a5, d3, d4, d5, d6, d7.w */
void SpaceFire4B0Of32(uint16_t obj, uint32_t d3, uint32_t d4, uint32_t d5, uint32_t d6); /* $04C20A a5, d3, d4, d5, d6 */
void SpaceShotAimed(uint16_t obj);                                   /* $04C2B8 a5 */
void SpaceSpread(uint16_t obj, uint16_t d6, uint16_t d7);            /* $04C23C a5, d6.w, d7.w */
void SpaceShotIfHardest(uint16_t obj);                               /* $04C164 a5 */
void AfterImage(uint16_t obj);                                       /* $03EA52 a5 */
void PuffTrail(uint16_t obj);                                        /* $03EB82 a5 */
void Stage6Clear(uint16_t keep1, uint16_t keep2);                    /* $0500E6 d0.w, d1.w */
void SpawnFromList(uint32_t list, uint16_t count);                   /* $051808 a1, d7.w */
void ScaleClear(void);                                               /* $0208D6 */
void ScaleRow(uint32_t step, uint32_t across, uint16_t src, uint32_t *d2, uint16_t *a5); /* $02070A d0, d1, a1.w -> d2, a5.w, a6=reg_a6 */
void ScaleRun(uint16_t list);                                        /* $0208C6 a0.w */
void ScaleCopy(uint16_t list, uint16_t vram);                        /* $0207D4 a0.w, a4.w */
void ScaleTiles(uint16_t list, uint16_t src, uint16_t vram);         /* $0206D4 a0.w, a1.w, a4.w -> a6=reg_a6 */
void Stage6Allies(uint16_t count);                                   /* $050012 d7.w */
void Stage6AllyA800(uint16_t d0);                                    /* $050182 d0.w */
void Stage6AlliesStep0(void);                                        /* $050122 */
void Stage6AlliesStep2(void);                                        /* $05012C */

/* ---- not ported yet ---- */

/* ---- dispatch (dispatch.c): a jsr / jmp through an address the game computed ---- */
void call68k(uint32_t addr);
void call68k_missing(uint32_t addr);     /* the tests' recorded call; natively a fatal error */
void call_object(uint32_t addr, uint16_t obj);       /* the same, the object in a5 */
void call_object_missing(uint32_t addr, uint16_t obj);
