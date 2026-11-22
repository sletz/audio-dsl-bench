import("stdfaust.lib");
// examples/reverb/dattorro.dsp: dm.dattorro_rev_demo written out, re.dattorro_rev with the demo's
// slider defaults (pre-delay 0) smoothed by si.smoo, then its dry/wet mix and output level.
bw = hslider("prefilter", 0.7, 0, 1, 0.001) : si.smoo;
i_diff1 = hslider("inDiffusion1", 0.625, 0, 1, 0.001) : si.smoo;
i_diff2 = hslider("inDiffusion2", 0.625, 0, 1, 0.001) : si.smoo;
d_diff1 = hslider("fbDiffusion1", 0.625, 0, 1, 0.001) : si.smoo;
d_diff2 = hslider("fbDiffusion2", 0.625, 0, 1, 0.001) : si.smoo;
decay = hslider("decay", 0.7, 0, 1, 0.001) : si.smoo;
damping = hslider("damping", 0.625, 0, 1, 0.001) : si.smoo;
drywet = hslider("drywet", 0, -1, 1, 0.01) : si.smoo;
gain = hslider("level", -6, -70, 40, 0.1) : ba.db2linear : si.smoo;

wet = 0.5*(drywet+1.0);
dry = 1.0-wet;
dry_wet(x, y) = *(dry) + wet*x, *(dry) + wet*y;
process = _,_ <: re.dattorro_rev(0, bw, i_diff1, i_diff2, decay, d_diff1, d_diff2, damping), _,_
    : dry_wet : *(gain), *(gain);
