#include "opencv2\opencv.hpp"
#include <iostream>
#include <fstream>
#include <cmath>

double scale[12] = { 65.41, 69.395, 73.415, 77.78, 
					 82.405, 87.305, 92.5, 98, 
					 103.825, 110, 116.52, 123.47 
};

namespace little_endian_io
{
	template <typename Word>
	std::ostream& write_word(std::ostream& outs, Word value, unsigned size = sizeof(Word))
	{
		for (; size; --size, value >>= 8)
			outs.put(static_cast <char> (value & 0xFF));
		return outs;
	}
}
using namespace little_endian_io;

struct Pixel {
	int R = 0, G = 0, B = 0;
	Pixel(int r, int g, int b) : R(r), G(g), B(b) {}
};

void ProcessPixel(Pixel pixel, double &frequency, double &amplitude, double &length) {
	amplitude = (0.299*pixel.R + 0.587*pixel.G + 0.144*pixel.B) * 50;

	int harm = pixel.B / (255 / 12);
	if (harm == 0) harm = 12;
	int octave = pixel.G / (255 / 8);
	if (octave == 0) octave = 1;

	frequency = scale[harm - 1] * octave;

	length = (double(pixel.R + 1) / 255) * 0.25;
	//std::cout << length << std::endl;
}

int main()
{
	std::ofstream f("example.wav", std::ios::binary);

	// Write the file headers
	f << "RIFF----WAVEfmt ";     // (chunk size to be filled in later)
	write_word(f, 16, 4);  // no extension data
	write_word(f, 1, 2);  // PCM - integer samples
	write_word(f, 2, 2);  // two channels (stereo file)
	write_word(f, 44100, 4);  // samples per second (Hz)
	write_word(f, 176400, 4);  // (Sample Rate * BitsPerSample * Channels) / 8
	write_word(f, 4, 2);  // data block size (size of two integer samples, one for each channel, in bytes)
	write_word(f, 16, 2);  // number of bits per sample (use a multiple of 8)

						   // Write the data chunk header
	size_t data_chunk_pos = f.tellp();
	f << "data----";  // (chunk size to be filled in later)

					  // Write the audio samples
					  // (We'll generate a single C4 note with a sine wave, fading from left to right)
	constexpr double two_pi = 6.283185307179586476925286766559;
	constexpr double hz = 44100;    // samples per second

	IplImage* img = cvLoadImage("test.png");
	for (int x = 0; x<img->height; ++x) {
		for (int y = 0; y<img->width; ++y) {
			double frequency = 0;  // middle C
			double length = 0;      // time
			double amplitude = 0;  // "volume"

			CvScalar pixelVal = cvGet2D(img, x, y);
			Pixel pixel = { (int)pixelVal.val[2], (int)pixelVal.val[1], (int)pixelVal.val[0] };
			ProcessPixel(pixel, frequency, amplitude, length);

			int N = hz * length;  // total number of samples
			for (int n = 0; n < N; n++) {
				double value = ((two_pi * n * frequency) / hz);
				write_word(f, (int)(amplitude * value), 2);
			}
		}

		std::cout << x << " out of " << img->height << std::endl;
	}
	cvReleaseImage(&img);

	// (We'll need the final file size to fix the chunk sizes above)
	size_t file_length = f.tellp();

	// Fix the data chunk header to contain the data size
	f.seekp(data_chunk_pos + 4);
	write_word(f, file_length - data_chunk_pos + 8);

	// Fix the file header to contain the proper RIFF chunk size, which is (file size - 8) bytes
	f.seekp(0 + 4);
	write_word(f, file_length - 8, 4);
}