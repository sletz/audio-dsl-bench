import("stdfaust.lib");
// examples/misc/autopan.dsp: stereo auto-panner, os.oscp LFOs through a tanh saturator
// (it.interpolate_linear) and it.remap; same sliders and defaults, except Amount set to 1 (its
// default 0 makes both gains constant 1).
amount = hslider("amount", 1., 0., 1., .001);
rate = hslider("rate", 1., .05, 90., .001);
phase = hslider("phase", 180., 0., 360., 15) : si.smoo;
shape = hslider("shape", 0., 0., 1., 0.001) : si.smoo;

saturator(shape, x) = x, ma.tanh(x*10.) : it.interpolate_linear(shape);
phase2Gain(phase) = os.oscp(rate, phase) : saturator(shape) : it.remap(-1., 1., 1.-amount, 1.);
gainLeft = 0. : phase2Gain;
gainRight = phase : ma.deg2rad : phase2Gain;

process = _*gainLeft, _*gainRight;
