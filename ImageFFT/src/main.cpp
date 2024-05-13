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

enum class ExportCurveType {
	LINEAR = 0,
	LOG2 = 1,
	MINKHOWSKI = 2
} typedef ExportCurveType;

const double UcharConvConst = 255. / 8.;
const double InvUcharConvConst = 8. / 255.;

double sign(double t){
    return signbit(t) ? -1 : 1;
}

// ---------------------------------------------------------

unsigned char convDoubleToUchar(double value, ExportCurveType curve_type, double p = 2) {

	switch (curve_type) {

		case ExportCurveType::LOG2:
			// 255/8 * log2(x+1)
			value = sign(value) * log2(abs(value) + 1) * UcharConvConst;
			break;

		case ExportCurveType::MINKHOWSKI:
			value = sign(value) * (-pow(-pow(abs(value), p) + pow(255, p), 1./p) + 255);
			break;

		default:
		case ExportCurveType::LINEAR:
			break;
	}
	value /= 2;
	value += 128;

	if (value < 0)
		return 0;
	if (value > 255)
		return 255;
	return (unsigned char)value;
}

double convUcharToDouble(unsigned char value, ExportCurveType curve_type, double p = 2) {
	double result = (double)value;
	result -= 128;
	result *= 2;

	switch (curve_type) {
		case ExportCurveType::LOG2:
			// 2^(8/255 * x) - 1
			result = sign(result) * (pow(2, InvUcharConvConst * abs(result)) - 1);
			break;

		case ExportCurveType::MINKHOWSKI:
			result = sign(result) * pow(pow(-abs(result) + 255, p) - pow(255, p), 1./p);
			break;

		default:
		case ExportCurveType::LINEAR:
			break;
	}

	return result;
}

unsigned char convIFFTresult(double value, ExportCurveType curve_type) {
	value /= 2;

	switch (curve_type) {
		case ExportCurveType::LOG2:
			// 2^(8/255 * x) - 1
			value = sign(value) * (pow(2, InvUcharConvConst * abs(value)) - 1);
			break;

		/*case ExportCurveType::MINKHOWSKI:
			value = sign(value) * pow(pow(-abs(value) + 255, p) - pow(255, p), 1. / p);
			break;*/

		default:
		case ExportCurveType::LINEAR:
			break;
	}
	return (unsigned char)value;
}


// ---------------------------------------------------------

void analyzeData(vector<vector<double>> data) {
	double max = 0;
	double min = 255;
	double avg = 0;
	double mid = 0; 
	double std = 0;

	for (size_t i = 0; i < data.size(); i++) {
		for (size_t j = 0; j < data[i].size(); j++) {
			avg += data[i][j];
			if (data[i][j] > max) max = data[i][j];
			if (data[i][j] < min) min = data[i][j];
		}
	}

	avg /= data.size() * data[0].size();

	// find mid
	vector<double> sorted;
	sorted.resize(data.size() * data[0].size());
	for (size_t i = 0; i < data.size(); i++)
		for (size_t j = 0; j < data[i].size(); j++)
			sorted[i * data[i].size() + j] = data[i][j];

	sort(sorted.begin(), sorted.end());

	mid = sorted[data.size() * data[0].size() / 2];

	// find std
	for (size_t i = 0; i < data.size(); i++) {
		for (size_t j = 0; j < data[i].size(); j++) {
			std += pow(data[i][j] - avg, 2);
		}
	}
	std = sqrt(std / (data.size() * data[0].size()));

	DEBUG(
		"[ImageFFT.analyzeData] Analyze: max: " + to_string(max)
		+ "; min: " + to_string(min)
		+ "; avg: " + to_string(avg)
		+ "; mid: " + to_string(mid)
		+ "; std: " + to_string(std)
	);
}

void analyzeData(vector<vector<complex<double>>> data) {
	DEBUG("[ImageFFT.analyzeData] Analyze complex data");
	
	complex<double> max = 0;
	complex<double> min = 255;
	complex<double> avg = 0;
	complex<double> mid = 0;
	complex<double> std = 0;

	for (size_t i = 0; i < data.size(); i++) {
		for (size_t j = 0; j < data[i].size(); j++) {
			avg += data[i][j];
			if (abs(data[i][j]) > abs(max)) max = data[i][j];
			if (abs(data[i][j]) < abs(min)) min = data[i][j];
		}
	}

	avg /= data.size() * data[0].size();

	// find mid
	vector<complex<double>> sorted;
	sorted.resize(data.size() * data[0].size());
	for (size_t i = 0; i < data.size(); i++)
		for (size_t j = 0; j < data[i].size(); j++)
			sorted[i * data[i].size() + j] = data[i][j];

	sort(sorted.begin(), sorted.end(), [](complex<double> a, complex<double> b) {
		return abs(a) < abs(b);
		});

	mid = sorted[data.size() * data[0].size() / 2];

	// find std
	for (size_t i = 0; i < data.size(); i++) {
		for (size_t j = 0; j < data[i].size(); j++) {
			std += pow(abs(data[i][j]) - abs(avg), 2);
		}
	}
	std = complex<double> (sqrt(std.real() / (data.size() * data[0].size())), sqrt(std.imag() / (data.size() * data[0].size())));

	DEBUG(
		"[ImageFFT.analyzeData] Analyze: max: " + to_string(max.real()) + "+" + to_string(max.imag()) + "i"
		+ "; min: " + to_string(min.real()) + "+" + to_string(min.imag()) + "i"
		+ "; avg: " + to_string(avg.real()) + "+" + to_string(avg.imag()) + "i"
		+ "; mid: " + to_string(mid.real()) + "+" + to_string(mid.imag()) + "i"
		+ "; std: " + to_string(std.real()) + "+" + to_string(std.imag()) + "i"
	);
}

void FFT(Pixel_RGBA* pixels, int w, int h, FFTchannel channel, ExportReIm export_type, bool is_swapping_quadrants, ExportCurveType export_curve_type, double curve_p = 2) {
	
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
	if (IS_DEBUG) analyzeData(result);

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
					export_data[i * w + j].r = convDoubleToUchar(result[0][i * w + j].real() / (N/2), export_curve_type, curve_p);
					export_data[i * w + j].g = convDoubleToUchar(result[1][i * w + j].real() / (N / 2), export_curve_type, curve_p);
					export_data[i * w + j].b = convDoubleToUchar(result[2][i * w + j].real() / (N / 2), export_curve_type, curve_p);
					export_data[i * w + j].a = convDoubleToUchar(result[3][i * w + j].real() / (N / 2), export_curve_type, curve_p);
				}
			}
		}
		else {
			// Export imaginary part
			for (int i = 0; i < h; i++) {
				for (int j = 0; j < w; j++) {
					export_data[i * w + j].r = convDoubleToUchar(result[0][i * w + j].imag() / (N / 2), export_curve_type, curve_p);
					export_data[i * w + j].g = convDoubleToUchar(result[1][i * w + j].imag() / (N / 2), export_curve_type, curve_p);
					export_data[i * w + j].b = convDoubleToUchar(result[2][i * w + j].imag() / (N / 2), export_curve_type, curve_p);
					export_data[i * w + j].a = convDoubleToUchar(result[3][i * w + j].imag() / (N / 2), export_curve_type, curve_p);
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
					export_data[i * w + j].r = convDoubleToUchar(result[0][i * w + j].real() / (N / 2), export_curve_type, curve_p);
					export_data[i * w + j].g = convDoubleToUchar(result[0][i * w + j].imag() / (N / 2), export_curve_type, curve_p);
				}
			break;

		case FFTchannel::GREEN:
			for (int i = 0; i < h; i++)
				for (int j = 0; j < w; j++) {
					export_data[i * w + j].r = convDoubleToUchar(result[0][i * w + j].real() / (N / 2), export_curve_type, curve_p);
					export_data[i * w + j].g = convDoubleToUchar(result[0][i * w + j].imag() / (N / 2), export_curve_type, curve_p);
				}
			break;

		case FFTchannel::BLUE:
			for (int i = 0; i < h; i++)
				for (int j = 0; j < w; j++) {
					export_data[i * w + j].r = convDoubleToUchar(result[0][i * w + j].real() / (N / 2), export_curve_type, curve_p);
					export_data[i * w + j].g = convDoubleToUchar(result[0][i * w + j].imag() / (N / 2), export_curve_type, curve_p);
				}
			break;

		case FFTchannel::ALPHA:
			for (int i = 0; i < h; i++)
				for (int j = 0; j < w; j++) {
					export_data[i * w + j].r = convDoubleToUchar(result[0][i * w + j].real() / (N / 2), export_curve_type, curve_p);
					export_data[i * w + j].g = convDoubleToUchar(result[0][i * w + j].imag() / (N / 2), export_curve_type, curve_p);
				}
			break;

		default:
		case FFTchannel::MONOCHROME:
			for (int i = 0; i < h; i++)
				for (int j = 0; j < w; j++) {
					export_data[i * w + j].r = convDoubleToUchar(result[0][i * w + j].real() / (N / 2), export_curve_type, curve_p);
					export_data[i * w + j].g = convDoubleToUchar(result[0][i * w + j].imag() / (N / 2), export_curve_type, curve_p);
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

void IFFT(Pixel_RGBA* pixels, int w, int h, FFTchannel channel, ExportReIm export_type, bool is_swapping_quadrants, ExportCurveType import_curve_type, double curve_p = 2, ExportCurveType output_curve_type = ExportCurveType::LINEAR) {

	// Convert Pixel_RGBA to vector of doubles
	DEBUG("[ImageFFT.IFFT] Converting Pixel_RGBA to vector of doubles");
	vector<vector<complex<double>>> data;
	const double N = w * h;

	if (channel == FFTchannel::ALL) {
		data.resize(4);
		for (int i = 0; i < h; i++) {
			for (int j = 0; j < w; j++) {
				Pixel_RGBA p = pixels[i * w + j];
				if (export_type == ExportReIm::EXPORT_REAL) {
					data[0].push_back(complex<double>(convUcharToDouble(p.r, import_curve_type, curve_p) * (N / 2), 0));
					data[1].push_back(complex<double>(convUcharToDouble(p.g, import_curve_type, curve_p) * (N / 2), 0));
					data[2].push_back(complex<double>(convUcharToDouble(p.b, import_curve_type, curve_p) * (N / 2), 0));
					data[3].push_back(complex<double>(convUcharToDouble(p.a, import_curve_type, curve_p) * (N / 2), 0));
				}
				else {
					data[0].push_back(complex<double>(0, convUcharToDouble(p.r, import_curve_type, curve_p) * (N / 2)));
					data[1].push_back(complex<double>(0, convUcharToDouble(p.g, import_curve_type, curve_p) * (N / 2)));
					data[2].push_back(complex<double>(0, convUcharToDouble(p.b, import_curve_type, curve_p) * (N / 2)));
					data[3].push_back(complex<double>(0, convUcharToDouble(p.a, import_curve_type, curve_p) * (N / 2)));
				}
			}
		}
	}
	else {
		data.resize(1);
		for (int i = 0; i < h; i++)
			for (int j = 0; j < w; j++) {
				Pixel_RGBA p = pixels[i * w + j];
				data[0].push_back(
					complex<double>(
						convUcharToDouble(p.r, import_curve_type, curve_p) * (N / 2),
						convUcharToDouble(p.g, import_curve_type, curve_p) * (N / 2)
					)
				);
			}
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
		c2r(shape, stride_out, stride_in, axes, BACKWARD, data[i].data(), res.data(), 1. / (double)(w * h), FFT_N_THREADS);
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
				export_data[i * w + j].r = convIFFTresult(result[0][i * w + j], output_curve_type);
				export_data[i * w + j].g = convIFFTresult(result[1][i * w + j], output_curve_type);
				export_data[i * w + j].b = convIFFTresult(result[2][i * w + j], output_curve_type);
				export_data[i * w + j].a = convIFFTresult(result[3][i * w + j], output_curve_type);
			}
		}
		break;

	case FFTchannel::RED:
		for (int i = 0; i < h; i++)
			for (int j = 0; j < w; j++)
				export_data[i * w + j].r = convIFFTresult(result[0][i * w + j], output_curve_type);
		break;

	case FFTchannel::GREEN:
		for (int i = 0; i < h; i++)
			for (int j = 0; j < w; j++)
				export_data[i * w + j].g = convIFFTresult(result[0][i * w + j], output_curve_type);
		break;

	case FFTchannel::BLUE:
		for (int i = 0; i < h; i++)
			for (int j = 0; j < w; j++)
				export_data[i * w + j].b = convIFFTresult(result[0][i * w + j], output_curve_type);
		break;

	case FFTchannel::ALPHA:
		for (int i = 0; i < h; i++)
			for (int j = 0; j < w; j++)
				export_data[i * w + j].a = convIFFTresult(result[0][i * w + j], output_curve_type);
		break;

	default:
	case FFTchannel::MONOCHROME:
		for (int i = 0; i < h; i++)
			for (int j = 0; j < w; j++)
				export_data[i * w + j].r = export_data[i * w + j].g = export_data[i * w + j].b = convIFFTresult(result[0][i * w + j], output_curve_type);
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
	ExportCurveType export_curve_type = static_cast<ExportCurveType>(lua_tointeger(L, 8));
	double curve_p = static_cast<double>(lua_tonumber(L, 9));

	DEBUG("[ImageFFT.main] Start");
	if (direction == FFTdirection::FORWARD) {
		DEBUG("[ImageFFT.main] Performing FFT");
		FFT(pixels, w, h, channel, export_type, is_swapping_quadrants, export_curve_type, curve_p);
	}
	else {
		DEBUG("[ImageFFT.main] Performing IFFT");
		IFFT(pixels, w, h, channel, export_type, is_swapping_quadrants, export_curve_type, curve_p);
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