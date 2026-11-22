import("stdfaust.lib");
// dm.virtual_analog_oscillator_demo (examples/generator/virtualAnalog.dsp) written out, controls at
// their defaults: the "Order 3" checkbox is held at 0 (pulsetrainN(3) branch dropped) and the
// "Saw Order" entry at 2 (os.sawN(2) instead of the selectn over sawN(1..4)).
ssaw = hslider("Sawtooth", 1, 0, 1, 0.01);
spt = hslider("Pulse Train", 0, 0, 1, 0.01);
ptd = hslider("Duty Cycle", 0.5, 0, 1, 0.01) : si.smooth(0.99);
ssq = hslider("Square", 0, 0, 1, 0.01);
stri = hslider("Triangle", 0, 0, 1, 0.01);
spn = hslider("Pink Noise", 0, 0, 1, 0.01);
sei = hslider("Ext Input", 0, 0, 1, 0.01);
amp = hslider("Mix Amplitude [unit:dB]", -20, -120, 10, 0.1) : ba.db2linear : si.smoo;
freq = hslider("Frequency [unit:PK]", 49, 1, 88, 0.01) : ba.pianokey2hz;
detune1 = 1 + 0.01 * hslider("Detuning 1 [unit:%%]", -0.1, -10, 10, 0.01);
detune2 = 1 + 0.01 * hslider("Detuning 2 [unit:%%]", 0.1, -10, 10, 0.01);
portamento = hslider("Portamento [unit:sec]", 0.1, 0.001, 10, 0.001);
sfreq = freq : si.smooth(ba.tau2pole(portamento));

sawsig = (amp/3) * (os.sawN(2,sfreq) + os.sawN(2,sfreq*detune1) + os.sawN(2,sfreq*detune2));
sqsig = (amp/3) * (os.square(sfreq) + os.square(sfreq*detune1) + os.square(sfreq*detune2));
trisig = (amp/3) * (os.triangle(sfreq) + os.triangle(sfreq*detune1) + os.triangle(sfreq*detune2));
ptsig = (amp/3) * (os.pulsetrain(sfreq,ptd) + os.pulsetrain(sfreq*detune1,ptd) + os.pulsetrain(sfreq*detune2,ptd));
pnsig = amp * no.pink_noise;

process = ssaw*sawsig + ssq*sqsig + stri*trisig + spt*ptsig + spn*pnsig + sei*_;
