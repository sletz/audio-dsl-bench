import("stdfaust.lib");
// dm.flanger_demo (examples/phasing/flanger.dsp): pf.flanger_stereo with dmax = 8192, os.oscrs and
// os.oscrc LFOs, the demo's slider defaults, "Invert Flange Sum" off (invert = 0); the bypass
// checkbox and the LFO bargraph (no audio effect) are dropped.
dflange = 0.001 * ma.SR * hslider("flangeDelay", 10, 0, 20, 0.001);
odflange = 0.001 * ma.SR * hslider("delayOffset", 1, 0, 20, 0.001);
freq = hslider("speed", 0.5, 0, 10, 0.01);
depth = hslider("depth", 1, 0, 1, 0.001);
fb = hslider("fb", 0, -0.999, 0.999, 0.001);
level = hslider("level", 0, -60, 10, 0.1) : ba.db2linear;
curdel1 = odflange + dflange*(1 + os.oscrs(freq))/2;
curdel2 = odflange + dflange*(1 + os.oscrc(freq))/2;
process = *(level), *(level) : pf.flanger_stereo(8192, curdel1, curdel2, depth, fb, 0);
