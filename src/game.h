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

/* ---- the level's collision (map.c) ---- */
uint16_t MapAttribute(uint16_t x, uint16_t y);                       /* $00B95A d0.w, d1.w -> d2.w */
uint16_t MapBoundsY(uint16_t x, uint16_t *y);                       /* $00C31C d0.w -> d2.w, d1.w */
uint16_t MapAttr(uint16_t x, uint16_t y);                            /* $00B94A d0.w, d1.w -> d2.w */
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

/* ---- not ported yet ---- */

/* ---- dispatch (dispatch.c): a jsr / jmp through an address the game computed ---- */
void call68k(uint32_t addr);
void call68k_missing(uint32_t addr);     /* the tests' recorded call; natively a fatal error */
void call_object(uint32_t addr, uint16_t obj);       /* the same, the object in a5 */
void call_object_missing(uint32_t addr, uint16_t obj);
