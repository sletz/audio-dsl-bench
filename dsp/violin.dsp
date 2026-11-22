import("stdfaust.lib");
// pm.violin_ui_MIDI (examples/physicalModeling/violinMIDI.dsp), line for line, except the
// gate: a slider held at 1 instead of a button, so that the benchmark plays a sustained note.
f = hslider("freq", 440, 50, 1000, 0.01);
bend = ba.semi2ratio(hslider("bend", 0, -2, 2, 0.01)) : si.polySmooth(gate, 0.999, 1);
gain = hslider("gain", 0.6, 0, 1, 0.01);
envAttack = hslider("envAttack", 1, 0, 30, 0.01)*0.001;
s = hslider("sustain", 0, 0, 1, 1);
bowPress = hslider("pressure", 0.5, 0, 1, 0.01) : si.smoo;
bowPos = hslider("position", 0.7, 0, 1, 0.01) : si.smoo;
vibratoFreq = hslider("vibratoFrequency", 6, 1, 10, 0.01);
vibratoGain = hslider("vibratoGain", 0.5, 0, 1, 0.01)*0.01;
outGain = hslider("outGain", 0.5, 0, 1, 0.01);
t = hslider("gate", 1, 0, 1, 1);

gate = t+s : min(1);
vibrato = 1+os.osc(vibratoFreq)*vibratoGain*envelope;
freq = f*bend*vibrato;
envelope = gate*gain : si.smooth(ba.tau2pole(envAttack));
stringLength = freq : pm.f2l;
bowVel = envelope;
process = pm.violinModel(stringLength, bowPress, bowVel, bowPos)*outGain <: _, _;
