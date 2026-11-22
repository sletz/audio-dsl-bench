import("stdfaust.lib");
// examples/filtering/diodeLadder.dsp: ve.diodeLadder on the internal 100 Hz os.sawtooth, the
// "Saw/Noise" checkbox held at its default 0 (noise branch dropped). A generator: no input is read.
Q = hslider("Q", 1, 0.7072, 25, 0.01);
normFreq = hslider("freq", 0.1, 0, 1, 0.001) : si.smoo;
process = os.sawtooth(100) : ve.diodeLadder(normFreq, Q) <: _, _;
