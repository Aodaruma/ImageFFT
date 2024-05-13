/*
========== LGPL LICENSE SECTION ================================
This file is part of script/plugin "ImageFFT" of AviUtl.

"ImageFFT" is free software: you can redistribute it and/or modify it
under the terms of the GNU Lesser General Public License as published by the Free software
Foundation, either version 3 of the License, or (at your option) any later version.

"ImageFFT" is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with this program.
If not, see <http://www.gnu.org/licenses/>.
========== LPGL LICENSE SECTION END ==============================

developed by Aodaruma
*/
#include <lua.hpp>
#include "pocketfft_hdronly.h"
#include <complex>
#include <cmath>
#include <vector>
#include <iostream>

#ifndef _DEBUG
#define DEBUG(x)
#define IS_DEBUG false
#else // print
#define DEBUG(x) cout << x << endl;
#define IS_DEBUG true
#endif

#define FFT_N_THREADS 8

using namespace pocketfft;
using namespace std;

struct Pixel_RGBA {
    unsigned char b;
    unsigned char g;
    unsigned char r;
    unsigned char a;
};

enum class FFTchannel {
	ALL = 0,
	MONOCHROME = 1,
	RED = 2,
	GREEN = 3,
	BLUE = 4,
	ALPHA = 5
} typedef FFTchannel;

enum class FFTdirection {
	FORWARD = 0,
	INVERSE = 1
} typedef FFTdirection;

enum class ExportReIm {
	EXPORT_REAL = 0,
	EXPORT_IMAGINARY = 1
} typedef ExportReIm;

// ---------------------------------------------------------

unsigned char clampDoubleToChar(double value) {
	if (value < 0)
		return 0;
	if (value > 255)
		return 255;
	return (unsigned char)value;
}

void FFT(Pixel_RGBA* pixels, int w, int h, FFTchannel channel, ExportReIm export_type, bool is_swapping_quadrants) {
	
	// Convert Pixel_RGBA to vector of doubles
	DEBUG("[ImageFFT.FFT] Converting Pixel_RGBA to vector of doubles");
	vector<vector<double>> data;

	switch (channel)
	{
	case FFTchannel::ALL:
		data.resize(4);
		for (int i = 0; i < h; i++) {
			for (int j = 0; j < w; j++) {
				data[0].push_back(pixels[i * w + j].r);
				data[1].push_back(pixels[i * w + j].g);
				data[2].push_back(pixels[i * w + j].b);
				data[3].push_back(pixels[i * w + j].a);
			}
		}
		break;

	case FFTchannel::RED:
		data.resize(1);
		for (int i = 0; i < h; i++)
			for (int j = 0; j < w; j++)
				data[0].push_back(pixels[i * w + j].r);
		break;

	case FFTchannel::GREEN:
		data.resize(1);
		for (int i = 0; i < h; i++)
			for (int j = 0; j < w; j++)
				data[0].push_back(pixels[i * w + j].g);
		break;

	case FFTchannel::BLUE:
		data.resize(1);
		for (int i = 0; i < h; i++)
			for (int j = 0; j < w; j++)
				data[0].push_back(pixels[i * w + j].b);
		break;

	case FFTchannel::ALPHA:
		data.resize(1);
		for (int i = 0; i < h; i++)
			for (int j = 0; j < w; j++)
				data[0].push_back(pixels[i * w + j].a);
		break;

	default:
	case FFTchannel::MONOCHROME:
		data.resize(1);
		for (int i = 0; i < h; i++)
			for (int j = 0; j < w; j++)
				data[0].push_back((pixels[i * w + j].r + pixels[i * w + j].g + pixels[i * w + j].b) / 3);
		break;
	}

	// Perform FFT
	DEBUG("[ImageFFT.FFT] Prepare data for FFT");
	shape_t shape{ (unsigned int)h, (unsigned int)w };
	shape_t axes;
	for (size_t i = 0; i < shape.size(); i++)
		axes.push_back(i);
	stride_t stride_in(shape.size()), stride_out(shape.size());
	size_t tmp_in = sizeof(double);
	size_t tmp_out = sizeof(complex<double>);
	for (int i = shape.size() - 1; i >= 0; i--)
	{
		stride_in[i] = tmp_in;
		tmp_in *= shape[i];
		stride_out[i] = tmp_out;
		tmp_out *= shape[i];
	}
	vector<vector<complex<double>>> result;

	DEBUG("[ImageFFT.FFT] Process FFT");
	for (unsigned int i = 0; i < data.size(); i++) {
		DEBUG("[ImageFFT.FFT] Process FFT channel " + to_string(i) + "");
		vector<complex<double>> res(w * h);
		r2c(shape, stride_in, stride_out, axes, FORWARD, data[i].data(), res.data(), 1., FFT_N_THREADS);
		result.push_back(res);
	}

	DEBUG("[ImageFFT.FFT] Done; result size: " + to_string(result.size()) + "; result[0] size: " + to_string(result[0].size()) + "; result[0][0]: " + to_string(result[0][0].real()) + "+" + to_string(result[0][0].imag()) + "i");

	// Export result
	DEBUG("[ImageFFT.FFT] Exporting result");
	vector<Pixel_RGBA> export_data;
	export_data.resize(w * h);
	for (int i = 0; i < h; i++)
		for (int j = 0; j < w; j++)
			export_data[i * w + j] = { 0, 0, 0, 255 };

	const double N = w * h;
	if (channel == FFTchannel::ALL) {
		if (export_type == ExportReIm::EXPORT_REAL) {
			// Export real part
			for (int i = 0; i < h; i++) {
				for (int j = 0; j < w; j++) {
					export_data[i * w + j].r = (unsigned char)(result[0][i * w + j].real() / N /2 +128);
					export_data[i * w + j].g = (unsigned char)(result[1][i * w + j].real() / N /2 +128);
					export_data[i * w + j].b = (unsigned char)(result[2][i * w + j].real() / N /2 +128);
					export_data[i * w + j].a = (unsigned char)(result[3][i * w + j].real() / N /2 +128);
				}
			}
		}
		else {
			// Export imaginary part
			for (int i = 0; i < h; i++) {
				for (int j = 0; j < w; j++) {
					export_data[i * w + j].r = (unsigned char)(result[0][i * w + j].imag() / N /2 +128);
					export_data[i * w + j].g = (unsigned char)(result[1][i * w + j].imag() / N /2 +128);
					export_data[i * w + j].b = (unsigned char)(result[2][i * w + j].imag() / N /2 +128);
					export_data[i * w + j].a = (unsigned char)(result[3][i * w + j].imag() / N /2 +128);
				}
			}
		}
	}
	else {
		// Export single channel (Re->R, Im->G)
		switch (channel) {
		case FFTchannel::RED:
			for (int i = 0; i < h; i++)
				for (int j = 0; j < w; j++) {
					export_data[i * w + j].r = (unsigned char)(result[0][i * w + j].real() / N /2 +128);
					export_data[i * w + j].g = (unsigned char)(result[0][i * w + j].imag() / N /2 +128);
				}
			break;

		case FFTchannel::GREEN:
			for (int i = 0; i < h; i++)
				for (int j = 0; j < w; j++) {
					export_data[i * w + j].r = (unsigned char)(result[0][i * w + j].real() / N /2 +128);
					export_data[i * w + j].g = (unsigned char)(result[0][i * w + j].imag() / N /2 +128);
				}
			break;

		case FFTchannel::BLUE:
			for (int i = 0; i < h; i++)
				for (int j = 0; j < w; j++) {
					export_data[i * w + j].r = (unsigned char)(result[0][i * w + j].real() / N /2 +128);
					export_data[i * w + j].g = (unsigned char)(result[0][i * w + j].imag() / N /2 +128);
				}
			break;

		case FFTchannel::ALPHA:
			for (int i = 0; i < h; i++)
				for (int j = 0; j < w; j++) {
					export_data[i * w + j].r = (unsigned char)(result[0][i * w + j].real() / N /2 +128);
					export_data[i * w + j].g = (unsigned char)(result[0][i * w + j].imag() / N /2 +128);
				}
			break;

		default:
		case FFTchannel::MONOCHROME:
			for (int i = 0; i < h; i++)
				for (int j = 0; j < w; j++) {
					export_data[i * w + j].r = (unsigned char)(result[0][i * w + j].real() / N /2 +128);
					export_data[i * w + j].g = (unsigned char)(result[0][i * w + j].imag() / N /2 +128);
				}
			break;
		}
	}

	// Swap quadrants
	if (is_swapping_quadrants) {
		DEBUG("[ImageFFT.FFT] Swapping quadrants");
		for (int i = 0; i < h / 2; i++) {
			for (int j = 0; j < w / 2; j++) {
				swap(export_data[i * w + j], export_data[(i + h / 2) * w + (j + w / 2)]);
				swap(export_data[(i + h / 2) * w + j], export_data[i * w + (j + w / 2)]);
			}
		}
	}

	// Copy result to original pixels
	DEBUG("[ImageFFT.FFT] Copying result to original pixels");
	for (int i = 0; i < h; i++)
		for (int j = 0; j < w; j++) 
			pixels[i * w + j] = export_data[i * w + j];
}

void IFFT(Pixel_RGBA* pixels, int w, int h, FFTchannel channel, ExportReIm export_type, bool is_swapping_quadrants) {

	// Convert Pixel_RGBA to vector of doubles
	DEBUG("[ImageFFT.IFFT] Converting Pixel_RGBA to vector of doubles");
	vector<vector<complex<double>>> data;
	const double N = w * h;

	if (channel == FFTchannel::ALL) {
		data.resize(4);
		for (int i = 0; i < h; i++) {
			for (int j = 0; j < w; j++) {
				if (export_type == ExportReIm::EXPORT_REAL){
					data[0].push_back(complex<double>((pixels[i * w + j].r - 128) * 2 * (N/2), 0));
					data[1].push_back(complex<double>((pixels[i * w + j].g - 128) * 2 * (N/2), 0));
					data[2].push_back(complex<double>((pixels[i * w + j].b - 128) * 2 * (N/2), 0));
					data[3].push_back(complex<double>((pixels[i * w + j].a - 128) * 2 * (N/2), 0));
				}else{
					data[0].push_back(complex<double>(0, (pixels[i * w + j].r - 128) * 2 * (N/2)));
					data[1].push_back(complex<double>(0, (pixels[i * w + j].g - 128) * 2 * (N/2)));
					data[2].push_back(complex<double>(0, (pixels[i * w + j].b - 128) * 2 * (N/2)));
					data[3].push_back(complex<double>(0, (pixels[i * w + j].a - 128) * 2 * (N/2)));
				}
			}
		}
	}
	else {
		data.resize(1);
		for (int i = 0; i < h; i++)
			for (int j = 0; j < w; j++)
				data[0].push_back(
					complex<double>(
						(pixels[i * w + j].r - 128) * 2 * (N / 2),
						(pixels[i * w + j].g - 128) * 2 * (N / 2)
					)
				);
	}

	// Swap quadrants
	if (is_swapping_quadrants) {
		DEBUG("[ImageFFT.IFFT] Swapping quadrants");
		for (int i = 0; i < h / 2; i++) {
			for (int j = 0; j < w / 2; j++) {
				swap(data[0][i * w + j], data[0][(i + h / 2) * w + (j + w / 2)]);
				swap(data[0][(i + h / 2) * w + j], data[0][i * w + (j + w / 2)]);
			}
		}
	}
	

	// Perform IFFT
	DEBUG("[ImageFFT.IFFT] Prepare data for IFFT");
	shape_t shape{ (unsigned int)h, (unsigned int)w };
	shape_t axes;
	for (size_t i = 0; i < shape.size(); i++)
		axes.push_back(i);
	stride_t stride_in(shape.size()), stride_out(shape.size());
	size_t tmp_in = sizeof(double);
	size_t tmp_out = sizeof(complex<double>);
	for (int i = shape.size() - 1; i >= 0; i--)
	{
		stride_in[i] = tmp_in;
		tmp_in *= shape[i];
		stride_out[i] = tmp_out;
		tmp_out *= shape[i];
	}
	vector<vector<double>> result;

	DEBUG("[ImageFFT.IFFT] Process IFFT");
	for (unsigned int i = 0; i < data.size(); i++) {
		DEBUG("[ImageFFT.IFFT] Process IFFT channel " + to_string(i) + "");
		vector<double> res(w * h);
		c2r(shape, stride_out, stride_in, axes, BACKWARD, data[i].data(), res.data(), 1./(double)(w*h), FFT_N_THREADS);
		result.push_back(res);
	}

	DEBUG("[ImageFFT.IFFT] Done; result size: " + to_string(result.size()) + "; result[0] size: " + to_string(result[0].size()) + "; result[0][0]: " + to_string(result[0][0]) + "");

	// Export result
	DEBUG("[ImageFFT.IFFT] Exporting result");
	vector<Pixel_RGBA> export_data;
	export_data.resize(w * h);
	for (int i = 0; i < h; i++)
		for (int j = 0; j < w; j++)
			export_data[i * w + j] = { 0, 0, 0, 255 };

	switch (channel) {
	case FFTchannel::ALL:
		for (int i = 0; i < h; i++) {
			for (int j = 0; j < w; j++) {
				export_data[i * w + j].r = clampDoubleToChar(result[0][i * w + j]);
				export_data[i * w + j].g = clampDoubleToChar(result[1][i * w + j]);
				export_data[i * w + j].b = clampDoubleToChar(result[2][i * w + j]);
				export_data[i * w + j].a = clampDoubleToChar(result[3][i * w + j]);
			}
		}
		break;

	case FFTchannel::RED:
		for (int i = 0; i < h; i++)
			for (int j = 0; j < w; j++)
				export_data[i * w + j].r = clampDoubleToChar(result[0][i * w + j]);
		break;

	case FFTchannel::GREEN:
		for (int i = 0; i < h; i++)
			for (int j = 0; j < w; j++)
				export_data[i * w + j].g = clampDoubleToChar(result[0][i * w + j]);
		break;

	case FFTchannel::BLUE:
		for (int i = 0; i < h; i++)
			for (int j = 0; j < w; j++)
				export_data[i * w + j].b = clampDoubleToChar(result[0][i * w + j]);
		break;

	case FFTchannel::ALPHA:
		for (int i = 0; i < h; i++)
			for (int j = 0; j < w; j++)
				export_data[i * w + j].a = clampDoubleToChar(result[0][i * w + j]);
		break;

	default:
	case FFTchannel::MONOCHROME:
		for (int i = 0; i < h; i++)
			for (int j = 0; j < w; j++)
				export_data[i * w + j].r = export_data[i * w + j].g = export_data[i * w + j].b = clampDoubleToChar(result[0][i * w + j]);
		break;
	}

	// Copy result to original pixels
	DEBUG("[ImageFFT.IFFT] Copying result to original pixels");
	for (int i = 0; i < h; i++)
		for (int j = 0; j < w; j++)
			pixels[i * w + j] = export_data[i * w + j];
}

int main(lua_State* L) {
    Pixel_RGBA* pixels = reinterpret_cast<Pixel_RGBA*>(lua_touserdata(L, 1));
    int w = static_cast<int>(lua_tointeger(L, 2));
    int h = static_cast<int>(lua_tointeger(L, 3));

	FFTdirection direction = static_cast<FFTdirection>(lua_tointeger(L, 4));
	FFTchannel channel = static_cast<FFTchannel>(lua_tointeger(L, 5));
	ExportReIm export_type = static_cast<ExportReIm>(lua_tointeger(L, 6));
	bool is_swapping_quadrants = lua_toboolean(L, 7);
	
	DEBUG("[ImageFFT.main] Start");
	if (direction == FFTdirection::FORWARD) {
		DEBUG("[ImageFFT.main] Performing FFT");
		FFT(pixels, w, h, channel, export_type, is_swapping_quadrants);
	}
	else {
		DEBUG("[ImageFFT.main] Performing IFFT");
		IFFT(pixels, w, h, channel, export_type, is_swapping_quadrants);
	}

	if (IS_DEBUG) {
		// analyze pixels: max, min, avg, mid
		Pixel_RGBA max = { 0, 0, 0, 0 };
		Pixel_RGBA min = { 255, 255, 255, 255 };
		double avg = 0;
		Pixel_RGBA mid = { 0, 0, 0, 0 };

		for (int i = 0; i < h; i++) {
			for (int j = 0; j < w; j++) {
				avg += pixels[i * w + j].r;
				avg += pixels[i * w + j].g;
				avg += pixels[i * w + j].b;
				avg += pixels[i * w + j].a;

				if (pixels[i * w + j].r > max.r) max.r = pixels[i * w + j].r;
				if (pixels[i * w + j].g > max.g) max.g = pixels[i * w + j].g;
				if (pixels[i * w + j].b > max.b) max.b = pixels[i * w + j].b;
				if (pixels[i * w + j].a > max.a) max.a = pixels[i * w + j].a;

				if (pixels[i * w + j].r < min.r) min.r = pixels[i * w + j].r;
				if (pixels[i * w + j].g < min.g) min.g = pixels[i * w + j].g;
				if (pixels[i * w + j].b < min.b) min.b = pixels[i * w + j].b;
				if (pixels[i * w + j].a < min.a) min.a = pixels[i * w + j].a;
			}
		}

		avg /= w * h * 4;

		// find mid
		vector<Pixel_RGBA> sorted;
		sorted.resize(w * h);
		for (int i = 0; i < h; i++)
			for (int j = 0; j < w; j++)
				sorted[i * w + j] = pixels[i * w + j];

		sort(sorted.begin(), sorted.end(), [](Pixel_RGBA a, Pixel_RGBA b) {
			return a.r + a.g + a.b + a.a < b.r + b.g + b.b + b.a;
			});

		mid = sorted[w * h / 2];

		DEBUG(
			"[ImageFFT.main] Analyze: max: " + to_string(max.r) + ", " + to_string(max.g) + ", " + to_string(max.b) + ", " + to_string(max.a)
			+ "; min: " + to_string(min.r) + ", " + to_string(min.g) + ", " + to_string(min.b) + ", " + to_string(min.a) 
			+ "; avg: " + to_string(avg) 
			+ "; mid: " + to_string(mid.r) + ", " + to_string(mid.g) + ", " + to_string(mid.b) + ", " + to_string(mid.a)
		);
	}

	DEBUG("[ImageFFT.main] Done");

    return 0;
}

static luaL_Reg functions[] = {
    { "main", main},
    { nullptr, nullptr }
};

extern "C" {
    __declspec(dllexport) int luaopen_ImageFFT(lua_State* L) {
        luaL_register(L, "ImageFFT", functions);
        return 1;
    }
}