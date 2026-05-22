#ifndef USB_MIDI_DEVICE_H
#define USB_MIDI_DEVICE_H

#ifdef __cplusplus
extern "C" {
#endif

// Bring up TinyUSB in MIDI device mode. The board enumerates as "Chordex"
// to whatever host it's plugged into (Mac/PC/iPad). MIDI bytes arriving
// from the host are decoded and routed to chord_detect via MIDI_SOURCE_USB.
void usb_midi_device_init(void);

#ifdef __cplusplus
}
#endif

#endif
