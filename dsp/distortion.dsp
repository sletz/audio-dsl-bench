import("stdfaust.lib");
// dm.cubicnl_demo (examples/dynamic/distortion.dsp): ef.cubicnl_nodc with si.smoo on drive and
// offset, same sliders and defaults, without the bypass checkbox.
drive = hslider("drive", 0, 0, 1, 0.01);
offset = hslider("offset", 0, 0, 1, 0.01);
process = ef.cubicnl_nodc(drive : si.smoo, offset : si.smoo);
