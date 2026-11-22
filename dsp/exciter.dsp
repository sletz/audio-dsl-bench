import("stdfaust.lib");
// dm.exciter (examples/psychoacoustic/harmonicExciter.dsp): highpass, co.compressor_mono, pregain,
// asymmetric cubic harmonic creator, postgain, mixed with the dry signal; same sliders and
// defaults, without the compressor bypass checkbox and the gain meter (bargraph + attach).
ratio = hslider("ratio", 5, 1, 20, 0.1);
threshold = hslider("threshold", -30, -100, 10, 0.1);
attack = hslider("attack", 50, 0, 500, 0.1) : *(0.001) : max(1/ma.SR);
release = hslider("release", 500, 0, 1000, 0.1) : *(0.001) : max(1/ma.SR);
fc = hslider("cutoff", 5000, 1000, 10000, 100);
ph = hslider("harmonics", 20, ma.EPSILON, 200, 1) / 100;
ml = hslider("mix", 0.5, 0, 1, 0.01);

compressor = co.compressor_mono(ratio, threshold, attack, release);
pregain = *(ph);
harmonicCreator(x) = x <: cubDist1, cubDist2, cubDist3 :> _;
cubDist1(x) = (x < 0) * x;
cubDist2(x) = (x >= 0) * (x <= 1) * (x - x ^ 3 / 3);
cubDist3(x) = (x > 1) * 2/3;
postgain = *(1/ph);
balance = (_ * ml), (_ * (1.0 - ml)) :> _;

process = _ <: (fi.highpass(2, fc) : compressor : pregain : harmonicCreator : postgain), _ : balance;
