// Legend of Faerghail magic-point table — transcribed from the DOS release.
// It lives only inside LOF.EXE (image offset 0x42748); no data file carries it,
// and the drop-in must run without the original EXE, so it keeps this copy.
// Regenerate with `tools/spec.py --emit-mp`.
// 12 professions x 16 levels; the MP fill routine indexes it by profession
// (+0x1b) and level (+0x17).  A non-zero base gets +1 for Wisdom > 15 and
// +2 more for Intelligence > 15 (see CharSheet::magic_max).

static const uint8_t kMagicTable[kMagicProfCount][kMagicLevels] = {
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,1,2,4,5,6,8,10,11,13,14,15,16,18,19,20},
    {0,0,0,0,0,0,1,1,2,2,2,2,2,2,2,2},
    {0,1,2,3,4,5,6,7,9,10,11,12,13,15,16,17},
    {0,1,2,3,4,5,6,7,9,10,11,12,13,15,16,17},
    {0,1,2,4,5,6,8,10,11,13,14,15,16,18,19,20},
    {0,1,2,4,5,6,8,10,11,13,14,15,16,18,19,20},
    {0,0,0,1,1,2,2,3,3,4,4,5,5,5,5,5},
    {0,1,2,4,5,6,8,10,11,13,14,15,16,18,19,20},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
};
