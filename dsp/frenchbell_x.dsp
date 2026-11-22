import("stdfaust.lib");
// pm.frenchBell_ui (examples/physicalModeling/frenchBell.dsp), except the gate: a slider held
// at 1 instead of a button, so that the bell is struck once at the start of the benchmark.
strikePosition = nentry("strikePosition", 0, 0, 6, 1);
strikeCutoff = hslider("strikeCutOff", 6500, 20, 20000, 1);
strikeSharpness = hslider("strikeSharpness", 0.5, 0.01, 5, 0.01);
gain = hslider("gain", 1, 0, 1, 0.01);
gate = hslider("gate", 1, 0, 1, 1);
process = pm.frenchBell(strikePosition, strikeCutoff, strikeSharpness, gain, gate) <: _, _;
