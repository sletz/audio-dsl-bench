import("stdfaust.lib");
// dm.gate_demo (examples/dynamic/noiseGate.dsp): ef.gate_stereo with the demo's sliders and
// defaults, without the bypass checkbox and the gate-gain meter (bargraph + attach, which have no
// audio effect). At the default -30 dB threshold the harness noise (about 0.29 RMS) keeps the gate open.
threshold = hslider("Threshold", -30, -120, 0, 0.1);
attack = hslider("Attack", 10, 10, 10000, 1) : *(0.000001) : max(1.0/float(ma.SR));
hold = hslider("Hold", 200, 1, 1000, 1) : *(0.001) : max(1.0/float(ma.SR));
release = hslider("Release", 100, 1, 1000, 1) : *(0.001) : max(1.0/float(ma.SR));
process = ef.gate_stereo(threshold, attack, hold, release);
