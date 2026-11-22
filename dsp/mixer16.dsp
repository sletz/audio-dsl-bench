import("stdfaust.lib");
// 16-channel mixer: dB gain and constant-power pan per channel, master gain.
g(i) = hslider("gain%i", -6, -60, 12, 0.1);
p(i) = hslider("pan%i", i/15.0, 0, 1, 0.001);
master = hslider("master", -3, -60, 12, 0.1);
ch(i) = *(ba.db2linear(g(i))) <: *(cos(p(i)*ma.PI*0.5)), *(sin(p(i)*ma.PI*0.5));
process = par(i, 16, ch(i)) :> *(ba.db2linear(master)), *(ba.db2linear(master));
