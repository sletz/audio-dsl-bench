import("stdfaust.lib");
// pm.flute_ui (examples/physicalModeling/flute.dsp), line for line, with pm.blower_ui and
// pm.fluteModel_ui written out. pm.flute_ui has no gate: the smoothed pressure (si.smoo) starts the
// note at time 0 and holds it. The tube length is smoothed from 0 too (a short glide at the start).
pressure = hslider("pressure", 0.9, 0, 1, 0.01) : si.smoo;
breathGain = hslider("breathGain", 0.1, 0, 1, 0.01)*0.05;
breathCutoff = hslider("breathCutoff", 2000, 20, 20000, 0.1);
vibratoFreq = hslider("vibratoFreq", 5, 0.1, 10, 0.1);
vibratoGain = hslider("vibratoGain", 0.25, 0, 1, 0.01)*0.03;
tubeLength = hslider("tubeLength", 0.8, 0.01, 3, 0.01) : si.smoo;
mouthPosition = hslider("mouthPosition", 0.5, 0, 1, 0.01) : si.smoo;
outGain = hslider("outGain", 0.5, 0, 1, 0.01);

blow = pm.blower(pressure, breathGain, breathCutoff, vibratoFreq, vibratoGain);
process = pm.fluteModel(tubeLength, mouthPosition, blow)*outGain <: _, _;
