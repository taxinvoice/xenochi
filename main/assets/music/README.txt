Music Assets for Xenochi
========================

Music files are typically too large to embed in firmware flash.
Place your MP3 or WAV files on the SD card at:

    /sdcard/assets/music/

Supported formats:
- MP3 (recommended for size efficiency)
- WAV (for simpler decoding, larger files)

Recommended encoding for efficiency:
- Sample rate: 22050 Hz or 44100 Hz
- Channels: Mono (for smaller size) or Stereo
- Bitrate: 64-128 kbps for MP3

Example FFmpeg commands:
------------------------

# Convert to mono 22kHz MP3 (small size):
ffmpeg -i input.mp3 -ac 1 -ar 22050 -ab 64k output.mp3

# Convert to stereo 44.1kHz MP3 (better quality):
ffmpeg -i input.mp3 -ac 2 -ar 44100 -ab 128k output.mp3

# Convert WAV to MP3:
ffmpeg -i input.wav -ac 1 -ar 22050 -ab 64k output.mp3
