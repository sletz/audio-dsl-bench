import("stdfaust.lib");
// tapiir(6, 2) (examples/delayEcho/tapiir.dsp), same structure, with non-degenerate defaults (the
// example's are all 0): tap i delays 0.05*(i+1) s, every tap gain 0.1 (6x6 feedback loop gain 0.6,
// output mixers 0.1 per tap), input gains 1, gains 1.
dsize = 524288;

// user interface
//---------------
tap(n) = vslider("tap %n", 0.1, 0, 1, 0.1);
in(n) = vslider("input %n", 1, 0, 1, 0.1);
gain = vslider("gain", 1, 0, 1, 0.1);
del(i) = vslider("delay (sec)", 0.05*(i+1), 0, 5, 0.01)*ma.SR;

// mixer and matrix
//-----------------------------------------------------------
mixer(taps, lines) = par(i, taps, *(tap(i))), par(i, lines, *(in(i))):>*(gain);

matrix(taps, lines) = (si.bus(lines+taps)<:tgroup("", par(i, taps, hgroup("Tap %i", mixer(taps, lines):de.delay(dsize, del(i))))))~si.bus(taps);

// tapiir
//--------
tapiir(taps, lines) = vgroup("Tapiir", si.bus(lines)<:(matrix(taps, lines), si.bus(lines))<:vgroup("outputs", par(i, lines, hgroup("output %i", mixer(taps, lines)))));

process = tapiir(6, 2);
