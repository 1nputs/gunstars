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
void SoundPan1Start(uint16_t t);                                     /* $065A30 a5 */
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

/* ---- not ported yet ---- */

/* ---- dispatch (dispatch.c): a jsr / jmp through an address the game computed ---- */
void call68k(uint32_t addr);
void call68k_missing(uint32_t addr);     /* the tests' recorded call; natively a fatal error */
