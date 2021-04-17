#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
#include "escapi.h"
#include <iostream>
#include <stdio.h>
#include <string>

int nFrameWidth = 320;
int nFrameHeight = 240;

bool bCamInput = false;
int cam_input;
//char sCamNum;
//int iCamNum;

struct frame {
	float *pixels = nullptr;

    frame()
    {

        pixels = new float[nFrameWidth * nFrameHeight];
    }

    ~frame()
    {
        delete[] pixels;
    }

	float get(int x, int y)
	{
		if (x >= 0 && x < nFrameWidth && y >= 0 && y < nFrameHeight)
		{
			return pixels[y * nFrameWidth + x];
		}
		else
			return 0.0f;
	}

	void set(int x, int y, float p)
	{
		if (x >= 0 && x < nFrameWidth && y >= 0 && y < nFrameHeight)
		{
			pixels[y * nFrameWidth + x] = p;
		}
	}

	void operator=(const frame& f)
	{
		memcpy(this->pixels, f.pixels, nFrameWidth * nFrameHeight * sizeof(float));
	}
};

class WebCamCapture : public olc::PixelGameEngine
{
public:
	WebCamCapture()
	{
		sAppName = "WebCamCapture";
	}

	union RGBint
	{
		int rgb;
		unsigned char c[4];
	};

	int nCameras = 0;
	SimpleCapParams capture;

	

public:
	bool OnUserCreate() override
	{
		
		//escapi initialization
		nCameras = setupESCAPI();
		if (nCameras == 0) {
			return false;
		}

		capture.mWidth = nFrameWidth;
		capture.mHeight = nFrameHeight;
		capture.mTargetBuf = new int[nFrameWidth * nFrameHeight];
		if (initCapture(0, &capture) == 0) return false;

		return true;
	}

	//prev_input needed for motion detection. activity for dilation and destruction
	frame input, output, prev_input, activity, threshold; 

public:
	void DrawFrame(frame &f, int x, int y)
	{
		for (int i = 0; i < nFrameWidth; i++) {
			for (int j = 0; j < nFrameHeight; j++)
			{
				int c = (int) (std::min(std::max(0.0f, f.pixels[j * nFrameWidth + i] * 255.0f), 255.0f));
				Draw(x + i, y + j, olc::Pixel(c, c, c));
			}
		}
	}

	//INTEGER DEFINITIONS ////////////////////////////////////////////////////////////////////////////

	//for threshold control
	float threshVal = 0.5;
	int first_key = (int)olc::Key::K1;
	//

	//for mode control
	int stage = 0;
	
	//for low pass filtering control
	float dLCoef = 0.05;
	int zero_key = (int)olc::Key::K0;
	bool bPrimer = false;

	//DILATION AND DESTRUCTION VALUES
	int dilationStrength = 1;
	int erosionStrength = 1;
	//Dilation Erosion int
	int Stage3_Sub_Stage = 0;
	bool bDilation; //check if Stage3_Sub_Stage == 4
	float sum;
	float pixCheck;
	float ratio = 25.0f;

	int convLoops = 1;
	int convDemo = 0;



	////////////////////////////////////////////////////////////////////////////////////////////////////

	bool OnUserUpdate(float fElapsedTime) override
	{
		while (!bCamInput) {
			DrawString(10, 10, "Detected " + std::to_string(nCameras) + " camera/s.", olc::YELLOW);
			DrawString(10, 20, "Please press 0-" + std::to_string(nCameras - 1) + " to pick a camera", olc::YELLOW);

			for (int i = 0; i < 10; ++i) {
				if (GetKey((olc::Key)(zero_key + i)).bPressed && i >= 0 && i < nCameras)
				{
					cam_input = i;
					bCamInput = !bCamInput;
				}
			}
			return true;
		}

		prev_input = input;

		//WEBCAM CAPTURE /////////////////////////////////////////////////////////
		doCapture(cam_input);
		while (isCaptureDone(cam_input) == 0) {}
		for (int y = 0; y < capture.mHeight; y++)
			for (int x = 0; x < capture.mWidth; x++)
			{
				RGBint col;
				int id = y * capture.mWidth + x;
				col.rgb = capture.mTargetBuf[id];

				input.pixels[y * nFrameWidth + x] = col.c[1] / 255.0f;
			}

		//IMAGE PROCESSING HERE ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		//THRESHOLD /////////////////////////////////////////////////////////////////////////////////////
		//THRESHOLD CONTROLS 
		if (stage == 0) {

			for (int i = 0; i < 10; ++i) {
				if (GetKey((olc::Key)(first_key + i)).bPressed)
				{
					threshVal = (i + 1.0f) / 10.0f;
				}
			}
			//THRESHOLD IMPLEMENTATION 
			for (int i = 0; i < nFrameWidth; i++) {
				for (int j = 0; j < nFrameHeight; j++) {
					output.set(i, j, input.get(i, j) > threshVal ? 1.0f : 0.0f);
				}
			}
		}
		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		//MOTION DETECTION /////////////////////////////////////////////////////////////////////////////////////
		else if (stage == 1) {

			for (int i = 0; i < nFrameWidth; i++)
			{
				for (int j = 0; j < nFrameHeight; j++)
				{
					output.set(i, j, fabs(input.get(i, j) - prev_input.get(i, j)));
				}
			}
		}
		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////


		// LOW PASS FILTERING 		/////////////////////////////////////////////////////////////////////////////////////
		else if (stage == 2) {
			if (!bPrimer) {
				output = input;
				bPrimer = true;
			}

			//0   0.05 0.10 0.15 0.20 0.25 0.30 0.35 0.40 0.45
			//0   1	   2	3	 4	  5	   6	7	 8    9

			for (int i = 0; i < 10; ++i) {
				if (GetKey((olc::Key)(zero_key + i)).bPressed)
				{
					dLCoef = i / 100.0f + i * 0.04f;
				}
			}

			for (int i = 0; i < nFrameWidth; i++)
			{
				for (int j = 0; j < nFrameHeight; j++)
				{
					float dL = input.get(i, j) - output.get(i, j);
					dL *= dLCoef;
					output.set(i, j, dL + output.get(i, j));
				}
			}
		}
		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		//DILATION AND DESTRUCTION //////////////////////////////////////////////////////////////////////////////////////
		//HAS TO IMPLEMENT THRESHOLD FIRST
		else if (stage == 3)
		{
			if ((GetKey)(olc::Key::UP).bPressed)
			{
				threshVal += 0.1;
			}
			else if ((GetKey)(olc::Key::DOWN).bPressed)
			{
				threshVal -= 0.1;
			}

			if (threshVal > 1)
				threshVal = 1;
			if (threshVal < 0)
				threshVal = 0;

			//THRESHOLD IMPLEMENTATION
			for (int i = 0; i < nFrameWidth; i++) {
				for (int j = 0; j < nFrameHeight; j++) {
					activity.set(i, j, input.get(i, j) > threshVal ? 1.0f : 0.0f);
					threshold.set(i, j, input.get(i, j) > threshVal ? 1.0f : 0.0f);
				}
			}


			//DILATION ////////////////////////////////////////////////////////////////////////////////////////////////////////
			if (Stage3_Sub_Stage == 0 || Stage3_Sub_Stage == 2 || Stage3_Sub_Stage == 4 || Stage3_Sub_Stage == 5) {
				bDilation = (Stage3_Sub_Stage == 4) ? 1 : 0;

				for (int i = 0; i < 10; ++i) {
					if (GetKey((olc::Key)(first_key + i)).bPressed)
					{
						dilationStrength = i + 1;
					}
				}

				for (int n = 0; n < dilationStrength; n++)
				{
					output = activity;

					for (int i = 0; i < nFrameWidth; i++)
					{
						for (int j = 0; j < nFrameHeight; j++)
						{
							if (activity.get(i, j) == 1.0f)
							{
								output.set(i, j, 1.0f);
								output.set(i - 1, j + bDilation, 1.0f);
								output.set(i + 1, j + bDilation, 1.0f);
								output.set(i + bDilation, j - 1, 1.0f);
								output.set(i - bDilation, j + 1, 1.0f);
							}
						}
					}

					activity = output;
				}
			}
			//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			//EROSION ////////////////////////////////////////////////////////////////////////////////////////////////////////////
			if (Stage3_Sub_Stage == 1 || Stage3_Sub_Stage == 2 || Stage3_Sub_Stage == 3 || Stage3_Sub_Stage == 4 || Stage3_Sub_Stage == 5) {
				for (int i = 0; i < 10; ++i) {
					if (GetKey((olc::Key)(first_key + i)).bPressed)
					{
						erosionStrength = i + 1;
					}
				}

				for (int n = 0; n < erosionStrength; n++)
				{
					output = activity;

					for (int i = 0; i < nFrameWidth; i++)
					{
						for (int j = 0; j < nFrameHeight; j++)
						{
							//Sums the value of all 8 pixels around it
							if (Stage3_Sub_Stage == 5) {
								pixCheck = 8.0;
								sum = activity.get(i - 1, j) + activity.get(i + 1, j) + activity.get(i, j - 1) + activity.get(i, j + 1) +
									activity.get(i - 1, j - 1) + activity.get(i + 1, j + 1) + activity.get(i + 1, j - 1) + activity.get(i - 1, j + 1);

							}
							//Sums only the left right up down pixels
							else {
								pixCheck = 4.0;
								sum = activity.get(i - 1, j) + activity.get(i + 1, j) + activity.get(i, j - 1) + activity.get(i, j + 1);
							}
							// Erosion
							if (activity.get(i, j) == 1.0f && sum < pixCheck && (Stage3_Sub_Stage == 1 || Stage3_Sub_Stage == 2 || Stage3_Sub_Stage == 4 || Stage3_Sub_Stage == 5))
							{
								output.set(i, j, 0.0f);
							}
							// Else if digital edge detect
							else if (activity.get(i, j) == 1.0f && sum == pixCheck && Stage3_Sub_Stage == 3)
							{
								output.set(i, j, 0.0f);
							}
						}
					}

					activity = output;
				}
			}
		}
		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


		// MEDIAN FILTER //////////////////////////////////////////////////////////////////////////////////////////////////////////
		if (stage == 4)
		{
			for (int i = 0; i < nFrameWidth; i++)
			{
				for (int j = 0; j < nFrameHeight; j++) {

					std::vector<float> v;

					for (int n = -2; n < 3; n++)
					{
						for (int m = -2; m < 3; m++) {

							v.push_back(input.get(i + n, j + m));

						}
					}

					std::sort(v.begin(), v.end(), std::greater<float>());
					output.set(i, j, v[4]);
				}
			}
		}
		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


		//LOCAL ADAPTIVE THRESHOLDING  TOGETHER WITH EROSION AND DIALATION ///////////////////////////////////////////
		if (stage == 5)
		{
			if ((GetKey)(olc::Key::UP).bPressed)
			{
				ratio += 0.25;
			}
			else if ((GetKey)(olc::Key::DOWN).bPressed)
			{
				ratio -= 0.25;
			}

			for (int i = 0; i < nFrameWidth; i++)
			{
				for (int j = 0; j < nFrameHeight; j++) {

					float fRegionSum = 0.0f;

					for (int n = -2; n < 3; n++)
					{
						for (int m = -2; m < 3; m++) {

							fRegionSum += input.get(i + n, j + m);
						}
					}

					fRegionSum /= ratio;
					activity.set(i, j, input.get(i, j) > fRegionSum ? 1.0f : 0.0f);

				}
			}


			for (int n = 0; n < dilationStrength; n++)
			{
				output = activity;

				for (int i = 0; i < nFrameWidth; i++)
				{
					for (int j = 0; j < nFrameHeight; j++)
					{
						if (activity.get(i, j) == 1.0f)
						{
							output.set(i, j, 1.0f);
							output.set(i - 1, j + bDilation, 1.0f);
							output.set(i + 1, j + bDilation, 1.0f);
							output.set(i + bDilation, j - 1, 1.0f);
							output.set(i - bDilation, j + 1, 1.0f);
						}
					}
				}

				activity = output;
			}


			// EROSION //

			for (int n = 0; n < erosionStrength; n++)
			{
				output = activity;

				for (int i = 0; i < nFrameWidth; i++)
				{
					for (int j = 0; j < nFrameHeight; j++)
					{
						//Sums the value of all 8 pixels around it
						pixCheck = 8.0;
						sum = activity.get(i - 1, j) + activity.get(i + 1, j) + activity.get(i, j - 1) + activity.get(i, j + 1) +
							activity.get(i - 1, j - 1) + activity.get(i + 1, j + 1) + activity.get(i + 1, j - 1) + activity.get(i - 1, j + 1);

						// Erosion
						if (activity.get(i, j) == 1.0f && sum < pixCheck && (Stage3_Sub_Stage == 1 || Stage3_Sub_Stage == 2 || Stage3_Sub_Stage == 4 || Stage3_Sub_Stage == 5))
						{
							output.set(i, j, 0.0f);
						}
					}
				}
				activity = output;
			}
		}
		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		// CONVOLUTION / BLUR //////////////////////////////////////////////////////////////////////////////////////////////////////////
		
		//Has to add up to one //
		float blurKernel[] =
		{
			0.0f, 0.125f, 0.0f,
			0.125f, 0.5f, 0.125f,
			0.0f, 0.125f, 0.0f
		};
		float sharpKernel[] =
		{
			0.0f, -1.0f, 0.0f,
			-1.0f, 5.0f, -1.0f,
			0.0f, -1.0f, 0.0f
		};
		float sobelKernel_h[] =
		{
			-1.0f, 0.0f, +1.0f,
			-2.0f, 0.0f, +2.0f,
			-1.0f, 0.0f, +1.0f
		};
		float sobelKernel_v[] =
		{
			-1.0f, -2.0f, -1.0f,
			 0.0f,  0.0f,  0.0f,
			+1.0f, +2.0f, +1.0f
		};

		if (stage == 6)
		{
			output = input;

			if ((GetKey)(olc::Key::UP).bPressed) 
			{
				convDemo += 1;
			}
			else if ((GetKey)(olc::Key::DOWN).bPressed)
			{
				convDemo -= 1;
			}

			if (convDemo == -1)
				convDemo = 2;
			if (convDemo == 3)
				convDemo = 0;

			if(convDemo == 0){
				for (int i = 0; i < 10; ++i) {
					if (GetKey((olc::Key)(zero_key + i)).bPressed)
					{
						convLoops = i * 3;
					}
				}
			}
			else
			{
				for (int i = 0; i < 10; ++i) {
					if (GetKey((olc::Key)(zero_key + i)).bPressed)
					{
						convLoops = i;
					}
				}
			}


			for (int a = 0; a < convLoops; a++) 
			{
				activity = output;
				for (int i = 0; i < nFrameWidth; i++)
				{
					for (int j = 0; j < nFrameHeight; j++) {

						float fKernelSum = 0.0f;
						float fKernelSumH = 0.0f;
						float fKernelSumV = 0.0f;

						for (int n = -1; n < 2; n++)
						{
							for (int m = -1; m < 2; m++) {
								if (convDemo == 0)
									fKernelSum += activity.get(i + n, j + m) * blurKernel[(m + 1) * 3 + (n + 1)];
								else if (convDemo == 1)
									fKernelSum += activity.get(i + n, j + m) * sharpKernel[(m + 1) * 3 + (n + 1)];
								else if (convDemo == 2)
								{
									fKernelSumH += activity.get(i + n, j + m) * sobelKernel_h[(m + 1) * 3 + (n + 1)];
									fKernelSumV += activity.get(i + n, j + m) * sobelKernel_v[(m + 1) * 3 + (n + 1)];
									fKernelSum = fabs(fKernelSumH + fKernelSumV) / 2.0;
								}

							}
						}
						output.set(i, j, fKernelSum);
					}
				}
			}
		}
		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////







		//Luminance increase
		/*for (int i = 0; i < nFrameWidth; i++)
			for (int j = 0; j < nFrameHeight; j++)
			{
				input.set(i, j, input.get(i, j) * 2.0f);
				output.set(i, j, output.get(i, j) * 2.0f);
			}*/


		//FOR LOW PASS FILTERING, HAS TO BE AFTER ALL THE IMAGE PROCESSING TASKS //////////////////////////////////////////////////
		if (stage != 2)
		{
			//for LP filtering
			bPrimer = false;
		}

		//////////////////////////////////////////////////////////////

		//do all pre processing stuff before this line
		Clear(olc::VERY_DARK_GREY);
		if (stage == 3)
			DrawFrame(threshold, 10, 10);
		else
			DrawFrame(input, 10, 10);

		DrawFrame(output, 340, 10);

		DrawString(0, 260, "z: threshold, x: motion detect, c: low pass filter, v: dilation and destruction");
		DrawString(0, 270, "b: median filter, n: sharpening, m: convolution");
		DrawString(0, 40, "ESC exits the program", olc::YELLOW);

		//STAGE SPECIFIC USER TEXT ////////////////////////////////////////////////////////////////////
		if (stage == 0) {
			DrawString(0, 0, "Keys 1-9 (not numpad) change threshold", olc::RED);
			DrawString(0, 10, "Threshold value: " + std::to_string(threshVal), olc::RED);
		}
		if (stage == 1) {
			DrawString(0, 0, "Basic motion detect", olc::RED);
		}
		if (stage == 2) {
			DrawString(0, 0, "Keys 0-9 (not numpad) changes difference in Luminance", olc::RED);
			DrawString(0, 10, "dLCoef: " + std::to_string(dLCoef), olc::RED);
		}

		if (stage == 3)
		{
			if ((GetKey)(olc::Key::SPACE).bPressed) {
				Stage3_Sub_Stage += 1;
				if (Stage3_Sub_Stage == 6)
					Stage3_Sub_Stage = 0;
			}
			DrawString(0, 30, "Threshold value: " + std::to_string(threshVal), olc::RED);
			DrawString(0, 280, "Space to switch between dilation/erosion/both/digital edge detect,", olc::RED);
			DrawString(0, 290, "4 way dialation and 8 way dialation (Threshold on the left");
		}
		if (stage == 3 && Stage3_Sub_Stage == 0)
		{
			DrawString(0, 0, "DILATION  Keys 0-9 (not numpad) change dilation loops", olc::RED);
			DrawString(0, 10, "Dilation loops: " + std::to_string(dilationStrength), olc::RED);
		}
		else if (stage == 3 && Stage3_Sub_Stage == 1)
		{
			DrawString(0, 0, "EROSION  Keys 0-9 (not numpad) change erosion loops", olc::RED);
			DrawString(0, 10, "Erosion loops: " + std::to_string(erosionStrength), olc::RED);
		}
		else if (stage == 3 && Stage3_Sub_Stage == 2)
		{
			DrawString(0, 0, "DILATION + EROSION 1  Keys 0-9 (not numpad) change erosion/dilation loops", olc::RED);
			DrawString(0, 10, "Dilation and erosion loops: " + std::to_string(erosionStrength), olc::RED);
		}
		else if (stage == 3 && Stage3_Sub_Stage == 3)
		{
			DrawString(0, 0, "DIGITAL EDGE DETECT  Keys 0-9 (not numpad) change erosion (does nothing)", olc::RED);
			DrawString(0, 10, "Erosion loops: " + std::to_string(erosionStrength), olc::RED);
		}
		else if (stage == 3 && Stage3_Sub_Stage == 4)
		{
			DrawString(0, 0, "DILATION+EROSION 2 Keys 0-9 (not numpad) change dilation loops", olc::RED);
			DrawString(0, 20, "Each pixel checks its left/right/up/down neighbor and compares itself to it", olc::RED);
			DrawString(0, 10, "Dilation and erosion loops: " + std::to_string(dilationStrength), olc::RED);
		}
		else if (stage == 3 && Stage3_Sub_Stage == 5)
		{
			DrawString(0, 0, "DILATION+EROSION 2  Keys 0-9 (not numpad) change dilation loops", olc::RED);
			DrawString(0, 20, "Each pixel checks its X/Y and diagonal neighbor and compares itself to it", olc::RED);
			DrawString(0, 10, "Dilation and erosion loops: " + std::to_string(dilationStrength), olc::RED);
		}

		if (stage == 4) {
			DrawString(0, 0, "Median filter");
		}
		if (stage == 5) {
			DrawString(0, 0, "Local adaptive thresholding with Erosion/Dilation", olc::RED);
			DrawString(0, 10, std::to_string(ratio), olc::RED);
			DrawString(0, 20, "Up/Down Keys control region sum", olc::RED);
		}
		if (stage == 6) {
			DrawString(0, 0, "Convolution / Blur / Sharpness Keys 0-9 (not numpad) change blur / sharpness loops", olc::RED);
			if (convDemo == 0)
				DrawString(0, 10, "blur loops: " + std::to_string(convLoops), olc::RED);
			if (convDemo == 1)
				DrawString(0, 10, "sharp loops: " + std::to_string(convLoops), olc::RED);
			if (convDemo == 2)
				DrawString(0, 10, "Sobel algorithm loops: " + std::to_string(convLoops), olc::RED);
			DrawString(0, 280, "Up/Down Keys switch between sharpening, blurring and sobel");
		}

		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		// STAGE CHANGE KEY DEFINITIONS /////////////////////////////////////////////////////////////////////////////
		if ((GetKey)(olc::Key::Z).bPressed)
			stage = 0;
		else if ((GetKey)(olc::Key::X).bPressed)
			stage = 1;
		else if ((GetKey)(olc::Key::C).bPressed)
			stage = 2;
		else if ((GetKey)(olc::Key::V).bPressed)
			stage = 3;
		else if ((GetKey)(olc::Key::B).bPressed)
			stage = 4;
		else if ((GetKey)(olc::Key::N).bPressed)
			stage = 5;
		else if ((GetKey)(olc::Key::M).bPressed)
			stage = 6;
		///////////////////////////////////////////////////////////////////////////////////////////////////////////////

		//EXITS THE PROGRAM
		if (GetKey(olc::Key::ESCAPE).bPressed)
		{
			return false;
		}

		return true;
	}
};

int main()
{
	WebCamCapture demo;
	if (demo.Construct(670, 300, 2, 2))
		demo.Start();

	return 0;
}