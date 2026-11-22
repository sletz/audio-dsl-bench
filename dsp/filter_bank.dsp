import("stdfaust.lib");
// dm.filterbank_demo = dm.mth_octave_filterbank_demo(1) (examples/filtering/filterBank.dsp): the
// octave filter bank fi.mth_octave_filterbank_default(1, 10000, 10) (5th-order Butterworth band
// splits, delay-equalizing allpasses), each band through its fader (dB, si.smoo), summed. The
// bypass checkbox is dropped (ba.bypass1 replaced by the effect alone).
N = 10;
fader(i) = hslider("Band%2i", -10, -70, 10, 0.1) : si.smoo;
process = fi.mth_octave_filterbank_default(1, 10000, N) : sum(i, N, *(ba.db2linear(fader(N-i))));
