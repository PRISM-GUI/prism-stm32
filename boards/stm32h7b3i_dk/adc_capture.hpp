#pragma once

// The ADC on Arduino A0 as PRISM's capture device: one device, "adc"/"A0", 16 kHz, mono, f32,
// default.

// Starts sampling and registers the ADC as PRISM's capture device. False when the ADC, the timer
// or the DMA refuse: nothing is registered then, and PRISM's scope app shows its own synthetic
// sweep by itself.
bool register_adc_capture();
