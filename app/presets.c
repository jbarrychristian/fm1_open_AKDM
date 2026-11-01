/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#include "instrument.h"
#include "fm_patch.h"
#include "drum808.h"
#include "sampledrum.h"
#include <string.h>

/* FM1 arrangements. Small musical recipes expand into the normal
 * editable project format, avoiding sixteen 4 KiB images in program Flash.
 * No random generation, engine allocation or live-state changes happen here.
 * Notes are MIDI pitches; masks read from bit 0 (the first sixteenth note).
 * Unused tracks stay empty, muted, and use the small PSG engine.
 *
 * This bank is voiced for classic and progressive rock: tonewheel, combo and
 * pipe organs, electric pianos, string machines, tape-replay flute and choir,
 * analog-style leads, brass and bass pedals. Scenes are style sketches, not
 * transcriptions of any recording. */
enum { KEYS, BASS, ARP, LEAD, PAD, BEAT, PERC, STAB };
enum { TONEWHEEL, COMBO_ORGAN, PIPE_ORGAN, WURLI, STAGE_EP, HARPSICHORD,
       CLAV, MINI_LEAD, SYNC_LEAD, TAURUS, MOOG_BASS, STRINGS,
       TAPE_FLUTE, TAPE_CHOIR, SYNTH_BRASS, PROG_PAD, POLY_VOX, DEMO_TONES };
/* 303 recipes: warm bass, deep pedal, singing lead, square bass. */
enum { AN_BASS, AN_PEDAL, AN_LEAD, AN_SQUARE };
struct part {
    uint8_t source : 3, role : 3;
    uint8_t tone, level, sends;
};
struct scene {
    char name[13];
    uint8_t bpm, root, minor, swing, rhythm, motif, count;
    uint8_t decay, delay_steps, feedback, fx_level;
    struct part part[5];
    uint8_t meter; /* enum meter; 0 is plain 4/4 */
    uint8_t seq;   /* ARP parts: 0 eighths up the chord, 1 sixteenth sequencer line */
};
/* Mixed-meter loops. The sequencer loops over 16..64 steps in pages of
 * 16, so odd bars are packed into one 64-step loop with a short filler bar.
 * Each bar lists its length in sixteenths and its beat groups: a mask of
 * the sixteenth where each group of eighths (2s and 3s) starts. */
enum { M44, M78, M54, M74, M98, M118, METERS };
struct bar_shape { uint8_t steps; uint32_t groups; };
struct meter_shape { uint8_t bars; struct bar_shape bar[5]; };
#define G(a, b, c, d) ((1UL << (a)) | (1UL << (b)) | (1UL << (c)) | (1UL << (d)))
static const struct meter_shape meters[METERS] = {
    {4, {{16,G(0,4,8,12)},{16,G(0,4,8,12)},{16,G(0,4,8,12)},{16,G(0,4,8,12)}}},
    /* 7/8 x4 (2+2+3) + 4/8 */
    {5, {{14,G(0,4,8,8)},{14,G(0,4,8,8)},{14,G(0,4,8,8)},{14,G(0,4,8,8)},{8,G(0,4,4,4)}}},
    /* 5/4 x2 (3+3+2+2) + 6/4 (3+3+3+3) */
    {3, {{20,G(0,6,12,16)},{20,G(0,6,12,16)},{24,G(0,6,12,18)}}},
    /* 7/4 x2 (4+4+3+3) + 2/4 */
    {3, {{28,G(0,8,16,22)},{28,G(0,8,16,22)},{8,G(0,4,4,4)}}},
    /* 9/8 x3 (2+2+2+3) + 5/8 (3+2) */
    {4, {{18,G(0,4,8,12)},{18,G(0,4,8,12)},{18,G(0,4,8,12)},{10,G(0,6,6,6)}}},
    /* 11/8 x2 (3+3+3+2) + 10/8 (3+3+2+2) */
    {3, {{22,G(0,6,12,18)},{22,G(0,6,12,18)},{20,G(0,6,12,16)}}}
};
#undef G
#define FM(tone, role, level, rev, dly) {GROOVE_DX7, role, tone, level, rev + 16*dly}
#define AC(tone, role, level, rev, dly) {GROOVE_303, role, tone, level, rev + 16*dly}
#define PS(tone, role, level, rev, dly) {GROOVE_PSG, role, tone, level, rev + 16*dly}
#define PL(tone, role, level, rev, dly) {GROOVE_PLUCK, role, tone, level, rev + 16*dly}
#define DR(level) {GROOVE_808, BEAT, 0, level, 0}
#define SA(role, level) {GROOVE_SAMPLE, role, 0, level, 1}
/* Sixteen style sketches. Bass stays dry, organs and keys get a room, leads
 * get tempo-related echoes. delay_steps counts sixteenths at the initial
 * tempo and must stay within the delay range (about bpm / 22 steps).
 * Rhythms: 0 straight, 1 driving, 2 half-time, 3 shuffle, 4 gallop,
 * 5 3+3+2, 6 four-floor, 7 stomp, 8 rhythm box, 9 new wave. Motif picks the bass figure and melody;
 * its low bit also picks the chord progression. The optional last field
 * picks a mixed meter; drums, bass and chords then follow its beat groups. */
static const struct scene scenes[GROOVE_FACTORY_PROJECTS] = {
    {"ARENA ROCK",  124,40,0, 0,1,0,4, 60,3,30,74, {FM(TONEWHEEL,KEYS,94,3,0), FM(MOOG_BASS,BASS,94,0,0), SA(BEAT,88), FM(SYNC_LEAD,LEAD,78,3,2)}, M44, 0},
    {"GALLOP PROG", 140,45,1, 0,4,3,4, 52,3,28,70, {FM(SYNTH_BRASS,STAB,88,3,0), AC(AN_SQUARE,BASS,38,0,0), SA(BEAT,88), FM(MINI_LEAD,LEAD,80,3,3)}, M44, 0},
    {"CATHEDRAL",    76,36,0, 0,2,2,3, 98,3,40,90, {FM(PIPE_ORGAN,PAD,98,7,0), FM(TAURUS,BASS,92,1,0), FM(MINI_LEAD,LEAD,78,5,3)}, M44, 0},
    {"RONDO 7/8",   132,40,0, 0,5,5,5, 58,2,30,72, {FM(TONEWHEEL,STAB,88,2,0), AC(AN_BASS,BASS,40,0,0), SA(BEAT,86), FM(MINI_LEAD,LEAD,80,3,2), FM(HARPSICHORD,ARP,66,3,1)}, M78, 0},
    {"SYMPHO POP",  120,38,0, 0,6,0,5, 70,2,30,76, {FM(STRINGS,PAD,86,5,0), FM(STAGE_EP,KEYS,96,4,0), FM(MOOG_BASS,BASS,90,0,0), SA(BEAT,86), FM(HARPSICHORD,ARP,62,3,2)}, M44, 0},
    {"LUNAR DRIFT",  64,38,1, 0,2,2,5, 92,2,54,84, {FM(PROG_PAD,PAD,86,7,0), FM(STAGE_EP,KEYS,94,5,2), FM(TAURUS,BASS,92,0,0), SA(BEAT,76), FM(MINI_LEAD,LEAD,74,6,6)}, M44, 0},
    {"ASTRO ORGAN", 112,40,1, 0,0,1,4, 72,3,46,78, {FM(COMBO_ORGAN,STAB,88,4,3), FM(MOOG_BASS,BASS,90,0,0), SA(BEAT,84), AC(AN_LEAD,ARP,28,3,4)}, M44, 0},
    {"WURLI BOUNCE",104,43,0,30,3,4,4, 56,2,26,70, {FM(WURLI,KEYS,102,3,0), FM(MOOG_BASS,BASS,88,0,0), SA(BEAT,86), FM(TAPE_FLUTE,LEAD,78,4,2)}, M44, 0},
    {"BAROQUE ROCK", 92,46,0, 0,0,2,4, 76,2,30,78, {FM(HARPSICHORD,KEYS,92,4,0), FM(TAPE_CHOIR,PAD,82,6,0), FM(TAURUS,BASS,88,0,0), SA(BEAT,86)}, M44, 0},
    {"TAPE CHOIR",   70,40,1, 0,2,2,4, 90,3,44,82, {FM(TAPE_CHOIR,PAD,88,6,0), FM(TAPE_FLUTE,LEAD,82,5,2), FM(TAURUS,BASS,92,0,0), PL(1,ARP,72,5,4)}, M44, 0},
    {"ODYSSEY 5/4", 136,42,1, 0,5,3,5, 60,3,32,72, {FM(SYNTH_BRASS,STAB,88,3,0), FM(MOOG_BASS,BASS,92,0,0), SA(BEAT,86), FM(SYNC_LEAD,LEAD,78,3,2), FM(PROG_PAD,PAD,60,5,0)}, M54, 0},
    {"DUEL 9/8",    128,45,1, 0,1,5,5, 58,3,30,72, {FM(MINI_LEAD,LEAD,82,3,2), AC(AN_SQUARE,BASS,38,0,0), SA(BEAT,86), FM(SYNC_LEAD,ARP,66,3,3), FM(SYNTH_BRASS,PAD,58,4,0)}, M98, 0},
    {"FUSION 11/8", 150,40,1, 0,4,1,5, 50,3,24,66, {FM(MINI_LEAD,LEAD,82,2,1), FM(CLAV,STAB,90,2,0), AC(AN_BASS,BASS,40,0,0), SA(BEAT,88), FM(TONEWHEEL,PAD,62,3,0)}, M118, 0},
    {"PEDAL 7/4",   100,40,0, 0,0,2,4, 78,3,36,78, {FM(TAURUS,BASS,100,0,0), FM(PROG_PAD,PAD,76,6,0), SA(BEAT,84), FM(MINI_LEAD,LEAD,78,4,3)}, M74, 0},
    {"SEQUENCER 78",118,40,1, 0,8,0,5, 86,3,46,82, {AC(AN_LEAD,ARP,30,3,3), FM(STRINGS,PAD,90,6,0), FM(SYNC_LEAD,LEAD,74,5,3), DR(82), FM(MOOG_BASS,BASS,84,0,0)}, M44, 1},
    {"COLD MACHINE",126,41,1, 0,9,0,4, 52,2,30,70, {FM(MOOG_BASS,BASS,96,0,0), FM(POLY_VOX,PAD,80,4,0), SA(BEAT,90), FM(MINI_LEAD,LEAD,72,3,2)}, M44, 0}
};
#undef FM
#undef AC
#undef PS
#undef PL
#undef DR
#undef SA

/* Kick, backbeat, closed hat, open hat and a small percussion response. */
static const uint16_t beats[10][5] = {
    {0x0501,0x1010,0x5555,0x0000,0x8000}, /* straight rock */
    {0x0141,0x1010,0x5555,0x4000,0x0080}, /* driving eighths */
    {0x0401,0x0100,0x5555,0x0000,0x2000}, /* half-time */
    {0x0901,0x1010,0x5555,0x0000,0x0080}, /* shuffle (with swing) */
    {0x0909,0x1010,0xffff,0x0000,0x8000}, /* gallop sixteenths */
    {0x4949,0x1010,0x5555,0x0000,0x0080}, /* 3+3+2 accents */
    {0x1111,0x1010,0x1111,0x4444,0x0080}, /* four on the floor */
    {0x0505,0x1010,0x0000,0x0000,0x0000}, /* stomp and backbeat */
    {0x0101,0x1010,0xffff,0x0000,0x4444}, /* rhythm box, sixteenth hats */
    {0x0101,0x1010,0x5555,0x0000,0x0000}  /* cold, steady new wave */
};
static const uint16_t bass_masks[6] = {0x5555,0x1525,0x0101,0xdddd,0x7575,0x4929};
static const int8_t melodies[6][16] = {
    {0,-1,2,-1,4,-1,2,1, 0,-1,4,-1,5,4,2,-1},
    {0,0,-1,4,0,-1,2,0, 5,-1,4,2,-1,0,2,-1},
    {-1,-1,2,-1,4,-1,-1,2, -1,1,-1,-1,0,-1,-1,-1},
    {4,-1,-1,-1,2,-1,1,-1, 0,-1,-1,2,5,-1,-1,-1},
    {0,2,4,5,4,-1,2,-1, 1,2,4,-1,5,4,2,-1},
    {-1,0,-1,2,-1,4,2,-1, 5,-1,4,2,-1,1,0,-1}
};
static unsigned bar_root(const struct scene *s, unsigned bar)
{
    /* I-V-vi-IV, i-VI-III-VII, I-bVII-IV-I, i-iv-VI-V. */
    static const uint8_t progression[4][4] = {{0,7,9,5},{0,8,3,10},{0,10,5,0},{0,5,8,7}};
    return s->root + progression[s->minor + 2*(s->motif & 1U)][bar];
}
static unsigned bar_minor(const struct scene *s, unsigned bar)
{
    static const uint8_t quality[4][4] = {{0,0,1,0},{1,0,0,0},{0,0,0,0},{1,1,0,0}};
    return quality[s->minor + 2*(s->motif & 1U)][bar];
}
static FM1_DSP_DATA const char fm_names[DEMO_TONES][11] = {
    "TONEWHEEL ","COMBO ORG ","PIPE ORGAN","WURLI     ","STAGE EP  ","HARPSICHRD",
    "FUNK CLAV ","MINI LEAD ","SYNC LEAD ","TAURUS    ","MOOG BASS ","STRINGS   ",
    "TAPE FLUTE","TAPE CHOIR","SYNTHBRASS","PROG PAD  ","POLY VOX  "
};
/* Three independently voiced operator pairs (DX7 algorithm 5): body, colour
 * and transient. Pair 0's modulator carries the feedback. Organs replace
 * the pairs with six additive drawbars (algorithm 32), see below. */
/* Pack recipe fields without changing the serialized patch layout. */
struct fm_pair { uint8_t level, coarse : 2, detune : 4, mod, ratio, fine, decay, body; };
_Static_assert(sizeof(struct part) == 4 && sizeof(struct fm_pair) == 7, "compact recipes");
struct fm_voice {
    uint8_t attack, decay, settle, release, body, sustain, mod_attack, mod_release, feedback;
    struct fm_pair pair[3];
};
static FM1_DSP_DATA const struct fm_voice voices[DEMO_TONES] = {
    /* Organs: only attack, decay, settle, release and feedback apply. */
    {99,60,45,80,99,99,99,80,0, {{0},{0},{0}}},
    {99,60,45,82,99,99,99,82,0, {{0},{0},{0}}},
    {78,50,40,58,99,99,78,58,0, {{0},{0},{0}}},
    /* WURLI: reedy bark that softens as the note decays. */
    {99,40,30,66,90,0,99,70,1, {{99,1,7,64,1,0,68,42},{82,1,8,50,1,0,60,30},{68,2,7,40,3,0,80,0}}},
    /* STAGE EP: round body with a bright, fast tine. */
    {99,45,26,62,88,0,99,66,0, {{99,1,7,42,1,0,62,30},{86,1,8,36,1,0,66,20},{72,1,6,60,14,0,90,0}}},
    /* HARPSICHRD: 8' and 4' plucked registers. */
    {99,58,40,74,72,0,99,80,2, {{97,1,7,74,1,0,70,40},{86,2,8,66,3,0,72,30},{70,3,6,58,5,0,80,0}}},
    /* FUNK CLAV */
    {99,75,47,82,60,0,99,84,2, {{99,1,7,70,1,0,82,24},{90,2,7,58,2,0,86,0},{78,2,7,50,3,0,88,0}}},
    /* MINI LEAD: two detuned saws and a square sub, gentle filter fall. */
    {99,60,50,70,99,95,99,75,6, {{99,1,7,80,1,0,55,82},{90,1,9,74,1,0,55,80},{70,0,5,60,1,0,60,74}}},
    /* SYNC LEAD: bright partials that sweep down like a synced oscillator. */
    {99,50,40,68,99,92,74,70,5, {{99,1,7,84,2,0,40,62},{82,1,9,76,3,0,38,58},{62,1,5,56,1,0,50,70}}},
    /* TAURUS: deep sustaining bass pedal. */
    {94,55,40,62,99,95,99,74,3, {{99,1,7,62,1,0,45,46},{90,0,8,42,1,0,50,30},{74,1,6,0,1,0,80,0}}},
    /* MOOG BASS: punchy filter envelope. */
    {99,62,46,78,95,86,99,82,5, {{99,1,7,76,1,0,62,42},{86,1,8,64,1,0,66,30},{74,0,7,42,1,0,70,20}}},
    /* STRINGS: string machine ensemble, slow attack. */
    {58,45,38,52,97,94,55,54,2, {{93,1,4,58,1,0,52,80},{89,1,10,56,1,0,52,78},{74,2,7,40,1,0,52,70}}},
    /* TAPE FLUTE: breathy sine with a little tape wow. */
    {72,50,40,58,96,92,82,60,7, {{99,1,7,38,1,0,50,30},{72,2,8,20,1,0,50,12},{50,1,6,0,1,0,50,0}}},
    /* TAPE CHOIR: soft vowel built from low-index partials. */
    {55,40,35,52,95,93,55,52,0, {{94,1,6,32,1,0,50,30},{84,2,8,30,1,0,50,26},{68,3,7,34,1,0,50,30}}},
    /* SYNTHBRASS: brightness swells after the attack. */
    {90,55,45,66,99,92,62,64,4, {{99,1,6,78,1,0,55,74},{93,1,9,72,1,0,55,70},{72,0,7,50,1,0,58,60}}},
    /* PROG PAD: slow, detuned and evolving. */
    {45,40,32,44,96,92,40,46,1, {{91,1,5,52,1,0,40,70},{87,1,9,48,2,0,40,60},{72,0,7,40,1,0,40,55}}},
    /* POLY VOX: bright, nasal polysynth ensemble (late-70s new wave). */
    {86,50,40,60,99,95,86,60,5, {{96,1,3,80,1,0,60,86},{92,1,11,78,1,0,60,84},{76,2,7,64,1,0,60,78}}}
};
/* LFO per tone: speed, delay, pitch depth, amp depth, wave (0 triangle,
 * 4 sine), pitch sensitivity, carrier amp sensitivity. Speed 6 is about
 * 1 Hz, 33 about 5.5 Hz, 39 about 6.5 Hz. Read only when building. */
struct fm_lfo { uint8_t speed, delay, pmd, amd, wave, pms, ams; };
static const struct fm_lfo lfos[DEMO_TONES] = {
    {39, 0, 5,14,4,2,1}, /* rotary speaker, fast */
    {34, 0,10, 0,0,3,0}, /* combo vibrato */
    { 6, 0, 0, 0,4,0,0},
    {30, 0, 0,10,0,0,1}, /* tremolo */
    {28, 0, 0, 8,4,0,1},
    {28,35, 0, 0,4,0,0},
    {28,35, 0, 0,4,0,0},
    {33,50,14, 0,4,3,0}, /* delayed vibrato */
    {33,50,12, 0,4,3,0},
    {28,35, 0, 0,4,0,0},
    {28,35, 0, 0,4,0,0},
    {36, 0, 7, 0,0,3,0}, /* ensemble shimmer */
    { 7, 0, 6, 0,0,2,0}, /* tape wow */
    { 8, 0, 6, 4,0,2,1},
    {33,60, 8, 0,4,2,0},
    {20, 0, 6, 0,0,2,0},
    {38, 0, 8, 0,0,3,0}  /* ensemble */
};
/* Drawbars for the three organs: coarse ratio (0 = 0.5), fine, level, and
 * a small detune spread for tonewheel beating. */
struct fm_organ { uint8_t coarse[6], fine[6], level[6], detune[6]; };
static const struct fm_organ organs[3] = {
    /* 16', 5 1/3', 8', 4', 2 2/3' (decaying percussion), 2'. */
    {{0,1,1,2,3,4}, {0,50,0,0,0,0}, {86,79,90,75,77,65}, {7,8,6,7,8,6}},
    /* Bright reedy combo: 8', 4', 2 2/3', 2', 1 3/5', 1'. */
    {{1,2,3,4,5,8}, {0,0,0,0,0,0}, {90,67,79,61,69,55}, {7,8,7,6,7,8}},
    /* Full pipe principal chorus: 16', 8', 4', 2 2/3', 2', 1'. */
    {{0,1,2,3,4,8}, {0,0,0,0,0,0}, {83,90,81,65,73,59}, {6,7,8,7,6,8}}
};
static void demo_voice(uint8_t p[156], unsigned tone)
{
    const struct fm_voice *sound = &voices[tone];
    const struct fm_lfo *lfo = &lfos[tone];
    memset(p, 0, 156);
    memcpy(p + 145, fm_names[tone], 10);
    for (unsigned op = 0; op < 6; ++op) {
        uint8_t *v = p + 21 * op;
        const struct fm_pair *pair = &sound->pair[op / 2];
        unsigned carrier = op & 1U;
        v[0] = carrier ? sound->attack : sound->mod_attack;
        v[1] = carrier ? sound->decay : pair->decay;
        v[2] = sound->settle; v[3] = carrier ? sound->release : sound->mod_release;
        v[4] = 99; v[5] = carrier ? sound->body : pair->body;
        v[6] = carrier ? sound->sustain : sound->sustain ? pair->body : 0;
        v[8] = 50; v[10] = carrier ? 8 : 18; v[13] = 1;
        v[14] = carrier ? lfo->ams : 0;
        v[15] = carrier ? 2 : 4;
        v[16] = carrier ? pair->level : pair->mod;
        v[18] = carrier ? pair->coarse : pair->ratio;
        v[19] = carrier ? 0 : pair->fine; v[20] = pair->detune;
        if (tone <= PIPE_ORGAN) {
            const struct fm_organ *o = &organs[tone];
            v[5] = v[6] = 99; v[10] = 0; v[14] = lfo->ams; v[15] = 1;
            v[16] = o->level[op]; v[18] = o->coarse[op];
            v[19] = o->fine[op]; v[20] = o->detune[op];
            v[0] = sound->attack; v[1] = sound->decay; v[3] = sound->release;
            /* Tonewheel percussion: the 2 2/3' drawbar fades to a lower level. */
            if (tone == TONEWHEEL && op == 4) { v[1] = 62; v[5] = v[6] = 70; }
        }
    }
    for (unsigned i = 126; i < 130; ++i) p[i] = 99;
    for (unsigned i = 130; i < 134; ++i) p[i] = 50;
    p[134] = tone <= PIPE_ORGAN ? 31 : 4; p[135] = sound->feedback;
    p[136] = 1; p[137] = lfo->speed; p[138] = lfo->delay;
    p[139] = lfo->pmd; p[140] = lfo->amd; p[141] = tone <= PIPE_ORGAN ? 0 : 1;
    p[142] = lfo->wave; p[143] = lfo->pms;
    p[144] = 24; p[155] = 63;
}
static void preset(struct groove_preset *p, const struct part *v)
{
    p->type = v->source; p->volume = v->level; p->modified = 1;
    p->controls.envelope[2] = 100;
    if (v->source == GROOVE_DX7) {
        demo_voice(p->data, v->tone); p->program = v->tone; p->modified = 0;
    }
    else if (v->source == GROOVE_303) {
        /* Cutoff, reso, env mod, decay, accent, wave, tune, volume,
         * drive, drive type, slide, accent decay. Low resonance and longer
         * decays move the acid engine towards classic ladder-filter tones. */
        static const uint8_t bass[4][BASS303_NPARAMS] = {
            {50,16,38,74,30,0,64,98,14,1,10,10}, /* AN_BASS */
            {36,10,22,92,12,0,64,100,6,1,8,10},  /* AN_PEDAL */
            {72,40,44,82,40,0,64,92,28,1,24,12}, /* AN_LEAD */
            {48,28,50,60,44,1,64,96,10,1,14,8}   /* AN_SQUARE */
        };
        memcpy(p->data, bass[v->tone], BASS303_NPARAMS);
        p->program = p->data[BASS303_WAVE]; p->controls.mono = 1;
    } else if (v->source == GROOVE_808) {
        /* Public D8_PRESET_BYTES format: selectors, then two parameter sets.
         * Sound selectors must match each set for a lossless store/load. */
        unsigned at = D8_NUM;
        for (unsigned variant = 0; variant < 2; ++variant)
            for (unsigned part = 0; part <= D8_NUM; ++part)
                for (int i = 0; i < drum808_nparams((int)part); ++i) {
                    const fm1_param_t *d = drum808_param((int)part, i);
                    static const uint8_t levels[D8_NUM] = {70,52,46,42,38,36,46,28,25,30,29};
                    static const uint8_t pans[D8_NUM] = {64,64,49,64,79,54,72,80,47,77,72};
                    unsigned value = d->def;
                    if (!strcmp(d->name, "Sound")) value = variant;
                    if (part < D8_NUM) {
                        if (!strcmp(d->name, "Level")) value = levels[part];
                        else if (!strcmp(d->name, "Pan")) value = pans[part];
                        else if (!strcmp(d->name, "Rev")) value = part == D8_SD || part == D8_CP ? 24 : 9;
                        else if (!strcmp(d->name, "Dly")) value = part == D8_RS ? 12 : 0;
                        else if (!strcmp(d->name, "Decay") && part == D8_SD) value = 84;
                    }
                    p->data[at++] = (uint8_t)value;
                }
    } else if (v->source == GROOVE_SAMPLE) {
        p->sampler_pads = SAMPLE_DRUM_PADS;
        for (unsigned pad = 0; pad < SAMPLE_DRUM_PADS; ++pad) {
            uint8_t *pot = p->data + pad * SAMPLE_DRUM_PARAMS;
            for (unsigned i = 0; i < SAMPLE_DRUM_PARAMS; ++i) pot[i] = sampledrum_param(i)->def;
            static const uint8_t levels[SAMPLE_DRUM_PADS] = {105,83,75,83,72,73,58,62};
            pot[SAMPLE_DRUM_LEVEL] = levels[pad];
            pot[SAMPLE_DRUM_VELOCITY_LPF] = pad == 0 ? 12 : 30;
            if (pad == 6 || pad == 7) pot[SAMPLE_DRUM_CHOKE] = 1;
        }
    } else if (v->source == GROOVE_PLUCK) {
        static const uint8_t pluck[3][4] = {{88,70,24,48},{103,52,24,66},{79,89,24,42}};
        memcpy(p->data, pluck[v->tone], 4);
    } else {
        p->program = p->data[0] = v->tone; p->data[1] = 16;
        p->data[2] = 24; p->data[3] = v->role == BASS ? 20 : 38;
    }
    p->data[209] = v->sends;
}
const char *groove_factory_name(unsigned index)
{
    return index < GROOVE_FACTORY_PROJECTS ? scenes[index].name : NULL;
}
int groove_factory_build(unsigned index, struct groove_project *p)
{
    if (!p || index >= GROOVE_FACTORY_PROJECTS) return GROOVE_PROJECT_INVALID;
    const struct scene *s = &scenes[index];
    memset(p, 0, sizeof *p);
    memset(p->name, ' ', sizeof p->name); memcpy(p->name, s->name, strlen(s->name));
    p->tempo = s->bpm; p->mute = (uint8_t)(255U << s->count);
    unsigned delay = FM1_SAMPLE_RATE * 60U * s->delay_steps / (s->bpm * 32U);
    p->effects[0] = s->decay;
    p->effects[1] = (uint8_t)((delay - 166U + 7U) / 14U);
    p->effects[2] = s->feedback; p->effects[3] = s->fx_level;
    groove_sequence_defaults(&p->sequence); p->sequence.swing = s->swing;
    static const uint8_t open_chords[2][8] = {{4,4,11,14},{4,3,10,14}};
    memcpy(p->sequence.chord[12], open_chords, sizeof open_chords);
    groove_project_arp_defaults(p);
    const struct meter_shape *m = &meters[s->meter < METERS ? s->meter : M44];
    unsigned odd = m != &meters[M44];
    for (unsigned t = 0; t < GROOVE_TRACKS; ++t) {
        const struct part empty = {GROOVE_PSG,LEAD,0,100,0};
        const struct part *v = t < s->count ? &s->part[t] : &empty;
        preset(&p->preset[t], v);
        if (t >= s->count) continue;
        for (unsigned bar = 0, first = 0; bar < m->bars; first += m->bar[bar++].steps) {
            const struct bar_shape *shape = &m->bar[bar];
            /* Four chords per loop; a fifth (filler) bar holds the last one. */
            unsigned harmony = bar < 4 ? bar * 4U / (m->bars < 4 ? m->bars : 4U) : 3U;
            unsigned root = bar_root(s, harmony), minor = bar_minor(s, harmony);
            unsigned last = bar + 1U == m->bars, group = 0;
            for (unsigned i = 0; i < shape->steps; ++i) {
                struct groove_step *n = &p->step[t][first + i];
                unsigned bit = 1U << (i & 15U), chord = 0, gate = 7, tie = 0, pitch = root + 24;
                unsigned start = (unsigned)(shape->groups >> i) & 1U, eighth = !(i & 1U);
                if (start && i) ++group;
                if (v->role == BEAT || v->role == PERC) {
                    const uint16_t *b = beats[s->rhythm];
                    unsigned sample = v->source == GROOVE_SAMPLE;
                    /* Odd meters: kick and snare alternate on group starts. */
                    unsigned kick = odd ? start && !(group & 1U) : b[0] & bit;
                    unsigned snare = odd ? start && (group & 1U) : b[1] & bit;
                    unsigned closed = odd ? eighth : b[2] & bit;
                    unsigned open = odd ? 0 : b[3] & bit;
                    unsigned perc = odd ? i + 2U == shape->steps : b[4] & bit;
                    if (v->role == BEAT && kick) n->drums |= 1U;
                    if (snare) n->drums |= (uint16_t)(1U << (v->role == PERC && sample ? 2 : 1));
                    if (closed) n->drums |= (uint16_t)(1U << (sample ? 6 : 10));
                    if (open) {
                        n->drums &= (uint16_t)~(1U << (sample ? 6 : 10));
                        n->drums |= (uint16_t)(1U << (sample ? 7 : 9));
                    }
                    if (perc && (bar & 1U)) n->drums |= (uint16_t)(1U << (sample ? 2 : 5));
                    if (last && i + 1U == shape->steps) n->drums |= (uint16_t)(1U << (sample ? 4 : 3));
                    if (n->drums) n->velocity = (uint8_t)((n->drums & 3U) ? 100 : (odd ? start : i % 4 == 0) ? 74 : 59 + (bar & 1U)*4);
                    continue;
                }
                if (v->role == BASS) {
                    if (odd) {
                        /* Eighths only: pedal on group starts, or drive with
                         * an octave pickup into each group. */
                        unsigned pickup = i + 2U >= shape->steps || ((shape->groups >> (i + 2U)) & 1U);
                        unsigned dense = s->motif == 0 || s->motif == 3 || s->motif == 4;
                        if (!eighth || (s->motif == 2 ? !start : !(start || dense || pickup))) continue;
                        pitch = root + (start ? (group == 2 ? 7U : 0U) : pickup ? 12U : 0U);
                    } else {
                        if (!(bass_masks[s->motif] & bit)) continue;
                        pitch = root + (i == 14 ? 12U : i == 7 || i == 11 ? 7U : 0U);
                    }
                    gate = 12;
                } else if (v->role == KEYS || v->role == PAD || v->role == STAB) {
                    unsigned mask = v->role == STAB ? 0x0444 : v->role == KEYS ? 0x0909 : 0x0001;
                    unsigned hit = !odd ? (mask & bit) != 0
                                 : v->role == PAD ? i == 0
                                 : v->role == KEYS ? start && !(group & 1U) : start;
                    if (!hit) continue;
                    pitch = root + 12; chord = minor ? 2 : 1;
                    if (v->role == KEYS && v->source == GROOVE_DX7) chord = 13U + minor;
                    if (v->role == PAD) { gate = 15; tie = 1; }
                    else if (v->role == KEYS) { gate = 12; tie = i == 0 || (!odd && i == 8); }
                    else gate = 9;
                } else if (v->role == ARP && s->seq) {
                    /* Sixteenth sequencer: chord tones bouncing off the octave. */
                    static const uint8_t line[16] = {0,3,2,3, 1,3,2,3, 0,3,2,4, 1,3,2,5};
                    const unsigned tones[6] = {0,minor ? 3U : 4U,7,12,minor ? 10U : 11U,minor ? 15U : 16U};
                    pitch = root + 12 + tones[line[i & 15U]];
                    gate = 6;
                } else if (v->role == ARP) {
                    if (i & 1U) continue;
                    const unsigned offsets[4] = {0,minor ? 3U : 4U,7,12};
                    pitch = root + 24 + offsets[(i / 2 + bar) % 4];
                    gate = 9;
                } else {
                    int degree = melodies[s->motif][i & 15U];
                    if (degree < 0) continue;
                    static const uint8_t scale[2][7] = {{0,2,4,7,9,12,14},{0,3,5,7,10,12,15}};
                    pitch = root + 24 + scale[minor][degree]; gate = 11;
                }
                n->note = (uint8_t)pitch;
                n->velocity = (uint8_t)(odd ? (start ? 100 : eighth ? 88 : 76) : i % 4 == 0 ? 100 : i % 2 == 0 ? 88 : 76);
                n->drums = (uint16_t)(chord | (gate << 5) | (tie << 12));
            }
        }
    }
    return groove_project_valid(p) ? 0 : GROOVE_PROJECT_INVALID;
}
