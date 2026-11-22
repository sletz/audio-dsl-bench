import("stdfaust.lib");
// dm.crybaby_demo (examples/filtering/cryBaby.dsp): ve.crybaby driven by the wah slider, with the
// bypass checkbox dropped (ba.bypass1 replaced by the effect alone).
wah = hslider("wah", 0.8, 0, 1, 0.01);
process = ve.crybaby(wah);
